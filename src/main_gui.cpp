#include <windows.h>
#include <string>
#include "../include/FileAccountRepository.h"
#include "../include/FileTransactionRepository.h"
#include "../include/LoanRepository.h"
#include "../include/TransactionEngine.h"
#include "../include/Validator.h"
#include "../include/AuditLogger.h"
#include "../include/DateTime.h"
#include "../include/CashInventory.h"

// --- Control IDs: every button/field needs a unique numeric ID ---
#define IDC_ACCNUM_EDIT   101
#define IDC_PIN_EDIT      102
#define IDC_LOGIN_BTN     103
#define IDC_STATUS_LABEL  104

#define IDC_BALANCE_LABEL 201
#define IDC_AMOUNT_EDIT   202
#define IDC_DEST_EDIT     203
#define IDC_WITHDRAW_BTN  204
#define IDC_DEPOSIT_BTN   205
#define IDC_TRANSFER_BTN  206
#define IDC_STATEMENT_BTN 207
#define IDC_LOGOUT_BTN    208

// --- Same repositories and engine as the console app - this is the whole point ---
FileAccountRepository*     g_accountRepo = nullptr;
FileTransactionRepository* g_txRepo = nullptr;
AuditLogger*                g_audit = nullptr;
TransactionEngine*          g_engine = nullptr;
Account                      g_currentAccount;
bool                         g_loggedIn = false;

// --- Handles to the controls, so we can show/hide/read them ---
HWND h_accNumEdit, h_pinEdit, h_loginBtn, h_statusLabel, h_accNumLabel, h_pinLabel;
HWND h_balanceLabel, h_amountEdit, h_destEdit, h_withdrawBtn, h_depositBtn, h_transferBtn, h_statementBtn, h_logoutBtn;

std::string getEditText(HWND hEdit) {
    char buf[256];
    GetWindowText(hEdit, buf, sizeof(buf));
    return std::string(buf);
}

void setStatus(const std::string& msg) {
    SetWindowText(h_statusLabel, msg.c_str());
}

void showLoginView(HWND hwnd) {
    ShowWindow(h_accNumLabel, SW_SHOW);
    ShowWindow(h_accNumEdit, SW_SHOW);
    ShowWindow(h_pinLabel, SW_SHOW);
    ShowWindow(h_pinEdit, SW_SHOW);
    ShowWindow(h_loginBtn, SW_SHOW);

    ShowWindow(h_balanceLabel, SW_HIDE);
    ShowWindow(h_amountEdit, SW_HIDE);
    ShowWindow(h_destEdit, SW_HIDE);
    ShowWindow(h_withdrawBtn, SW_HIDE);
    ShowWindow(h_depositBtn, SW_HIDE);
    ShowWindow(h_transferBtn, SW_HIDE);
    ShowWindow(h_statementBtn, SW_HIDE);
    ShowWindow(h_logoutBtn, SW_HIDE);
}

void showMenuView(HWND hwnd) {
    ShowWindow(h_accNumLabel, SW_HIDE);
    ShowWindow(h_accNumEdit, SW_HIDE);
    ShowWindow(h_pinLabel, SW_HIDE);
    ShowWindow(h_pinEdit, SW_HIDE);
    ShowWindow(h_loginBtn, SW_HIDE);

    ShowWindow(h_balanceLabel, SW_SHOW);
    ShowWindow(h_amountEdit, SW_SHOW);
    ShowWindow(h_destEdit, SW_SHOW);
    ShowWindow(h_withdrawBtn, SW_SHOW);
    ShowWindow(h_depositBtn, SW_SHOW);
    ShowWindow(h_transferBtn, SW_SHOW);
    ShowWindow(h_statementBtn, SW_SHOW);
    ShowWindow(h_logoutBtn, SW_SHOW);

    std::string bal = "Balance: " + std::to_string(g_currentAccount.balance);
    SetWindowText(h_balanceLabel, bal.c_str());
}

void doLogin() {
    std::string num = getEditText(h_accNumEdit);
    std::string pin = getEditText(h_pinEdit);

    auto res = g_accountRepo->findByNumber(num);
    if (!res.ok) {
        setStatus("Account not found.");
        return;
    }
    Account acc = res.value;

    if (acc.status == AccountStatus::Closed)  { setStatus("This account is closed."); return; }
    if (acc.status == AccountStatus::Frozen)  { setStatus("This account is frozen."); return; }
    if (acc.status == AccountStatus::Locked)  { setStatus("This account is locked."); return; }

    if (Validator::hashPin(pin) != acc.pinHash) {
        acc.pinAttempts++;
        if (acc.pinAttempts >= 3) {
            acc.status = AccountStatus::Locked;
            acc.lockTimestamp = DateTime::now();
            setStatus("Too many failed attempts. Account locked.");
        } else {
            setStatus("Incorrect PIN. Attempts: " + std::to_string(acc.pinAttempts) + "/3");
        }
        g_accountRepo->save(acc);
        return;
    }

    acc.pinAttempts = 0;
    g_accountRepo->save(acc);
    g_currentAccount = acc;
    g_loggedIn = true;
    g_audit->log(DateTime::now(), acc.accountNumber, "LOGIN", "GUI ATM login successful");
    setStatus("Welcome, " + acc.holderName + "!");
    showMenuView(NULL);
}

void doWithdraw() {
    double amount = atof(getEditText(h_amountEdit).c_str());
    auto result = g_engine->withdraw(g_currentAccount, amount, DateTime::now());
    if (result.ok) {
        setStatus("Withdrawal successful. New balance: " + std::to_string(g_currentAccount.balance));
        SetWindowText(h_balanceLabel, ("Balance: " + std::to_string(g_currentAccount.balance)).c_str());
    } else {
        setStatus("Failed: " + result.error);
    }
}

