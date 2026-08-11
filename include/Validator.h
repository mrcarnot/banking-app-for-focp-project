#pragma once
#include "Account.h"
#include "Config.h"
#include <string>
#include <cctype>

namespace Validator {

    inline bool isValidAmount(double amt) {
        return amt > 0;
    }

    inline bool isValidPin(const std::string& pin) {
        if (pin.length() != 4) return false;
        for (char c : pin) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }

    inline bool isValidCnic(const std::string& cnic) {
        if (cnic.length() != 13) return false;
        for (char c : cnic) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }

    inline bool isAccountActive(const Account& a) {
        return a.status == AccountStatus::Active;
    }

    inline bool respectsMinBalance(const Account& a, double withdrawAmt) {
        return (a.balance - withdrawAmt) >= Config::MIN_BALANCE;
    }

    inline bool isWithinDailyLimit(const Account& a, double withdrawAmt) {
        return (a.dailyWithdrawn + withdrawAmt) <= Config::DAILY_WITHDRAW_LIMIT;
    }

    // BONUS #9: PIN hashing (djb2 algorithm)
    inline std::string hashPin(const std::string& pin) {
        unsigned long hash = 5381;
        for (char c : pin) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
        }
        return std::to_string(hash);
    }
}