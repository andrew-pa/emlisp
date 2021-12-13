#include "emlisp.h"
#include <algorithm>
#include <iostream>

namespace emlisp {
    heap::heap(size_t num_cons, size_t num_str_bytes) {
        cons = new value[num_cons*2];
        assert(cons != nullptr);
        next_cons = cons;

        strings = new char[num_str_bytes];
        assert(strings != nullptr);
        next_str = strings;
    }

    value heap::alloc_cons(value fst, value snd) {
        value* addr = next_cons;
        addr[0] = fst;
        addr[1] = snd;
        next_cons += 2;
        // todo: deal with oom
        return (((uint64_t)addr) << 4) | (uint64_t)value_type::cons;
    }

    value heap::make_str(std::string_view src) {
        //todo: deal with oom
        char* str = next_str;
        next_str += src.size() + 1;
        std::copy(src.begin(), src.end(), str);
        str[src.size()] = 0;
        return (((uint64_t)str) << 4) | (uint64_t)value_type::str;
    }

    runtime::runtime()
        : h(std::make_unique<heap>())
    {
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
                return reverse_list(res);
            } else if(src[i] == '\'') {
                i++;
                return cons(symbol("quote"), cons(parse_value(src, i)));
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
        }
    }

    value eval(value x) {
        //todo
        return NIL;
    }

}
