#pragma once
#include "FileAccountRepository.h"
#include "FileTransactionRepository.h"
#include "TransactionEngine.h"
#include "Validator.h"
#include "AuditLogger.h"
#include "InputHelper.h"
#include "DateTime.h"
#include "Config.h"
#include <iostream>
#include <iomanip>

class AdminModule {
    IAccountRepository&     accountRepo;
    ITransactionRepository& txRepo;
    AuditLogger&            audit;

    // --- Feature 1: Admin login ---
    bool authenticate() {
        std::string user = InputHelper::getString("Admin username: ");
        std::string pass = InputHelper::getString("Admin password: ");
        if (user == "admin" && pass == "admin123") {
            std::cout << "\nLogin successful.\n";
            audit.log(DateTime::now(), "admin", "LOGIN", "Admin logged in");
            return true;
        }
        std::cout << "\nInvalid credentials.\n";
        audit.log(DateTime::now(), "unknown", "LOGIN_FAILED", "Failed admin login attempt");
        return false;
    }

    // --- Feature 2: Create account ---
    void createAccount() {
        std::cout << "\n=== Create New Account ===\n";
        std::string name    = InputHelper::getString("Holder name: ");
        std::string cnic    = InputHelper::getString("CNIC (13 digits): ");
        if (!Validator::isValidCnic(cnic)) {
            std::cout << "Invalid CNIC. Account not created.\n";
            return;
        }
        std::string contact = InputHelper::getString("Contact number: ");
        std::string address = InputHelper::getString("Address: ");

        std::string pin = InputHelper::getString("Set 4-digit PIN: ");
        if (!Validator::isValidPin(pin)) {
            std::cout << "Invalid PIN (must be exactly 4 digits). Account not created.\n";
            return;
        }

        double opening = InputHelper::getPositiveDouble("Opening deposit: ");
        if (opening < Config::MIN_OPENING_DEPOSIT) {
            std::cout << "Opening deposit must be at least " << Config::MIN_OPENING_DEPOSIT << ". Account not created.\n";
            return;
        }

        int typeChoice = InputHelper::getInt("Account type (1 = Current, 2 = Savings): ");

        Account acc;
        acc.accountNumber       = generateAccountNumber();
        acc.holderName          = name;
        acc.cnic                = cnic;
        acc.contact             = contact;
        acc.address             = address;
        acc.balance             = opening;
        acc.status              = AccountStatus::Active;
        acc.pinHash             = Validator::hashPin(pin);
        acc.pinAttempts         = 0;
        acc.dailyWithdrawn      = 0.0;
        acc.lastTransactionDate = DateTime::today();
        acc.type                = (typeChoice == 2) ? AccountType::Savings : AccountType::Current;
        acc.lockTimestamp       = "";
        acc.openedDate          = DateTime::today();
        acc.archivedBalance     = 0.0;
        acc.loanOutstanding     = 0.0;

        if (accountRepo.save(acc)) {
            std::cout << "\nAccount created successfully. Account number: " << acc.accountNumber << "\n";
            audit.log(DateTime::now(), "admin", "CREATE_ACCOUNT",
                      "Created account " + acc.accountNumber + " for " + name);
        } else {
            std::cout << "Failed to save account.\n";
        }
    }

    std::string generateAccountNumber() {
        auto all = accountRepo.loadAll();
        int maxNum = 100000;
        for (const auto& a : all) {
            try {
                int n = std::stoi(a.accountNumber);
                if (n > maxNum) maxNum = n;
            } catch (...) { /* skip any non-numeric account number */ }
        }
        return std::to_string(maxNum + 1);
    }

    std::string generateTxId() {
        auto all = txRepo.loadAll();
        int maxNum = 1000;
        for (const auto& t : all) {
            if (t.txId.size() > 2 && t.txId.substr(0, 2) == "TX") {
                try {
                    int n = std::stoi(t.txId.substr(2));
                    if (n > maxNum) maxNum = n;
                } catch (...) { }
            }
        }
        return "TX" + std::to_string(maxNum + 1);
    }
    // --- Feature 3: View one account / list all ---
    void viewAccount() {
        std::string num = InputHelper::getString("\nAccount number: ");
        auto res = accountRepo.findByNumber(num);
        if (!res.ok) { std::cout << res.error << "\n"; return; }
        printAccount(res.value);
    }

    void listAllAccounts() {
        auto all = accountRepo.loadAll();
        std::cout << "\n=== All Accounts (" << all.size() << ") ===\n";
        std::cout << std::left
                  << std::setw(10) << "Number"
                  << std::setw(20) << "Holder"
                  << std::setw(12) << "Balance"
                  << std::setw(10) << "Status" << "\n";
        std::cout << std::string(52, '-') << "\n";
        for (const auto& a : all) {
            std::cout << std::left
                      << std::setw(10) << a.accountNumber
                      << std::setw(20) << a.holderName
                      << std::setw(12) << std::fixed << std::setprecision(2) << a.balance
                      << std::setw(10) << statusToString(a.status) << "\n";
        }
    }

