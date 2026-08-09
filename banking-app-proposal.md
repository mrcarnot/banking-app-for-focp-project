# Advanced C++ Banking Application — Project Proposal
> FOCP Group Project | Team of 3 | Advanced Track

---

## What Makes This "Advanced"

The spec asks you to build a working banking app. This proposal does that — but along the way you learn things that don't appear in a first-year course:

- **Design patterns** (Command, Observer, Strategy, Repository)
- **Move semantics, RAII, smart pointers** — no raw `new`/`delete` anywhere
- **Custom error handling** instead of `if (success == false)` everywhere
- **Binary file I/O and your own serializer**
- **Real fintech concepts** baked into the architecture — double-entry bookkeeping, atomicity, audit trails, PIN hashing

The deliverable for your instructor is identical to what the spec asks. The internals are just significantly better.

---

## Team & Codenames

| Handle | Primary Zone |
|---|---|
| **Alpha** | Data layer — Account/Transaction models, file I/O, serialization |
| **Beta** | Business logic — validation rules, transaction engine, atomicity |
| **Gamma** | Interface layer — admin module, ATM module, menus, reports |

All three must understand the full codebase for viva. The zones just say who *owns* each part.

---

## Phase Overview

```
Phase 0  ──►  Phase 1  ──►  Phase 2  ──►  Phase 3  ──►  Phase 4
 Setup         Core           Engine         Modules        Polish
 (Day 1)      (Days 2-4)     (Days 5-7)     (Days 8-11)    (Days 12-14)
```

---

## Phase 0 — Project Setup (Day 1)

**Goal:** Everyone starts from the same foundation. No one writes business logic before the skeleton exists.

### What gets built
- Folder structure
- Git repository with branches (`dev`, `alpha-data`, `beta-logic`, `gamma-ui`)
- A `main.cpp` that compiles and prints "Banking System Online"
- A shared header file with all struct/class *declarations* (no implementations yet)
- A `Makefile` or `CMakeLists.txt` everyone agrees on

### Folder structure
```
banking-app/
├── include/
│   ├── Account.h
│   ├── Transaction.h
│   ├── Result.h          ← custom error type (explained in Phase 1)
│   ├── Repository.h      ← interface for file I/O
│   └── Validator.h
├── src/
│   ├── Account.cpp
│   ├── Transaction.cpp
│   ├── FileRepository.cpp
│   ├── Validator.cpp
│   ├── AdminModule.cpp
│   ├── ATMModule.cpp
│   └── main.cpp
├── data/
│   ├── accounts.dat
│   └── transactions.dat
├── logs/
│   └── audit.log
└── CMakeLists.txt
```

### Task distribution

| Who | Task |
|---|---|
| **Alpha** | Create repo, set up CMake, create all `.h` files with empty declarations |
| **Beta** | Write the `Result<T, E>` type (see Phase 1), add to `Result.h` |
| **Gamma** | Write `main.cpp` skeleton — top-level menu, calls placeholder functions |

**Done when:** `cmake .. && make` compiles with zero errors and runs.

---

## Phase 1 — Data Models (Days 2–4)

**Goal:** Define exactly what an Account and Transaction *are*, and how errors are represented. Everything else in the project depends on this.

**Owner: Alpha** (Beta and Gamma review and must understand it)

### 1A — The `Result<T, E>` type (Beta writes this)

In beginners' C++, when something goes wrong you return `-1` or `false`. This is terrible — the caller has no idea *why* it failed.

Instead, every function that can fail returns a `Result`:

```cpp
// In Result.h
template<typename T, typename E>
class Result {
public:
    bool ok;
    T    value;   // only valid if ok == true
    E    error;   // only valid if ok == false

    static Result success(T val) { return {true, val, {}}; }
    static Result failure(E err) { return {false, {}, err}; }
};
```

**Why this matters (fintech angle):** Every payment API in the real world returns a structured result — Stripe returns `{status: "failed", code: "insufficient_funds", decline_code: "do_not_honor"}`. You're building the same concept.

