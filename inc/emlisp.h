#pragma once
#include <cstdint>
#include <cassert>
#include <memory>
#include <string>
#include <vector>

namespace emlisp {
    using value = uint64_t;

    enum class value_type {
        nil     = 0x0,
        bool_t  = 0x1,
        int_t   = 0x2,
        float_t = 0x3,
        sym   = 0x4,
        str   = 0x5,
        cons    = 0xf
    };

    constexpr value NIL = value(0);
    constexpr value TRUE = value(0x11);
    constexpr value FALSE = value(001);
    inline value_type type_of(value v) {
        return value_type(v & 0xf);
    }
    inline value& first(value cell) {
        assert(type_of(cell) == value_type::cons);
        return *(value*)(cell >> 4);
    }
    inline value& second(value cell) {
        assert(type_of(cell) == value_type::cons);
        return *((value*)(cell >> 4) + 1);
    }

    struct heap {
        value* cons;
        value* next_cons;
        char* strings;
        char* next_str;

        heap(size_t num_cons = 8192, size_t num_str_bytes = 1024*1024);
        value alloc_cons(value fst = NIL, value snd = NIL);

        value make_str(std::string_view src);
    };

    class runtime {
        std::unique_ptr<struct heap> h;
        std::vector<std::string> symbols;
        value parse_value(std::string_view src, size_t& i);
    public:
        runtime();

        inline value from_bool(bool b) {
            return b ? 0x11 : 0x01;
        }

        inline value from_int(int64_t v) {
            return (uint64_t)(v << 4) | (uint64_t)value_type::int_t;
        }

        inline value from_float(float v) {
            auto *vp = (uint64_t*)&v;
            return (*vp << 4) | (uint64_t)value_type::float_t;
        }

        value from_str(std::string_view s);

        value symbol(std::string_view s);

        value cons(value fst = NIL, value snd = NIL);

        value read(std::string_view src);
        void write(std::ostream&, value);

        value eval(value x);
    };

}