void doDeposit() {
    double amount = atof(getEditText(h_amountEdit).c_str());
    auto result = g_engine->deposit(g_currentAccount, amount, DateTime::now());
    if (result.ok) {
        setStatus("Deposit successful. New balance: " + std::to_string(g_currentAccount.balance));
        SetWindowText(h_balanceLabel, ("Balance: " + std::to_string(g_currentAccount.balance)).c_str());
    } else {
        setStatus("Failed: " + result.error);
    }
}

void doTransfer() {
    double amount = atof(getEditText(h_amountEdit).c_str());
    std::string destNum = getEditText(h_destEdit);

    auto destRes = g_accountRepo->findByNumber(destNum);
    if (!destRes.ok) {
        setStatus("Destination account not found.");
        return;
    }
    Account dest = destRes.value;

    auto result = g_engine->transfer(g_currentAccount, dest, amount, DateTime::now());
    if (result.ok) {
        setStatus("Transfer successful. New balance: " + std::to_string(g_currentAccount.balance));
        SetWindowText(h_balanceLabel, ("Balance: " + std::to_string(g_currentAccount.balance)).c_str());
    } else {
        setStatus("Failed: " + result.error);
    }
}

void doStatement() {
    auto txs = g_txRepo->loadForAccount(g_currentAccount.accountNumber);
    std::string msg;
    int count = 0;
    for (auto it = txs.rbegin(); it != txs.rend() && count < 5; ++it, ++count) {
        msg += it->timestamp + " - " + std::to_string(it->amount) + "\n";
    }
    if (msg.empty()) msg = "No transactions yet.";
    MessageBox(NULL, msg.c_str(), "Mini Statement", MB_OK);
}

void doLogout() {
    g_audit->log(DateTime::now(), g_currentAccount.accountNumber, "LOGOUT", "GUI ATM session ended");
    g_loggedIn = false;
    setStatus("Logged out. Enter account number to login again.");
    SetWindowText(h_accNumEdit, "");
    SetWindowText(h_pinEdit, "");
    showLoginView(NULL);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            h_accNumLabel = CreateWindow("STATIC", "Account Number:", WS_CHILD | WS_VISIBLE, 20, 20, 150, 20, hwnd, NULL, NULL, NULL);
            h_accNumEdit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 180, 20, 200, 20, hwnd, (HMENU)IDC_ACCNUM_EDIT, NULL, NULL);

            h_pinLabel = CreateWindow("STATIC", "PIN:", WS_CHILD | WS_VISIBLE, 20, 50, 150, 20, hwnd, NULL, NULL, NULL);
            h_pinEdit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD, 180, 50, 200, 20, hwnd, (HMENU)IDC_PIN_EDIT, NULL, NULL);

            h_loginBtn = CreateWindow("BUTTON", "Login", WS_CHILD | WS_VISIBLE, 180, 80, 100, 30, hwnd, (HMENU)IDC_LOGIN_BTN, NULL, NULL);

            h_balanceLabel = CreateWindow("STATIC", "Balance: 0.00", WS_CHILD, 20, 20, 300, 20, hwnd, (HMENU)IDC_BALANCE_LABEL, NULL, NULL);

            CreateWindow("STATIC", "Amount:", WS_CHILD, 20, 50, 100, 20, hwnd, NULL, NULL, NULL);
            h_amountEdit = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 130, 50, 150, 20, hwnd, (HMENU)IDC_AMOUNT_EDIT, NULL, NULL);

            CreateWindow("STATIC", "Dest. Account (for transfer):", WS_CHILD, 20, 80, 200, 20, hwnd, NULL, NULL, NULL);
            h_destEdit = CreateWindow("EDIT", "", WS_CHILD | WS_BORDER, 220, 80, 150, 20, hwnd, (HMENU)IDC_DEST_EDIT, NULL, NULL);

            h_withdrawBtn  = CreateWindow("BUTTON", "Withdraw",       WS_CHILD, 20, 120, 100, 30, hwnd, (HMENU)IDC_WITHDRAW_BTN, NULL, NULL);
            h_depositBtn   = CreateWindow("BUTTON", "Deposit",        WS_CHILD, 130, 120, 100, 30, hwnd, (HMENU)IDC_DEPOSIT_BTN, NULL, NULL);
            h_transferBtn  = CreateWindow("BUTTON", "Transfer",       WS_CHILD, 240, 120, 100, 30, hwnd, (HMENU)IDC_TRANSFER_BTN, NULL, NULL);
            h_statementBtn = CreateWindow("BUTTON", "Mini-Statement", WS_CHILD, 20, 160, 150, 30, hwnd, (HMENU)IDC_STATEMENT_BTN, NULL, NULL);
            h_logoutBtn    = CreateWindow("BUTTON", "Logout",         WS_CHILD, 180, 160, 100, 30, hwnd, (HMENU)IDC_LOGOUT_BTN, NULL, NULL);

            h_statusLabel = CreateWindow("STATIC", "Enter your account number and PIN.", WS_CHILD | WS_VISIBLE, 20, 200, 440, 60, hwnd, (HMENU)IDC_STATUS_LABEL, NULL, NULL);

            showLoginView(hwnd);
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case IDC_LOGIN_BTN:    doLogin();    break;
                case IDC_WITHDRAW_BTN: doWithdraw(); break;
                case IDC_DEPOSIT_BTN:  doDeposit();  break;
                case IDC_TRANSFER_BTN: doTransfer(); break;
                case IDC_STATEMENT_BTN:doStatement();break;
                case IDC_LOGOUT_BTN:   doLogout();   break;
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
    wc.lpszClassName = "BankingATM";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        "BankingATM", "Banking System - ATM",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 320,
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