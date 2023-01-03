#include "emlisp.h"
#include <algorithm>
#include <iostream>
#include <sstream>

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
type_mismatch_error::type_mismatch_error(const type_mismatch_error& e, runtime* rt, value resp)
    : std::runtime_error(e.what()), expected(e.expected), actual(e.actual),
      trace(rt->cons(resp, e.trace)) {}

runtime::runtime(size_t heap_size, bool load_std_lib)
    : heap_size(heap_size), next_extern_value_handle(1) {
    sym_quote    = symbol("quote");
    sym_lambda   = symbol("lambda");
    sym_if       = symbol("if");
    sym_set      = symbol("set!");
    sym_define   = symbol("define");
    sym_defmacro = symbol("defmacro");

    sym_let    = symbol("let");
    sym_letseq = symbol("let*");
    sym_letrec = symbol("letrec");

    sym_unique_sym = symbol("unique-symbol");

    sym_quasiquote       = symbol("quasiquote");
    sym_unquote          = symbol("unquote");
    sym_unquote_splicing = symbol("unquote-splicing");

    sym_ellipsis    = symbol("...");
    sym_macro_error = symbol("macro-expand-error");

    reserved_syms
        = {sym_quote,
           sym_quasiquote,
           sym_lambda,
           sym_if,
           sym_set,
           sym_define,
           sym_ellipsis,
           sym_let,
           sym_letseq,
           sym_letrec,
           sym_unquote,
           sym_unquote_splicing,
           sym_defmacro};

    scopes.emplace_back();

    heap = new uint8_t[heap_size];
    assert(heap != nullptr);
    heap_next = heap;

    define_intrinsics();

    if(load_std_lib) {
        define_std_functions();
        value code = expand(read_all(EMLISP_STD_SRC));
        while(code != NIL) {
            eval(first(code));
            code = second(code);
        }
    }
}

function::function(value arg_list, value body, value sym_ellipsis) : body(body), varadic(false) {
    if(arg_list != NIL && first(arg_list) == sym_ellipsis) {
        varadic = true;
        arguments.push_back(first(second(arg_list)));
    } else {
        while(arg_list != NIL) {
            arguments.push_back(first(arg_list));
            arg_list = second(arg_list);
        }
    }
}

std::shared_ptr<function> runtime::create_function(value arg_list, value body) {
    std::shared_ptr<function> fn = nullptr;
    auto existing_fn = std::find_if(std::begin(functions), std::end(functions), [&](auto f) {
        return f->body == body;  // NOTE: this is a (eq?) check for reference equality on the body,
                                 // which should make sure that we deduplicate all function bodies
                                 // coming from the same syntactic location, but is janky!
    });
    if(existing_fn != std::end(functions)) {
        fn = *existing_fn;
    } else {
        fn = std::make_shared<function>(arg_list, body, sym_ellipsis);
        functions.emplace_back(fn);
    }
    return fn;
}

void runtime::compute_closure(value v, const std::set<value>& bound, std::set<value>& free) {
    auto ty = type_of(v);
    if(ty == value_type::sym) {
        if(bound.find(v) == bound.end()) free.insert(v);
    } else if(ty == value_type::cons) {
        if(first(v) == sym_lambda) {
            // TODO: deal with varadic functions
            value args      = first(second(v));
            auto  new_bound = bound;
            while(args != NIL) {
                new_bound.insert(first(args));
                args = second(args);
            }
            compute_closure(first(second(second(v))), new_bound, free);
        } else if(first(v) == sym_define) {
            // TODO: deal with varadic functions
            value args      = second(first(second(v)));
            auto  new_bound = bound;
            while(args != NIL) {
                new_bound.insert(first(args));
                args = second(args);
            }
            compute_closure(first(second(second(v))), new_bound, free);
        } else if(first(v) == sym_let || first(v) == sym_letseq || first(v) == sym_letrec) {
            value bindings  = first(second(v));
            auto  new_bound = bound;
            while(bindings != NIL) {
                new_bound.insert(first(first(bindings)));
                bindings = second(bindings);
            }
            compute_closure(first(second(second(v))), new_bound, free);
        } else if(first(v) == sym_quote) {
            // skip inside of quote
        } else if(first(v) == sym_quasiquote) {
            std::vector<value> stack{first(second(v))};
            while(stack.size() > 0) {
                value inner = stack.back();
                stack.pop_back();
                if(type_of(inner) == value_type::cons) {
                    while(inner != NIL) {
                        value item = first(inner);
                        if(type_of(item) == value_type::cons) {
                            if(first(item) == sym_unquote || first(item) == sym_unquote_splicing)
                                compute_closure(first(second(item)), bound, free);
                            else
                                stack.push_back(item);
                        }
                        inner = second(inner);
                    }
                }
            }
        } else {
            while(v != NIL) {
                compute_closure(first(v), bound, free);
                v = second(v);
            }
        }
    }
}

