#include "emlisp.h"
#include <algorithm>
#include <iostream>

// TODO:
//  + fix floats
//  + vec2..4
//  + garbage collection
//  + external value handles
//  + eval tests
//  / let expressions
//  + macros
//  + standard library

namespace emlisp {
    runtime::runtime(size_t heap_size, bool load_std_lib)
        : heap_size(heap_size), next_extern_value_handle(1)
    {
        sym_quote  = symbol("quote");
        sym_lambda = symbol("lambda");
        sym_if = symbol("if");
        sym_set = symbol("set!");
        sym_define = symbol("define");
        sym_defmacro = symbol("defmacro");
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

        sym_let = symbol("let");
        sym_letseq = symbol("let*");
        sym_letrec = symbol("letrec");

        sym_unique_sym = symbol("unique-symbol");

        sym_quasiquote  = symbol("quasiquote");
        sym_unquote  = symbol("unquote");
        sym_unquote_splicing  = symbol("unquote-splicing");

        sym_ellipsis = symbol("...");

        reserved_syms = { sym_quote, sym_quasiquote, sym_lambda, sym_if, sym_set,
				sym_cons, sym_car, sym_cdr, sym_define,
				sym_eq, sym_nilp, sym_boolp, sym_intp, sym_ellipsis,
				sym_floatp, sym_strp, sym_symp, sym_consp, sym_procp,
				sym_let, sym_letseq, sym_letrec, sym_unquote, sym_unquote_splicing, sym_defmacro };

        scopes.emplace_back();

        heap = new uint8_t[heap_size];
        assert(heap != nullptr);
        heap_next = heap;

        if(load_std_lib) {
            value code = expand(read_all(EMLISP_STD_SRC));
            while(code != NIL) {
                eval(first(code));
                code = second(code);
            }
        }
    }

    function::function(value arg_list, value body, value sym_ellipsis)
        : body(body), varadic(false)
    {
        if (arg_list != NIL && first(arg_list) == sym_ellipsis) {
            varadic = true;
            arguments.push_back(first(second(arg_list)));
        } else {
            while (arg_list != NIL) {
                arguments.push_back(first(arg_list));
                arg_list = second(arg_list);
            }
        }
    }

