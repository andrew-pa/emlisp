#pragma once
#include <string>
#include "emlisp_autobind.h"

EL_OBJ struct test_simple_pod {
    EL_PROP(rw) int num1;
    EL_PROP(r) size_t num2;
    EL_PROP(r) char* num3;
    EL_PROP(rw) std::string str;
    EL_PROP(rw) std::shared_ptr<std::string> shared_str;
};

EL_OBJ struct test_simple_method {
    EL_M void run() {}
    EL_M void skip(int amount) {}
    EL_M void leap(int amount, float yaw) {}
    EL_M void hop(int* amount) {}
    EL_M void skirt(int* amount, char x) {}
    EL_M size_t* jump(bool yeet) {return nullptr;}
};

EL_OBJ struct counter {
    EL_PROP(r) size_t value;

    EL_M void reset() {
        value = 0;
    }

    EL_M size_t increment(size_t amount = 1) {
        value += amount;
        return value;
    }
};
