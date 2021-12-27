#include "emlisp.h"
#include <iostream>

namespace emlisp {
    value runtime::cons(value fst, value snd) {
        value* addr = next_cons;
        addr[0] = fst;
        addr[1] = snd;
        next_cons += 2;
        // todo: deal with oom
        return (((uint64_t)addr) << 4) | (uint64_t)value_type::cons;
    }

    frame* runtime::alloc_frame() {
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

    value runtime::from_str(std::string_view src) {
        //todo: deal with oom
        char* str = next_str;
        next_str += src.size() + 1;
        std::copy(src.begin(), src.end(), str);
        str[src.size()] = 0;
        return (((uint64_t)str) << 4) | (uint64_t)value_type::str;
    }

    void runtime::gc_process(value& c,
            std::map<value, value>& live_cons,
            std::map<frame*, frame*>& live_frames,
            value*& new_next_cons,
            uint8_t*& new_next_frame)
    {
        if (type_of(c) == value_type::cons || type_of(c) == value_type::closure || type_of(c) == value_type::_extern) {
            auto existing_copy = live_cons.find(c);
            if (existing_copy != live_cons.end()) {
                c = existing_copy->second;
            }
            else {
                auto new_addr = ((uint64_t)(new_next_cons) << 4) | (uint64_t)type_of(c);
                live_cons[c] = new_addr;
                memcpy((void*)new_next_cons, (void*)(c >> 4), sizeof(value) * 2);
                c = new_addr;
                new_next_cons += 2;
            }
            if (type_of(c) == value_type::cons) {
                gc_process(first(c), live_cons, live_frames, new_next_cons, new_next_frame);
                gc_process(second(c), live_cons, live_frames, new_next_cons, new_next_frame);
            }
            else if (type_of(c) == value_type::closure) {
				function* fn = &functions[*(uint64_t*)(c >> 4) >> 4];
                gc_process(fn->body, live_cons, live_frames, new_next_cons, new_next_frame);
				auto fr = (frame*)(*((value*)(c >> 4) + 1) >> 4);
                auto exi_fr = live_frames.find(fr);
                if (exi_fr != live_frames.end()) {
                    fr = exi_fr->second;
                }
                else {
                    auto new_fr = (frame*)new_next_frame;
                    memcpy(new_fr, fr, sizeof(frame));
                    *((value*)(c >> 4) + 1) = (value)(((uint64_t)(new_fr) << 4) | (uint64_t)value_type::_extern);
                    live_frames[fr] = new_fr;
                    fr = new_fr;
                }
                new_next_frame += sizeof(frame);
                for (auto& [name, value] : fr->data) {
					gc_process(value, live_cons, live_frames, new_next_cons, new_next_frame);
                }
            }
        }
    }

    void runtime::collect_garbage(heap_info* res_info) {
        std::map<value, value> live_cons;
        std::map<frame*, frame*> live_frames;

        auto new_cons = new value[num_cons*2];
        assert(new_cons != nullptr);
        auto new_next_cons = new_cons;

        auto new_frames = new uint8_t[num_frame_bytes];
        assert(new_frames != nullptr);
        auto new_next_frame = new_frames;

        for (auto& sc : scopes) {
            for (auto& [name, val] : sc) {
				gc_process(val, live_cons, live_frames, new_next_cons, new_next_frame);
            }
        }

        if (res_info != nullptr) {
            res_info->old_cons_size = (next_cons - acons) / 2;
            res_info->old_frames_size = (next_frame - frames) / sizeof(frame);
            res_info->new_cons_size = (new_next_cons - new_cons) / 2;
            res_info->new_frames_size = (new_next_frame - new_frames) / sizeof(frame);
        }

        delete acons;
        delete frames;
        acons = new_cons;
        next_cons = new_next_cons;
        frames = new_frames;
        next_frame = new_next_frame;
    }
}
