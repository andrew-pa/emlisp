#include "emlisp.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace emlisp {
value runtime::cons(value fst, value snd) {
    if(heap_next - heap > heap_size) throw std::runtime_error("out of memory");
    auto* addr = (value*)heap_next;
    addr[0]    = fst;
    addr[1]    = snd;
    heap_next += 2 * sizeof(value);
    return (((uint64_t)addr) << 4) | (uint64_t)value_type::cons;
}

frame* runtime::alloc_frame() {
    if(heap_next - heap > heap_size) throw std::runtime_error("out of memory");
    auto* f = (frame*)heap_next;
    heap_next += sizeof(frame);
    new(f) frame();
    return f;
}

value runtime::from_str(std::string_view src) {
    if(heap_next - heap > heap_size) throw std::runtime_error("out of memory");
    char* str = (char*)heap_next;
    heap_next += src.size() + sizeof(uint32_t);
    std::copy(src.begin(), src.end(), str + sizeof(uint32_t));
    *((uint32_t*)str) = src.size();
    return (((uint64_t)str) << 4) | (uint64_t)value_type::str;
}

std::string_view runtime::to_str(value v) {
    check_type(v, value_type::str, "get string from value");
    auto length = *(uint32_t*)(v >> 4);
    auto data   = (char*)((v >> 4) + sizeof(uint32_t));
    return {data, length};
}

value runtime::from_vec(const std::vector<value>& vec) {
    value n = NIL;
    for(auto i = vec.rbegin(); i != vec.rend(); ++i)
        n = cons(*i, n);
    return n;
}

std::vector<value> runtime::to_vec(value list) {
    std::vector<value> vals;
    value              cur = list;
    while(cur != NIL) {
        vals.push_back(first(cur));
        cur = second(cur);
    }
    std::reverse(vals.begin(), vals.end());
    return vals;
}

value frame::get(value name) {
    check_type(name, value_type::sym);
    auto v = data.find(name);
    if(v != data.end()) return v->second;
    throw std::runtime_error("unknown name " + std::to_string(name));
}

void frame::set(value name, value val) {
    check_type(name, value_type::sym);
    data[name] = val;
}

value runtime::symbol(std::string_view s) {
    auto ix = std::find(std::begin(symbols), std::end(symbols), s);
    if(ix == std::end(symbols)) {
        auto i = symbols.size();
        symbols.emplace_back(s);
        return (i << 4) | (uint64_t)value_type::sym;
    }
    return (std::distance(std::begin(symbols), ix) << 4) | (uint64_t)value_type::sym;
}

const std::string& runtime::symbol_str(value sym) const {
    check_type(sym, value_type::sym);
    return symbols[sym >> 4];
}

struct gc_state {
    runtime*                          rt;
    std::unordered_map<value, value>& live_vals;
    uint8_t*&                         new_next;
    std::unordered_set<size_t>        old_owned_externs, new_owned_externs;
    uint8_t*                          gc_copy_limit;

  private:
    inline void copy_conslike(value& c, value_type ty) {
        auto new_addr = ((uint64_t)(new_next) << 4) | (uint64_t)ty;
        live_vals[c]  = new_addr;
        memcpy((void*)new_next, (void*)(c >> 4), sizeof(value) * 2);
        c = new_addr;
        new_next += 2 * sizeof(value);
        if(new_next > gc_copy_limit) {
#ifdef GC_LOG
            std::cout << "!!! copying cons/closure/extern value " << ty << " " << std::hex << c
                      << std::dec << "\n\t";
            this->write(std::cout, c);
            std::cout << "\n";
#endif
            throw std::runtime_error("garbage collector has allocated more than the previous heap");
        }
    }

    inline void copy_str(value& c, value old_c) {
        uint32_t* p   = (uint32_t*)(c >> 4);
        uint32_t  len = *p;
        memcpy(new_next, p, len + sizeof(uint32_t));
        c = (((uint64_t)new_next) << 4) | (uint64_t)value_type::str;
        new_next += len + sizeof(uint32_t);
        live_vals[old_c] = c;
        if(new_next > gc_copy_limit) {
#ifdef GC_LOG
            std::cout << "!!! copying string\n";
#endif
            throw std::runtime_error("garbage collector has allocated more than the previous heap");
        }
    }

    inline void process_closure_internals(value c, value old_c) {
        function* fn = (function*)(*(uint64_t*)(c >> 4) >> 4);
        process(fn->body);
        auto old_frv = *((value*)(c >> 4) + 1);

        auto exi_fr = live_vals.find(old_frv);
        if(exi_fr != live_vals.end()) {
#ifdef GC_LOG
            std::cout << "\tframe already collected\n";
#endif
            *((value*)(c >> 4) + 1) = exi_fr->second;
            return;
        }

        auto* fr     = (frame*)(old_frv >> 4);
        auto* new_fr = (frame*)new_next;
        new_next += sizeof(frame);
        if(new_next > gc_copy_limit) {
#ifdef GC_LOG
            std::cout << "!!! copying frame\n";
#endif
            throw std::runtime_error("garbage collector has allocated more than the previous heap");
        }
        new(new_fr) frame(fr->data);
        // memcpy(new_fr, fr, sizeof(frame));
        auto new_frv = (value)((((uint64_t)new_fr) << 4) | (uint64_t)value_type::_extern);
        *((value*)(c >> 4) + 1) = new_frv;
        live_vals[old_frv]      = new_frv;

        for(auto& [name, val] : new_fr->data) {
            if(val == old_c) {  // this is sus?
                val = c;
                continue;
            }
#ifdef GC_LOG
            std::cout << "\tcollecting frame value " << symbols[name >> 4] << " → ";
            this->write(std::cout, val);
            std::cout << "\n";
#endif
            process(val);
        }
    }