    void printAccount(const Account& a) {
        std::cout << "\n--- Account " << a.accountNumber << " ---\n";
        std::cout << "Holder:   " << a.holderName << "\n";
        std::cout << "CNIC:     " << a.cnic << "\n";
        std::cout << "Contact:  " << a.contact << "\n";
        std::cout << "Address:  " << a.address << "\n";
        std::cout << "Balance:  " << std::fixed << std::setprecision(2) << a.balance << "\n";
        std::cout << "Type:     " << (a.type == AccountType::Savings ? "Savings" : "Current") << "\n";
        std::cout << "Status:   " << statusToString(a.status) << "\n";
        std::cout << "Opened:   " << a.openedDate << "\n";
    }

    std::string statusToString(AccountStatus s) {
        switch (s) {
            case AccountStatus::Active: return "Active";
            case AccountStatus::Frozen: return "Frozen";
            case AccountStatus::Locked: return "Locked";
            case AccountStatus::Closed: return "Closed";
        }
        return "Unknown";
    }

    // --- Feature 4: Edit account (contact + address only; number and CNIC are immutable) ---
    void editAccount() {
        std::string num = InputHelper::getString("\nAccount number to edit: ");
        auto res = accountRepo.findByNumber(num);
        if (!res.ok) { std::cout << res.error << "\n"; return; }

        Account acc = res.value;
        std::cout << "Editing " << acc.accountNumber << " (leave blank to keep current)\n";

        std::string newContact = InputHelper::getString("New contact [" + acc.contact + "]: ");
        if (!newContact.empty()) acc.contact = newContact;

        std::string newAddress = InputHelper::getString("New address [" + acc.address + "]: ");
        if (!newAddress.empty()) acc.address = newAddress;

        if (accountRepo.save(acc)) {
            std::cout << "Account updated.\n";
            audit.log(DateTime::now(), "admin", "EDIT_ACCOUNT", "Edited account " + acc.accountNumber);
        } else {
            std::cout << "Failed to save changes.\n";
        }
    }

    // --- Feature 5: Close account ---
    void closeAccount() {
        std::string num = InputHelper::getString("\nAccount number to close: ");
        auto res = accountRepo.findByNumber(num);
        if (!res.ok) { std::cout << res.error << "\n"; return; }

        Account acc = res.value;
        if (acc.status == AccountStatus::Closed) {
            std::cout << "Account is already closed.\n";
            return;
        }

        acc.archivedBalance = acc.balance;
        acc.status          = AccountStatus::Closed;

        if (accountRepo.save(acc)) {
            std::cout << "Account " << acc.accountNumber << " closed. Final balance archived: "
                      << std::fixed << std::setprecision(2) << acc.archivedBalance << "\n";
            audit.log(DateTime::now(), "admin", "CLOSE_ACCOUNT",
                      "Closed account " + acc.accountNumber + ", archived balance " + std::to_string(acc.archivedBalance));
        } else {
            std::cout << "Failed to close account.\n";
        }
    }

    // --- Feature 6: Freeze / unfreeze / reset PIN attempts ---
    void manageStatus() {
        std::string num = InputHelper::getString("\nAccount number: ");
        auto res = accountRepo.findByNumber(num);
        if (!res.ok) { std::cout << res.error << "\n"; return; }

        Account acc = res.value;
        std::cout << "Current status: " << statusToString(acc.status) << "\n";
        std::cout << "1. Freeze\n2. Unfreeze\n3. Unlock + reset PIN attempts\n";
        int choice = InputHelper::getInt("Choose: ");

        std::string action;
        switch (choice) {
            case 1:
                if (acc.status == AccountStatus::Closed) { std::cout << "Cannot freeze a closed account.\n"; return; }
                acc.status = AccountStatus::Frozen; action = "FREEZE";
                break;
            case 2:
                if (acc.status != AccountStatus::Frozen) { std::cout << "Account is not frozen.\n"; return; }
                acc.status = AccountStatus::Active; action = "UNFREEZE";
                break;
            case 3:
                acc.status = AccountStatus::Active;
                acc.pinAttempts = 0;
                acc.lockTimestamp = "";
                action = "UNLOCK_RESET";
                break;
            default:
                std::cout << "Invalid choice.\n"; return;
        }

        if (accountRepo.save(acc)) {
            std::cout << "Done. New status: " << statusToString(acc.status) << "\n";
            audit.log(DateTime::now(), "admin", action, "Account " + acc.accountNumber);
        } else {
            std::cout << "Failed to update.\n";
        }
    }

