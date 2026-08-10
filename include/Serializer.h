#pragma once
#include <fstream>
#include <string>

namespace Serializer {

    inline void writeString(std::ostream& os, const std::string& s) {
        size_t len = s.size();
        os.write(reinterpret_cast<const char*>(&len), sizeof(len));
        os.write(s.data(), len);
    }

    inline std::string readString(std::istream& is) {
        size_t len = 0;
        is.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string s(len, '\0');
        is.read(&s[0], len);
        return s;
    }

    template<typename T>
    inline void writeRaw(std::ostream& os, const T& value) {
        os.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    template<typename T>
    inline T readRaw(std::istream& is) {
        T value;
        is.read(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }
}