Usage looks like:
```cpp
Result<Account, string> findAccount(string id);

auto res = findAccount("100001");
if (!res.ok) {
    cout << "Error: " << res.error << endl;
    return;
}
Account acc = res.value;
```

### 1B — Account model (Alpha)

```cpp
// In Account.h
enum class AccountStatus { Active, Frozen, Locked };

struct Account {
    string   accountNumber;      // auto-generated, 6-digit, starts at 100001
    string   holderName;
    string   cnic;
    string   contact;
    string   address;
    double   balance;
    AccountStatus status;
    string   pinHash;            // NEVER store plain PIN — store its hash
    int      pinAttempts;        // counts consecutive failures, resets on success
    double   dailyWithdrawn;     // resets when date changes
    string   lastTransactionDate; // "YYYY-MM-DD" format

    // Helper: is this account allowed to transact?
    bool isActive() const { return status == AccountStatus::Active; }

    // Helper: does the given PIN match?
    bool checkPin(const string& inputPin) const;
};
```

**Why `pinHash` and not `pin` (fintech angle):** PCI-DSS (the international card security standard) forbids storing PINs in plaintext. Even your tiny `.dat` file should hash them. You'll implement a simple hash — `std::hash<string>` or a basic djb2 — and store the hash value. When the user types their PIN, you hash the input and compare hashes. The actual PIN is never stored anywhere.

### 1C — Transaction model (Alpha)

```cpp
// In Transaction.h
enum class TxType { Deposit, Withdrawal, Transfer_Debit, Transfer_Credit, Interest, PinChange };

struct Transaction {
    string txId;            // auto-generated: "TX" + timestamp + sequence
    string accountNumber;
    TxType type;
    double amount;
    string timestamp;       // "YYYY-MM-DD HH:MM:SS"
    double resultingBalance;
    string referenceId;     // for transfers: both legs share the same referenceId
    string description;     // human-readable note
};
```

**Why `referenceId` (fintech angle):** When you transfer money, two transactions are created — a debit and a credit. Both get the same `referenceId`. This is how real banks link corresponding entries. If you ever need to reverse a transfer, you find the referenceId and flip both legs. This is the core of double-entry bookkeeping.

### 1D — The Repository interface (Alpha)

Instead of spreading `fstream` code everywhere, you define *one interface* that says "I can load accounts, save accounts, etc." Then you write one class that actually does it using files.

```cpp
// In Repository.h
class IAccountRepository {
public:
    virtual vector<Account>     loadAll() = 0;
    virtual bool                save(const Account& acc) = 0;
    virtual bool                saveAll(const vector<Account>& accounts) = 0;
    virtual Result<Account, string> findByNumber(const string& num) = 0;
    virtual ~IAccountRepository() = default;
};

class ITransactionRepository {
public:
    virtual bool               append(const Transaction& tx) = 0;
    virtual vector<Transaction> loadForAccount(const string& num) = 0;
    virtual vector<Transaction> loadAll() = 0;
    virtual ~ITransactionRepository() = default;
};
```

**Why this matters (design pattern):** This is the **Repository pattern**. Your ATM module calls `repo->findByNumber("100001")` and doesn't care if data comes from a file, a database, or memory. During testing you can swap in a fake in-memory repository without touching any business logic.

### Phase 1 Done When
- `Account` and `Transaction` can be created in `main.cpp` and printed to console
- `Result<T, E>` compiles and the two static constructors work
- All three team members can explain what `pinHash` and `referenceId` are for

---

## Phase 2 — The Engine (Days 5–7)

**Goal:** Implement the actual file I/O and the business rules. No UI yet — just the machinery.

### 2A — File I/O Layer (Alpha)

Implement `FileAccountRepository` and `FileTransactionRepository`.

**Use binary files.** The spec allows it and it's more educational than CSV.

