#pragma once
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace emlisp {
using value = uint64_t;

enum class value_type {
    nil     = 0x0,
    bool_t  = 0x1,
    int_t   = 0x2,
    float_t = 0x3,
    sym     = 0x4,
    str     = 0x5,
    fvec    = 0x6,
    _extern = 0xd,
    closure = 0xe,
    cons    = 0xf
};

std::ostream& operator<<(std::ostream&, value_type vt);

constexpr value NIL   = value(0);
constexpr value TRUE  = value(0x11);
constexpr value FALSE = value(001);

inline value_type type_of(value v) { return value_type(v & 0xf); }

struct type_mismatch_error : public std::runtime_error {
    value_type expected;
    value_type actual;
    value      trace;

    type_mismatch_error(const std::string& msg, value_type ex, value_type ac)
        : std::runtime_error(msg), expected(ex), actual(ac), trace(NIL) {}

    type_mismatch_error(const type_mismatch_error& e, class runtime*, value responsible);
};

inline void check_type(value v, value_type t, const std::string& msg = "type check failed") {
    if(type_of(v) != t) throw type_mismatch_error(msg, t, type_of(v));
}

inline value& first(value cell) {
    check_type(cell, value_type::cons);
    return *(value*)(cell >> 4);
}

inline value& second(value cell) {
    check_type(cell, value_type::cons);
    return *((value*)(cell >> 4) + 1);
}

inline value nth(value list, int n) {
    if(list == NIL) return NIL;
    if(n == 0) return first(list);
    return nth(second(list), n - 1);
}

inline bool to_bool(value v) {
    check_type(v, value_type::bool_t);
    return v != FALSE;
}

inline int64_t to_int(value v) {
    check_type(v, value_type::int_t);
    return v >> 4;
}

inline float to_float(value v) {
    check_type(v, value_type::float_t);
    uint64_t x = v >> 4;
    return *((float*)&x);
}

struct function {
    std::vector<value> arguments;
    value              body;
    bool               varadic;
    function(value arg_list, value body, value sym_ellipsis);
};

struct frame {
    std::unordered_map<value, value> data;

    frame() = default;

    frame(std::unordered_map<value, value>& data) : data(std::move(data)) {}

    value get(value name);
    void  set(value name, value val);
};

using extern_func_t = value (*)(class runtime *, value, void *);

struct heap_info {
    size_t new_size, old_size;
};

using owned_extern_deconstructor_t = void (*)(void *);
struct owned_extern_header {
    uint64_t size;
    owned_extern_deconstructor_t deconstructor;
};

class runtime {
    std::vector<std::string>               symbols;
    std::vector<std::shared_ptr<function>> functions;
    value parse_value(std::string_view src, size_t& i, bool quasimode = false);

    value sym_quote, sym_lambda, sym_if, sym_set, sym_define, sym_let, sym_letseq, sym_letrec,
        sym_quasiquote, sym_unquote, sym_unquote_splicing, sym_defmacro, sym_ellipsis,
        sym_unique_sym, sym_macro_error;

    void define_intrinsics();
    void define_std_functions();

    std::vector<value> reserved_syms;

    std::unordered_map<value, std::shared_ptr<function>> macros;
    std::vector<std::unordered_map<value, value>>        scopes;

    value look_up(value name);

    void  compute_closure(value v, const std::set<value>& bound, std::set<value>& free);
    value apply_quasiquote(value s);
    value eval_list(value x);

    uint8_t* heap;
    uint8_t* heap_next;
    size_t   heap_size;

    frame* alloc_frame();

    void     gc_process(value& c, struct gc_state& st);

    std::unordered_map<uint64_t, std::pair<value, uint64_t>> value_handles;
    uint64_t                                       next_extern_value_handle;

    std::unordered_set<size_t> owned_externs;

    std::shared_ptr<function> create_function(value arg_list, value body);

    void ser_value(std::ostream&, std::set<value>&, value);

  public:
    runtime(size_t heap_size = 1024 * 1024, bool load_std_lib = true);

    inline value from_bool(bool b) { return b ? 0x11 : 0x01; }

    inline value from_int(int64_t v) { return (uint64_t)(v << 4) | (uint64_t)value_type::int_t; }

    inline value from_float(float v) {
        auto* vp = (uint32_t*)&v;
        return (uint64_t(*vp) << 4) | (uint64_t)value_type::float_t;
    }

    value            from_str(std::string_view s);
    std::string_view to_str(value v);

    value                       from_fvec(uint32_t size, const float* v);
    std::pair<uint32_t, float*> to_fvec(value v);

    value              symbol(std::string_view s);
    const std::string& symbol_str(value sym) const;

    value              from_vec(const std::vector<value>& vec);
    std::vector<value> to_vec(value list);

    value cons(value fst = NIL, value snd = NIL);

    value         read(std::string_view src);
    value         read_all(std::string_view src);
    std::ostream& write(std::ostream&, value);

    value eval(value x);
    value apply(value f, value arguments);

    value expand(value v);

    void define_fn(std::string_view name, extern_func_t fn, void* data = nullptr);
    void define_global(std::string_view name, value val);

    /// running the GC will invalidate any pointers returned from this runtime
    /// if you need to maintain references over GC runs, use value_handles
    void collect_garbage(heap_info* res_info = nullptr);

    inline size_t current_heap_size() const { return heap_next - heap; }

    friend class value_handle;
    class value_handle handle_for(value v);

    template<typename T>
    value make_extern_reference(T* ob) {
        // TODO: make this just cast the ptr in release mode and not bother with type checking
        return (cons((value)ob, typeid(T).hash_code())
            & ~0xf)
            | (value)value_type::_extern;
    }

    template<typename T>
    T* get_extern_reference(value v) {
        assert(type_of(v) == value_type::_extern);
        v = (v & ~0xf) | (value)value_type::cons;
        if(typeid(T).hash_code() != second(v))
            throw std::runtime_error(
                std::string("mismatched type unwraping extern value, expected: ") + typeid(T).name()
            );
        return (T*)first(v);
    }

    template<typename T, typename ...Args>
    value make_owned_extern(Args... args) {
        if(heap_next - heap > heap_size) throw std::runtime_error("out of memory");
        auto* h = (owned_extern_header*)heap_next;
        auto* t = (T*)(heap_next + sizeof(owned_extern_header));
        h->size = sizeof(T) + sizeof(owned_extern_header);
        heap_next += h->size;
        h->deconstructor = [](void* x) {
            ((T*)x)->~T();
        };
        new(t) T(args...);
        owned_externs.insert((uint64_t)t);
        return make_extern_reference(t);
    }

    template<typename T>
    T take_owned_extern(value v) {
        T* p = get_extern_reference<T>(v);
        assert((size_t)p >= (size_t)heap && (size_t)p < (size_t)(heap+heap_size));
        owned_externs.erase((size_t)p);
        return std::move(*p);
    }
};

// must live as long as the runtime from which it was obtained
class value_handle {
  protected:
    runtime* rt;
    uint64_t h;

  public:
    value_handle(runtime* rt, uint64_t h) : rt(rt), h(h) {}

    value_handle(const value_handle& other);
    value_handle& operator=(const value_handle& other);
    value_handle(value_handle&& other) noexcept;
    value_handle& operator=(value_handle&& other) noexcept;
    const value&  operator*() const;
    value&        operator*();
                  operator value();
    ~value_handle();
};

// template<typename T>
// class typed_value_handle : public value_handle {
//     const T& operator*() const {
//     }
//
//     T& operator*() {
//     }
//
//     T* operator->() {
//     }
// };

extern const char* EMLISP_STD_SRC;
}  // namespace emlisp
