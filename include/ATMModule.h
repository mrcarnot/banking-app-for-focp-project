#pragma once
#include <random>
#include "FileAccountRepository.h"
#include "FileTransactionRepository.h"
#include "TransactionEngine.h"
#include "Validator.h"
#include "AuditLogger.h"
#include "InputHelper.h"
#include "DateTime.h"
#include "CashInventory.h"
#include "Config.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>

class ATMModule {
    IAccountRepository&     accountRepo;
    ITransactionRepository& txRepo;
    TransactionEngine       engine;
    AuditLogger&            audit;
    CashInventory           atmCash;
    Account                 currentAccount;

    std::string txTypeToString(TxType t) {
        switch (t) {
            case TxType::Deposit:          return "Deposit";
            case TxType::Withdrawal:       return "Withdrawal";
            case TxType::Transfer_Debit:   return "Transfer-Out";
            case TxType::Transfer_Credit:  return "Transfer-In";
            case TxType::Interest:         return "Interest";
            case TxType::PinChange:        return "PIN-Change";
            case TxType::LoanDisbursement: return "Loan-Out";
            case TxType::LoanRepayment:    return "Loan-Repay";
        }
        return "Unknown";
    }

    void resetDailyIfNewDay(Account& acc) {
        std::string today = DateTime::today();
        if (acc.lastTransactionDate != today) {
            acc.dailyWithdrawn = 0.0;
            acc.lastTransactionDate = today;
        }
    }

    // BONUS #4: receipt generation
    void writeReceipt(const std::string& txId, const std::string& type, double amount, double newBalance) {
        std::ofstream out("../receipts/receipt_" + txId + ".txt");
        if (!out.is_open()) return;
        out << "========================================\n";
        out << "           TRANSACTION RECEIPT\n";
        out << "========================================\n";
        out << "Account:     " << currentAccount.accountNumber << "\n";
        out << "Holder:      " << currentAccount.holderName << "\n";
        out << "Type:        " << type << "\n";
        out << "Amount:      " << std::fixed << std::setprecision(2) << amount << "\n";
        out << "New Balance: " << std::fixed << std::setprecision(2) << newBalance << "\n";
        out << "Timestamp:   " << DateTime::now() << "\n";
        out << "========================================\n";
    }
    // BONUS #7: OTP generation for large transfers
    std::string generateOTP() {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(1000, 9999);
        return std::to_string(dist(rng));
    }
    // --- Feature 1: Authentication ---
    bool authenticate() {
        std::string num = InputHelper::getString("Account number: ");
        auto res = accountRepo.findByNumber(num);
        if (!res.ok) {
            std::cout << "Account not found.\n";
            return false;
        }

        Account acc = res.value;

        if (acc.status == AccountStatus::Closed) {
            std::cout << "This account is closed.\n";
            return false;
        }
        if (acc.status == AccountStatus::Frozen) {
            std::cout << "This account is frozen. Contact the bank.\n";
            return false;
        }
        if (acc.status == AccountStatus::Locked) {
            std::cout << "This account is locked due to repeated failed PIN attempts.\n";
            std::cout << "Locked at: " << acc.lockTimestamp << ". Contact the bank to unlock.\n";
            return false;
        }

        std::string pin = InputHelper::getHidden("PIN: ");

        if (Validator::hashPin(pin) == acc.pinHash) {
            acc.pinAttempts = 0;
            accountRepo.save(acc);
            currentAccount = acc;
            audit.log(DateTime::now(), acc.accountNumber, "LOGIN", "ATM login successful");
            return true;
        }

        // Wrong PIN
        acc.pinAttempts++;
        std::cout << "Incorrect PIN. Attempt " << acc.pinAttempts << " of " << Config::MAX_PIN_ATTEMPTS << ".\n";

        if (acc.pinAttempts >= Config::MAX_PIN_ATTEMPTS) {
            acc.status = AccountStatus::Locked;
            acc.lockTimestamp = DateTime::now();
            std::cout << "Too many failed attempts. Account is now locked.\n";
            audit.log(DateTime::now(), acc.accountNumber, "AUTO_LOCK", "Account locked after 3 failed PIN attempts");
        }
        accountRepo.save(acc);
        return false;
    }

