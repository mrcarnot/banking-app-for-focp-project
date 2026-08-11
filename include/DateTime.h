#pragma once
#include <string>
#include <ctime>
#include <cstdio>

namespace DateTime {

    // Returns "YYYY-MM-DD HH:MM:SS" for right now
    inline std::string now() {
        std::time_t t = std::time(nullptr);
        std::tm* lt = std::localtime(&t);
        char buf[20];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                      lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                      lt->tm_hour, lt->tm_min, lt->tm_sec);
        return std::string(buf);
    }

    // Returns just "YYYY-MM-DD" for today (used for the daily-withdrawal reset)
    inline std::string today() {
        std::time_t t = std::time(nullptr);
        std::tm* lt = std::localtime(&t);
        char buf[11];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                      lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
        return std::string(buf);
    }
}