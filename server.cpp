#include <ctime>
#include <iomanip>
#include <cstdlib>
#include <map>
#include <mutex>
#include <random>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

// Escape text safely for CSV files.
std::string csvEscape(const std::string& value) {
    std::string result = "\"";

    for (char ch : value) {
        if (ch == '"') {
            result += "\"\"";
        } else {
            result += ch;
        }
    }

    result += "\"";
    return result;
}


std::vector<std::string> parseCsvRow(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool insideQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];

        if (ch == '"') {
            if (insideQuotes && i + 1 < line.size()
                && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (ch == ',' && !insideQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += ch;
        }
    }

    fields.push_back(field);
    return fields;
}

using Clock = std::chrono::steady_clock;

struct TeacherSession {
    Clock::time_point expiresAt;
};

std::map<std::string, TeacherSession> teacherSessions;
std::mutex sessionsMutex;

constexpr auto SESSION_DURATION = std::chrono::hours(2);

std::string getCookie(
    const httplib::Request& req,
    const std::string& cookieName
) {
    if (!req.has_header("Cookie")) {
        return "";
    }

    const std::string cookies = req.get_header_value("Cookie");
    std::istringstream stream(cookies);
    std::string item;

    while (std::getline(stream, item, ';')) {
        const auto first = item.find_first_not_of(" \t");
        if (first == std::string::npos) {
            continue;
        }

        item.erase(0, first);

        const auto equals = item.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        if (item.substr(0, equals) == cookieName) {
            return item.substr(equals + 1);
        }
    }

    return "";
}

std::string createSessionToken() {
    // Generate a 256-bit token represented as 64 hexadecimal characters.
    std::random_device randomDevice;
    std::uniform_int_distribution<unsigned int> distribution(0, 255);
    const char hex[] = "0123456789abcdef";
    std::string token;
    token.reserve(64);

    for (int i = 0; i < 32; ++i) {
        const unsigned int value = distribution(randomDevice);
        token += hex[(value >> 4) & 0x0F];
        token += hex[value & 0x0F];
    }

    return token;
}

