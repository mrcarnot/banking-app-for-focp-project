#include <windows.h>
#include <string>
#include <sstream>
#include "../include/FileAccountRepository.h"
#include "../include/FileTransactionRepository.h"
#include "../include/TransactionEngine.h"
#include "../include/Validator.h"
#include "../include/AuditLogger.h"
#include "../include/DateTime.h"
#include "../include/Config.h"

enum {
    IDC_ADMIN_ROLE_BTN = 100, IDC_ATM_ROLE_BTN,
    IDC_A_USER, IDC_A_PASS, IDC_A_LOGIN_BTN, IDC_A_BACK1,
    IDC_A_CREATE_BTN, IDC_A_LIST_BTN, IDC_A_FREEZE_BTN, IDC_A_UNFREEZE_BTN, IDC_A_UNLOCK_BTN, IDC_A_EOD_BTN, IDC_A_LOGOUT_BTN,
    IDC_A_NAME, IDC_A_CNIC, IDC_A_CONTACT, IDC_A_ADDRESS, IDC_A_PIN, IDC_A_DEPOSIT, IDC_A_TARGETACC, IDC_A_SUBMIT_CREATE, IDC_A_BACK2,
    IDC_T_ACC, IDC_T_PIN, IDC_T_LOGIN_BTN, IDC_T_BACK1,
    IDC_T_AMOUNT, IDC_T_DEST, IDC_T_WITHDRAW, IDC_T_DEPOSIT, IDC_T_TRANSFER, IDC_T_STATEMENT, IDC_T_LOGOUT,
    IDC_OUTPUT
};

enum ViewState { V_START, V_ADMIN_LOGIN, V_ADMIN_MENU, V_ADMIN_CREATE, V_ATM_LOGIN, V_ATM_MENU };
ViewState g_view = V_START;

FileAccountRepository*     g_accountRepo = nullptr;
FileTransactionRepository* g_txRepo = nullptr;
AuditLogger*                g_audit = nullptr;
TransactionEngine*          g_engine = nullptr;
Account                      g_currentAccount;

HFONT g_fontHeader, g_fontLabel, g_fontMono, g_fontBtn;
HBRUSH g_hbWhite, g_hbHeader, g_hbSidebar;

HWND h_header, h_subtitle;
HWND h_startAdminBtn, h_startAtmBtn;
HWND h_aUserLbl, h_aUser, h_aPassLbl, h_aPass, h_aLoginBtn, h_aBack1;
HWND h_aCreateBtn, h_aListBtn, h_aFreezeBtn, h_aUnfreezeBtn, h_aUnlockBtn, h_aEodBtn, h_aLogoutBtn;
HWND h_aNameLbl, h_aName, h_aCnicLbl, h_aCnic, h_aContactLbl, h_aContact, h_aAddressLbl, h_aAddress;
HWND h_aPinLbl, h_aPin, h_aDepositLbl, h_aDeposit, h_aTargetLbl, h_aTargetAcc, h_aSubmitCreate, h_aBack2;
HWND h_tAccLbl, h_tAcc, h_tPinLbl, h_tPin, h_tLoginBtn, h_tBack1;
HWND h_tAmountLbl, h_tAmount, h_tDestLbl, h_tDest, h_tWithdraw, h_tDeposit, h_tTransfer, h_tStatement, h_tLogout;
HWND h_output;

std::string txt(HWND h) {
    char buf[512];
    GetWindowText(h, buf, sizeof(buf));
    return std::string(buf);
}

void log(const std::string& s) {
    int len = GetWindowTextLength(h_output);
    SendMessage(h_output, EM_SETSEL, len, len);
    std::string line = s + "\r\n";
    SendMessage(h_output, EM_REPLACESEL, 0, (LPARAM)line.c_str());
}

void clearOutput() { SetWindowText(h_output, ""); }

std::string statusStr(AccountStatus s) {
    switch (s) {
        case AccountStatus::Active: return "Active";
        case AccountStatus::Frozen: return "Frozen";
        case AccountStatus::Locked: return "Locked";
        case AccountStatus::Closed: return "Closed";
    }
    return "?";
}

