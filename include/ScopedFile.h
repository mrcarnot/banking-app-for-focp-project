#pragma once
#include <fstream>
#include <string>

class ScopedFile {
    std::fstream f;
public:
    ScopedFile(const std::string& path, std::ios::openmode mode) {
        f.open(path, mode);
    }

    ~ScopedFile() {
        if (f.is_open()) {
            f.close();
        }
    }

    bool ok() const { return f.is_open(); }

    std::fstream& stream() { return f; }

    // Prevent copying — files shouldn't be duplicated this way
    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;
};