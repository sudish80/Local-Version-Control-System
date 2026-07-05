#pragma once

#include <iostream>
#include <string>

namespace vcs {

class Output {
public:
    enum Color {
        RESET = 0,
        BOLD = 1,
        DIM = 2,
        RED = 31,
        GREEN = 32,
        YELLOW = 33,
        BLUE = 34,
        MAGENTA = 35,
        CYAN = 36,
        WHITE = 37,
    };

    void print(const std::string& msg, Color color = RESET) const {
        if (color != RESET)
            std::cout << "\033[" << static_cast<int>(color) << "m";
        std::cout << msg;
        if (color != RESET) std::cout << "\033[0m";
    }

    void println(const std::string& msg, Color color = RESET) const {
        print(msg, color);
        std::cout << "\n";
    }

    void success(const std::string& msg) const {
        println(msg, GREEN);
    }

    void error(const std::string& msg) const {
        std::cerr << "\033[31mError: " << msg << "\033[0m\n";
    }

    void warn(const std::string& msg) const {
        std::cout << "\033[33mWarning: " << msg << "\033[0m\n";
    }

    void info(const std::string& msg) const {
        println(msg, CYAN);
    }

    void header(const std::string& msg) const {
        println(msg, BOLD);
    }
};

} // namespace vcs
