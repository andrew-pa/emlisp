#include "emlisp.h"
#include <algorithm>
#include <iostream>

// TODO:
//  - fix floats
//  - vec2..4
//  - garbage collection
//  - eval tests

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


    runtime::runtime()
        : h(std::make_unique<memory>())
    {
        sym_quote  = symbol("quote");
        sym_lambda = symbol("lambda");
        sym_if = symbol("if");
        sym_set = symbol("set!");
        sym_cons = symbol("cons");
        sym_car = symbol("car");
        sym_cdr = symbol("cdr");
        sym_eq = symbol("eq?");

        scopes.push_back({});
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
                i++;
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
                if (src[i] == 't') { i++; return TRUE; }
                if (src[i] == 'f') {
                    i++; return FALSE;
                }
                if (src[i] == 'n') {
                    i++;
					return NIL;
                }
                throw std::runtime_error("unknown #");
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

    void runtime::compute_closure(value v, const std::set<value>& bound, std::set<value>& free) {
        auto ty = type_of(v);
        if (ty == value_type::sym) {
            if (bound.find(v) == bound.end())
                free.insert(v);
        } else if (ty == value_type::cons) {
            if (first(v) == sym_lambda) {
                value args = first(second(v));
                auto new_bound = bound;
                while (args != NIL) {
                    new_bound.insert(first(args));
                    args = second(args);
                }
                compute_closure(first(second(second(v))), new_bound, free);
            }
            else {
                while (v != NIL) {
                    compute_closure(first(v), bound, free);
                    v = second(v);
                }
            }
        }
    }

    value runtime::look_up(value name) {
		int i;
		for (i = scopes.size() - 1; i >= 0; i--) {
			auto f = scopes[i].find(name);
			if (f != scopes[i].end()) {
                return f->second;
			}
		}
		throw std::runtime_error("unknown name " + symbol_str(name));
    }

    value runtime::eval(value x) {
        value result = NIL;
        switch (type_of(x)) {
        case value_type::nil:
        case value_type::bool_t:
        case value_type::int_t:
        case value_type::float_t:
        case value_type::str:
            result = x;
            break;

        case value_type::sym:
            result = look_up(x);
            break;

        case value_type::cons: {
            value f = first(x);
            if (f == sym_quote) {
                result = first(second(x));
            } else if (f == sym_cons) {
                result = cons(
                    eval(first(second(x))),
                    eval(first(second(second(x))))
                );
            } else if (f == sym_car) {
                result = first(eval(first(second(x))));
            } else if (f == sym_cdr) {
                result = second(eval(first(second(x))));
            } else if (f == sym_eq) {
                value a = eval(first(second(x)));
                value b = eval(first(second(second(x))));
                return (a == b) ? TRUE : FALSE;
            } else if (f == sym_lambda) {
                value args = first(second(x));
                value body = first(second(second(x)));
                // create function
                uint64_t fn = functions.size();
                functions.emplace_back(args, body);
                frame* clo = h->alloc_frame();
                std::set<value> bound(functions[fn].arguments.begin(), functions[fn].arguments.end()),
                    free;
                bound.insert({sym_quote, sym_lambda, sym_if, sym_set, sym_cons, sym_car, sym_cdr, sym_eq});
                compute_closure(body, bound, free);
                for (value free_name : free) {
                    clo->set(free_name, look_up(free_name));
                }
                value closure = cons(
                    (fn << 4) | (uint64_t)value_type::_extern,
                    (((uint64_t)clo) << 4) | (uint64_t)value_type::_extern);
                closure -= 1; // cons -> closure
                result = closure;
            } else if (f == sym_if) {
                value cond = eval(first(second(x)));
                if (cond == TRUE) {
                    result = eval(first(second(second(x))));
                } else if (cond == FALSE) {
                    result = eval(first(second(second(second(x)))));
                }
            } else if (f == sym_set) {
                value name = first(second(x));
                value val = first(second(second(x)));
                scopes[scopes.size()-1][name] = eval(val);
                result = NIL;
            } else {
                f = eval(f);
                if (type_of(f) == value_type::_extern) {
					extern_func_t fn = (extern_func_t)(*(uint64_t*)(f >> 4) >> 4);
					void* closure = (frame*)(*((uint64_t*)(f >> 4) + 1) >> 4);
                    value a = second(x);
                    while (a != NIL) {
                        first(a) = eval(first(a));
                        a = second(a);
                    }
                    result = (*fn)(this, second(x), closure);
                }
                else {
                    check_type(f, value_type::closure, "expected function for function call");
                    function* fn = &functions[*(uint64_t*)(f >> 4) >> 4];
                    frame* closure = (frame*)(*((uint64_t*)(f >> 4) + 1) >> 4);
                    std::map<value, value> fr;
                    value args = second(x);
                    for (size_t i = 0; i < fn->arguments.size(); ++i) {
                        if (args == NIL) throw std::runtime_error("argument count mismatch");
                        fr[fn->arguments[i]] = eval(first(args));
                        args = second(args);
                    }
                    scopes.push_back(closure->data);
                    scopes.push_back(fr);
                    result = eval(fn->body);
                    scopes.pop_back();
                    closure->data = scopes[scopes.size() - 1];
                    scopes.pop_back();
                }
            }
        } break;

        }
        return result;
    }

    void runtime::define_fn(std::string_view name, extern_func_t fn, void* data) {
        scopes[0][symbol(name)] = h->alloc_cons(
            ((uint64_t)fn << 4) | (uint64_t)value_type::_extern,
            ((uint64_t)data << 4) | (uint64_t)value_type::_extern
        ) - 2;
    }
}
