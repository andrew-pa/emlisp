#include "emlisp.h"
#include <algorithm>
#include <iostream>

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

    frame* memory::alloc_frame(frame* parent, size_t size) {
        frame* f = (frame*)next_frame;
        //todo: deal with oom
        next_frame += sizeof(frame) + size * 2 * sizeof(value);
        f->parent = parent;
        f->size = size;
        return f;
    }

    runtime::runtime()
        : h(std::make_unique<memory>())
    {
        global_scope = h->alloc_frame(nullptr, 0);
        sym_quote  = symbol("quote");
        sym_lambda = symbol("lambda");
        sym_if = symbol("if");
        sym_set = symbol("set!");
        sym_cons = symbol("cons");
        sym_car = symbol("car");
        sym_cdr = symbol("cdr");
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

    value runtime::from_str(std::string_view s) {
        return h->make_str(s);
    }

    value reverse_list(value head) {
        if(head == NIL || second(head) == NIL) return head;
        value rest = reverse_list(second(head));
        second(second(head)) = head;
        second(head) = NIL;
        return rest;
    }

    value runtime::parse_value(std::string_view src, size_t& i) {
        for(; i < src.size(); ++i) {
            if(std::isspace(src[i]) != 0) continue;

            if(src[i] == '(') {
                i++;
                value res = NIL;
                while(i < src.size() && src[i] != ')') {
                    value e = parse_value(src, i);
                    res = cons(e, res);
                }
                i++;
                return reverse_list(res);
            } else if(src[i] == '\'') {
                i++;
                return cons(sym_quote, cons(parse_value(src, i)));
            } else if(src[i] == '"') {
                ++i;
                std::string s;
                while(i < src.size() && src[i] != '"') {
                    if(src[i] == '\\' && i < src.size() - 1) {
                        i++;
                        switch(src[i]) {
                            case '\\': s.push_back('\\'); break;
                            case 'n': s.push_back('\n'); break;
                            case 't': s.push_back('\t'); break;
                            case '"': s.push_back('"'); break;
                        }
                    } else {
                        s.push_back(src[i]);
                    }
                    i++;
                }
                return this->from_str(s);
            } else if(src[i] == '-' || std::isdigit(src[i]) != 0) {
                size_t start = i;
                bool is_float = false;
                while(i < src.size() && (std::isdigit(src[i]) != 0 || src[i] == '.')) {
                    if(src[i] == '.') is_float = true;
                    i++;
                }
                if(is_float) {
                    auto v = (float)std::atof(src.data() + start);
                    return ((*(uint64_t*)&v) << 4) | (uint64_t)value_type::float_t;
                }
                auto v = std::atoll(src.data() + start);
                return (v << 4) | (uint64_t)value_type::int_t;
            } else if(src[i] == '#') {
                i++;
                if(src[i] == 't') return TRUE;
                if(src[i] == 'f') return FALSE;
                if(src[i] == 'n') return NIL;
            } else if(src[i] == ';') {
                while(i < src.size() && src[i] != '\n') i++;
            } else {
                size_t start = i;
                while(i < src.size() && (std::isspace(src[i]) == 0) && src[i] != '(' && src[i] != ')') i++;
                return this->symbol(src.substr(start, i-start));
            }
        }
        return NIL;
    }

    value runtime::read(std::string_view src) {
        size_t i = 0;
        return parse_value(src, i);
    }

    void runtime::write(std::ostream& os, value v) {
        switch(type_of(v)) {
            case value_type::nil:
                os << "nil";
                break;
            case value_type::bool_t:
                os << (v == TRUE ? "#t" : "#f");
                break;
            case value_type::int_t:
                os << std::to_string((long long)v >> 4);
                break;
            case value_type::float_t: {
                auto vv = v >> 4;
                auto* vf = (float*)&vv;
                os << *vf;
            } break;
            case value_type::sym:
                os << this->symbols[v >> 4];
                break;
            case value_type::str:
                os << '"' << (char*)(v >> 4) << '"';
                break;
            case value_type::cons:
                os << "(";
                write(os, first(v));
                os << " . ";
                write(os, second(v));
                os << ")";
                break;
            case value_type::closure:
                os << "#closure" << "<" << std::hex << v << std::dec << ">";
                break;
            case value_type::_extern:
                os << "<" << std::hex << v << std::dec << ">";
                break;
        }
    }

    std::ostream& operator <<(std::ostream& os, value_type vt) {
        const char* names[] = {
            "nil", "bool", "int", "float", "symbol", "string",
            "?", "?", "?", "?", "?", "?", "?", "?",
            "closure", "cons"
        };
        return os << (names[(size_t)vt]);
    }

    function::function(value arg_list, value body)
        : body(body)
    {
        while (arg_list != NIL) {
            arguments.push_back(first(arg_list));
            arg_list = second(arg_list);
        }
    }

    value runtime::eval(value x) {
        return eval(x, global_scope);
    }

    value runtime::eval(value x, frame* cur_frame) {
        switch (type_of(x)) {
        case value_type::nil:
        case value_type::bool_t:
        case value_type::int_t:
        case value_type::float_t:
        case value_type::str:
            return x;

        case value_type::sym:
            return cur_frame->get(x);

        case value_type::cons: {
            value f = first(x);
            if (f == sym_quote) {
                return first(second(x));
            } else if (f == sym_cons) {
                return cons(
                    eval(first(second(x)), cur_frame),
                    eval(first(second(second(x))), cur_frame)
                );
            } else if (f == sym_car) {
                return first(eval(first(second(x))));
            } else if (f == sym_cdr) {
                return second(eval(first(second(x))));
            } else if (f == sym_lambda) {
                value args = first(second(x));
                value body = first(second(second(x)));
                // create function
                uint64_t fn = functions.size();
                functions.emplace_back(args, body);
                value closure = cons(
                    (fn << 4) | (uint64_t)value_type::_extern,
                    (((uint64_t)cur_frame) << 4) | (uint64_t)value_type::_extern);
                closure -= 1; // cons -> closure
                return closure;
            } else if (f == sym_if) {
                value cond = eval(first(second(x)), cur_frame);
                if (cond == TRUE) {
                    return eval(first(second(second(x))), cur_frame);
                } else if (cond == FALSE) {
                    return eval(first(second(second(second(x)))), cur_frame);
                }
            } else if (f == sym_set) {
                value name = first(second(x));
                value val = first(second(second(x)));
                cur_frame->set(name, eval(val, cur_frame));
                return NIL;
            } else {
                f = eval(f, cur_frame);
                check_type(f, value_type::closure, "expected function for function call");
                function* fn = &functions[*(uint64_t*)(f >> 4) >> 4];
                frame* closure = (frame*)(*((uint64_t*)(f >> 4) + 1) >> 4);
                frame* fr = h->alloc_frame(closure, fn->arguments.size());
                value args = second(x);
                for (size_t i = 0; i < fr->size; ++i) {
                    if(args == NIL) throw std::runtime_error("argument count mismatch");
                    fr->set_at(i, fn->arguments[i], eval(first(args), cur_frame));
                    args = second(args);
                }
                return eval(fn->body, fr);
            }
        } break;

        }
    }
}
