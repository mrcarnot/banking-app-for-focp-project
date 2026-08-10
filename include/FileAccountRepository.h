#pragma once
#include "Repository.h"
#include "ScopedFile.h"
#include "Serializer.h"
#include <fstream>

class FileAccountRepository : public IAccountRepository {
    std::string filePath;

    void writeOne(std::ostream& os, const Account& a) {
        Serializer::writeString(os, a.accountNumber);
        Serializer::writeString(os, a.holderName);
        Serializer::writeString(os, a.cnic);
        Serializer::writeString(os, a.contact);
        Serializer::writeString(os, a.address);
        Serializer::writeRaw<double>(os, a.balance);
        Serializer::writeRaw<AccountStatus>(os, a.status);
        Serializer::writeString(os, a.pinHash);
        Serializer::writeRaw<int>(os, a.pinAttempts);
        Serializer::writeRaw<double>(os, a.dailyWithdrawn);
        Serializer::writeString(os, a.lastTransactionDate);
        Serializer::writeRaw<AccountType>(os, a.type);
        Serializer::writeString(os, a.lockTimestamp);
        Serializer::writeString(os, a.openedDate);
        Serializer::writeRaw<double>(os, a.archivedBalance);
        Serializer::writeRaw<double>(os, a.loanOutstanding);
    }

    Account readOne(std::istream& is) {
        Account a;
        a.accountNumber       = Serializer::readString(is);
        a.holderName          = Serializer::readString(is);
        a.cnic                = Serializer::readString(is);
        a.contact              = Serializer::readString(is);
        a.address              = Serializer::readString(is);
        a.balance              = Serializer::readRaw<double>(is);
        a.status               = Serializer::readRaw<AccountStatus>(is);
        a.pinHash              = Serializer::readString(is);
        a.pinAttempts          = Serializer::readRaw<int>(is);
        a.dailyWithdrawn       = Serializer::readRaw<double>(is);
        a.lastTransactionDate  = Serializer::readString(is);
        a.type                 = Serializer::readRaw<AccountType>(is);
        a.lockTimestamp        = Serializer::readString(is);
        a.openedDate           = Serializer::readString(is);
        a.archivedBalance      = Serializer::readRaw<double>(is);
        a.loanOutstanding      = Serializer::readRaw<double>(is);
        return a;
    }

public:
    FileAccountRepository(const std::string& path) : filePath(path) {}

    std::vector<Account> loadAll() override {
        std::vector<Account> result;
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open()) return result;   // file doesn't exist yet -> empty list, not an error

        while (in.peek() != EOF) {
            result.push_back(readOne(in));
        }
        return result;
    }

    bool saveAll(const std::vector<Account>& accounts) override {
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        for (const auto& a : accounts) {
            writeOne(out, a);
        }
        return true;
    }

    bool save(const Account& acc) override {
        auto all = loadAll();
        bool found = false;
        for (auto& a : all) {
            if (a.accountNumber == acc.accountNumber) {
                a = acc;
                found = true;
                break;
            }
        }
        if (!found) all.push_back(acc);
        return saveAll(all);
    }

    Result<Account, std::string> findByNumber(const std::string& num) override {
        auto all = loadAll();
        for (auto& a : all) {
            if (a.accountNumber == num) {
                return Result<Account, std::string>::success(a);
            }
        }
        return Result<Account, std::string>::failure("Account not found: " + num);
    }
};