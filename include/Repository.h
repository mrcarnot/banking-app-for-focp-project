#pragma once
#include <vector>
#include <string>
#include "Account.h"
#include "Transaction.h"
#include "Result.h"

class IAccountRepository {
public:
    virtual std::vector<Account>         loadAll() = 0;
    virtual bool                         save(const Account& acc) = 0;
    virtual bool                         saveAll(const std::vector<Account>& accounts) = 0;
    virtual Result<Account, std::string> findByNumber(const std::string& num) = 0;
    virtual ~IAccountRepository() = default;
};

struct TxFilter {
    std::string accountNumber;
    std::string dateFrom;
    std::string dateTo;
    bool        filterByType = false;
    TxType      type = TxType::Deposit;
    double      minAmount = -1;
    double      maxAmount = -1;
};

class ITransactionRepository {
public:
    virtual bool                      append(const Transaction& tx) = 0;
    virtual std::vector<Transaction>  loadForAccount(const std::string& num) = 0;
    virtual std::vector<Transaction>  loadAll() = 0;
    virtual std::vector<Transaction>  query(const TxFilter& f) = 0;
    virtual ~ITransactionRepository() = default;
};