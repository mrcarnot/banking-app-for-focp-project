#pragma once
#include <string>

enum class AccountStatus { Active, Frozen, Locked, Closed };
enum class AccountType   { Current, Savings };

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

    AccountType   type;
    std::string   lockTimestamp;
    std::string   openedDate;
    double        archivedBalance;
    double        loanOutstanding;

    bool isActive() const { return status == AccountStatus::Active; }
    bool isClosed() const { return status == AccountStatus::Closed; }
    bool checkPin(const std::string& inputPin) const;
};