```cpp
// Binary write for an Account — Alpha implements this
void FileAccountRepository::writeBinary(ofstream& f, const Account& acc) {
    // Write each field: first write length (int), then the string bytes
    auto writeStr = [&](const string& s) {
        int len = s.size();
        f.write(reinterpret_cast<char*>(&len), sizeof(int));
        f.write(s.data(), len);
    };

    writeStr(acc.accountNumber);
    writeStr(acc.holderName);
    writeStr(acc.cnic);
    writeStr(acc.contact);
    writeStr(acc.address);
    f.write(reinterpret_cast<char*>(&acc.balance), sizeof(double));
    int status = static_cast<int>(acc.status);
    f.write(reinterpret_cast<char*>(&status), sizeof(int));
    writeStr(acc.pinHash);
    f.write(reinterpret_cast<char*>(&acc.pinAttempts), sizeof(int));
    f.write(reinterpret_cast<char*>(&acc.dailyWithdrawn), sizeof(double));
    writeStr(acc.lastTransactionDate);
}
```

The read function does the exact reverse. **This is a custom binary serializer** — you're learning what protobuf and MessagePack do under the hood.

**RAII for file handles:** Wrap every file open in a class whose destructor closes it. Never forget to close a file again:

```cpp
class ScopedFile {
    FILE* f;
public:
    ScopedFile(const char* path, const char* mode) : f(fopen(path, mode)) {}
    ~ScopedFile() { if (f) fclose(f); }
    FILE* get() { return f; }
    bool  valid() { return f != nullptr; }
};
```

### 2B — Validation Layer (Beta)

Write `Validator.cpp` — a collection of pure functions that enforce business rules.

```cpp
// In Validator.h
namespace Validator {
    Result<bool, string> canWithdraw(const Account& acc, double amount, double minBalance, double dailyLimit);
    Result<bool, string> canDeposit(const Account& acc, double amount);
    Result<bool, string> canTransfer(const Account& sender, const Account& receiver, double amount);
    bool isValidAmount(double amount);
    bool isValidPin(const string& pin);         // exactly 4 digits
    bool isAccountActive(const Account& acc);
    string hashPin(const string& pin);           // returns hash string
    string generateAccountNumber(const vector<Account>& existing); // next sequential ID
    string generateTxId();                        // "TX" + timestamp + counter
    string currentDate();                         // "YYYY-MM-DD"
    string currentTimestamp();                    // "YYYY-MM-DD HH:MM:SS"
}
```

**Why separate validators (fintech angle):** In real banking systems, validation rules change constantly — regulators update limits, risk teams change thresholds. Keeping them in one place means you change them once. The ATM module and Admin module both call the same validators — no duplicated logic.

### 2C — Transaction Engine (Beta)

The most important part. Implements the **Command pattern** — each transaction is an object that knows how to execute itself and how to roll back.

```cpp
// The core transfer logic — Beta implements this
Result<pair<Transaction, Transaction>, string>
TransactionEngine::transfer(
    Account& sender,
    Account& receiver,
    double amount,
    IAccountRepository& accountRepo,
    ITransactionRepository& txRepo)
{
    // 1. Validate (uses Validator layer — no logic duplicated here)
    auto check = Validator::canTransfer(sender, receiver, amount);
    if (!check.ok) return Result::failure(check.error);

    // 2. Generate shared reference — links both legs of the transfer
    string refId = "REF" + Validator::generateTxId();

    // 3. Mutate in memory FIRST — don't touch files yet
    sender.balance   -= amount;
    receiver.balance += amount;

    // 4. Build both transaction records
    Transaction debit  = buildTx(sender,   TxType::Transfer_Debit,  -amount, refId);
    Transaction credit = buildTx(receiver, TxType::Transfer_Credit, +amount, refId);

    // 5. Write to disk — if either write fails, rollback memory changes
    bool savedSender   = accountRepo.save(sender);
    bool savedReceiver = accountRepo.save(receiver);
    bool savedDebit    = txRepo.append(debit);
    bool savedCredit   = txRepo.append(credit);

    if (!savedSender || !savedReceiver || !savedDebit || !savedCredit) {
        // ROLLBACK — undo the in-memory changes
        sender.balance   += amount;
        receiver.balance -= amount;
        return Result::failure("Write failed — transfer rolled back");
    }

    return Result::success({debit, credit});
}
```

**What you just learned (fintech angle):** You implemented *atomicity* — the A in ACID. Either both sides of the transfer happen, or neither does. Real databases use write-ahead logs and two-phase commit for this. You used in-memory rollback. Same concept, simpler implementation.

