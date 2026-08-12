#pragma once
#include "Loan.h"
#include "Result.h"
#include "ScopedFile.h"
#include "Serializer.h"
#include <fstream>
#include <vector>
#include <string>

class ILoanRepository {
public:
    virtual std::vector<Loan>      loadAll() = 0;
    virtual bool                   save(const Loan& loan) = 0;
    virtual bool                   saveAll(const std::vector<Loan>& loans) = 0;
    virtual std::vector<Loan>      findByAccount(const std::string& accNum) = 0;
    virtual ~ILoanRepository() = default;
};

class FileLoanRepository : public ILoanRepository {
    std::string filePath;

    void writeOne(std::ostream& os, const Loan& l) {
        Serializer::writeString(os, l.loanId);
        Serializer::writeString(os, l.accountNumber);
        Serializer::writeRaw<double>(os, l.principal);
        Serializer::writeRaw<double>(os, l.outstanding);
        Serializer::writeRaw<double>(os, l.monthlyInstallment);
        Serializer::writeRaw<int>(os, l.totalInstallments);
        Serializer::writeRaw<int>(os, l.paidInstallments);
        Serializer::writeString(os, l.issuedDate);
        Serializer::writeRaw<LoanStatus>(os, l.status);
    }

    Loan readOne(std::istream& is) {
        Loan l;
        l.loanId             = Serializer::readString(is);
        l.accountNumber      = Serializer::readString(is);
        l.principal           = Serializer::readRaw<double>(is);
        l.outstanding         = Serializer::readRaw<double>(is);
        l.monthlyInstallment  = Serializer::readRaw<double>(is);
        l.totalInstallments   = Serializer::readRaw<int>(is);
        l.paidInstallments    = Serializer::readRaw<int>(is);
        l.issuedDate          = Serializer::readString(is);
        l.status              = Serializer::readRaw<LoanStatus>(is);
        return l;
    }

public:
    FileLoanRepository(const std::string& path) : filePath(path) {}

    std::vector<Loan> loadAll() override {
        std::vector<Loan> result;
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open()) return result;
        while (in.peek() != EOF) {
            result.push_back(readOne(in));
        }
        return result;
    }

    bool saveAll(const std::vector<Loan>& loans) override {
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        for (const auto& l : loans) writeOne(out, l);
        return true;
    }

    bool save(const Loan& loan) override {
        auto all = loadAll();
        bool found = false;
        for (auto& l : all) {
            if (l.loanId == loan.loanId) { l = loan; found = true; break; }
        }
        if (!found) all.push_back(loan);
        return saveAll(all);
    }

    std::vector<Loan> findByAccount(const std::string& accNum) override {
        auto all = loadAll();
        std::vector<Loan> result;
        for (auto& l : all) if (l.accountNumber == accNum) result.push_back(l);
        return result;
    }
};