value runtime::look_up(value name) {
    int i;
    for(i = scopes.size() - 1; i >= 0; i--) {
        auto f = scopes[i].find(name);
        if(f != scopes[i].end()) return f->second;
    }
    throw std::runtime_error("unknown name " + symbol_str(name));
}

value runtime::apply_quasiquote(value s) {
    if(type_of(s) != value_type::cons) return s;
    if(first(s) == sym_unquote) return eval(first(second(s)));
    if(type_of(first(s)) == value_type::cons && first(first(s)) == sym_unquote_splicing) {
        value list = eval(first(second(first(s))));
        if(list == NIL) return apply_quasiquote(second(s));
        check_type(list, value_type::cons, "unquote-splicing expression must yield a list");
        value end = list;
        while(second(end) != NIL)
            end = second(end);
        second(end) = apply_quasiquote(second(s));
        return list;
    }
    return cons(apply_quasiquote(first(s)), apply_quasiquote(second(s)));
}

value runtime::eval_list(value x) {
    if(x == NIL) return NIL;
    return cons(eval(first(x)), eval_list(second(x)));
}

// TODO: f should probably be a function object, we should check for builtins somewhere else
value runtime::apply(value f, value arguments) {
    value result = NIL;
    if(f == sym_quote) {
        result = first(arguments);
    } else if(f == sym_unique_sym) {
        value name = first(arguments);
        check_type(name, value_type::sym, "unique-symbol expected symbol argument");
        value sym = (uint64_t)(symbols.size() << 4) | (uint64_t)value_type::sym;
        symbols.push_back(symbols[name >> 4]);
        return sym;
    } else if(f == sym_let) {
        value                  bindings = first(arguments);
        value                  body     = first(second(arguments));
        std::unordered_map<value, value> scope;
        value                  bc = bindings;
        while(bc != NIL) {
            value name = first(first(bc));
            value val  = first(second(first(bc)));
            check_type(name, value_type::sym, "let binding name must be symbol");
            scope[name] = eval(val);
            bc          = second(bc);
        }
        scopes.push_back(scope);
        result = eval(body);
        scopes.pop_back();
    } else if(f == sym_letseq) {
        value bindings = first(arguments);
        value body     = first(second(arguments));
        scopes.emplace_back();
        value bc = bindings;
        while(bc != NIL) {
            value name = first(first(bc));
            value val  = first(second(first(bc)));
            check_type(name, value_type::sym, "let* binding name must be symbol");
            scopes[scopes.size() - 1][name] = eval(val);
            bc                              = second(bc);
        }
        result = eval(body);
        scopes.pop_back();
    } else if(f == sym_letrec) {
        value                  bindings = first(arguments);
        value                  body     = first(second(arguments));
        std::unordered_map<value, value> scope;
        value                  bc = bindings;
        while(bc != NIL) {
            value name = first(first(bc));
            check_type(name, value_type::sym, "letrec binding name must be symbol");
            scope[name] = NIL;
            bc          = second(bc);
        }
        scopes.push_back(scope);
        bc = bindings;
        while(bc != NIL) {
            value name = first(first(bc));
            value val  = first(second(first(bc)));
            check_type(name, value_type::sym, "letrec binding name must be symbol");
            scope[name] = eval(val);
            bc          = second(bc);
        }
        // TODO: this doesn't really work as intended because closures copy values
        //       and so when we reset scopes here with the new values, any captured
        //       values won't get changed in closures and will simply remain NIL
        scopes[scopes.size() - 1] = scope;
        result                    = eval(body);
        scopes.pop_back();
    }

    else if(f == sym_lambda) {
        value args = first(arguments);
        value body = first(second(arguments));
        // create function
        auto fn = create_function(args, body);
        // TODO: deal with varadic functions
        frame*          clo = alloc_frame();
        std::set<value> bound(fn->arguments.begin(), fn->arguments.end());
        std::set<value> free;
        bound.insert(reserved_syms.begin(), reserved_syms.end());
        compute_closure(body, bound, free);
        for(value free_name : free)
            clo->set(free_name, look_up(free_name));
        value closure = cons(
            ((uint64_t)fn.get() << 4) | (uint64_t)value_type::_extern,
            (((uint64_t)clo) << 4) | (uint64_t)value_type::_extern
        );
        closure -= 1;  // cons -> closure
        result = closure;
    }

    else if(f == sym_if) {
        value cond = eval(first(arguments));
        if(cond != FALSE)
            result = eval(first(second(arguments)));
        else
            result = eval(first(second(second(arguments))));
    } else if(f == sym_set) {
        value name = first(arguments);
        value val  = eval(first(second(arguments)));
        int   i;
        for(i = scopes.size() - 1; i >= 0; i--) {
            auto f = scopes[i].find(name);
            if(f != scopes[i].end()) f->second = val;
        }
        scopes[scopes.size() - 1][name] = val;
        result                          = NIL;
    } else if(f == sym_define) {
        value head = first(arguments);
        if(type_of(head) == value_type::sym) {
            value val                       = first(second(arguments));
            scopes[scopes.size() - 1][head] = eval(val);
            result                          = NIL;
        } else if(type_of(head) == value_type::cons) {
            value name = first(head);
            value args = second(head);
            value body = first(second(arguments));
            // create function
            auto fn = create_function(args, body);
            // TODO: deal with varadic functions
            frame*          clo = alloc_frame();
            std::set<value> bound(fn->arguments.begin(), fn->arguments.end()), free;
            bound.insert(reserved_syms.begin(), reserved_syms.end());
            bound.insert(name);
            compute_closure(body, bound, free);
            for(value free_name : free)
                clo->set(free_name, look_up(free_name));
            value closure = cons(
                ((uint64_t)fn.get() << 4) | (uint64_t)value_type::_extern,
                (((uint64_t)clo) << 4) | (uint64_t)value_type::_extern
            );
            closure -= 1;             // cons -> closure
            clo->set(name, closure);  // enable recursion
            scopes[scopes.size() - 1][name] = closure;
            result                          = NIL;
        } else {
            throw std::runtime_error("invalid define");
        }
    } else if(f == sym_quasiquote) {
        result = apply_quasiquote(first(arguments));
    }

    else {
        auto fv = eval(f);
        // this->write(std::cout << "!!! ", fv) << "\n";
        if(type_of(fv) == value_type::_extern) {
            extern_func_t fn      = (extern_func_t)(*(uint64_t*)(fv >> 4) >> 4);
            void*         closure = (frame*)(*((uint64_t*)(fv >> 4) + 1) >> 4);
            value         a       = eval_list(arguments);
            result                = (*fn)(this, a, closure);
        } else {
            check_type(fv, value_type::closure, "expected function for function call");
            function* fn = (function*)(*(uint64_t*)(fv >> 4) >> 4);
            // std::cout << "calling funtion " << std::hex << fn << std::dec << "/" <<
            // fn->arguments.size() << "\n\tbody = "; this->write(std::cout, fn->body) << "\n";
            frame*                 closure = (frame*)(*((uint64_t*)(fv >> 4) + 1) >> 4);
            std::unordered_map<value, value> fr;
            value                  args = arguments;
            for(size_t i = 0; i < fn->arguments.size(); ++i) {
                if(args == NIL) throw std::runtime_error("argument count mismatch");
                auto arg = fn->arguments[i];
                // std::cout << "function " << std::hex << fn << std::dec << "| i = " << i << ",
                // fa[i] = ";
                //  this->write(std::cout, arg);
                // std::cout << ", a[i] = ";
                //  this->write(std::cout, first(args));
                // std::cout << " →\n";
                auto val = eval(first(args)
                );  // some how executing this line of code manages to wreck fn->arguments and also
                    // somehow takes out scoping info because at it is currently written, it
                    // complains that it can't find local variables
                // this->write(std::cout, val);
                // std::cout << "\n";
                fr.emplace(arg, val);
                // fr[fn->arguments[i]] = eval(first(args));
                args = second(args);
            }
            scopes.push_back(closure->data);
            scopes.push_back(fr);
            /*std::cout << "scopes:\n";
            for(const auto& sc : scopes) {
                std::cout << "\t{  ";
                for(const auto&[name, val] : sc) {
                    this->write(std::cout, name) << ": ";
                    this->write(std::cout, val) << "  ";
                }
                std::cout << "}\n";
            }*/

            result = eval(fn->body);
            scopes.pop_back();
            closure->data = scopes[scopes.size() - 1];
            scopes.pop_back();
        }
    }
    return result;
}

