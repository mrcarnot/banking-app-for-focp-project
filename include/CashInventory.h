#pragma once
#include "ScopedFile.h"
#include "Serializer.h"
#include "Config.h"
#include <fstream>
#include <string>

class CashInventory {
    std::string filePath;

    double readAmount() {
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open()) {
            return Config::ATM_STARTING_CASH;  // file doesn't exist yet -> start fresh
        }
        return Serializer::readRaw<double>(in);
    }

    void writeAmount(double amount) {
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        Serializer::writeRaw<double>(out, amount);
    }

public:
    CashInventory(const std::string& path) : filePath(path) {}

    double getBalance() {
        return readAmount();
    }

    bool hasEnough(double amount) {
        return readAmount() >= amount;
    }

    void decrease(double amount) {
        double current = readAmount();
        writeAmount(current - amount);
    }

    void increase(double amount) {
        double current = readAmount();
        writeAmount(current + amount);
    }
};