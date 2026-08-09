#pragma once
#include <string>

enum class TxType { Deposit, Withdrawal, Transfer_Debit, Transfer_Credit, Interest, PinChange };

struct Transaction {
    std::string txId;
    std::string accountNumber;
    TxType      type;
    double      amount;
    std::string timestamp;
    double      resultingBalance;
    std::string referenceId;
    std::string description;
};