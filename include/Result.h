#pragma once
#include <string>

template<typename T, typename E>
class Result {
public:
    bool ok;
    T    value;
    E    error;

    static Result success(T val) {
        Result r;
        r.ok = true;
        r.value = val;
        return r;
    }

    static Result failure(E err) {
        Result r;
        r.ok = false;
        r.error = err;
        return r;
    }
};