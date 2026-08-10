#pragma once
#include <fstream>
#include <string>

class AuditLogger {
    std::string filePath;

public:
    AuditLogger(const std::string& path) : filePath(path) {}

    void log(const std::string& timestamp,
              const std::string& actor,
              const std::string& action,
              const std::string& detail) {
        std::ofstream out(filePath, std::ios::app);
        if (out.is_open()) {
            out << timestamp << " | " << actor << " | " << action << " | " << detail << "\n";
        }
    }
};