bool isTeacherAuthenticated(const httplib::Request& req) {
    const std::string token = getCookie(req, "teacher_session");

    if (token.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(sessionsMutex);
    const auto it = teacherSessions.find(token);

    if (it == teacherSessions.end()) {
        return false;
    }

    if (Clock::now() >= it->second.expiresAt) {
        teacherSessions.erase(it);
        return false;
    }

    return true;
}

void requireTeacherLogin(
    const httplib::Request& req,
    httplib::Response& res
) {
    res.status = 401;
    res.set_header("Cache-Control", "no-store");
    res.set_content(
        R"({"error":"Teacher login required."})",
        "application/json"
    );
}


json loadQuestions() {
    std::ifstream file("questions.json");

    if (!file.is_open()) {
        throw std::runtime_error(
            "Could not open questions.json"
        );
    }

    json data;
    file >> data;

    if (!data.is_object() ||
        !data.contains("questions") ||
        !data["questions"].is_array()) {
        throw std::runtime_error(
            "Invalid questions.json format"
        );
    }

    return data["questions"];
}

bool saveQuestions(const json& questions) {
    if (!questions.is_array()) {
        return false;
    }

    std::ofstream file("questions.json");

    if (!file.is_open()) {
        return false;
    }

    file << json{{"questions", questions}}.dump(4);
    file.close();

    return !file.fail();
}


int main() {
    httplib::Server server;

    const char* teacherUsernameEnv = std::getenv("TEACHER_USERNAME");
const char* teacherPasswordEnv = std::getenv("TEACHER_PASSWORD");

if (teacherUsernameEnv == nullptr ||
    teacherPasswordEnv == nullptr ||
    std::string(teacherUsernameEnv).empty() ||
    std::string(teacherPasswordEnv).empty()) {
    std::cerr
        << "Error: Set TEACHER_USERNAME and TEACHER_PASSWORD "
        << "before starting the server.\n";
    return 1;
}

const std::string teacherUsername = teacherUsernameEnv;
const std::string teacherPassword = teacherPasswordEnv;

    json questions;

try {
    questions = loadQuestions();
} catch (const std::exception& e) {
    std::cerr << "Question loading error: "
              << e.what() << '\n';
    return 1;
}

    // Correct option indexes: question 1 = 1, question 2 = 0,
    // question 3 = 2. Indexes start from zero.
    const int correctAnswers[] = {1, 0, 2};

    // Health-check API
    server.Get("/api/health",
        [](const httplib::Request& req, httplib::Response& res) {
            res.set_content(
                json({
                    {"status", "ok"},
                    {"message", "C++ server is running"}
                }).dump(4),
                "application/json"
            );
        }
    );

    
server.Get("/api/questions",
    [](const httplib::Request& req,
       httplib::Response& res) {
        try {
            json questions = loadQuestions();
            json publicQuestions = json::array();

            for (const auto& question : questions) {
                json item = question;
                item.erase("correctAnswer");
                publicQuestions.push_back(item);
            }

            res.set_header("Cache-Control", "no-store");
            res.set_content(
                publicQuestions.dump(4),
                "application/json"
            );

        } catch (const std::exception& e) {
            std::cerr << "Could not load questions: "
                      << e.what() << '\n';

            res.status = 500;
            res.set_content(
                R"({"error":"Could not load quiz questions."})",
                "application/json"
            );
        }
    }
);


    
server.Post("/api/submit",
    [](const httplib::Request& req,
       httplib::Response& res) {
        try {
            const json data = json::parse(req.body);

            if (!data.is_object() ||
                !data.contains("name") ||
                !data.contains("studentId") ||
                !data.contains("answers") ||
                !data["name"].is_string() ||
                !data["studentId"].is_string() ||
                !data["answers"].is_object()) {
                res.status = 400;
                res.set_content(
                    R"({"error":"Invalid submission data."})",
                    "application/json"
                );
                return;
            }

            const std::string name =
                data["name"].get<std::string>();
            const std::string studentId =
                data["studentId"].get<std::string>();
            const json answers = data["answers"];

            if (name.find_first_not_of(" \t\r\n") ==
                    std::string::npos ||
                studentId.find_first_not_of(" \t\r\n") ==
                    std::string::npos) {
                res.status = 400;
                res.set_content(
                    R"({"error":"Name and student ID are required."})",
                    "application/json"
                );
                return;
            }

            const json questions = loadQuestions();

            if (questions.empty()) {
                res.status = 400;
                res.set_content(
                    R"({"error":"No quiz questions are available."})",
                    "application/json"
                );
                return;
            }

            // Require exactly one answer per current question.
            if (answers.size() != questions.size()) {
                res.status = 400;
                res.set_content(
                    R"({"error":"Please answer every question."})",
                    "application/json"
                );
                return;
            }

            int score = 0;

            for (const auto& question : questions) {
                if (!question.contains("id") ||
                    !question["id"].is_number_integer() ||
                    !question.contains("options") ||
                    !question["options"].is_array() ||
                    !question.contains("correctAnswer") ||
                    !question["correctAnswer"].is_number_integer()) {
                    throw std::runtime_error(
                        "Invalid question data in questions.json"
                    );
                }

                const int questionId =
                    question["id"].get<int>();
                const std::string key =
                    std::to_string(questionId);
                const auto& options = question["options"];
                const int correctAnswer =
                    question["correctAnswer"].get<int>();

                if (questionId < 1 ||
                    options.empty() ||
                    correctAnswer < 0 ||
                    correctAnswer >=
                        static_cast<int>(options.size())) {
                    throw std::runtime_error(
                        "Invalid question ID, options, or answer"
                    );
                }

                if (!answers.contains(key) ||
                    !answers[key].is_number_integer()) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"One or more answers are invalid."})",
                        "application/json"
                    );
                    return;
                }

                const int selected = answers[key].get<int>();

                if (selected < 0 ||
                    selected >= static_cast<int>(options.size())) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"An answer option is invalid."})",
                        "application/json"
                    );
                    return;
                }

                if (selected == correctAnswer) {
                    ++score;
                }
            }

            // Record the local date and time of the submission.
const auto now = std::chrono::system_clock::now();
const std::time_t currentTime =
    std::chrono::system_clock::to_time_t(now);

std::tm localTime{};

#ifdef _WIN32
localtime_s(&localTime, &currentTime);
#else
localtime_r(&currentTime, &localTime);
#endif

std::ostringstream timestampStream;
timestampStream << std::put_time(
    &localTime,
    "%Y-%m-%d %H:%M:%S"
);

const std::string timestamp = timestampStream.str();

