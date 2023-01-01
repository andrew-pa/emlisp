#include "emlisp.h"
#include <cstring>
#include <iostream>

namespace emlisp {
value reverse_list(value head) {
    if(head == NIL || second(head) == NIL) return head;
    value rest           = reverse_list(second(head));
    second(second(head)) = head;
    second(head)         = NIL;
    return rest;
}

value runtime::parse_value(std::string_view src, size_t& i, bool quasimode) {
    for(; i < src.size(); ++i) {
        if(std::isspace(src[i]) != 0) continue;

        if(src[i] == '(' || src[i] == '[') {
            char end = src[i] == '[' ? ']' : ')';
            i++;
            value res = NIL;
            while(i < src.size() && src[i] != end) {
                value e = parse_value(src, i, quasimode);
                res     = cons(e, res);
            }
            i++;
            return reverse_list(res);
        } else if(src[i] == '\'') {
            i++;
            return cons(sym_quote, cons(parse_value(src, i)));
        } else if(src[i] == '`') {
            i++;
            return cons(sym_quasiquote, cons(parse_value(src, i, true)));
        } else if(src[i] == ',' && quasimode) {
            i++;
            bool splicing = false;
            if(src[i] == '@') {
                splicing = true;
                i++;
            }
            return cons(splicing ? sym_unquote_splicing : sym_unquote, cons(parse_value(src, i)));
        } else if(src[i] == '"') {
            i++;
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
            i++;
            return this->from_str(s);
        } else if(src[i] == '-' || std::isdigit(src[i]) != 0) {
            size_t start    = i;
            bool   is_float = false;
            if(src[i] == '-') i++;
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
            if(src[i] == 't') {
                i++;
                return TRUE;
            }
            if(src[i] == 'f') {
                i++;
                return FALSE;
            }
            if(src[i] == 'n') {
                i++;
                return NIL;
            }
            if(src[i] == 'v') {
                i++;
                if(src[i++] != '(') throw std::runtime_error("unknown #v");
                std::vector<float> vals;
                while(i < src.size() && src[i] != ')') {
                    if(std::isspace(src[i]) != 0) {
                        i++;
                        continue;
                    }
                    size_t start = i;
                    if(src[i] == '-') i++;
                    while(i < src.size() && (std::isdigit(src[i]) != 0 || src[i] == '.'))
                        i++;
                    auto v = (float)std::atof(src.data() + start);
                    vals.push_back(v);
                }
                i++;
                return this->from_fvec(vals.size(), vals.data());
            }
            throw std::runtime_error("unknown #");
        } else if(src[i] == ';') {
            while(i < src.size() && src[i] != '\n')
                i++;
        } else {
            size_t start = i;
            while(i < src.size() && (std::isspace(src[i]) == 0) && src[i] != '(' && src[i] != ')')
                i++;
            return this->symbol(src.substr(start, i - start));
        }
    }
    return NIL;
}

value runtime::read(std::string_view src) {
    size_t i = 0;
    return parse_value(src, i);
}

value runtime::read_all(std::string_view src) {
    size_t i    = 0;
    value  vals = NIL;
    while(i < src.size())
        vals = cons(parse_value(src, i), vals);
    return reverse_list(vals);
}

std::ostream& runtime::write(std::ostream& os, value v) {
    switch(type_of(v)) {
        case value_type::nil: os << "nil"; break;
        case value_type::bool_t: os << (v == TRUE ? "#t" : "#f"); break;
        case value_type::int_t: os << std::to_string((long long)v >> 4); break;
        case value_type::float_t: {
            auto  vv = v >> 4;
            auto* vf = (float*)&vv;
            os << *vf;
        } break;
        case value_type::sym: os << this->symbols[v >> 4]; break;
        case value_type::str: {
            os << '"' << to_str(v) << '"';
        } break;
        case value_type::fvec: {
            auto len  = *((uint32_t*)(v >> 4));
            auto data = (float*)((v >> 4) + sizeof(uint32_t));
            os << "#v(";
            for(auto i = 0; i < len; ++i) {
                os << data[i];
                if(i < len - 1) os << " ";
            }
            os << ")";
        } break;
        case value_type::cons: {
            os << "(";
            write(os, first(v));
            value cur = second(v);
            while(type_of(cur) == value_type::cons) {
                os << " ";
                write(os, first(cur));
                cur = second(cur);
            }
            if(cur != NIL) {
                os << " . ";
                write(os, second(v));
            }
            os << ")";
        } break;
        case value_type::closure:
            os << "#closure"
               << "<" << std::hex << v << std::dec << ">";
            break;
        case value_type::_extern: os << "<" << std::hex << v << std::dec << ">"; break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, value_type vt) {
    const char* names[]
        = {"nil",
           "bool",
           "int",
           "float",
           "symbol",
           "string",
           "?",
           "?",
           "?",
           "?",
           "?",
           "?",
           "?",
           "?",
           "closure",
           "cons"};
    return os << (names[(size_t)vt]);
}
}  // namespace emlisp
