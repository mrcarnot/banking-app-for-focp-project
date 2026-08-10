#pragma once
#include "Repository.h"
#include "Serializer.h"
#include <fstream>
#include <algorithm>

class FileTransactionRepository : public ITransactionRepository {
    std::string filePath;

    void writeOne(std::ostream& os, const Transaction& t) {
        Serializer::writeString(os, t.txId);
        Serializer::writeString(os, t.accountNumber);
        Serializer::writeRaw<TxType>(os, t.type);
        Serializer::writeRaw<double>(os, t.amount);
        Serializer::writeString(os, t.timestamp);
        Serializer::writeRaw<double>(os, t.resultingBalance);
        Serializer::writeString(os, t.referenceId);
        Serializer::writeString(os, t.description);
    }

    Transaction readOne(std::istream& is) {
        Transaction t;
        t.txId              = Serializer::readString(is);
        t.accountNumber     = Serializer::readString(is);
        t.type              = Serializer::readRaw<TxType>(is);
        t.amount            = Serializer::readRaw<double>(is);
        t.timestamp         = Serializer::readString(is);
        t.resultingBalance  = Serializer::readRaw<double>(is);
        t.referenceId       = Serializer::readString(is);
        t.description       = Serializer::readString(is);
        return t;
    }

public:
    FileTransactionRepository(const std::string& path) : filePath(path) {}

    bool append(const Transaction& tx) override {
        std::ofstream out(filePath, std::ios::binary | std::ios::app);
        if (!out.is_open()) return false;
        writeOne(out, tx);
        return true;
    }

    std::vector<Transaction> loadAll() override {
        std::vector<Transaction> result;
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open()) return result;

        while (in.peek() != EOF) {
            result.push_back(readOne(in));
        }
        return result;
    }

    std::vector<Transaction> loadForAccount(const std::string& num) override {
        auto all = loadAll();
        std::vector<Transaction> result;
        for (auto& t : all) {
            if (t.accountNumber == num) result.push_back(t);
        }
        return result;
    }

    std::vector<Transaction> query(const TxFilter& f) override {
        auto all = loadAll();
        std::vector<Transaction> result;
        for (auto& t : all) {
            if (!f.accountNumber.empty() && t.accountNumber != f.accountNumber) continue;
            if (!f.dateFrom.empty() && t.timestamp < f.dateFrom) continue;
            if (!f.dateTo.empty() && t.timestamp > f.dateTo) continue;
            if (f.filterByType && t.type != f.type) continue;
            if (f.minAmount >= 0 && t.amount < f.minAmount) continue;
            if (f.maxAmount >= 0 && t.amount > f.maxAmount) continue;
            result.push_back(t);
        }
        return result;
    }
};