// Append the result to results.csv.

            const std::string filename = "results.csv";
            bool needsHeader = false;

            {
                std::ifstream existing(filename);
                needsHeader =
                    !existing.good() ||
                    existing.peek() ==
                        std::ifstream::traits_type::eof();
            }

            std::ofstream file(filename, std::ios::app);

            if (!file.is_open()) {
                res.status = 500;
                res.set_content(
                    R"({"error":"Could not save the result. Check server folder permissions."})",
                    "application/json"
                );
                return;
            }

            if (needsHeader) {
    file << "Student Name,Student ID,Score,Total,Timestamp\n";
}

file << csvEscape(name) << ","
     << csvEscape(studentId) << ","
     << score << ","
     << questions.size() << ","
     << csvEscape(timestamp) << "\n";

            file.close();

            if (!file) {
                res.status = 500;
                res.set_content(
                    R"({"error":"Failed to write the result file."})",
                    "application/json"
                );
                return;
            }

            res.set_header("Cache-Control", "no-store");
            res.set_content(
                json({
                    {"message", "Quiz submitted successfully!"},
                    {"score", score},
                    {"total", questions.size()}
                }).dump(4),
                "application/json"
            );

        } catch (const json::exception& e) {
            res.status = 400;
            res.set_content(
                R"({"error":"Could not read submission JSON."})",
                "application/json"
            );

        } catch (const std::exception& e) {
            std::cerr << "Submission error: "
                      << e.what() << '\n';

            res.status = 500;
            res.set_content(
                R"({"error":"An unexpected server error occurred."})",
                "application/json"
            );
        }
    }
);


    // Teacher login API
server.Post("/api/teacher/login",
    [&teacherUsername, &teacherPassword](
        const httplib::Request& req,
        httplib::Response& res
    ) {
        res.set_header("Cache-Control", "no-store");
        res.set_header("X-Content-Type-Options", "nosniff");

        try {
            const json data = json::parse(req.body);

            if (!data.is_object() ||
                !data.contains("username") ||
                !data.contains("password") ||
                !data["username"].is_string() ||
                !data["password"].is_string()) {
                res.status = 400;
                res.set_content(
                    R"({"error":"Enter a username and password."})",
                    "application/json"
                );
                return;
            }

            const std::string username =
                data["username"].get<std::string>();
            const std::string password =
                data["password"].get<std::string>();

            if (username != teacherUsername ||
                password != teacherPassword) {
                res.status = 401;
                res.set_content(
                    R"({"error":"Invalid username or password."})",
                    "application/json"
                );
                return;
            }

            const std::string token = createSessionToken();

            {
                std::lock_guard<std::mutex> lock(sessionsMutex);
                teacherSessions[token] = {
                    Clock::now() + SESSION_DURATION
                };
            }

            // Secure requires the website to be opened over HTTPS.
            res.set_header(
                "Set-Cookie",
                "teacher_session=" + token +
                "; Path=/; HttpOnly; Secure; SameSite=Strict"
            );

            res.status = 200;
            res.set_content(
                R"({"message":"Login successful."})",
                "application/json"
            );

        } catch (const json::exception&) {
            res.status = 400;
            res.set_content(
                R"({"error":"Invalid request data."})",
                "application/json"
            );
        }
    }
);

// Teacher logout API
server.Post("/api/teacher/logout",
    [](const httplib::Request& req, httplib::Response& res) {
        const std::string token = getCookie(req, "teacher_session");

        if (!token.empty()) {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            teacherSessions.erase(token);
        }

        res.set_header("Cache-Control", "no-store");
        res.set_header(
            "Set-Cookie",
            "teacher_session=; Path=/; HttpOnly; Secure; "
            "SameSite=Strict; Max-Age=0"
        );

        res.set_content(
            R"({"message":"Logged out."})",
            "application/json"
        );
    }
);
    
