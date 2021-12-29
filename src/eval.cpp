#include "emlisp.h"
#include <algorithm>
#include <iostream>

// TODO:
//  + fix floats
//  - vec2..4
//  + garbage collection
//  + external value handles
//  + eval tests
//  ? macros
//  + standard library

namespace emlisp {
    runtime::runtime(size_t heap_size)
        : heap_size(heap_size), next_extern_value_handle(1)
    {
        sym_quote  = symbol("quote");
        sym_lambda = symbol("lambda");
        sym_if = symbol("if");
        sym_set = symbol("set!");
        sym_define = symbol("define");
        sym_cons = symbol("cons");
        sym_car = symbol("car");
        sym_cdr = symbol("cdr");
        sym_eq = symbol("eq?");

        sym_nilp = symbol("nil?");
        sym_boolp = symbol("bool?");
        sym_intp = symbol("int?");
        sym_floatp = symbol("float?");
        sym_strp = symbol("str?");
        sym_symp = symbol("sym?");
        sym_consp = symbol("cons?");
        sym_procp = symbol("proc?");

        reserved_syms = { sym_quote, sym_lambda, sym_if, sym_set, sym_cons, sym_car, sym_cdr, sym_define,
                    sym_eq, sym_nilp, sym_boolp, sym_intp, sym_floatp, sym_strp, sym_symp, sym_consp, sym_procp };

        scopes.push_back({});

        heap = new uint8_t[heap_size];
        assert(heap != nullptr);
        heap_next = heap;
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
            } else if (first(v) == sym_quote) {
                // skip inside of quote
            } else {
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
            }
            else if (f == sym_cons) {
                result = cons(
                    eval(first(second(x))),
                    eval(first(second(second(x))))
                );
            }
            else if (f == sym_car) {
                result = first(eval(first(second(x))));
            }
            else if (f == sym_cdr) {
                result = second(eval(first(second(x))));
            }
            else if (f == sym_eq) {
                value a = eval(first(second(x)));
                value b = eval(first(second(second(x))));
                result = from_bool(a == b);
            }
            else if (f == sym_nilp) result = from_bool(type_of(eval(first(second(x)))) == value_type::nil);
            else if (f == sym_boolp) result = from_bool(type_of(eval(first(second(x)))) == value_type::bool_t);
            else if (f == sym_intp) result = from_bool(type_of(eval(first(second(x)))) == value_type::int_t);
            else if (f == sym_floatp) result = from_bool(type_of(eval(first(second(x)))) == value_type::float_t);
            else if (f == sym_strp) result = from_bool(type_of(eval(first(second(x)))) == value_type::str);
            else if (f == sym_symp) result = from_bool(type_of(eval(first(second(x)))) == value_type::sym);
            else if (f == sym_consp) result = from_bool(type_of(eval(first(second(x)))) == value_type::cons);
            else if (f == sym_procp) result = from_bool(type_of(eval(first(second(x)))) == value_type::closure);
            else if (f == sym_lambda) {
                value args = first(second(x));
                value body = first(second(second(x)));
                // create function
                uint64_t fn = functions.size();
                functions.emplace_back(args, body);
                frame* clo = alloc_frame();
                std::set<value> bound(functions[fn].arguments.begin(), functions[fn].arguments.end()),
                    free;
                bound.insert(reserved_syms.begin(), reserved_syms.end());
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
                value val = eval(first(second(second(x))));
				int i;
				for (i = scopes.size() - 1; i >= 0; i--) {
					auto f = scopes[i].find(name);
					if (f != scopes[i].end()) {
						f->second = val;
					}
				}
                scopes[scopes.size()-1][name] = val;
                result = NIL;
            }
            else if (f == sym_define) {
                value head = first(second(x));
                if (type_of(head) == value_type::sym) {
					value val = first(second(second(x)));
					scopes[scopes.size() - 1][head] = eval(val);
					result = NIL;
                }
                else if (type_of(head) == value_type::cons) {
                    value name = first(head);
					value args = second(head);
					value body = first(second(second(x)));
					// create function
					uint64_t fn = functions.size();
					functions.emplace_back(args, body);
					frame* clo = alloc_frame();
					std::set<value> bound(functions[fn].arguments.begin(), functions[fn].arguments.end()),
						free;
					bound.insert(reserved_syms.begin(), reserved_syms.end());
                    bound.insert(name);
					compute_closure(body, bound, free);
					for (value free_name : free) {
						clo->set(free_name, look_up(free_name));
					}
					value closure = cons(
						(fn << 4) | (uint64_t)value_type::_extern,
						(((uint64_t)clo) << 4) | (uint64_t)value_type::_extern);
					closure -= 1; // cons -> closure
                    clo->set(name, closure); //enable recursion
					scopes[scopes.size() - 1][name] = closure;
                    result = NIL;
                }
                else {
                    throw std::runtime_error("invalid define");
                }
            } 
            else {
                auto fv = eval(f);
                if (type_of(fv) == value_type::_extern) {
					extern_func_t fn = (extern_func_t)(*(uint64_t*)(fv >> 4) >> 4);
					void* closure = (frame*)(*((uint64_t*)(fv >> 4) + 1) >> 4);
                    value a = second(x);
                    while (a != NIL) {
                        first(a) = eval(first(a));
                        a = second(a);
                    }
                    result = (*fn)(this, second(x), closure);
                }
                else {
                    check_type(fv, value_type::closure, "expected function for function call");
                    function* fn = &functions[*(uint64_t*)(fv >> 4) >> 4];
                    frame* closure = (frame*)(*((uint64_t*)(fv >> 4) + 1) >> 4);
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
        scopes[0][symbol(name)] = cons(
            ((uint64_t)fn << 4) | (uint64_t)value_type::_extern,
            ((uint64_t)data << 4) | (uint64_t)value_type::_extern
        ) - 2;
    }
}
