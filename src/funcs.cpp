#include "emlisp.h"

namespace emlisp {
    void runtime::define_intrinsics() {
        define_fn("cons", [](runtime* rt, value args, void* d) {
            return rt->cons(first(args), first(second(args)));
        });

        define_fn("car", [](runtime* rt, value args, void* d) {
            return first(first(args));
        });

        define_fn("cdr", [](runtime* rt, value args, void* d) {
            return second(first(args));
        });

        define_fn("eq?", [](runtime* rt, value args, void* d) {
            value a = first(args);
            value b = first(second(args));
            return rt->from_bool(a == b);
        });

        define_fn("nil?", [](runtime* rt, value args, void* d) { return rt->from_bool(type_of(first(args)) == value_type::nil); });
        define_fn("bool?", [](runtime* rt, value args, void* d) { return rt->from_bool(type_of(first(args)) == value_type::bool_t); });
        define_fn("int?", [](runtime* rt, value args, void* d) { return rt->from_bool(type_of(first(args)) == value_type::int_t); });
        define_fn("float?", [](runtime* rt, value args, void* d) { return rt->from_bool(type_of(first(args)) == value_type::float_t); });
        define_fn("str?", [](runtime* rt, value args, void* d) { return rt->from_bool(type_of(first(args)) == value_type::str); });
        define_fn("sym?", [](runtime* rt, value args, void* d) { return rt->from_bool(type_of(first(args)) == value_type::sym); });
        define_fn("cons?", [](runtime* rt, value args, void* d) { return rt->from_bool(type_of(first(args)) == value_type::cons); });
        define_fn("proc?", [](runtime* rt, value args, void* d) { return rt->from_bool(type_of(first(args)) == value_type::closure); });
    }

    void runtime::define_std_functions() {
    }
}