void hideAll() {
    HWND list[] = {
        h_startAdminBtn, h_startAtmBtn,
        h_aUserLbl, h_aUser, h_aPassLbl, h_aPass, h_aLoginBtn, h_aBack1,
        h_aCreateBtn, h_aListBtn, h_aFreezeBtn, h_aUnfreezeBtn, h_aUnlockBtn, h_aEodBtn, h_aLogoutBtn,
        h_aNameLbl, h_aName, h_aCnicLbl, h_aCnic, h_aContactLbl, h_aContact, h_aAddressLbl, h_aAddress,
        h_aPinLbl, h_aPin, h_aDepositLbl, h_aDeposit, h_aTargetLbl, h_aTargetAcc, h_aSubmitCreate, h_aBack2,
        h_tAccLbl, h_tAcc, h_tPinLbl, h_tPin, h_tLoginBtn, h_tBack1,
        h_tAmountLbl, h_tAmount, h_tDestLbl, h_tDest, h_tWithdraw, h_tDeposit, h_tTransfer, h_tStatement, h_tLogout
    };
    for (HWND h : list) if (h) ShowWindow(h, SW_HIDE);
}

void setSubtitle(const std::string& s) {
    SetWindowText(h_subtitle, s.c_str());
    InvalidateRect(h_subtitle, NULL, TRUE);
    UpdateWindow(h_subtitle);
}

void showView(ViewState v) {
    hideAll();
    g_view = v;
    clearOutput();
    switch (v) {
        case V_START:
            setSubtitle("Choose how you'd like to sign in");
            ShowWindow(h_startAdminBtn, SW_SHOW);
            ShowWindow(h_startAtmBtn, SW_SHOW);
            break;
        case V_ADMIN_LOGIN:
            setSubtitle("Bank administration sign in");
            ShowWindow(h_aUserLbl, SW_SHOW); ShowWindow(h_aUser, SW_SHOW);
            ShowWindow(h_aPassLbl, SW_SHOW); ShowWindow(h_aPass, SW_SHOW);
            ShowWindow(h_aLoginBtn, SW_SHOW);
            ShowWindow(h_aBack1, SW_SHOW);
            break;
        case V_ADMIN_MENU:
            setSubtitle("Administration dashboard");
            ShowWindow(h_aCreateBtn, SW_SHOW);
            ShowWindow(h_aListBtn, SW_SHOW);
            ShowWindow(h_aEodBtn, SW_SHOW);
            ShowWindow(h_aLogoutBtn, SW_SHOW);
            ShowWindow(h_aTargetLbl, SW_SHOW);
            ShowWindow(h_aTargetAcc, SW_SHOW);
            ShowWindow(h_aFreezeBtn, SW_SHOW);
            ShowWindow(h_aUnfreezeBtn, SW_SHOW);
            ShowWindow(h_aUnlockBtn, SW_SHOW);
            log("Ready. Pick an action on the left, or manage an account's status on the right.");
            break;
        case V_ADMIN_CREATE:
            setSubtitle("Create a new account");
            ShowWindow(h_aNameLbl, SW_SHOW);    ShowWindow(h_aName, SW_SHOW);
            ShowWindow(h_aCnicLbl, SW_SHOW);    ShowWindow(h_aCnic, SW_SHOW);
            ShowWindow(h_aContactLbl, SW_SHOW); ShowWindow(h_aContact, SW_SHOW);
            ShowWindow(h_aAddressLbl, SW_SHOW); ShowWindow(h_aAddress, SW_SHOW);
            ShowWindow(h_aPinLbl, SW_SHOW);     ShowWindow(h_aPin, SW_SHOW);
            ShowWindow(h_aDepositLbl, SW_SHOW); ShowWindow(h_aDeposit, SW_SHOW);
            ShowWindow(h_aSubmitCreate, SW_SHOW);
            ShowWindow(h_aBack2, SW_SHOW);
            break;
        case V_ATM_LOGIN:
            setSubtitle("Customer sign in");
            ShowWindow(h_tAccLbl, SW_SHOW); ShowWindow(h_tAcc, SW_SHOW);
            ShowWindow(h_tPinLbl, SW_SHOW); ShowWindow(h_tPin, SW_SHOW);
            ShowWindow(h_tLoginBtn, SW_SHOW);
            ShowWindow(h_tBack1, SW_SHOW);
            break;
        case V_ATM_MENU:
            setSubtitle("Welcome, " + g_currentAccount.holderName);
            ShowWindow(h_tAmountLbl, SW_SHOW); ShowWindow(h_tAmount, SW_SHOW);
            ShowWindow(h_tDestLbl, SW_SHOW);   ShowWindow(h_tDest, SW_SHOW);
            ShowWindow(h_tWithdraw, SW_SHOW);
            ShowWindow(h_tDeposit, SW_SHOW);
            ShowWindow(h_tTransfer, SW_SHOW);
            ShowWindow(h_tStatement, SW_SHOW);
            ShowWindow(h_tLogout, SW_SHOW);
            log("Balance: " + std::to_string(g_currentAccount.balance));
            break;
    }
}