### 2D — Audit Logger (Beta)

Append-only log for admin actions. Write to a separate file, never delete entries.

```cpp
class AuditLogger {
    string logPath;
public:
    AuditLogger(const string& path) : logPath(path) {}
    void log(const string& adminId, const string& action, const string& detail) {
        ofstream f(logPath, ios::app);  // append mode
        f << Validator::currentTimestamp()
          << " | ADMIN:" << adminId
          << " | " << action
          << " | " << detail << "\n";
    }
};
```

Every admin action (create account, close account, reset PIN, freeze account) calls `auditLogger.log(...)`. **This is a compliance requirement** in real banking — regulators can demand the audit trail for any account action.

### Phase 2 Done When
- `FileAccountRepository::loadAll()` and `saveAll()` work — data survives program restart
- `Validator::canWithdraw()` correctly rejects insufficient balance, below-minimum, and over-daily-limit cases
- `TransactionEngine::transfer()` correctly creates two linked transactions with the same `referenceId`
- Audit log file grows correctly when admin functions are called

---

## Phase 3 — Modules (Days 8–11)

**Goal:** Build the actual menus the user interacts with. By now the engine is solid — this phase is mostly wiring things together.

**Owner: Gamma** (calls Alpha's repos and Beta's engine)

### 3A — Admin Module

```
ADMINISTRATOR MENU
══════════════════════════════════
 [1] Create Account
 [2] View Account
 [3] View All Accounts
 [4] Edit Account
 [5] Close / Deactivate Account
 [6] Freeze / Unfreeze Account
 [7] Reset PIN Attempts
 [8] View All Transactions
 [9] End-of-Session Report
 [0] Logout
══════════════════════════════════
```

Each menu option calls one clearly-named function. No logic inside the menu loop itself — the menu only reads input and dispatches.

```cpp
void AdminModule::run(IAccountRepository& repo, ITransactionRepository& txRepo, AuditLogger& logger) {
    while (true) {
        printMenu();
        int choice = getIntInput(0, 9);
        switch (choice) {
            case 1: createAccount(repo, logger);            break;
            case 2: viewAccount(repo);                      break;
            case 3: viewAllAccounts(repo);                  break;
            case 4: editAccount(repo, logger);              break;
            case 5: closeAccount(repo, logger);             break;
            case 6: freezeAccount(repo, logger);            break;
            case 7: resetPinAttempts(repo, logger);         break;
            case 8: viewAllTransactions(txRepo);            break;
            case 9: printSessionReport(repo, txRepo);       break;
            case 0: return;
        }
    }
}
```

**PIN input masking (bonus feature):** Use `getch()` on Windows or a terminal raw-mode trick on Linux to show `*` for each PIN digit typed. This is a visible feature the instructor will notice immediately.

### 3B — ATM Module

```
╔══════════════════════════════════╗
║   NUST ATM SYSTEM                ║
╠══════════════════════════════════╣
║  Account : 100001                ║
║  Balance : Rs. 24,500.00         ║
╠══════════════════════════════════╣
║  [1] Balance Inquiry             ║
║  [2] Withdraw Cash               ║
║  [3] Deposit Cash                ║
║  [4] Transfer Funds              ║
║  [5] Mini Statement (Last 5)     ║
║  [6] Change PIN                  ║
║  [0] Logout                      ║
╚══════════════════════════════════╝
```

Authentication flow with lock logic:

```cpp
Result<Account, string> ATMModule::authenticate(IAccountRepository& repo) {
    cout << "Account Number: ";
    string accNum = getStringInput();

    auto res = repo.findByNumber(accNum);
    if (!res.ok) return Result::failure("Account not found");

    Account acc = res.value;
    if (acc.status == AccountStatus::Locked)
        return Result::failure("Account locked — contact bank");

    for (int attempt = 1; attempt <= 3; attempt++) {
        string pin = getHiddenInput("PIN: ");  // shows asterisks
        if (acc.checkPin(pin)) {
            acc.pinAttempts = 0;   // reset on success
            repo.save(acc);
            return Result::success(acc);
        }
        acc.pinAttempts++;
        cout << "Wrong PIN. Attempts left: " << (3 - attempt) << "\n";
    }

    // 3 failures — lock the account
    acc.status = AccountStatus::Locked;
    repo.save(acc);
    return Result::failure("Account locked after 3 failed attempts");
}
```

