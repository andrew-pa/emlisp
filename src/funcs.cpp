#include "emlisp.h"
#include <cmath>

namespace emlisp {
void runtime::define_intrinsics() {
    define_fn("cons", [](runtime* rt, value args, void* d) {
        return rt->cons(first(args), first(second(args)));
    });

    define_fn("car", [](runtime* rt, value args, void* d) { return first(first(args)); });

    define_fn("cdr", [](runtime* rt, value args, void* d) { return second(first(args)); });

    define_fn("eq?", [](runtime* rt, value args, void* d) {
        value a = first(args);
        value b = first(second(args));
        return rt->from_bool(a == b);
    });

    define_fn("nil?", [](runtime* rt, value args, void* d) {
        return rt->from_bool(type_of(first(args)) == value_type::nil);
    });
    define_fn("bool?", [](runtime* rt, value args, void* d) {
        return rt->from_bool(type_of(first(args)) == value_type::bool_t);
    });
    define_fn("int?", [](runtime* rt, value args, void* d) {
        return rt->from_bool(type_of(first(args)) == value_type::int_t);
    });
    define_fn("float?", [](runtime* rt, value args, void* d) {
        return rt->from_bool(type_of(first(args)) == value_type::float_t);
    });
    define_fn("str?", [](runtime* rt, value args, void* d) {
        return rt->from_bool(type_of(first(args)) == value_type::str);
    });
    define_fn("sym?", [](runtime* rt, value args, void* d) {
        return rt->from_bool(type_of(first(args)) == value_type::sym);
    });
    define_fn("cons?", [](runtime* rt, value args, void* d) {
        return rt->from_bool(type_of(first(args)) == value_type::cons);
    });
    define_fn("proc?", [](runtime* rt, value args, void* d) {
        return rt->from_bool(type_of(first(args)) == value_type::closure);
    });

    // bool //
    define_fn("not", [](runtime* rt, value args, void* d) {
        return first(args) == TRUE ? FALSE : TRUE;
    });

    // shared math //
#define MATH_OP(NAME, OP)                                                                          \
    define_fn(NAME, [](runtime* rt, value args, void* d) {                                         \
        auto ty = type_of(first(args));                                                            \
        if(ty == value_type::int_t) {                                                              \
            int64_t result = to_int(first(args));                                                  \
            args           = second(args);                                                         \
            while(args != NIL) {                                                                   \
                auto      x = to_int(first(args));                                                 \
                result OP x;                                                                       \
                args = second(args);                                                               \
            }                                                                                      \
            return rt->from_int(result);                                                           \
        }                                                                                          \
        if(ty == value_type::float_t) {                                                            \
            float result = to_float(first(args));                                                  \
            args         = second(args);                                                           \
            while(args != NIL) {                                                                   \
                auto      x = to_float(first(args));                                               \
                result OP x;                                                                       \
                args = second(args);                                                               \
            }                                                                                      \
            return rt->from_float(result);                                                         \
        }                                                                                          \
        throw std::runtime_error("expected numerical type to math " NAME);                         \
    })

    MATH_OP("+", +=);
    MATH_OP("-", -=);
    MATH_OP("*", *=);
    MATH_OP("/", /=);

    // int //
#define BIT_OP(NAME, OP)                                                                           \
    define_fn(NAME, [](runtime* rt, value args, void* d) {                                         \
        int64_t result = to_int(first(args));                                                      \
        args           = second(args);                                                             \
        while(args != NIL) {                                                                       \
            auto      x = to_int(first(args));                                                     \
            result OP x;                                                                           \
            args = second(args);                                                                   \
        }                                                                                          \
        return rt->from_int(result);                                                               \
    })
    BIT_OP("bit&", &=);
    BIT_OP("bit|", |=);
    BIT_OP("bit^", ^=);
    BIT_OP("bit-lsh", <<=);
    BIT_OP("bit-rsh", >>=);

    // float //
    define_fn("sin", [](runtime* rt, value args, void* d) {
        return rt->from_float(std::sin(to_float(first(args))));
    });
    define_fn("cos", [](runtime* rt, value args, void* d) {
        return rt->from_float(std::cos(to_float(first(args))));
    });
    define_fn("tan", [](runtime* rt, value args, void* d) {
        return rt->from_float(std::tan(to_float(first(args))));
    });
    define_fn("exp", [](runtime* rt, value args, void* d) {
        return rt->from_float(std::exp(to_float(first(args))));
    });
    define_fn("ln", [](runtime* rt, value args, void* d) {
        return rt->from_float(std::log(to_float(first(args))));
    });

    // string //
    define_fn("string-length", [](runtime* rt, value args, void* d) {
        value v = first(args);
        check_type(v, value_type::str);
        return rt->from_int(*(uint32_t*)(v >> 4));
    });

    define_fn("string->symbol", [](runtime* rt, value args, void* d) {
        return rt->symbol(rt->to_str(first(args)));
    });

    // symbol //
    define_fn("symbol->string", [](runtime* rt, value args, void* d) {
        return rt->from_str(rt->symbol_str(first(args)));
    });
}

void runtime::define_std_functions() {}
}  // namespace emlisp