std::string genAccNum() {
    auto all = g_accountRepo->loadAll();
    int mx = 100000;
    for (auto& a : all) { try { int n = std::stoi(a.accountNumber); if (n > mx) mx = n; } catch (...) {} }
    return std::to_string(mx + 1);
}

void adminLogin() {
    if (txt(h_aUser) == "admin" && txt(h_aPass) == "admin123") {
        g_audit->log(DateTime::now(), "admin", "LOGIN", "Admin GUI login");
        showView(V_ADMIN_MENU);
    } else {
        clearOutput();
        log("Invalid credentials.");
    }
}

void adminCreateSubmit() {
    std::string cnic = txt(h_aCnic);
    if (!Validator::isValidCnic(cnic)) { clearOutput(); log("Invalid CNIC - must be exactly 13 digits."); return; }
    std::string pin = txt(h_aPin);
    if (!Validator::isValidPin(pin)) { clearOutput(); log("Invalid PIN - must be exactly 4 digits."); return; }
    double deposit = atof(txt(h_aDeposit).c_str());
    if (deposit < Config::MIN_OPENING_DEPOSIT) { clearOutput(); log("Opening deposit must be at least " + std::to_string(Config::MIN_OPENING_DEPOSIT)); return; }

    Account acc;
    acc.accountNumber = genAccNum();
    acc.holderName = txt(h_aName);
    acc.cnic = cnic;
    acc.contact = txt(h_aContact);
    acc.address = txt(h_aAddress);
    acc.balance = deposit;
    acc.status = AccountStatus::Active;
    acc.pinHash = Validator::hashPin(pin);
    acc.pinAttempts = 0;
    acc.dailyWithdrawn = 0;
    acc.lastTransactionDate = DateTime::today();
    acc.type = AccountType::Current;
    acc.lockTimestamp = "";
    acc.openedDate = DateTime::today();
    acc.archivedBalance = 0;
    acc.loanOutstanding = 0;

    g_accountRepo->save(acc);
    g_audit->log(DateTime::now(), "admin", "CREATE_ACCOUNT", "Created " + acc.accountNumber);
    clearOutput();
    log("Account created successfully.");
    log("Account number: " + acc.accountNumber);
    SetWindowText(h_aName, ""); SetWindowText(h_aCnic, ""); SetWindowText(h_aContact, "");
    SetWindowText(h_aAddress, ""); SetWindowText(h_aPin, ""); SetWindowText(h_aDeposit, "");
}

void adminList() {
    clearOutput();
    auto all = g_accountRepo->loadAll();
    log("All accounts (" + std::to_string(all.size()) + "):");
    log("----------------------------------------------------");
    for (auto& a : all) {
        std::ostringstream row;
        row << a.accountNumber << "   " << a.holderName << "   bal " << a.balance << "   " << statusStr(a.status);
        log(row.str());
    }
}