### 3C — Receipt Generation (bonus)

After every successful transaction, write a receipt file:

```cpp
void writeReceipt(const Transaction& tx) {
    string filename = "receipts/receipt_" + tx.txId + ".txt";
    ofstream f(filename);
    f << "╔══════════════════════════════════╗\n";
    f << "║         TRANSACTION RECEIPT      ║\n";
    f << "╠══════════════════════════════════╣\n";
    f << "║ TX ID   : " << tx.txId           << "\n";
    f << "║ Account : " << tx.accountNumber  << "\n";
    f << "║ Type    : " << txTypeToString(tx.type) << "\n";
    f << "║ Amount  : Rs. " << tx.amount     << "\n";
    f << "║ Balance : Rs. " << tx.resultingBalance << "\n";
    f << "║ Time    : " << tx.timestamp      << "\n";
    f << "╚══════════════════════════════════╝\n";
}
```

### Phase 3 Done When
- Full admin menu works end-to-end, data persists between runs
- Full ATM menu works, all 6 operations correctly update files
- Account locking after 3 PIN failures works
- Daily withdrawal limit resets when date changes

---

## Phase 4 — Polish & Bonus Features (Days 12–14)

**Goal:** Add the features that push you from a passing project to a memorable one.

### 4A — PIN Hashing (Alpha)

Replace plaintext PIN storage with a hash.

```cpp
// Simple but real enough to demonstrate the concept
string Validator::hashPin(const string& pin) {
    // djb2 hash — a classic string hash
    unsigned long hash = 5381;
    for (char c : pin)
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    return to_string(hash);
}
```

When creating an account: `acc.pinHash = Validator::hashPin(inputPin)`
When checking: `inputHash == acc.pinHash`

The actual PIN digits never touch the file.

### 4B — OTP for Large Transfers (Beta)

For transfers above Rs. 25,000 (SBP-inspired threshold):

```cpp
bool requiresOTP(double amount) { return amount > 25000.0; }

string generateOTP() {
    srand(time(nullptr));
    int otp = 1000 + (rand() % 9000);  // 4-digit OTP
    return to_string(otp);
}
```

Before committing the transfer:
1. Generate OTP, display it on screen (simulating an SMS)
2. Prompt user to enter it
3. If wrong, abort transfer

**Why this matters:** Every Pakistani mobile banking app (JazzCash, EasyPaisa, HBL Konnect) requires OTP for large transfers. SBP's payment regulations mandate it above certain thresholds. You're implementing the actual rule.

### 4C — Savings Interest (Beta)

Add an admin-triggered "month-end" that credits interest to eligible accounts:

```cpp
void AdminModule::runMonthEnd(IAccountRepository& repo, ITransactionRepository& txRepo) {
    const double ANNUAL_RATE = 0.07;   // 7% per annum
    const double MONTHLY_RATE = ANNUAL_RATE / 12.0;
    const double MIN_BALANCE_FOR_PROFIT = 5000.0;

    auto accounts = repo.loadAll();
    for (auto& acc : accounts) {
        if (acc.isActive() && acc.balance >= MIN_BALANCE_FOR_PROFIT) {
            double interest = acc.balance * MONTHLY_RATE;
            acc.balance += interest;
            Transaction tx = buildInterestTx(acc, interest);
            repo.save(acc);
            txRepo.append(tx);
            auditLogger.log("SYSTEM", "INTEREST_CREDIT",
                acc.accountNumber + " | Rs." + to_string(interest));
        }
    }
}
```

**Fintech angle:** This is exactly how savings accounts work. The interest calculation is also how Islamic banks compute *murabaha* profit — same math, different name.

### 4D — End-of-Session Report (Gamma)

