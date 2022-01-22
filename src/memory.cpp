#include "emlisp.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace emlisp {
    value runtime::cons(value fst, value snd) {
        if (heap_next - heap > heap_size) {
            throw std::runtime_error("out of memory");
        }
        auto* addr = (value*)heap_next;
        addr[0] = fst;
        addr[1] = snd;
        heap_next += 2*sizeof(value);
        return (((uint64_t)addr) << 4) | (uint64_t)value_type::cons;
    }

    frame* runtime::alloc_frame() {
        if (heap_next - heap > heap_size) {
            throw std::runtime_error("out of memory");
        }
        auto* f = (frame*)heap_next;
        heap_next += sizeof(frame);
        new(f) frame();
        return f;
    }

    value runtime::from_str(std::string_view src) {
        if (heap_next - heap > heap_size) {
            throw std::runtime_error("out of memory");
        }
        char* str = (char*)heap_next;
        heap_next += src.size() + sizeof(uint32_t);
        std::copy(src.begin(), src.end(), str + sizeof(uint32_t));
        *((uint32_t*)str) = src.size();
        return (((uint64_t)str) << 4) | (uint64_t)value_type::str;
    }

    value runtime::from_fvec(uint32_t size, float* src_v) {
		if (heap_next - heap > heap_size) {
			throw std::runtime_error("out of memory");
		}
		auto* v = (uint32_t*)heap_next;
		heap_next += size*sizeof(float) + sizeof(uint32_t);
        memcpy(v + 1, src_v, sizeof(float) * size);
        *v = size;
		return (((uint64_t)v) << 4) | (uint64_t)value_type::fvec;
    
    }

    value frame::get(value name) {
        check_type(name, value_type::sym);
        auto v = data.find(name);
        if (v != data.end())
            return v->second;
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

    void runtime::gc_process(value& c, std::map<value, value>& live_vals, uint8_t*& new_next) {
        auto ty = type_of(c);
        // only proceed if the value is on the heap
        if (!(ty == value_type::cons
                    || ty == value_type::closure
                    || ty == value_type::_extern
                    || ty == value_type::str
                    || ty == value_type::fvec)) return;

        auto old_c = c;
        // check to see if we've already processed this value
        auto existing_copy = live_vals.find(c);
        if (existing_copy != live_vals.end()) {
            c = existing_copy->second;
            return;
        }

        // copy the value itself
        if (ty == value_type::cons || ty == value_type::closure || ty == value_type::_extern) {
            auto new_addr = ((uint64_t)(new_next) << 4) | (uint64_t)ty;
            live_vals[c] = new_addr;
            memcpy((void*)new_next, (void*)(c >> 4), sizeof(value) * 2);
            c = new_addr;
            new_next += 2 * sizeof(value);
        } else if (ty == value_type::str) {
            uint32_t* p = (uint32_t*)(c >> 4);
            uint32_t len = *p;
            memcpy(new_next, p, len + sizeof(uint32_t));
            c = (((uint64_t)new_next) << 4) | (uint64_t)value_type::str;
            live_vals[old_c] = c;
            new_next += len + sizeof(uint32_t);
        } else if (ty == value_type::fvec) {
            uint32_t* p = (uint32_t*)(c >> 4);
            uint32_t len = *p;
            memcpy(new_next, p, len*sizeof(float) + sizeof(uint32_t));
            c = (((uint64_t)new_next) << 4) | (uint64_t)value_type::fvec;
            live_vals[old_c] = c;
            new_next += len*sizeof(float) + sizeof(uint32_t);
        }

        // recursively process any internal references
        if (ty == value_type::cons) {
            gc_process(first(c), live_vals, new_next);
            gc_process(second(c), live_vals, new_next);
        }
        else if (ty == value_type::closure) {
            function* fn = &functions[*(uint64_t*)(c >> 4) >> 4];
            gc_process(fn->body, live_vals, new_next);
            auto old_frv = *((value*)(c >> 4) + 1);

            auto exi_fr = live_vals.find(old_frv);
            if (exi_fr != live_vals.end()) {
                *((value*)(c >> 4) + 1) = exi_fr->second;
                return;
            }

            auto fr = (frame*)(old_frv >> 4);
            auto* new_fr = (frame*)new_next;
            new_next += sizeof(frame);
            new (new_fr) frame(fr->data);
            //memcpy(new_fr, fr, sizeof(frame));
            auto new_frv =
                (value)((((uint64_t)new_fr) << 4) | (uint64_t)value_type::_extern);
            *((value*)(c >> 4) + 1) = new_frv;
            live_vals[old_frv] = new_frv;

            for (auto& [name, val] : new_fr->data) {
                if (val == old_c) {
                    val = c;
                    continue;
                }
                gc_process(val, live_vals, new_next);
            }
        }
    }

    void runtime::collect_garbage(heap_info* res_info) {
        std::map<value, value> live_vals;

        auto* new_heap = new uint8_t[heap_size];
        assert(new_heap != nullptr);
        auto* new_heap_next = new_heap;

        for (auto& sc : scopes) {
            for (auto& [name, val] : sc) {
                gc_process(val, live_vals, new_heap_next);
            }
        }

        for (auto& p : extern_values) {
            gc_process(p.second.first, live_vals, new_heap_next);
        }

        for(auto& fn : functions) {
            gc_process(fn.body, live_vals, new_heap_next);
        }

        if (res_info != nullptr) {
            res_info->old_size = heap_next - heap;
            res_info->new_size = new_heap_next - new_heap;
        }

#ifdef _DEBUG
        // make it abundantly clear if we still have pointers to the old heap
        memset(heap, 0xcdcdcdcd, heap_size);
#endif

        delete heap;
        heap = new_heap;
        heap_next = new_heap_next;
    }

    value_handle runtime::handle_for(value v) {
        auto h = next_extern_value_handle++;
        extern_values[h] = { v, 1 };
        return { this, h };
    }

    value_handle::value_handle(const value_handle& other)
        : rt(other.rt), h(other.h)
    {
        rt->extern_values[h].second++;
    }
    value_handle& value_handle::operator =(const value_handle& other) {
        rt->extern_values[h].second--;
        rt = other.rt;
        h = other.h;
        rt->extern_values[h].second++;
        return *this;
    }
    value_handle::value_handle(value_handle&& other)
        : rt(other.rt), h(other.h)
    {
        other.rt = nullptr;
        other.h = 0;
    }

    value_handle& value_handle::operator =(value_handle&& other) {
        rt->extern_values[h].second--;
        rt = other.rt;
        h = other.h;
        other.rt = nullptr;
        other.h = 0;
        return *this;
    }

    const value& value_handle::operator*() const {
        return rt->extern_values[h].first;
    }
    value& value_handle::operator*() {
        return rt->extern_values[h].first;
    }
    value_handle::operator value() {
        return rt->extern_values[h].first;
    }

    value_handle::~value_handle() {
        if (h == 0 || rt == nullptr) return;
        if (rt->extern_values[h].second-- <= 0) {
            rt->extern_values.erase(h);
        }
    }
}