void adminSetStatus(AccountStatus newStatus, const std::string& label) {
    std::string num = txt(h_aTargetAcc);
    auto res = g_accountRepo->findByNumber(num);
    clearOutput();
    if (!res.ok) { log(res.error); return; }
    Account acc = res.value;
    acc.status = newStatus;
    if (newStatus == AccountStatus::Active) { acc.pinAttempts = 0; acc.lockTimestamp = ""; }
    g_accountRepo->save(acc);
    g_audit->log(DateTime::now(), "admin", label, "Account " + num);
    log("Account " + num + " is now " + statusStr(newStatus) + ".");
}

void adminEOD() {
    clearOutput();
    auto accs = g_accountRepo->loadAll();
    auto txs = g_txRepo->loadAll();
    int active=0, frozen=0, locked=0, closed=0; double total=0;
    for (auto& a : accs) {
        switch (a.status) {
            case AccountStatus::Active: active++; break;
            case AccountStatus::Frozen: frozen++; break;
            case AccountStatus::Locked: locked++; break;
            case AccountStatus::Closed: closed++; break;
        }
        total += a.balance;
    }
    std::string today = DateTime::today();
    int today_tx = 0;
    for (auto& t : txs) if (t.timestamp.substr(0,10) == today) today_tx++;

    log("End-of-day report (" + today + ")");
    log("----------------------------------------------------");
    log("Total accounts: " + std::to_string(accs.size()));
    log("  Active: " + std::to_string(active) + "  Frozen: " + std::to_string(frozen)
        + "  Locked: " + std::to_string(locked) + "  Closed: " + std::to_string(closed));
    log("Total balance held: " + std::to_string(total));
    log("Transactions today: " + std::to_string(today_tx));
    g_audit->log(DateTime::now(), "admin", "EOD_REPORT", "Generated via GUI");
}

void atmLogin() {
    std::string num = txt(h_tAcc);
    std::string pin = txt(h_tPin);
    auto res = g_accountRepo->findByNumber(num);
    clearOutput();
    if (!res.ok) { log("Account not found."); return; }
    Account acc = res.value;
    if (acc.status == AccountStatus::Closed) { log("This account is closed."); return; }
    if (acc.status == AccountStatus::Frozen) { log("This account is frozen. Contact the bank."); return; }
    if (acc.status == AccountStatus::Locked) { log("This account is locked. Contact the bank."); return; }
    if (Validator::hashPin(pin) != acc.pinHash) {
        acc.pinAttempts++;
        if (acc.pinAttempts >= Config::MAX_PIN_ATTEMPTS) {
            acc.status = AccountStatus::Locked;
            acc.lockTimestamp = DateTime::now();
            log("Too many failed attempts. Account locked.");
        } else {
            log("Incorrect PIN. Attempt " + std::to_string(acc.pinAttempts) + " of " + std::to_string(Config::MAX_PIN_ATTEMPTS));
        }
        g_accountRepo->save(acc);
        return;
    }
    acc.pinAttempts = 0;
    g_accountRepo->save(acc);
    g_currentAccount = acc;
    g_audit->log(DateTime::now(), acc.accountNumber, "LOGIN", "ATM GUI login");
    showView(V_ATM_MENU);
}

