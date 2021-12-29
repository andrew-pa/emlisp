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

    typedef value(*extern_func_t)(class runtime*, value, void*);

    struct heap_info {
        size_t new_size, old_size;
    };

    class runtime {
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
        
        uint8_t* heap;
        uint8_t* heap_next;
        size_t heap_size;

        frame* alloc_frame();
    
        void gc_process(value& c,
            std::map<value, value>& live_vals,
            uint8_t*& new_next);

        std::map<uint64_t, std::pair<value, uint64_t>> extern_values;
        uint64_t next_extern_value_handle;
    public:
        runtime(size_t heap_size = 1024*1024);

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
        value read_all(std::string_view src);
        void write(std::ostream&, value);

        value eval(value x);

        void define_fn(std::string_view name, extern_func_t fn, void* data);

        void collect_garbage(heap_info* res_info = nullptr);

        friend class value_handle;

        value_handle handle_for(value v);
    };

    // must live as long as the runtime from which it was obtained
    class value_handle {
        runtime* rt;
        uint64_t h;
    public:
        value_handle(runtime* rt, uint64_t h) : rt(rt), h(h) {}
        value_handle(const value_handle& other);
        value_handle& operator =(const value_handle& other);
        value_handle(value_handle&& other);
        value_handle& operator =(value_handle&& other);
        const value& operator*() const;
        value& operator*();
        operator value();
        ~value_handle();
    };
}