    void runtime::compute_closure(value v, const std::set<value>& bound, std::set<value>& free) {
        auto ty = type_of(v);
        if (ty == value_type::sym) {
            if (bound.find(v) == bound.end())
                free.insert(v);
        } else if (ty == value_type::cons) {
            if (first(v) == sym_lambda) {
                // TODO: deal with varadic functions
                value args = first(second(v));
                auto new_bound = bound;
                while (args != NIL) {
                    new_bound.insert(first(args));
                    args = second(args);
                }
                compute_closure(first(second(second(v))), new_bound, free);
            } else if (first(v) == sym_define) {
                // TODO: deal with varadic functions
                value args = second(first(second(v)));
                auto new_bound = bound;
                while (args != NIL) {
                    new_bound.insert(first(args));
                    args = second(args);
                }
                compute_closure(first(second(second(v))), new_bound, free);
            }  else if (first(v) == sym_let || first(v) == sym_letseq || first(v) == sym_letrec) {
				value bindings = first(second(v));
                auto new_bound = bound;
                while (bindings != NIL) {
                    new_bound.insert(first(first(bindings)));
                    bindings = second(bindings);
                }
                compute_closure(first(second(second(v))), new_bound, free);
            } else if (first(v) == sym_quote) {
                // skip inside of quote
            } else if (first(v) == sym_quasiquote) {
                std::vector<value> stack{ first(second(v)) };
                while (stack.size() > 0) {
                    value inner = stack.back(); stack.pop_back();
                    if (type_of(inner) == value_type::cons) {
                        while (inner != NIL) {
                            value item = first(inner);
                            if (type_of(item) == value_type::cons) {
                                if (first(item) == sym_unquote || first(item) == sym_unquote_splicing) {
                                    compute_closure(first(second(item)), bound, free);
                                }
                                else {
                                    stack.push_back(item);
                                }
                            }
                            inner = second(inner);
                        }
                    }
                }
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

    value runtime::apply_quasiquote(value s) {
        if (type_of(s) != value_type::cons) {
            return s;
        } else if (first(s) == sym_unquote) {
            return eval(first(second(s)));
        } else {
            if (type_of(first(s)) == value_type::cons
                    && first(first(s)) == sym_unquote_splicing)
            {
                value list = eval(first(second(first(s))));
                if (list == NIL) return apply_quasiquote(second(s));
                check_type(list, value_type::cons);
                value end = list;
                while (second(end) != NIL) end = second(end);
                second(end) = apply_quasiquote(second(s));
                return list;
            }
            return cons(apply_quasiquote(first(s)), apply_quasiquote(second(s)));
        }
    }

    value runtime::eval(value x) {
        value result = NIL;
        switch (type_of(x)) {
        case value_type::nil:
        case value_type::bool_t:
        case value_type::int_t:
        case value_type::float_t:
        case value_type::str:
        case value_type::fvec:
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
            else if (f == sym_unique_sym) {
				value name = first(second(x));
				check_type(name, value_type::sym);
				value sym = (uint64_t)(symbols.size() << 4) | (uint64_t)value_type::sym;
				symbols.push_back(symbols[name >> 4]);
				return sym;
			}
            else if (f == sym_let) {
                value bindings = first(second(x));
                value body = first(second(second(x)));
                std::map<value, value> scope;
                value bc = bindings;
                while (bc != NIL) {
                    value name = first(first(bc));
                    value val = first(second(first(bc)));
                    check_type(name, value_type::sym);
                    scope[name] = eval(val);
                    bc = second(bc);
                }
                scopes.push_back(scope);
                result = eval(body);
                scopes.pop_back();
            } else if (f == sym_letseq) {
                value bindings = first(second(x));
                value body = first(second(second(x)));
                scopes.push_back({});
                value bc = bindings;
                while (bc != NIL) {
                    value name = first(first(bc));
                    value val = first(second(first(bc)));
                    check_type(name, value_type::sym);
                    scopes[scopes.size()-1][name] = eval(val);
                    bc = second(bc);
                }
                result = eval(body);
                scopes.pop_back();
            } else if (f == sym_letrec) {
                value bindings = first(second(x));
                value body = first(second(second(x)));
                std::map<value, value> scope;
                value bc = bindings;
                while (bc != NIL) {
                    value name = first(first(bc));
                    check_type(name, value_type::sym);
                    scope[name] = NIL;
                    bc = second(bc);
                }
                scopes.push_back(scope);
                bc = bindings;
                while (bc != NIL) {
                    value name = first(first(bc));
                    value val = first(second(first(bc)));
                    check_type(name, value_type::sym);
                    scope[name] = eval(val);
                    bc = second(bc);
                }
                //TODO: this doesn't really work as intended because closures copy values
                //      and so when we reset scopes here with the new values, any captured
                //      values won't get changed in closures and will simply remain NIL
                scopes[scopes.size() - 1] = scope;
                result = eval(body);
                scopes.pop_back();
            } else if (f == sym_lambda) {
                value args = first(second(x));
                value body = first(second(second(x)));
                // create function
                uint64_t fn = functions.size();
                functions.emplace_back(args, body, sym_ellipsis);
                // TODO: deal with varadic functions
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
                if (cond != FALSE) {
                    result = eval(first(second(second(x))));
                } else {
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
					functions.emplace_back(args, body, sym_ellipsis);
                    // TODO: deal with varadic functions
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
            } else if (f == sym_quasiquote) {
                result = apply_quasiquote(first(second(x)));
            } else {
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

    value runtime::expand(value v) {
        if (type_of(v) != value_type::cons) return v;
        if (first(v) == sym_defmacro) {
            value head = first(second(v));
            value body = first(second(second(v)));
			uint64_t fn = functions.size();
			functions.emplace_back(second(head), body, sym_ellipsis);
            macros[first(head)] = fn;
            return NIL;
        }
        
        if (type_of(first(v)) == value_type::sym) {
            auto mc = macros.find(first(v));
            if (mc != macros.end()) {
                auto fn = &functions[mc->second];
                std::map<value, value> arguments;
                if (fn->varadic) {
                    arguments[fn->arguments[0]] = second(v);
                } else {
                    value a = second(v);
                    size_t i = 0;
                    while (a != NIL) {
                        arguments[fn->arguments[i++]] = first(a);
                        a = second(a);
                    }
                }
                scopes.push_back(arguments);
                auto res = eval(fn->body);
                scopes.pop_back();
                return expand(res);
            }
        }
		first(v) = expand(first(v));
		second(v) = expand(second(v));
		return v;
	}
}
