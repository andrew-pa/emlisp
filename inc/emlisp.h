#pragma once
#include <cstdint>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <set>

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
        std::map<value, value> data;

        value get(value name);
		void set(value name, value val);
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
        frame* alloc_frame();
    };

    typedef value(*extern_func_t)(class runtime*, value, void*);

    class runtime {
        std::unique_ptr<struct memory> h;
        std::vector<std::string> symbols;
        std::vector<function> functions;
        value parse_value(std::string_view src, size_t& i);

        value sym_quote, sym_lambda, sym_if, sym_set,
            sym_cons, sym_car, sym_cdr, sym_eq, sym_define,
            sym_nilp, sym_boolp, sym_intp, sym_floatp, sym_strp,
            sym_symp, sym_consp, sym_procp;

        std::vector<value> reserved_syms;

        std::vector<std::map<value, value>> scopes;
        value look_up(value name);

        void compute_closure(value v, const std::set<value>& bound, std::set<value>& free);
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
        const std::string& symbol_str(value sym) const;

        value cons(value fst = NIL, value snd = NIL);

        value read(std::string_view src);
        std::vector<value> read_all(std::string_view src);
        void write(std::ostream&, value);

        value eval(value x);

        void define_fn(std::string_view name, extern_func_t fn, void* data);
    };
}
