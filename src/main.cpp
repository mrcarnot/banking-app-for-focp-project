#include <iostream>
#include "../include/FileAccountRepository.h"
#include "../include/FileTransactionRepository.h"
#include "../include/AuditLogger.h"
#include "../include/AdminModule.h"
using namespace std;

int main() {
    // Real file-backed storage — data lives in the data/ and logs/ folders
    FileAccountRepository     accountRepo("../data/accounts.dat");
    FileTransactionRepository txRepo("../data/transactions.dat");
    AuditLogger               audit("../logs/audit.log");

    AdminModule admin(accountRepo, txRepo, audit);
    admin.run();

    return 0;
}