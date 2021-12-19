#pragma once
#include <cstdint>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

namespace emlisp {
    using value = uint64_t;

    enum class value_type {
        nil = 0x0,
        bool_t = 0x1,
        int_t = 0x2,
        float_t = 0x3,
        sym = 0x4,
        str = 0x5,
        _extern = 0xd,
        closure = 0xe,
        cons    = 0xf
    };

    std::ostream& operator <<(std::ostream&, value_type vt);

    constexpr value NIL = value(0);
    constexpr value TRUE = value(0x11);
    constexpr value FALSE = value(001);

    inline value_type type_of(value v) {
        return value_type(v & 0xf);
    }

    struct type_mismatch_error : public std::runtime_error {
        value_type expected;
        value_type actual;
        type_mismatch_error(const std::string& msg, value_type ex, value_type ac)
            : std::runtime_error(msg), expected(ex), actual(ac) {}
    };

    inline void check_type(value v, value_type t, const std::string& msg = "type check failed") {
        if (type_of(v) != t)
            throw type_mismatch_error(msg, t, type_of(v));
    }

    inline value& first(value cell) {
        check_type(cell, value_type::cons);
        return *(value*)(cell >> 4);
    }
    inline value& second(value cell) {
        check_type(cell, value_type::cons);
        return *((value*)(cell >> 4) + 1);
    }

    struct function {
        std::vector<value> arguments;
        value body;
        
        function(value arg_list, value body);
    };

    struct frame {
        frame* parent;
        size_t size;
        
        value& indexed(size_t index) {
            assert(index < size);
            return *(value*)(((uint8_t*)this) + (sizeof(frame*)+sizeof(size_t)) + sizeof(value)*2*index);
        }
        
        // this could really be a hashtable, it's pretty darn close
        // basically just use name%size to find the index, then if that slot is full move to the next one etc
        void set_at(size_t index, value name, value val) {
            assert(index < size);
            value* v = (value*)(((uint8_t*)this) 
                + (sizeof(frame*)+sizeof(size_t)) + sizeof(value)*2*index);
            *v = name;
            *(v + 1) = val;
        }

        value get(value sym) {
            check_type(sym, value_type::sym);
            value* v = (value*)(((uint8_t*)this) + (sizeof(frame*) + sizeof(size_t)));
            for (size_t i = 0; i < size; ++i) {
                if (v[i * 2] == sym) {
                    return v[i * 2 + 1];
                }
            }
            if (parent == nullptr) return NIL;
            return parent->get(sym);
        }
        
        void set(value sym, value val) {
            check_type(sym, value_type::sym);
            value* v = (value*)(((uint8_t*)this) + (sizeof(frame*) + sizeof(size_t)));
            for (size_t i = 0; i < size; ++i) {
                if (v[i * 2] == sym) {
                    v[i * 2 + 1] = val;
                }
            }
            if (parent == nullptr) return;
            return parent->set(sym, val);
        }
    };


    struct memory {
        value* cons;
        value* next_cons;
        char* strings;
        char* next_str;
        uint8_t* frames;
        uint8_t* next_frame;

        memory(
            size_t num_cons = 8192,
            size_t num_str_bytes = 1024*1024,
            size_t num_frame_bytes = 8192
        );

        value alloc_cons(value fst = NIL, value snd = NIL);
        value make_str(std::string_view src);
        frame* alloc_frame(frame* parent, size_t size);
    };

    class runtime {
        std::unique_ptr<struct memory> h;
        std::vector<std::string> symbols;
        std::vector<function> functions;
        value parse_value(std::string_view src, size_t& i);

        value sym_quote, sym_lambda, sym_if, sym_set,
            sym_cons, sym_car, sym_cdr;

        frame* global_scope;
        value eval(value x, frame* f);
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