    // --- Feature 2: Balance inquiry ---
    void checkBalance() {
        std::cout << "\nCurrent balance: " << std::fixed << std::setprecision(2) << currentAccount.balance << "\n";
    }

    // --- Feature 3: Withdrawal ---
    void withdraw() {
        resetDailyIfNewDay(currentAccount);
        double amount = InputHelper::getPositiveDouble("\nAmount to withdraw: ");

        if (!atmCash.hasEnough(amount)) {
            std::cout << "Withdrawal failed: ATM does not have enough cash available. Please try a smaller amount or visit a branch.\n";
            audit.log(DateTime::now(), currentAccount.accountNumber, "ATM_CASH_SHORTAGE",
                      "Withdrawal of " + std::to_string(amount) + " blocked - insufficient ATM cash");
            return;
        }

        auto result = engine.withdraw(currentAccount, amount, DateTime::now());
        if (result.ok) {
            atmCash.decrease(amount);
            std::cout << "Withdrawal successful. New balance: "
                      << std::fixed << std::setprecision(2) << currentAccount.balance << "\n";
            audit.log(DateTime::now(), currentAccount.accountNumber, "WITHDRAW",
                      "Withdrew " + std::to_string(amount));
            writeReceipt(result.value.txId, "Withdrawal", amount, currentAccount.balance);
        } else {
            std::cout << "Withdrawal failed: " << result.error << "\n";
        }
    }

    // --- Feature 4: Deposit ---
    void deposit() {
        double amount = InputHelper::getPositiveDouble("\nAmount to deposit: ");

        auto result = engine.deposit(currentAccount, amount, DateTime::now());
        if (result.ok) {
            atmCash.increase(amount);
            std::cout << "Deposit successful. New balance: "
                      << std::fixed << std::setprecision(2) << currentAccount.balance << "\n";
            audit.log(DateTime::now(), currentAccount.accountNumber, "DEPOSIT",
                      "Deposited " + std::to_string(amount));
            writeReceipt(result.value.txId, "Deposit", amount, currentAccount.balance);
        } else {
            std::cout << "Deposit failed: " << result.error << "\n";
        }
    }

    // --- Feature 5: Fund transfer ---
    void transfer() {
        resetDailyIfNewDay(currentAccount);
        std::string toNum = InputHelper::getString("\nTransfer to account number: ");

        auto destRes = accountRepo.findByNumber(toNum);
        if (!destRes.ok) {
            std::cout << "Destination account not found.\n";
            return;
        }
        Account destAcc = destRes.value;

        double amount = InputHelper::getPositiveDouble("Amount to transfer: ");

        // BONUS #7 hook: large transfers need OTP - checked here, implemented fully in a later phase
        if (amount >= Config::OTP_THRESHOLD) {
            std::string otp = generateOTP();
            std::cout << "\n[SMS Simulation] Your one-time code is: " << otp << "\n";
            std::string entered = InputHelper::getString("Enter the OTP to confirm this transfer: ");
            if (entered != otp) {
                std::cout << "Incorrect OTP. Transfer cancelled.\n";
                return;
            }
        }

        auto result = engine.transfer(currentAccount, destAcc, amount, DateTime::now());
        if (result.ok) {
            std::cout << "Transfer successful. Your new balance: "
                      << std::fixed << std::setprecision(2) << currentAccount.balance << "\n";
            audit.log(DateTime::now(), currentAccount.accountNumber, "TRANSFER",
                      "Transferred " + std::to_string(amount) + " to " + toNum);
            writeReceipt(result.value.first.txId, "Transfer-Out", amount, currentAccount.balance);
        } else {
            std::cout << "Transfer failed: " << result.error << "\n";
        }
    }

