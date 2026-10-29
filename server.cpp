
#include <iostream>
#include <fstream>
#include <sstream>

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

int main() {
    httplib::Server server;

    // Health-check API: confirms the C++ server is running
    server.Get("/api/health",
        [](const httplib::Request& req,
           httplib::Response& res) {

            json response = {
                {"status", "ok"},
                {"message", "C++ server is running"}
            };

            res.set_content(
                response.dump(4),
                "application/json"
            );
        }
    );
    
    // API: Send quiz questions to the website
    server.Get("/api/questions",
        [](const httplib::Request& req,
           httplib::Response& res) {

            json questions = json::array({
                {
                    {"id", 1},
                    {"question", "Which language is used for this project's backend?"},
                    {"options", {"Python", "C++", "Java", "HTML"}}
                },
                {
                    {"id", 2},
                    {"question", "Which OOP concept bundles data and methods together?"},
                    {"options", {"Encapsulation", "Compilation", "Iteration", "Networking"}}
                },
                {
                    {"id", 3},
                    {"question", "Which HTML element creates a button?"},
                    {"options", {"<heading>", "<link>", "<button>", "<image>"}}
                }
            });

            res.set_content(
                questions.dump(4),
                "application/json"
            );
        }
    );


    // Serve the website's home page
    server.Get("/",
        [](const httplib::Request& req,
           httplib::Response& res) {

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

    // Serve CSS, JavaScript, and other website files
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
