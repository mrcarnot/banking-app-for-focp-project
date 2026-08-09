#pragma once
#include <string>

enum class AccountStatus { Active, Frozen, Locked };

struct Account {
    std::string   accountNumber;
    std::string   holderName;
    std::string   cnic;
    std::string   contact;
    std::string   address;
    double        balance;
    AccountStatus status;
    std::string   pinHash;
    int           pinAttempts;
    double        dailyWithdrawn;
    std::string   lastTransactionDate;

    bool isActive() const { return status == AccountStatus::Active; }

    bool checkPin(const std::string& inputPin) const;
};