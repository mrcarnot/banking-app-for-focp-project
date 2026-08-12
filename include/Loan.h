#pragma once
#include <string>

enum class LoanStatus { Active, Repaid, Defaulted };

struct Loan {
    std::string loanId;
    std::string accountNumber;
    double      principal;
    double      outstanding;
    double      monthlyInstallment;
    int         totalInstallments;
    int         paidInstallments;
    std::string issuedDate;
    LoanStatus  status;
};