#include "emlisp.h"

namespace emlisp {
    memory::memory(size_t num_cons, size_t num_str_bytes, size_t num_frame_bytes) {
        cons = new value[num_cons*2];
        assert(cons != nullptr);
        next_cons = cons;

        strings = new char[num_str_bytes];
        assert(strings != nullptr);
        next_str = strings;

        frames = new uint8_t[num_frame_bytes];
        assert(frames != nullptr);
        next_frame = frames;
    }

    value memory::alloc_cons(value fst, value snd) {
        value* addr = next_cons;
        addr[0] = fst;
        addr[1] = snd;
        next_cons += 2;
        // todo: deal with oom
        return (((uint64_t)addr) << 4) | (uint64_t)value_type::cons;
    }

    value memory::make_str(std::string_view src) {
        //todo: deal with oom
        char* str = next_str;
        next_str += src.size() + 1;
        std::copy(src.begin(), src.end(), str);
        str[src.size()] = 0;
        return (((uint64_t)str) << 4) | (uint64_t)value_type::str;
    }

    frame* memory::alloc_frame() {
        frame* f = (frame*)next_frame;
        //todo: deal with oom
        next_frame += sizeof(frame);
        new(f) frame();
        return f;
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

    value runtime::cons(value fst, value snd) {
        return h->alloc_cons(fst, snd);
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

    value runtime::from_str(std::string_view s) {
        return h->make_str(s);
    }


}
