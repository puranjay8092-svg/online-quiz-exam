
#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <fstream>
#include <iomanip>

class Question {
private:
    std::string text;
    std::array<std::string, 4> options;
    int correctOption;

public:
    Question(std::string questionText,
             std::array<std::string, 4> choices,
             int answer)
        : text(questionText),
          options(choices),
          correctOption(answer) {}

    void display(int number) const {
        std::cout << "\nQ" << number << ". " << text << "\n";
        for (int i = 0; i < 4; i++) {
            std::cout << i + 1 << ". " << options[i] << "\n";
        }
    }

    bool checkAnswer(int answer) const {
        return answer == correctOption;
    }
};

class Result {
private:
    std::string studentName;
    std::string studentId;
    int score;
    int totalQuestions;

public:
    Result(std::string name, std::string id, int marks, int total)
        : studentName(name), studentId(id),
          score(marks), totalQuestions(total) {}

    double percentage() const {
        return totalQuestions == 0
            ? 0.0 : score * 100.0 / totalQuestions;
    }

    void display() const {
        std::cout << "\n===== EXAM RESULT =====\n"
                  << "Student: " << studentName << "\n"
                  << "Student ID: " << studentId << "\n"
                  << "Score: " << score << "/" << totalQuestions << "\n"
                  << std::fixed << std::setprecision(2)
                  << "Percentage: " << percentage() << "%\n";
    }

    void saveToFile() const {
        std::ofstream file("results.csv", std::ios::app);
        if (!file) {
            std::cout << "Could not save result.\n";
            return;
        }

        file << std::quoted(studentName) << ","
             << std::quoted(studentId) << ","
             << score << "," << totalQuestions << ","
             << std::fixed << std::setprecision(2)
             << percentage() << "\n";

        std::cout << "Result saved.\n";
    }
};

class Exam {
private:
    std::vector<Question> questions;

public:
    void addQuestion(const Question& question) {
        questions.push_back(question);
    }

    Result start(const std::string& name,
                 const std::string& studentId) const {
        int score = 0;

        for (int i = 0; i < static_cast<int>(questions.size()); i++) {
            questions[i].display(i + 1);

            int answer;
            std::cout << "Your answer (1-4): ";
            if (!(std::cin >> answer)) {
                std::cout << "Invalid input. Exam cancelled.\n";
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                return Result(name, studentId, 0, 0);
            }

            if (answer < 1 || answer > 4) {
                std::cout << "Invalid option. Exam cancelled.\n";
                return Result(name, studentId, 0, 0);
            }

            if (questions[i].checkAnswer(answer)) {
                std::cout << "Correct!\n";
                score++;
            } else {
                std::cout << "Incorrect!\n";
            }
        }

        return Result(name, studentId, score,
                      static_cast<int>(questions.size()));
    }
};

void teacherDashboard() {
    std::ifstream file("results.csv");

    if (!file) {
        std::cout << "\nNo results file found yet.\n";
        return;
    }

    std::string name, id;
    int score, total;
    double percentage;

    std::cout << "\n========== TEACHER DASHBOARD ==========\n";
    std::cout << std::left
              << std::setw(22) << "Student"
              << std::setw(16) << "Student ID"
              << std::setw(12) << "Score"
              << "Percentage\n";
    std::cout << std::string(62, '-') << "\n";

    while (file >> std::quoted(name)
                && file.ignore(1, ',')
                && file >> std::quoted(id)
                && file.ignore(1, ',')
                && file >> score
                && file.ignore(1, ',')
                && file >> total
                && file.ignore(1, ',')
                && file >> percentage) {
        std::cout << std::left
                  << std::setw(22) << name
                  << std::setw(16) << id
                  << std::setw(12)
                  << (std::to_string(score) + "/" + std::to_string(total))
                  << std::fixed << std::setprecision(2)
                  << percentage << "%\n";
    }
}

int main() {
    int choice;

    std::cout << "===== ONLINE QUIZ SYSTEM =====\n";
    std::cout << "1. Student: Take exam\n";
    std::cout << "2. Teacher: View results\n";
    std::cout << "Choose: ";

    if (!(std::cin >> choice)) {
        std::cout << "Invalid choice.\n";
        return 1;
    }

    if (choice == 1) {
        std::cin.ignore(10000, '\n');

        std::string name, id;
        std::cout << "Enter student name: ";
        std::getline(std::cin, name);

        std::cout << "Enter student ID: ";
        std::getline(std::cin, id);

        Exam exam;

        exam.addQuestion(Question(
            "Which language are we using?",
            {"Python", "C++", "Java", "JavaScript"}, 2
        ));
        exam.addQuestion(Question(
            "Which keyword defines a class in C++?",
            {"function", "object", "class", "define"}, 3
        ));
        exam.addQuestion(Question(
            "Which symbol ends most C++ statements?",
            {".", ";", ":", ","}, 2
        ));

        Result result = exam.start(name, id);
        result.display();
        result.saveToFile();
    } else if (choice == 2) {
        teacherDashboard();
    } else {
        std::cout << "Invalid choice.\n";
    }

    return 0;
}
