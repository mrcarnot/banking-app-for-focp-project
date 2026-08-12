#include <iostream>
#include "../include/FileAccountRepository.h"
#include "../include/FileTransactionRepository.h"
#include "../include/LoanRepository.h"
#include "../include/AuditLogger.h"
#include "../include/AdminModule.h"
#include "../include/ATMModule.h"
#include "../include/InputHelper.h"
using namespace std;

int main() {
    FileAccountRepository     accountRepo("../data/accounts.dat");
    FileTransactionRepository txRepo("../data/transactions.dat");
    AuditLogger                audit("../logs/audit.log");
    FileLoanRepository         loanRepo("../data/loans.dat");

    while (true) {
        cout << "\n========================================\n";
        cout << "   BANKING SYSTEM - MAIN MENU\n";
        cout << "========================================\n";
        cout << "1. Admin Login\n";
        cout << "2. ATM (Customer)\n";
        cout << "0. Exit\n";
        int choice = InputHelper::getInt("Choose: ");

        if (choice == 1) {
            AdminModule admin(accountRepo, txRepo, audit, loanRepo);
            admin.run();
        } else if (choice == 2) {
            ATMModule atm(accountRepo, txRepo, audit, "../data/atm_cash.dat", loanRepo);
            atm.run();
        } else if (choice == 0) {
            cout << "Goodbye.\n";
            break;
        } else {
            cout << "Invalid choice.\n";
        }
    }

    return 0;
}