value runtime::eval(value x) {
    try {
        value result = NIL;
        switch(type_of(x)) {
            case value_type::nil:
            case value_type::bool_t:
            case value_type::int_t:
            case value_type::float_t:
            case value_type::str:
            case value_type::fvec: result = x; break;

            case value_type::sym: result = look_up(x); break;

            case value_type::cons: result = apply(first(x), second(x)); break;

            default:
                std::ostringstream oss;
                oss << "cannot evaluate value ";
                write(oss, x);
                throw std::runtime_error(oss.str());
        }
        return result;
    } catch(const type_mismatch_error& e) { throw type_mismatch_error(e, this, x); }
}

void runtime::define_fn(std::string_view name, extern_func_t fn, void* data) {
    define_global(
        name,
        cons(
            ((uint64_t)fn << 4) | (uint64_t)value_type::_extern,
            ((uint64_t)data << 4) | (uint64_t)value_type::_extern
        ) - 2
    );
}

void runtime::define_global(std::string_view name, value val) { scopes[0][symbol(name)] = val; }

value runtime::expand(value v) {
    if(type_of(v) != value_type::cons) return v;
    if(first(v) == sym_defmacro) {
        value head          = first(second(v));
        value body          = first(second(second(v)));
        macros[first(head)] = create_function(second(head), body);
        return NIL;
    }
    if(first(v) == sym_macro_error) {
        auto msg = std::string("macro expansion error: ");
        msg.append(to_str(first(second(v))));
        throw std::runtime_error(msg);
    }
    if(type_of(first(v)) == value_type::sym) {
        auto mc = macros.find(first(v));
        if(mc != macros.end()) {
            auto                   fn = mc->second;
            std::unordered_map<value, value> arguments;
            if(fn->varadic) {
                arguments[fn->arguments[0]] = second(v);
            } else {
                value  a = second(v);
                size_t i = 0;
                while(a != NIL) {
                    arguments[fn->arguments[i++]] = first(a);
                    a                             = second(a);
                }
            }
            scopes.push_back(arguments);
            auto res = eval(fn->body);
            scopes.pop_back();
            return expand(res);
        }
    }
    first(v)  = expand(first(v));
    second(v) = expand(second(v));
    return v;
}
}  // namespace emlisp