    // --- Feature 7: Global transaction history (with bonus filter) ---
    void globalHistory() {
        std::cout << "\n=== Transaction History ===\n";
        std::cout << "1. All transactions\n2. Filter by account\n";
        int choice = InputHelper::getInt("Choose: ");

        std::vector<Transaction> results;
        if (choice == 2) {
            TxFilter f;
            f.accountNumber = InputHelper::getString("Account number: ");
            results = txRepo.query(f);
        } else {
            results = txRepo.loadAll();
        }

        std::cout << "\n" << std::left
                  << std::setw(10) << "TxID"
                  << std::setw(10) << "Account"
                  << std::setw(16) << "Type"
                  << std::setw(12) << "Amount"
                  << std::setw(22) << "Timestamp" << "\n";
        std::cout << std::string(70, '-') << "\n";
        for (const auto& t : results) {
            std::cout << std::left
                      << std::setw(10) << t.txId
                      << std::setw(10) << t.accountNumber
                      << std::setw(16) << txTypeToString(t.type)
                      << std::setw(12) << std::fixed << std::setprecision(2) << t.amount
                      << std::setw(22) << t.timestamp << "\n";
        }
        std::cout << "Total: " << results.size() << " transactions\n";
    }

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

    // --- Feature 8: End-of-day report ---
    void endOfDayReport() {
        auto accounts = accountRepo.loadAll();
        auto txs      = txRepo.loadAll();
        std::string today = DateTime::today();

        int active = 0, frozen = 0, locked = 0, closed = 0;
        double totalHeld = 0.0;
        for (const auto& a : accounts) {
            switch (a.status) {
                case AccountStatus::Active: active++; break;
                case AccountStatus::Frozen: frozen++; break;
                case AccountStatus::Locked: locked++; break;
                case AccountStatus::Closed: closed++; break;
            }
            totalHeld += a.balance;
        }

        int txToday = 0;
        for (const auto& t : txs) {
            if (t.timestamp.substr(0, 10) == today) txToday++;
        }

        std::cout << "\n===== END OF DAY REPORT (" << today << ") =====\n";
        std::cout << "Total accounts:        " << accounts.size() << "\n";
        std::cout << "  Active:              " << active << "\n";
        std::cout << "  Frozen:              " << frozen << "\n";
        std::cout << "  Locked:              " << locked << "\n";
        std::cout << "  Closed:              " << closed << "\n";
        std::cout << "Total balance held:    " << std::fixed << std::setprecision(2) << totalHeld << "\n";
        std::cout << "Transactions today:    " << txToday << "\n";
        std::cout << "=======================================\n";
        audit.log(DateTime::now(), "admin", "EOD_REPORT", "Generated end-of-day report");
    }
    // BONUS #5: savings interest / month-end run
    void runInterest() {
        auto accounts = accountRepo.loadAll();
        int creditedCount = 0;
        double totalCredited = 0.0;

        for (auto& acc : accounts) {
            if (acc.type != AccountType::Savings) continue;
            if (acc.status != AccountStatus::Active) continue;
            if (acc.balance < Config::MIN_BALANCE_FOR_PROFIT) continue;

            double interest = acc.balance * (Config::ANNUAL_INTEREST_RATE / 12.0);
            double newBalance = acc.balance + interest;

            Transaction tx;
            tx.txId = generateTxId();
            tx.accountNumber = acc.accountNumber;
            tx.type = TxType::Interest;
            tx.amount = interest;
            tx.timestamp = DateTime::now();
            tx.resultingBalance = newBalance;
            tx.referenceId = "";
            tx.description = "Monthly savings interest";

            acc.balance = newBalance;

            if (accountRepo.save(acc)) {
                txRepo.append(tx);
                creditedCount++;
                totalCredited += interest;
            }
        }

        std::cout << "\nInterest run complete. Credited " << creditedCount
                  << " account(s), total interest paid: "
                  << std::fixed << std::setprecision(2) << totalCredited << "\n";
        audit.log(DateTime::now(), "admin", "INTEREST_RUN",
                  "Credited interest to " + std::to_string(creditedCount) + " accounts, total " + std::to_string(totalCredited));
    }
public:
    AdminModule(IAccountRepository& accRepo, ITransactionRepository& transRepo, AuditLogger& auditLog)
        : accountRepo(accRepo), txRepo(transRepo), audit(auditLog) {}

    void run() {
        if (!authenticate()) return;

        while (true) {
            std::cout << "\n===== ADMIN MENU =====\n"
                      << "1. Create account\n"
                      << "2. View account\n"
                      << "3. List all accounts\n"
                      << "4. Edit account\n"
                      << "5. Close account\n"
                      << "6. Freeze/Unfreeze/Unlock\n"
                      << "7. Transaction history\n"
                      << "8. End-of-day report\n"
                      << "9. Run monthly interest\n"
                      << "0. Logout\n";
            int choice = InputHelper::getInt("Choose: ");
            switch (choice) {
                case 1: createAccount();   break;
                case 2: viewAccount();     break;
                case 3: listAllAccounts(); break;
                case 4: editAccount();     break;
                case 5: closeAccount();    break;
                case 6: manageStatus();    break;
                case 7: globalHistory();   break;
                case 8: endOfDayReport();  break;
                case 9: runInterest();     break;
                case 0:
                    audit.log(DateTime::now(), "admin", "LOGOUT", "Admin logged out");
                    std::cout << "Logged out.\n";
                    return;
                default:
                    std::cout << "Invalid choice.\n";
            }
        }
    }
};