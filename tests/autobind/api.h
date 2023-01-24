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

EL_TYPEDEF using long_name_for_int = int;

EL_OBJ struct test_simple_method {
    EL_M void run() {}
    EL_M void skip(int amount) {}
    EL_M void leap(long_name_for_int amount, float yaw) {}
    EL_M void hop(int* amount) {}
    EL_M void skirt(int* amount, char x) {}
    EL_M size_t* jump(bool yeet) {return nullptr;}
    EL_M int test(std::shared_ptr<int> x) { return *x; }
};

struct asdf {
    using test = int;
};

EL_OBJ struct counter {
    EL_PROP(r) size_t value;

    EL_C counter(size_t v) : value(v) {};

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

    EL_M template<typename T> EL_KNOWN_INSTS(<asdf>)
    void test_templates2(typename T::test t) {
    }
};

EL_OBJ struct test_fn {
    test_fn() = default;

    EL_C test_fn(int x) {
        assert(x == 3);
    }

    EL_M void times(int count, const std::function<void(int)>& f) {
        for(int i = 0; i < count; ++i) {
            f(i);
        }
    }

    EL_M int times2(int count, int stride, const std::function<int(int, bool)>& f) {
        int x = 0;
        for(int i = 0; i < count; ++i) {
            x += f(i*stride, i % 2 == 0);
        }
        return x;
    }
};
