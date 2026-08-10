#pragma once
#include "Account.h"
#include "Transaction.h"
#include "Validator.h"
#include "Repository.h"
#include "Result.h"
#include <string>
#include <utility>

class TransactionEngine {
    IAccountRepository&     accountRepo;
    ITransactionRepository& txRepo;

    std::string makeTxId() {
        static int counter = 1000;
        return "TX" + std::to_string(++counter);
    }

public:
    TransactionEngine(IAccountRepository& accRepo, ITransactionRepository& transRepo)
        : accountRepo(accRepo), txRepo(transRepo) {}

    Result<Transaction, std::string> deposit(Account& acc, double amount, const std::string& timestamp) {
        if (!Validator::isValidAmount(amount)) {
            return Result<Transaction, std::string>::failure("Invalid deposit amount");
        }
        if (!Validator::isAccountActive(acc)) {
            return Result<Transaction, std::string>::failure("Account is not active");
        }

        double newBalance = acc.balance + amount;

        Transaction tx;
        tx.txId = makeTxId();
        tx.accountNumber = acc.accountNumber;
        tx.type = TxType::Deposit;
        tx.amount = amount;
        tx.timestamp = timestamp;
        tx.resultingBalance = newBalance;
        tx.referenceId = "";
        tx.description = "Cash deposit";

        acc.balance = newBalance;
        if (!accountRepo.save(acc)) {
            acc.balance -= amount;   // rollback in-memory change
            return Result<Transaction, std::string>::failure("Failed to save account");
        }
        txRepo.append(tx);
        return Result<Transaction, std::string>::success(tx);
    }

    Result<Transaction, std::string> withdraw(Account& acc, double amount, const std::string& timestamp) {
        if (!Validator::isValidAmount(amount)) {
            return Result<Transaction, std::string>::failure("Invalid withdrawal amount");
        }
        if (!Validator::isAccountActive(acc)) {
            return Result<Transaction, std::string>::failure("Account is not active");
        }
        if (!Validator::respectsMinBalance(acc, amount)) {
            return Result<Transaction, std::string>::failure("Withdrawal would breach minimum balance");
        }
        if (!Validator::isWithinDailyLimit(acc, amount)) {
            return Result<Transaction, std::string>::failure("Withdrawal exceeds daily limit");
        }

        double newBalance = acc.balance - amount;

        Transaction tx;
        tx.txId = makeTxId();
        tx.accountNumber = acc.accountNumber;
        tx.type = TxType::Withdrawal;
        tx.amount = amount;
        tx.timestamp = timestamp;
        tx.resultingBalance = newBalance;
        tx.referenceId = "";
        tx.description = "Cash withdrawal";

        acc.balance = newBalance;
        acc.dailyWithdrawn += amount;

        if (!accountRepo.save(acc)) {
            acc.balance += amount;             // rollback
            acc.dailyWithdrawn -= amount;
            return Result<Transaction, std::string>::failure("Failed to save account");
        }
        txRepo.append(tx);
        return Result<Transaction, std::string>::success(tx);
    }

    Result<std::pair<Transaction, Transaction>, std::string>
    transfer(Account& from, Account& to, double amount, const std::string& timestamp) {
        // --- Validate EVERYTHING first, before touching any real data ---
        if (!Validator::isValidAmount(amount)) {
            return Result<std::pair<Transaction, Transaction>, std::string>::failure("Invalid transfer amount");
        }
        if (!Validator::isAccountActive(from)) {
            return Result<std::pair<Transaction, Transaction>, std::string>::failure("Sender account not active");
        }
        if (!Validator::isAccountActive(to)) {
            return Result<std::pair<Transaction, Transaction>, std::string>::failure("Receiver account not active");
        }
        if (from.accountNumber == to.accountNumber) {
            return Result<std::pair<Transaction, Transaction>, std::string>::failure("Cannot transfer to the same account");
        }
        if (!Validator::respectsMinBalance(from, amount)) {
            return Result<std::pair<Transaction, Transaction>, std::string>::failure("Transfer would breach sender's minimum balance");
        }
        if (!Validator::isWithinDailyLimit(from, amount)) {
            return Result<std::pair<Transaction, Transaction>, std::string>::failure("Transfer exceeds sender's daily limit");
        }

        // --- Take a backup of both accounts' current state, in case we need to roll back ---
        Account fromBackup = from;
        Account toBackup = to;

        std::string refId = "REF" + makeTxId();

        from.balance -= amount;
        from.dailyWithdrawn += amount;
        to.balance += amount;

        Transaction debitTx;
        debitTx.txId = makeTxId();
        debitTx.accountNumber = from.accountNumber;
        debitTx.type = TxType::Transfer_Debit;
        debitTx.amount = amount;
        debitTx.timestamp = timestamp;
        debitTx.resultingBalance = from.balance;
        debitTx.referenceId = refId;
        debitTx.description = "Transfer to " + to.accountNumber;

        Transaction creditTx;
        creditTx.txId = makeTxId();
        creditTx.accountNumber = to.accountNumber;
        creditTx.type = TxType::Transfer_Credit;
        creditTx.amount = amount;
        creditTx.timestamp = timestamp;
        creditTx.resultingBalance = to.balance;
        creditTx.referenceId = refId;
        creditTx.description = "Transfer from " + from.accountNumber;

        // --- Attempt to persist both accounts. If either fails, roll back BOTH in memory. ---
        bool fromSaved = accountRepo.save(from);
        bool toSaved = fromSaved && accountRepo.save(to);

        if (!fromSaved || !toSaved) {
            from = fromBackup;
            to = toBackup;
            if (fromSaved && !toSaved) {
                accountRepo.save(from);   // undo the partial save on disk too
            }
            return Result<std::pair<Transaction, Transaction>, std::string>::failure("Failed to save one or both accounts - transfer rolled back");
        }

        txRepo.append(debitTx);
        txRepo.append(creditTx);

        return Result<std::pair<Transaction, Transaction>, std::string>::success({debitTx, creditTx});
    }
};