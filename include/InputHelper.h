#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <limits>
#include <conio.h>

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

    // BONUS #1: hidden PIN entry (shows * instead of the typed digits)
    inline std::string getHidden(const std::string& prompt) {
        std::cout << prompt;
        std::string input;
        char ch;
        while ((ch = _getch()) != '\r') {
            if (ch == '\b') {
                if (!input.empty()) {
                    input.pop_back();
                    std::cout << "\b \b";
                }
            } else {
                input += ch;
                std::cout << '*';
            }
        }
        std::cout << "\n";
        return input;
    }
}