// Protect the teacher dashboard page.
server.Get("/teacher.html",
    [](const httplib::Request& req, httplib::Response& res) {
        if (!isTeacherAuthenticated(req)) {
            res.status = 302;
            res.set_header("Location", "/teacher-login.html");
            res.set_header("Cache-Control", "no-store");
            return;
        }

        std::ifstream file("web/teacher.html");

        if (!file.is_open()) {
            res.status = 404;
            res.set_content("Teacher dashboard not found.", "text/plain");
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        res.set_header("Cache-Control", "no-store");
        res.set_content(buffer.str(), "text/html");
    }
);

    
    // API: Return saved student results for the teacher dashboard.
    server.Get("/api/results",
        [](const httplib::Request& req,
           httplib::Response& res) {

            if (!isTeacherAuthenticated(req)) {
                requireTeacherLogin(req, res);
                return;
            }

            res.set_header("Cache-Control", "no-store");

            std::ifstream file("results.csv");
            json results = json::array();

            // No submissions yet: return an empty result list.
            if (!file.is_open()) {
                res.set_content(
                    results.dump(4),
                    "application/json"
                );
                return;
            }

            std::string line;

            // Skip the CSV header.
            if (std::getline(file, line)) {
                while (std::getline(file, line)) {
                    if (line.empty()) {
                        continue;
                    }

                    std::vector<std::string> fields =
                        parseCsvRow(line);

                    // Support old rows (4 fields) and new rows (5).
                    if (fields.size() != 4 &&
                        fields.size() != 5) {
                        continue;
                    }

                    try {
                        int score = std::stoi(fields[2]);
                        int total = std::stoi(fields[3]);

                        std::string timestamp = "Unavailable";

                        if (fields.size() == 5 &&
                            !fields[4].empty()) {
                            timestamp = fields[4];
                        }

                        results.push_back({
                            {"name", fields[0]},
                            {"studentId", fields[1]},
                            {"score", score},
                            {"total", total},
                            {"timestamp", timestamp}
                        });
                    } catch (const std::exception&) {
                        // Skip malformed rows.
                    }
                }
            }

            res.set_content(
                results.dump(4),
                "application/json"
            );
        }
    );
    
    // ==========================================
    // Teacher Question Management API
    // ==========================================

    // Get all questions, including answers, for authenticated teachers.
    server.Get("/api/teacher/questions",
        [](const httplib::Request& req, httplib::Response& res) {
            if (!isTeacherAuthenticated(req)) {
                requireTeacherLogin(req, res);
                return;
            }

            try {
                json questions = loadQuestions();
                res.set_header("Cache-Control", "no-store");
                res.set_content(
                    questions.dump(4),
                    "application/json"
                );
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(
                    R"({"error":"Could not load questions."})",
                    "application/json"
                );
            }
        }
    );

    // Add a new question.
    server.Post("/api/teacher/questions",
        [](const httplib::Request& req, httplib::Response& res) {
            if (!isTeacherAuthenticated(req)) {
                requireTeacherLogin(req, res);
                return;
            }

            try {
                json body = json::parse(req.body);

                if (!body.is_object() ||
                    !body.contains("question") ||
                    !body["question"].is_string() ||
                    !body.contains("options") ||
                    !body["options"].is_array() ||
                    body["options"].size() != 4 ||
                    !body.contains("correctAnswer") ||
                    !body["correctAnswer"].is_number_integer()) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"Provide a question, four options, and a valid answer index."})",
                        "application/json"
                    );
                    return;
                }

                std::string questionText =
                    body["question"].get<std::string>();

                json options = body["options"];
                int correctAnswer =
                    body["correctAnswer"].get<int>();

                if (questionText.empty() ||
                    correctAnswer < 0 ||
                    correctAnswer > 3) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"Question text or answer index is invalid."})",
                        "application/json"
                    );
                    return;
                }

                for (const auto& option : options) {
                    if (!option.is_string() ||
                        option.get<std::string>().empty()) {
                        res.status = 400;
                        res.set_content(
                            R"({"error":"All four options must be non-empty strings."})",
                            "application/json"
                        );
                        return;
                    }
                }

                json questions = loadQuestions();
                int nextId = 1;

                for (const auto& q : questions) {
                    if (q.contains("id") && q["id"].is_number_integer()) {
                        nextId = std::max(
                            nextId,
                            q["id"].get<int>() + 1
                        );
                    }
                }

                questions.push_back({
                    {"id", nextId},
                    {"question", questionText},
                    {"options", options},
                    {"correctAnswer", correctAnswer}
                });

                if (!saveQuestions(questions)) {
                    res.status = 500;
                    res.set_content(
                        R"({"error":"Could not save questions."})",
                        "application/json"
                    );
                    return;
                }

                res.status = 201;
                res.set_content(
                    json{{"message", "Question added."},
                         {"id", nextId}}.dump(),
                    "application/json"
                );
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(
                    R"({"error":"Invalid question data or question file."})",
                    "application/json"
                );
            }
        }
    );

    // Edit an existing question.
    server.Put("/api/teacher/questions",
        [](const httplib::Request& req, httplib::Response& res) {
            if (!isTeacherAuthenticated(req)) {
                requireTeacherLogin(req, res);
                return;
            }

            try {
                json body = json::parse(req.body);

                if (!body.is_object() ||
                    !body.contains("id") ||
                    !body["id"].is_number_integer() ||
                    !body.contains("question") ||
                    !body["question"].is_string() ||
                    !body.contains("options") ||
                    !body["options"].is_array() ||
                    body["options"].size() != 4 ||
                    !body.contains("correctAnswer") ||
                    !body["correctAnswer"].is_number_integer()) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"Invalid question data."})",
                        "application/json"
                    );
                    return;
                }

                int id = body["id"].get<int>();
                int correctAnswer = body["correctAnswer"].get<int>();
                std::string questionText =
                    body["question"].get<std::string>();
                json options = body["options"];

                if (id <= 0 ||
                    questionText.empty() ||
                    correctAnswer < 0 ||
                    correctAnswer > 3) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"Question data is invalid."})",
                        "application/json"
                    );
                    return;
                }

                for (const auto& option : options) {
                    if (!option.is_string() ||
                        option.get<std::string>().empty()) {
                        res.status = 400;
                        res.set_content(
                            R"({"error":"All four options must be non-empty strings."})",
                            "application/json"
                        );
                        return;
                    }
                }

                json questions = loadQuestions();
                bool found = false;

                for (auto& q : questions) {
                    if (q.contains("id") &&
                        q["id"].is_number_integer() &&
                        q["id"].get<int>() == id) {
                        q["question"] = questionText;
                        q["options"] = options;
                        q["correctAnswer"] = correctAnswer;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    res.status = 404;
                    res.set_content(
                        R"({"error":"Question not found."})",
                        "application/json"
                    );
                    return;
                }

                if (!saveQuestions(questions)) {
                    res.status = 500;
                    res.set_content(
                        R"({"error":"Could not save questions."})",
                        "application/json"
                    );
                    return;
                }

                res.set_content(
                    R"({"message":"Question updated."})",
                    "application/json"
                );
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(
                    R"({"error":"Invalid question data or question file."})",
                    "application/json"
                );
            }
        }
    );

    // Delete a question by ID.
    server.Delete("/api/teacher/questions",
        [](const httplib::Request& req, httplib::Response& res) {
            if (!isTeacherAuthenticated(req)) {
                requireTeacherLogin(req, res);
                return;
            }

            try {
                json body = json::parse(req.body);

                if (!body.is_object() ||
                    !body.contains("id") ||
                    !body["id"].is_number_integer()) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"Provide a valid question ID."})",
                        "application/json"
                    );
                    return;
                }

                int id = body["id"].get<int>();
                json questions = loadQuestions();

                auto it = std::find_if(
                    questions.begin(),
                    questions.end(),
                    [id](const json& q) {
                        return q.contains("id") &&
                               q["id"].is_number_integer() &&
                               q["id"].get<int>() == id;
                    }
                );

                if (it == questions.end()) {
                    res.status = 404;
                    res.set_content(
                        R"({"error":"Question not found."})",
                        "application/json"
                    );
                    return;
                }

                if (questions.size() <= 1) {
                    res.status = 400;
                    res.set_content(
                        R"({"error":"At least one question must remain."})",
                        "application/json"
                    );
                    return;
                }

                questions.erase(it);

                if (!saveQuestions(questions)) {
                    res.status = 500;
                    res.set_content(
                        R"({"error":"Could not save questions."})",
                        "application/json"
                    );
                    return;
                }

                res.set_content(
                    R"({"message":"Question deleted."})",
                    "application/json"
                );
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(
                    R"({"error":"Invalid request or question file."})",
                    "application/json"
                );
            }
        }
    );



    // Serve the homepage
    server.Get("/",
        [](const httplib::Request& req, httplib::Response& res) {
            std::ifstream file("web/index.html");

            if (!file.is_open()) {
                res.status = 404;
                res.set_content(
                    "index.html not found. Check the web folder.",
                    "text/plain"
                );
                return;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            res.set_content(buffer.str(), "text/html");
        }
    );

    // Serve CSS, JavaScript, and other static files.
    if (!server.set_mount_point("/", "./web")) {
        std::cerr << "Error: Could not mount the web folder.\n";
        return 1;
    }

    std::cout << "Online Quiz System server is starting...\n";
    std::cout << "Website: http://localhost:8080\n";
    std::cout << "Health check: http://localhost:8080/api/health\n";
    std::cout << "Press Ctrl+C to stop the server.\n";

    if (!server.listen("127.0.0.1", 8080)) {
        std::cerr << "Could not start server on port 8080.\n";
        return 1;
    }

    return 0;
}