    // --- Feature 6: Mini-statement ---
    void miniStatement() {
        auto all = txRepo.loadForAccount(currentAccount.accountNumber);

        std::sort(all.begin(), all.end(), [](const Transaction& a, const Transaction& b) {
            return a.timestamp > b.timestamp;
        });

        int count = std::min((int)all.size(), Config::MINI_STATEMENT_COUNT);

        std::cout << "\n=== Last " << count << " Transactions ===\n";
        std::cout << std::left
                  << std::setw(16) << "Type"
                  << std::setw(12) << "Amount"
                  << std::setw(22) << "Timestamp" << "\n";
        std::cout << std::string(50, '-') << "\n";
        for (int i = 0; i < count; i++) {
            std::cout << std::left
                      << std::setw(16) << txTypeToString(all[i].type)
                      << std::setw(12) << std::fixed << std::setprecision(2) << all[i].amount
                      << std::setw(22) << all[i].timestamp << "\n";
        }
        if (all.empty()) std::cout << "No transactions yet.\n";
    }

    // --- Feature 7: Change PIN ---
    void changePin() {
        std::string current = InputHelper::getHidden("\nCurrent PIN: ");
        if (Validator::hashPin(current) != currentAccount.pinHash) {
            std::cout << "Incorrect current PIN.\n";
            return;
        }

        std::string newPin = InputHelper::getHidden("New PIN (4 digits): ");
        if (!Validator::isValidPin(newPin)) {
            std::cout << "Invalid PIN format. Must be exactly 4 digits.\n";
            return;
        }

        std::string confirmPin = InputHelper::getHidden("Confirm new PIN: ");
        if (newPin != confirmPin) {
            std::cout << "PINs do not match. PIN not changed.\n";
            return;
        }

        currentAccount.pinHash = Validator::hashPin(newPin);
        if (accountRepo.save(currentAccount)) {
            std::cout << "PIN changed successfully.\n";

            Transaction tx;
            tx.txId = engine.makeTxId();
            tx.accountNumber = currentAccount.accountNumber;
            tx.type = TxType::PinChange;
            tx.amount = 0;
            tx.timestamp = DateTime::now();
            tx.resultingBalance = currentAccount.balance;
            tx.referenceId = "";
            tx.description = "PIN changed by customer";
            txRepo.append(tx);

            audit.log(DateTime::now(), currentAccount.accountNumber, "PIN_CHANGE", "Customer changed PIN");
        } else {
            std::cout << "Failed to save new PIN.\n";
        }
    }

public:
    ATMModule(IAccountRepository& accRepo, ITransactionRepository& transRepo, AuditLogger& auditLog, const std::string& cashFilePath)
        : accountRepo(accRepo), txRepo(transRepo), engine(accRepo, transRepo), audit(auditLog), atmCash(cashFilePath) {}

    // --- Feature 8: Clean logout (built into the run loop) ---
    void run() {
        std::cout << "\n===== ATM LOGIN =====\n";
        if (!authenticate()) return;

        std::cout << "\nWelcome, " << currentAccount.holderName << "!\n";

        while (true) {
            std::cout << "\n===== ATM MENU =====\n"
                      << "1. Check balance\n"
                      << "2. Withdraw\n"
                      << "3. Deposit\n"
                      << "4. Transfer\n"
                      << "5. Mini-statement\n"
                      << "6. Change PIN\n"
                      << "0. Logout\n";
            int choice = InputHelper::getInt("Choose: ");
            switch (choice) {
                case 1: checkBalance();   break;
                case 2: withdraw();       break;
                case 3: deposit();        break;
                case 4: transfer();       break;
                case 5: miniStatement();  break;
                case 6: changePin();      break;
                case 0:
                    std::cout << "Thank you for banking with us. Goodbye, " << currentAccount.holderName << ".\n";
                    audit.log(DateTime::now(), currentAccount.accountNumber, "LOGOUT", "ATM session ended");
                    return;
                default:
                    std::cout << "Invalid choice.\n";
            }
        }
    }
};