    inline void process_owned_extern(value c) {
        void* p = *(void**)(c >> 4);
        if(p >= rt->heap && p < rt->heap + rt->heap_size) {
            // we own this value
            auto* h = (owned_extern_header*)((char*)p - sizeof(owned_extern_header));
#ifdef GC_LOG
            std::cout << "\tmoving C++ type, size = " << h->size << "\n";
#endif
            auto* t = new_next + sizeof(owned_extern_header);
            old_owned_externs.erase((size_t)p);
            new_owned_externs.insert((size_t)t);
            *(void**)(c >> 4)                 = t;
            *((owned_extern_header*)new_next) = *h;
            h->move(t, p);
            new_next += h->size;
        }
    }

  public:
    void process(value& c) {
        auto ty = type_of(c);
        // only proceed if the value is on the heap
        if(!(ty == value_type::cons || ty == value_type::closure || ty == value_type::_extern
             || ty == value_type::str))
            return;

        auto old_c = c;

        // check to see if we've already processed this value
        auto existing_copy = live_vals.find(c);
        if(existing_copy != live_vals.end()) {
            c = existing_copy->second;
            return;
        }

#ifdef GC_LOG
        std::cout << "collecting ";
        this->write(std::cout, c);
        std::cout << " @ " << std::hex << c << std::dec << "\n";
#endif

        // copy the value itself to the new heap, replacing c so it points to the new heap
        if(ty == value_type::cons || ty == value_type::closure || ty == value_type::_extern)
            copy_conslike(c, ty);
        else if(ty == value_type::str)
            copy_str(c, old_c);

        // recursively process any internal references for compound structures
        if(ty == value_type::cons) {
            process(first(c));
            process(second(c));
        } else if(ty == value_type::closure) {
            process_closure_internals(c, old_c);
        } else if(ty == value_type::_extern) {
            process_owned_extern(c);
        }
    }
};

void runtime::collect_garbage(heap_info* res_info) {
    std::unordered_map<value, value> live_vals;

    auto* new_heap = new uint8_t[heap_size];
    assert(new_heap != nullptr);
    auto* new_heap_next = new_heap;

    gc_state st{
        .rt                = this,
        .live_vals         = live_vals,
        .new_next          = new_heap_next,
        .old_owned_externs = owned_externs,
        .new_owned_externs = {},
        .gc_copy_limit     = new_heap + (heap_next - heap)};

    for(auto& sc : scopes)
        for(auto& [name, val] : sc)
            st.process(val);

    for(auto& p : value_handles)
        st.process(p.second.first);

    if(res_info != nullptr) {
        res_info->old_size = heap_next - heap;
        res_info->new_size = new_heap_next - new_heap;
    }

    // run deconstructors for any collected C++ values
    for(auto x : st.old_owned_externs) {
        auto* h = (owned_extern_header*)(x - sizeof(owned_extern_header));
#ifdef GC_LOG
        std::cout << "deconstructing value at " << std::hex << x << std::dec
                  << " size = " << h->size << "\n";
#endif
        h->deconstructor((void*)x);
    }

#ifdef _DEBUG
    // make it abundantly clear if we still have pointers to the old heap
    memset(heap, 0xcdcdcdcd, heap_size);
#endif

    delete heap;
    heap          = new_heap;
    heap_next     = new_heap_next;
    owned_externs = st.new_owned_externs;
}

value_handle runtime::handle_for(value v) {
    auto h           = next_extern_value_handle++;
    value_handles[h] = {v, 1};
    return {this, h};
}

value_handle::value_handle(const value_handle& other) : rt(other.rt), h(other.h) {
    rt->value_handles[h].second++;
}

value_handle& value_handle::operator=(const value_handle& other) {
    rt->value_handles[h].second--;
    rt = other.rt;
    h  = other.h;
    rt->value_handles[h].second++;
    return *this;
}

value_handle::value_handle(value_handle&& other) noexcept : rt(other.rt), h(other.h) {
    other.rt = nullptr;
    other.h  = 0;
}

value_handle& value_handle::operator=(value_handle&& other) noexcept {
    rt->value_handles[h].second--;
    rt       = other.rt;
    h        = other.h;
    other.rt = nullptr;
    other.h  = 0;
    return *this;
}

const value& value_handle::operator*() const { return rt->value_handles[h].first; }

value& value_handle::operator*() { return rt->value_handles[h].first; }

value_handle::operator value() { return rt->value_handles[h].first; }

value_handle::~value_handle() {
    if(h == 0 || rt == nullptr) return;
    if(rt->value_handles[h].second-- <= 0) rt->value_handles.erase(h);
}
}  // namespace emlisp
