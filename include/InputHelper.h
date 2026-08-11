#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <limits>

namespace InputHelper {

    inline std::string getString(const std::string& prompt) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        return line;
    }

    inline int getInt(const std::string& prompt) {
        while (true) {
            std::cout << prompt;
            std::string line;
            std::getline(std::cin, line);
            std::stringstream ss(line);
            int value;
            char leftover;
            if ((ss >> value) && !(ss >> leftover)) {
                return value;
            }
            std::cout << "  Invalid input. Please enter a whole number.\n";
        }
    }

    inline double getDouble(const std::string& prompt) {
        while (true) {
            std::cout << prompt;
            std::string line;
            std::getline(std::cin, line);
            std::stringstream ss(line);
            double value;
            char leftover;
            if ((ss >> value) && !(ss >> leftover)) {
                return value;
            }
            std::cout << "  Invalid input. Please enter a number.\n";
        }
    }

    inline double getPositiveDouble(const std::string& prompt) {
        while (true) {
            double value = getDouble(prompt);
            if (value > 0) return value;
            std::cout << "  Amount must be greater than zero.\n";
        }
    }
}