```
╔══════════════════════════════════════════════════╗
║            END-OF-SESSION REPORT                 ║
║            2025-01-15 17:43:22                   ║
╠══════════════════════════════════════════════════╣
║  Total Accounts       :   47                     ║
║    Active             :   44                     ║
║    Frozen             :    2                     ║
║    Locked             :    1                     ║
╠══════════════════════════════════════════════════╣
║  Total Deposits Held  :  Rs. 2,847,500.00        ║
╠══════════════════════════════════════════════════╣
║  Transactions Today   :   23                     ║
║    Withdrawals        :    9                     ║
║    Deposits           :    7                     ║
║    Transfers          :    7                     ║
╚══════════════════════════════════════════════════╝
```

This is a real concept — banks generate *end-of-day* (EOD) reports for their core banking systems every night. Yours is a simplified version.

### Phase 4 Done When
- PINs in `accounts.dat` are hashed, not plaintext — verify by opening the file
- Transfers above Rs. 25,000 require OTP
- Month-end interest run correctly credits all eligible accounts
- Session report shows accurate counts and totals

---

## Full Task Distribution Summary

| Phase | Alpha | Beta | Gamma |
|---|---|---|---|
| **0 — Setup** | Repo + CMake + headers | `Result<T,E>` type | `main.cpp` skeleton |
| **1 — Models** | `Account`, `Transaction`, `IRepository` interfaces | Review + `hashPin`, `generateTxId` | Review + `printMenu` helpers |
| **2 — Engine** | `FileAccountRepository`, `FileTransactionRepository`, binary serializer | `Validator`, `TransactionEngine`, `AuditLogger` | Input helper functions (`getIntInput`, `getHiddenInput`) |
| **3 — Modules** | Integrate + test file layer end-to-end | Test engine with real file calls | `AdminModule`, `ATMModule` — all menus |
| **4 — Polish** | PIN hashing wired into all auth flows | OTP, interest engine | Receipts, session report, ATM cash inventory bonus |

---

## Concepts Map — What You Learned vs. What It's Called in Industry

| You Built | Real-World Name | Where It Appears |
|---|---|---|
| `Result<T, E>` type | Monadic error handling | Rust's `Result`, Kotlin's `Either`, Go's `(value, err)` |
| `IAccountRepository` + `FileAccountRepository` | Repository pattern | Spring Data, Django ORM, Entity Framework |
| Binary serializer / deserializer | Custom wire format | Protocol Buffers, FlatBuffers, MessagePack |
| `ScopedFile` closing itself | RAII | C++ smart pointers, Python `with`, Java try-with-resources |
| Shared `referenceId` on transfer legs | Double-entry bookkeeping | Every bank's core ledger since 1494 |
| Rollback on write failure | Atomicity (ACID) | PostgreSQL transactions, MySQL InnoDB |
| Append-only audit log | Compliance audit trail | PCI-DSS requirement 10, SOX, SBP regulations |
| Hash-stored PINs | PCI-DSS 3.x PIN protection | Every real payment system |
| OTP for large transfers | Step-up authentication | SBP Payment Systems Regulations, SWIFT, 3D Secure |
| Month-end interest run | Batch processing / EOD job | Scheduled jobs in core banking (Temenos, Finacle) |
| Account status FSM (Active/Frozen/Locked) | Account lifecycle management | Core banking account state machine |

---

## Viva Tips

Every member must be able to answer:

1. **"Why is the PIN hashed?"** → PCI-DSS forbids plaintext PIN storage. If the `.dat` file is stolen, hashes don't reveal actual PINs.

2. **"What happens if the program crashes mid-transfer?"** → Sender's balance may be decremented but receiver's not yet updated. That's why we write both accounts to disk before committing — and why real banks use database transactions.

3. **"What does `referenceId` do?"** → Links the debit and credit legs of a transfer so the bank can trace, reconcile, or reverse both sides together.

4. **"Why a separate `IAccountRepository` interface instead of just writing file code directly?"** → If we later switch from binary files to a database, we only change `FileAccountRepository` — no changes to ATM or Admin modules.

5. **"What is RAII?"** → Resource Acquisition Is Initialization. When the `ScopedFile` object goes out of scope, its destructor closes the file automatically. You can't forget to close it.