void atmWithdraw() {
    double amt = atof(txt(h_tAmount).c_str());
    auto r = g_engine->withdraw(g_currentAccount, amt, DateTime::now());
    clearOutput();
    if (r.ok) log("Withdrawal successful. Balance: " + std::to_string(g_currentAccount.balance));
    else log("Failed: " + r.error);
}
void atmDeposit() {
    double amt = atof(txt(h_tAmount).c_str());
    auto r = g_engine->deposit(g_currentAccount, amt, DateTime::now());
    clearOutput();
    if (r.ok) log("Deposit successful. Balance: " + std::to_string(g_currentAccount.balance));
    else log("Failed: " + r.error);
}
void atmTransfer() {
    double amt = atof(txt(h_tAmount).c_str());
    std::string dest = txt(h_tDest);
    clearOutput();
    auto destRes = g_accountRepo->findByNumber(dest);
    if (!destRes.ok) { log("Destination account not found."); return; }
    Account d = destRes.value;
    auto r = g_engine->transfer(g_currentAccount, d, amt, DateTime::now());
    if (r.ok) log("Transfer successful. Balance: " + std::to_string(g_currentAccount.balance));
    else log("Failed: " + r.error);
}
void atmStatement() {
    clearOutput();
    auto txs = g_txRepo->loadForAccount(g_currentAccount.accountNumber);
    log("Recent transactions:");
    int count = 0;
    for (auto it = txs.rbegin(); it != txs.rend() && count < 5; ++it, ++count) {
        log(it->timestamp + "   " + std::to_string(it->amount));
    }
    if (txs.empty()) log("No transactions yet.");
}
void atmLogout() {
    g_audit->log(DateTime::now(), g_currentAccount.accountNumber, "LOGOUT", "ATM GUI logout");
    SetWindowText(h_tAcc, ""); SetWindowText(h_tPin, "");
    showView(V_START);
}

