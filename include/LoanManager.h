#pragma once
#include "Account.h"
#include "Loan.h"
#include "Repository.h"
#include "Validator.h"
#include "LoanRepository.h"
#include "Result.h"
#include "Config.h"
#include "DateTime.h"
#include <string>

class LoanManager {
    IAccountRepository&     accountRepo;
    ITransactionRepository& txRepo;
    ILoanRepository&        loanRepo;

    std::string makeLoanId() {
        auto all = loanRepo.loadAll();
        int maxNum = 5000;
        for (const auto& l : all) {
            if (l.loanId.size() > 2 && l.loanId.substr(0, 2) == "LN") {
                try {
                    int n = std::stoi(l.loanId.substr(2));
                    if (n > maxNum) maxNum = n;
                } catch (...) {}
            }
        }
        return "LN" + std::to_string(maxNum + 1);
    }

public:
    LoanManager(IAccountRepository& accRepo, ITransactionRepository& transRepo, ILoanRepository& lnRepo)
        : accountRepo(accRepo), txRepo(transRepo), loanRepo(lnRepo) {}

    // Returns the maximum eligible loan amount on success
    Result<double, std::string> checkEligibility(const Account& acc) {
        if (!Validator::isAccountActive(acc)) {
            return Result<double, std::string>::failure("Account must be Active to apply for a loan");
        }
        if (acc.loanOutstanding > 0) {
            return Result<double, std::string>::failure("Existing loan must be fully repaid first");
        }
        if (DateTime::daysSince(acc.openedDate) < 30) {
            return Result<double, std::string>::failure("Account must be at least 30 days old");
        }
        if (acc.balance < Config::MIN_BALANCE_FOR_PROFIT) {
            return Result<double, std::string>::failure("Insufficient balance to qualify for a loan");
        }
        return Result<double, std::string>::success(acc.balance * Config::MAX_LOAN_MULTIPLE);
    }

    Result<Loan, std::string> disburseLoan(Account& acc, double amount, int installments, const std::string& timestamp) {
        auto eligible = checkEligibility(acc);
        if (!eligible.ok) {
            return Result<Loan, std::string>::failure(eligible.error);
        }
        if (amount <= 0 || amount > eligible.value) {
            return Result<Loan, std::string>::failure("Requested amount exceeds eligible limit");
        }
        if (installments <= 0) {
            return Result<Loan, std::string>::failure("Installment count must be positive");
        }

        Account backup = acc;

        Loan loan;
        loan.loanId             = makeLoanId();
        loan.accountNumber      = acc.accountNumber;
        loan.principal           = amount;
        loan.outstanding         = amount;
        loan.monthlyInstallment  = amount / installments;
        loan.totalInstallments   = installments;
        loan.paidInstallments    = 0;
        loan.issuedDate          = DateTime::today();
        loan.status              = LoanStatus::Active;

        acc.balance          += amount;
        acc.loanOutstanding  += amount;

        if (!accountRepo.save(acc)) {
            acc = backup;
            return Result<Loan, std::string>::failure("Failed to save account - loan not issued");
        }
        loanRepo.save(loan);

        Transaction tx;
        tx.txId = "TX" + loan.loanId.substr(2);   // reuse the loan's number for a related tx id
        tx.accountNumber = acc.accountNumber;
        tx.type = TxType::LoanDisbursement;
        tx.amount = amount;
        tx.timestamp = timestamp;
        tx.resultingBalance = acc.balance;
        tx.referenceId = loan.loanId;
        tx.description = "Loan disbursed";
        txRepo.append(tx);

        return Result<Loan, std::string>::success(loan);
    }

    Result<Transaction, std::string> repayInstallment(Account& acc, Loan& loan, const std::string& timestamp) {
        if (loan.status != LoanStatus::Active) {
            return Result<Transaction, std::string>::failure("This loan is not active");
        }
        double payment = (loan.outstanding < loan.monthlyInstallment) ? loan.outstanding : loan.monthlyInstallment;
        if (acc.balance < payment) {
            return Result<Transaction, std::string>::failure("Insufficient balance to make this payment");
        }

        Account accBackup = acc;
        Loan loanBackup = loan;

        acc.balance          -= payment;
        acc.loanOutstanding   -= payment;
        if (acc.loanOutstanding < 0) acc.loanOutstanding = 0;

        loan.outstanding      -= payment;
        loan.paidInstallments += 1;
        if (loan.outstanding <= 0.01) {
            loan.outstanding = 0;
            loan.status = LoanStatus::Repaid;
        }

        if (!accountRepo.save(acc)) {
            acc = accBackup;
            loan = loanBackup;
            return Result<Transaction, std::string>::failure("Failed to save account - payment rolled back");
        }
        loanRepo.save(loan);

        Transaction tx;
        tx.txId = "TXLR" + loan.loanId.substr(2) + std::to_string(loan.paidInstallments);
        tx.accountNumber = acc.accountNumber;
        tx.type = TxType::LoanRepayment;
        tx.amount = payment;
        tx.timestamp = timestamp;
        tx.resultingBalance = acc.balance;
        tx.referenceId = loan.loanId;
        tx.description = "Loan installment payment";
        txRepo.append(tx);

        return Result<Transaction, std::string>::success(tx);
    }
};