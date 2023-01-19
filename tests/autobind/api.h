#pragma once
#include <iostream>
#include <string>
#include <memory>
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
    EL_M int test(std::shared_ptr<int> x) { return *x; }
};

EL_OBJ struct counter {
    EL_PROP(r) size_t value;

    EL_M void reset() {
        std::cout << "counter reset!\n";
        value = 0;
    }

    EL_M size_t increment(size_t amount = 1) {
        value += amount;
        std::cout << "counter = " << value << "!\n";
        return value;
    }

    EL_M EL_WITH_CX void test_context(int* cx) {
        *cx = value;
    }

    EL_M template<typename T> EL_KNOWN_INSTS(<int> <float>)
    void test_templates(T t) {
        value += size_t(t);
    }
};