void applyFont(HWND h, HFONT f) { if (h) SendMessage(h, WM_SETFONT, (WPARAM)f, TRUE); }

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_fontHeader = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fontLabel = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fontBtn = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            g_fontMono = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");

            g_hbWhite  = CreateSolidBrush(RGB(255,255,255));
            g_hbHeader = CreateSolidBrush(RGB(20,60,110));

            h_header = CreateWindow("STATIC", "SecureBank", WS_CHILD | WS_VISIBLE | SS_CENTER,
                0, 0, 700, 64, hwnd, NULL, NULL, NULL);
            SendMessage(h_header, WM_SETFONT, (WPARAM)g_fontHeader, TRUE);

            h_subtitle = CreateWindow("STATIC", "", WS_CHILD | WS_VISIBLE | SS_CENTER,
                0, 72, 700, 24, hwnd, NULL, NULL, NULL);
            applyFont(h_subtitle, g_fontLabel);

            // Start view - vertical, centered
            h_startAdminBtn = CreateWindow("BUTTON", "Admin login", WS_CHILD, 270, 130, 160, 42, hwnd, (HMENU)IDC_ADMIN_ROLE_BTN, NULL, NULL);
            h_startAtmBtn   = CreateWindow("BUTTON", "ATM login",   WS_CHILD, 270, 186, 160, 42, hwnd, (HMENU)IDC_ATM_ROLE_BTN, NULL, NULL);
            applyFont(h_startAdminBtn, g_fontBtn); applyFont(h_startAtmBtn, g_fontBtn);

            // Admin login
            h_aUserLbl = CreateWindow("STATIC", "Username", WS_CHILD, 250, 116, 200, 18, hwnd, NULL, NULL, NULL);
            h_aUser    = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 250, 136, 200, 28, hwnd, (HMENU)IDC_A_USER, NULL, NULL);
            h_aPassLbl = CreateWindow("STATIC", "Password", WS_CHILD, 250, 172, 200, 18, hwnd, NULL, NULL, NULL);
            h_aPass    = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER | ES_PASSWORD, 250, 192, 200, 28, hwnd, (HMENU)IDC_A_PASS, NULL, NULL);
            h_aLoginBtn = CreateWindow("BUTTON", "Sign in", WS_CHILD, 250, 232, 95, 34, hwnd, (HMENU)IDC_A_LOGIN_BTN, NULL, NULL);
            h_aBack1 = CreateWindow("BUTTON", "Back", WS_CHILD, 355, 232, 95, 34, hwnd, (HMENU)IDC_A_BACK1, NULL, NULL);
            applyFont(h_aUserLbl, g_fontLabel); applyFont(h_aUser, g_fontLabel); applyFont(h_aPassLbl, g_fontLabel); applyFont(h_aPass, g_fontLabel);
            applyFont(h_aLoginBtn, g_fontBtn); applyFont(h_aBack1, g_fontBtn);

            // Admin menu - left sidebar (vertical), right side status panel
            h_aCreateBtn = CreateWindow("BUTTON", "New account",   WS_CHILD, 20, 100, 170, 40, hwnd, (HMENU)IDC_A_CREATE_BTN, NULL, NULL);
            h_aListBtn   = CreateWindow("BUTTON", "List accounts", WS_CHILD, 20, 146, 170, 40, hwnd, (HMENU)IDC_A_LIST_BTN, NULL, NULL);
            h_aEodBtn    = CreateWindow("BUTTON", "EOD report",    WS_CHILD, 20, 192, 170, 40, hwnd, (HMENU)IDC_A_EOD_BTN, NULL, NULL);
            h_aLogoutBtn = CreateWindow("BUTTON", "Logout",        WS_CHILD, 20, 238, 170, 40, hwnd, (HMENU)IDC_A_LOGOUT_BTN, NULL, NULL);
            applyFont(h_aCreateBtn, g_fontBtn); applyFont(h_aListBtn, g_fontBtn); applyFont(h_aEodBtn, g_fontBtn); applyFont(h_aLogoutBtn, g_fontBtn);

            h_aTargetLbl   = CreateWindow("STATIC", "Manage account status - account number:", WS_CHILD, 220, 100, 300, 18, hwnd, NULL, NULL, NULL);
            h_aTargetAcc   = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 220, 120, 200, 28, hwnd, (HMENU)IDC_A_TARGETACC, NULL, NULL);
            h_aFreezeBtn   = CreateWindow("BUTTON", "Freeze",   WS_CHILD, 220, 160, 120, 36, hwnd, (HMENU)IDC_A_FREEZE_BTN, NULL, NULL);
            h_aUnfreezeBtn = CreateWindow("BUTTON", "Unfreeze", WS_CHILD, 220, 202, 120, 36, hwnd, (HMENU)IDC_A_UNFREEZE_BTN, NULL, NULL);
            h_aUnlockBtn   = CreateWindow("BUTTON", "Unlock",   WS_CHILD, 220, 244, 120, 36, hwnd, (HMENU)IDC_A_UNLOCK_BTN, NULL, NULL);
            applyFont(h_aTargetLbl, g_fontLabel); applyFont(h_aTargetAcc, g_fontLabel);
            applyFont(h_aFreezeBtn, g_fontBtn); applyFont(h_aUnfreezeBtn, g_fontBtn); applyFont(h_aUnlockBtn, g_fontBtn);

            // Admin create - vertical single column
            h_aNameLbl    = CreateWindow("STATIC", "Full name", WS_CHILD, 20, 96, 300, 18, hwnd, NULL, NULL, NULL);
            h_aName       = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 20, 116, 300, 28, hwnd, (HMENU)IDC_A_NAME, NULL, NULL);
            h_aCnicLbl    = CreateWindow("STATIC", "CNIC (13 digits)", WS_CHILD, 20, 152, 300, 18, hwnd, NULL, NULL, NULL);
            h_aCnic       = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 20, 172, 300, 28, hwnd, (HMENU)IDC_A_CNIC, NULL, NULL);
            h_aContactLbl = CreateWindow("STATIC", "Contact number", WS_CHILD, 20, 208, 300, 18, hwnd, NULL, NULL, NULL);
            h_aContact    = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 20, 228, 300, 28, hwnd, (HMENU)IDC_A_CONTACT, NULL, NULL);
            h_aAddressLbl = CreateWindow("STATIC", "Address", WS_CHILD, 350, 96, 300, 18, hwnd, NULL, NULL, NULL);
            h_aAddress    = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 350, 116, 300, 28, hwnd, (HMENU)IDC_A_ADDRESS, NULL, NULL);
            h_aPinLbl     = CreateWindow("STATIC", "PIN (4 digits)", WS_CHILD, 350, 152, 300, 18, hwnd, NULL, NULL, NULL);
            h_aPin        = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER | ES_PASSWORD, 350, 172, 300, 28, hwnd, (HMENU)IDC_A_PIN, NULL, NULL);
            h_aDepositLbl = CreateWindow("STATIC", "Opening deposit", WS_CHILD, 350, 208, 300, 18, hwnd, NULL, NULL, NULL);
            h_aDeposit    = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 350, 228, 300, 28, hwnd, (HMENU)IDC_A_DEPOSIT, NULL, NULL);
            h_aSubmitCreate = CreateWindow("BUTTON", "Create account", WS_CHILD, 20, 268, 170, 38, hwnd, (HMENU)IDC_A_SUBMIT_CREATE, NULL, NULL);
            h_aBack2        = CreateWindow("BUTTON", "Back", WS_CHILD, 200, 268, 120, 38, hwnd, (HMENU)IDC_A_BACK2, NULL, NULL);
            applyFont(h_aNameLbl, g_fontLabel); applyFont(h_aName, g_fontLabel); applyFont(h_aCnicLbl, g_fontLabel); applyFont(h_aCnic, g_fontLabel);
            applyFont(h_aContactLbl, g_fontLabel); applyFont(h_aContact, g_fontLabel); applyFont(h_aAddressLbl, g_fontLabel); applyFont(h_aAddress, g_fontLabel);
            applyFont(h_aPinLbl, g_fontLabel); applyFont(h_aPin, g_fontLabel); applyFont(h_aDepositLbl, g_fontLabel); applyFont(h_aDeposit, g_fontLabel);
            applyFont(h_aSubmitCreate, g_fontBtn); applyFont(h_aBack2, g_fontBtn);

            // ATM login
            h_tAccLbl = CreateWindow("STATIC", "Account number", WS_CHILD, 250, 116, 200, 18, hwnd, NULL, NULL, NULL);
            h_tAcc    = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 250, 136, 200, 28, hwnd, (HMENU)IDC_T_ACC, NULL, NULL);
            h_tPinLbl = CreateWindow("STATIC", "PIN", WS_CHILD, 250, 172, 200, 18, hwnd, NULL, NULL, NULL);
            h_tPin    = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER | ES_PASSWORD, 250, 192, 200, 28, hwnd, (HMENU)IDC_T_PIN, NULL, NULL);
            h_tLoginBtn = CreateWindow("BUTTON", "Sign in", WS_CHILD, 250, 232, 95, 34, hwnd, (HMENU)IDC_T_LOGIN_BTN, NULL, NULL);
            h_tBack1 = CreateWindow("BUTTON", "Back", WS_CHILD, 355, 232, 95, 34, hwnd, (HMENU)IDC_T_BACK1, NULL, NULL);
            applyFont(h_tAccLbl, g_fontLabel); applyFont(h_tAcc, g_fontLabel); applyFont(h_tPinLbl, g_fontLabel); applyFont(h_tPin, g_fontLabel);
            applyFont(h_tLoginBtn, g_fontBtn); applyFont(h_tBack1, g_fontBtn);

            // ATM menu - fields on top, vertical sidebar of actions on left, output to the right
            h_tAmountLbl = CreateWindow("STATIC", "Amount", WS_CHILD, 20, 96, 200, 18, hwnd, NULL, NULL, NULL);
            h_tAmount    = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 20, 116, 200, 28, hwnd, (HMENU)IDC_T_AMOUNT, NULL, NULL);
            h_tDestLbl   = CreateWindow("STATIC", "Destination account (for transfer)", WS_CHILD, 240, 96, 300, 18, hwnd, NULL, NULL, NULL);
            h_tDest      = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 240, 116, 200, 28, hwnd, (HMENU)IDC_T_DEST, NULL, NULL);

            h_tWithdraw  = CreateWindow("BUTTON", "Withdraw",       WS_CHILD, 20, 160, 170, 38, hwnd, (HMENU)IDC_T_WITHDRAW, NULL, NULL);
            h_tDeposit   = CreateWindow("BUTTON", "Deposit",        WS_CHILD, 20, 204, 170, 38, hwnd, (HMENU)IDC_T_DEPOSIT, NULL, NULL);
            h_tTransfer  = CreateWindow("BUTTON", "Transfer",       WS_CHILD, 20, 248, 170, 38, hwnd, (HMENU)IDC_T_TRANSFER, NULL, NULL);
            h_tStatement = CreateWindow("BUTTON", "Mini-statement", WS_CHILD, 20, 292, 170, 38, hwnd, (HMENU)IDC_T_STATEMENT, NULL, NULL);
            h_tLogout    = CreateWindow("BUTTON", "Logout",         WS_CHILD, 20, 336, 170, 38, hwnd, (HMENU)IDC_T_LOGOUT, NULL, NULL);
            applyFont(h_tAmountLbl, g_fontLabel); applyFont(h_tAmount, g_fontLabel); applyFont(h_tDestLbl, g_fontLabel); applyFont(h_tDest, g_fontLabel);
            applyFont(h_tWithdraw, g_fontBtn); applyFont(h_tDeposit, g_fontBtn); applyFont(h_tTransfer, g_fontBtn);
            applyFont(h_tStatement, g_fontBtn); applyFont(h_tLogout, g_fontBtn);

            h_output = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                20, 300, 660, 220, hwnd, (HMENU)IDC_OUTPUT, NULL, NULL);
            SendMessage(h_output, WM_SETFONT, (WPARAM)g_fontMono, TRUE);

            showView(V_START);
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND ctl = (HWND)lParam;
            if (ctl == h_header) {
                SetTextColor(hdc, RGB(255,255,255));
                SetBkColor(hdc, RGB(20,60,110));
                return (LRESULT)g_hbHeader;
            }
            SetTextColor(hdc, RGB(30,30,30));
            SetBkColor(hdc, RGB(255,255,255));
            SetBkMode(hdc, OPAQUE);
            return (LRESULT)g_hbWhite;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(255,255,255));
            return (LRESULT)g_hbWhite;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_ADMIN_ROLE_BTN: showView(V_ADMIN_LOGIN); break;
                case IDC_ATM_ROLE_BTN:   showView(V_ATM_LOGIN);   break;
                case IDC_A_BACK1: case IDC_T_BACK1: case IDC_A_BACK2: showView(V_START); break;
                case IDC_A_LOGIN_BTN: adminLogin(); break;
                case IDC_A_LOGOUT_BTN: showView(V_START); break;
                case IDC_A_CREATE_BTN: showView(V_ADMIN_CREATE); break;
                case IDC_A_SUBMIT_CREATE: adminCreateSubmit(); break;
                case IDC_A_LIST_BTN: adminList(); break;
                case IDC_A_EOD_BTN: adminEOD(); break;
                case IDC_A_FREEZE_BTN:   adminSetStatus(AccountStatus::Frozen, "FREEZE"); break;
                case IDC_A_UNFREEZE_BTN: adminSetStatus(AccountStatus::Active, "UNFREEZE"); break;
                case IDC_A_UNLOCK_BTN:   adminSetStatus(AccountStatus::Active, "UNLOCK_RESET"); break;
                case IDC_T_LOGIN_BTN: atmLogin(); break;
                case IDC_T_WITHDRAW: atmWithdraw(); break;
                case IDC_T_DEPOSIT: atmDeposit(); break;
                case IDC_T_TRANSFER: atmTransfer(); break;
                case IDC_T_STATEMENT: atmStatement(); break;
                case IDC_T_LOGOUT: atmLogout(); break;
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    FileAccountRepository     accountRepo("../data/accounts.dat");
    FileTransactionRepository txRepo("../data/transactions.dat");
    AuditLogger                audit("../logs/audit.log");
    TransactionEngine           engine(accountRepo, txRepo);

    g_accountRepo = &accountRepo;
    g_txRepo = &txRepo;
    g_audit = &audit;
    g_engine = &engine;

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = "BankingGUI";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        "BankingGUI", "SecureBank",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 580,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}