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

value runtime::from_fvec(uint32_t size, const float* src_v) {
    if(heap_next - heap > heap_size) throw std::runtime_error("out of memory");
    auto* v = (uint32_t*)heap_next;
    heap_next += size * sizeof(float) + sizeof(uint32_t);
    memcpy(v + 1, src_v, sizeof(float) * size);
    *v = size;
    return (((uint64_t)v) << 4) | (uint64_t)value_type::fvec;
}

std::pair<uint32_t, float*> runtime::to_fvec(value v) {
    check_type(v, value_type::fvec, "get fvec from value");
    auto length = *(uint32_t*)(v >> 4);
    auto data   = (float*)((v >> 4) + sizeof(uint32_t));
    return {length, data};
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
    std::unordered_map<value, value>& live_vals;
    uint8_t*& new_next;
    std::unordered_set<size_t> old_owned_externs, new_owned_externs;
    uint8_t* gc_copy_limit;
};

#define GC_LOG

void runtime::gc_process(value& c, gc_state& st) {
    auto ty = type_of(c);
    // only proceed if the value is on the heap
    if(!(ty == value_type::cons || ty == value_type::closure || ty == value_type::_extern
         || ty == value_type::str || ty == value_type::fvec))
        return;

    auto old_c = c;
    // check to see if we've already processed this value
    auto existing_copy = st.live_vals.find(c);
    if(existing_copy != st.live_vals.end()) {
        c = existing_copy->second;
        return;
    }

#ifdef GC_LOG
    std::cout << "collecting ";
    this->write(std::cout, c);
    std::cout << " @ " << std::hex << c << std::dec << "\n";
#endif

    // copy the value itself
    if(ty == value_type::cons || ty == value_type::closure || ty == value_type::_extern) {
        auto new_addr = ((uint64_t)(st.new_next) << 4) | (uint64_t)ty;
        st.live_vals[c]  = new_addr;
        memcpy((void*)st.new_next, (void*)(c >> 4), sizeof(value) * 2);
        c = new_addr;
        st.new_next += 2 * sizeof(value);
        if(st.new_next > st.gc_copy_limit) {
#ifdef GC_LOG
            std::cout << "!!! copying cons/closure/extern value " << ty << " " << std::hex << c
                      << std::dec << "\n\t";
            this->write(std::cout, c);
            std::cout << "\n";
#endif
            throw std::runtime_error("garbage collector has allocated more than the previous heap");
        }
    } else if(ty == value_type::str) {
        uint32_t* p   = (uint32_t*)(c >> 4);
        uint32_t  len = *p;
        memcpy(st.new_next, p, len + sizeof(uint32_t));
        c                = (((uint64_t)st.new_next) << 4) | (uint64_t)value_type::str;
        st.live_vals[old_c] = c;
        st.new_next += len + sizeof(uint32_t);
        if(st.new_next > st.gc_copy_limit) {
#ifdef GC_LOG
            std::cout << "!!! copying string\n";
#endif
            throw std::runtime_error("garbage collector has allocated more than the previous heap");
        }
    } else if(ty == value_type::fvec) {
        uint32_t* p   = (uint32_t*)(c >> 4);
        uint32_t  len = *p;
        memcpy(st.new_next, p, len * sizeof(float) + sizeof(uint32_t));
        c                = (((uint64_t)st.new_next) << 4) | (uint64_t)value_type::fvec;
        st.live_vals[old_c] = c;
        st.new_next += len * sizeof(float) + sizeof(uint32_t);
        if(st.new_next > st.gc_copy_limit) {
#ifdef GC_LOG
            std::cout << "!!! copying fvec\n";
#endif
            throw std::runtime_error("garbage collector has allocated more than the previous heap");
        }
    }

    // recursively process any internal references
    if(ty == value_type::cons) {
        gc_process(first(c), st);
        gc_process(second(c), st);
    } else if(ty == value_type::closure) {
        function* fn = (function*)(*(uint64_t*)(c >> 4) >> 4);
        gc_process(fn->body, st);
        auto old_frv = *((value*)(c >> 4) + 1);

        auto exi_fr = st.live_vals.find(old_frv);
        if(exi_fr != st.live_vals.end()) {
#ifdef GC_LOG
            std::cout << "\tframe already collected\n";
#endif
            *((value*)(c >> 4) + 1) = exi_fr->second;
            return;
        }

        auto*  fr     = (frame*)(old_frv >> 4);
        auto* new_fr = (frame*)st.new_next;
        st.new_next += sizeof(frame);
        if(st.new_next > st.gc_copy_limit) {
#ifdef GC_LOG
            std::cout << "!!! copying frame\n";
#endif
            throw std::runtime_error("garbage collector has allocated more than the previous heap");
        }
        new(new_fr) frame(fr->data);
        // memcpy(new_fr, fr, sizeof(frame));
        auto new_frv = (value)((((uint64_t)new_fr) << 4) | (uint64_t)value_type::_extern);
        *((value*)(c >> 4) + 1) = new_frv;
        st.live_vals[old_frv]      = new_frv;

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
            gc_process(val, st);
        }
    } else if(ty == value_type::_extern) {
        void* p = *(void**)(c >> 4);
        if(p >= heap && p < heap+heap_size) {
            // we own this value
            auto* h = (owned_extern_header*)((char*)p - sizeof(owned_extern_header));
#ifdef GC_LOG
            std::cout << "\tmoving C++ type, size = " << h->size << "\n";
#endif
            st.old_owned_externs.erase((size_t)p);
            st.new_owned_externs.insert((size_t)st.new_next);
            *(void**)(c >> 4) = st.new_next;
            memcpy(st.new_next, h, h->size);
            st.new_next += h->size;

        }
    }
}

void runtime::collect_garbage(heap_info* res_info) {
    std::unordered_map<value, value> live_vals;

    auto* new_heap = new uint8_t[heap_size];
    assert(new_heap != nullptr);
    auto* new_heap_next = new_heap;

    gc_state st{
        .live_vals = live_vals,
        .new_next = new_heap_next,
        .old_owned_externs = owned_externs,
        .new_owned_externs = {owned_externs.bucket_count()},
        .gc_copy_limit = new_heap + (heap_next - heap)
    };

    for(auto& sc : scopes)
        for(auto& [name, val] : sc)
            gc_process(val, st);

    for(auto& p : value_handles)
        gc_process(p.second.first, st);

    if(res_info != nullptr) {
        res_info->old_size = heap_next - heap;
        res_info->new_size = new_heap_next - new_heap;
    }

#ifdef _DEBUG
    // make it abundantly clear if we still have pointers to the old heap
    memset(heap, 0xcdcdcdcd, heap_size);
#endif

    // run deconstructors for any collected C++ values
    for(auto x : st.old_owned_externs) {
        auto* h = (owned_extern_header*)(x - sizeof(owned_extern_header));
#ifdef GC_LOG
        std::cout << "deconstructing value at " << std::hex << x << std::dec << " size = " << h->size << "\n";
#endif
        h->deconstructor((void*)x);
    }

    delete heap;
    heap      = new_heap;
    heap_next = new_heap_next;
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
