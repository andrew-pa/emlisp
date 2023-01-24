#include "parser.h"

void parser::check_next_symbol(symbol_type s, const std::string& msg) {
    token tk = toks.next();
    if(!tk.is_symbol(s)) throw parse_error(tk, toks.line_number, msg);
}

std::vector<std::shared_ptr<cpptype>> parser::parse_template_param_list() {
    std::vector<std::shared_ptr<cpptype>> args;
    token                                 tk = toks.peek();
    if(tk.is_symbol(symbol_type::close_angle))
        toks.next();  // consume close angle, we will skip the loop

    while(!tk.is_symbol(symbol_type::close_angle)) {
        args.push_back(parse_type());
        tk = toks.next();
        if(tk.is_symbol(symbol_type::close_angle)) break;
        if(!tk.is_symbol(symbol_type::comma)) {
            throw parse_error(
                tk, toks.line_number, "expected comma to seperate types in template instance args"
            );
        }
    }

    return args;
}

std::shared_ptr<cpptype> parser::parse_type() {
    auto tk = toks.next();
    // TODO: eof
    if(tk.is_keyword(keyword::const_)) return std::make_shared<const_type>(parse_type());

    std::shared_ptr<cpptype> ty;

    if(tk.is_keyword(keyword::typename_)) {
        std::vector<id> names;
        while(true) {
            tk = toks.next();
            if(!tk.is_id())
                throw parse_error(tk, toks.line_number, "expected identifier in dependent name");
            std::cout << toks.identifiers[tk.data] << "\n";
            names.emplace_back(tk.data);
            tk = toks.peek();
            if(tk.is_symbol(symbol_type::double_colon))
                toks.next();
            else
                break;
        }
        ty = std::make_shared<dependent_name_type>(names);
    } else if(tk.is_id()) {
        auto name = tk.data;
        tk        = toks.peek();
        if(tk.is_symbol(symbol_type::double_colon)) {
            std::ostringstream oss;
            oss << toks.identifiers[name];
            while(tk.is_symbol(symbol_type::double_colon)) {
                toks.next();
                tk = toks.next();
                if(!tk.is_id())
                    throw parse_error(
                        tk, toks.line_number, "expected identifier in qualified name"
                    );
                oss << "::" << toks.identifiers[tk.data];
                tk = toks.peek();
            }
            name = toks.identifiers.size();
            toks.identifiers.emplace_back(oss.str());
        }
        ty = std::make_shared<plain_type>(name);

        tk = toks.peek();
        if(tk.is_symbol(symbol_type::open_angle)) {
            toks.next();
            auto args = parse_template_param_list();
            ty        = std::make_shared<template_instance>(name, args);
        }
    }

    if(ty != nullptr) {
        do {
            tk = toks.peek();
            if(tk.is_symbol(symbol_type::star))
                ty = std::make_shared<ptr_type>(ty);
            else if(tk.is_symbol(symbol_type::ampersand))
                ty = std::make_shared<ref_type>(ty);
            else
                break;
            tk = toks.next();
        } while(!tk.is_eof());

        tk = toks.peek();
        if(tk.is_symbol(symbol_type::open_paren)) {
            toks.next();
            tk = toks.peek();
            std::vector<std::shared_ptr<cpptype>> args;
            while(!tk.is_symbol(symbol_type::close_paren)) {
                args.push_back(parse_type());
                tk = toks.next();
                if(tk.is_symbol(symbol_type::close_paren)) break;
                if(!tk.is_symbol(symbol_type::comma)) {
                    throw parse_error(
                        tk, toks.line_number, "expected comma to seperate types of function args"
                    );
                }
            }
            return std::make_shared<fn_type>(ty, args);
        } else {
            return ty;
        }
    }

    throw parse_error(tk, toks.line_number, "parse type, expected id or dependent name");
}

property parser::parse_property() {
    token tk = toks.next();
    if(!tk.is_symbol(symbol_type::open_paren))
        throw parse_error(tk, toks.line_number, "expected (");
    tk = toks.next();
    if(!(tk.type == token::keyword
         && (tk.data == (size_t)keyword::read || tk.data == (size_t)keyword::readwrite)))
        throw parse_error(tk, toks.line_number, "expected read or readwrite mode for property");
    bool readonly = tk.data == (size_t)keyword::read;
    check_next_symbol(symbol_type::close_paren, "expected )");
    auto ty = parse_type();
    tk      = toks.next();
    if(!tk.is_id()) throw parse_error(tk, toks.line_number, "expected name of property");
    auto prop_name = tk.data;
    check_next_symbol(symbol_type::semicolon, "expected ;");
    return property{ty, prop_name, readonly};
}

template_known_instances parser::parse_known_instance_map() {
    token                    tk = toks.peek();
    template_known_instances known_insts;
    while(!tk.is_symbol(symbol_type::close_paren)) {
        check_next_symbol(
            symbol_type::open_angle, "expected open angle to open known instance type parameters"
        );
        known_insts.emplace_back(parse_template_param_list());
        tk = toks.peek();
    }
    return known_insts;
}

std::tuple<std::vector<id>, template_known_instances> parser::parse_template_def() {
    check_next_symbol(symbol_type::open_angle, "expected < to open template parameter list");
    std::vector<id> names;
    token           tk = toks.peek();
    if(tk.is_symbol(symbol_type::close_angle))
        toks.next();  // consume close angle, we will skip the loop

    while(!tk.is_symbol(symbol_type::close_angle)) {
        if(!tk.is_keyword(keyword::typename_))
            throw parse_error(
                tk, toks.line_number, "expected typename to procede template parameter name"
            );
        toks.next();
        token name = toks.next();
        if(!name.is_id())
            throw parse_error(
                name, toks.line_number, "expected identifier for template parameter name"
            );
        names.push_back(name.data);
        tk = toks.next();
        if(tk.is_symbol(symbol_type::close_angle)) break;
        if(!tk.is_symbol(symbol_type::comma)) {
            throw parse_error(
                tk, toks.line_number, "expected comma to seperate types in template instance args"
            );
        }
    }

    tk = toks.next();
    if(!tk.is_keyword(keyword::el_known_insts))
        throw parse_error(
            tk, toks.line_number, "must provide known instances to bind template function"
        );
    check_next_symbol(symbol_type::open_paren, "expected open paren for known instance list");
    auto known_insts = parse_known_instance_map();
    if(known_insts.empty())
        throw template_error(
            toks.line_number, "must provide known instances to bind template function"
        );
    check_next_symbol(symbol_type::close_paren, "expected close paren for known instance list");

    return {names, known_insts};
}

method parser::parse_method() {
    bool with_cx = false;
    if(toks.peek().is_keyword(keyword::el_with_cx)) {
        toks.next();
        with_cx = true;
    }

    std::optional<std::tuple<std::vector<id>, template_known_instances>> template_info;
    if(toks.peek().is_keyword(keyword::template_)) {
        toks.next();
        template_info = parse_template_def();
    }

    auto ret_ty = parse_type();

    token tk = toks.next();
    if(!tk.is_id()) throw parse_error(tk, toks.line_number, "expected name of method");

    auto m = method{tk.data, ret_ty, with_cx, template_info};

    check_next_symbol(symbol_type::open_paren, "expected (");
    tk = toks.peek();
    while(!tk.is_symbol(symbol_type::close_paren)) {
        auto ty = parse_type();

        tk = toks.next();
        if(!tk.is_id()) throw parse_error(tk, toks.line_number, "expected name for argument");
        auto name = tk.data;

        tk = toks.peek();
        if(tk.is_symbol(symbol_type::eq)) {
            // skip default value expression
            toks.next();
            while(!tk.is_eof()) {
                tk = toks.peek();
                if(tk.is_symbol(symbol_type::comma) || tk.is_symbol(symbol_type::close_paren))
                    break;
                toks.next();
            }
        }

        m.args.emplace_back(ty, name);

        tk = toks.next();
        if(tk.is_symbol(symbol_type::close_paren)) break;
        if(!tk.is_symbol(symbol_type::comma))
            throw parse_error(tk, toks.line_number, "expected comma to seperate args");
    }

    return m;
}

object parser::parse_object() {
    token tk = toks.next();
    if(!tk.is_keyword(keyword::class_) && !tk.is_keyword(keyword::struct_))
        throw parse_error(tk, toks.line_number, "can only apply EL_OBJ to classes and structs");
    tk = toks.next();
    if(!tk.is_id()) throw parse_error(tk, toks.line_number, "expected name of class/struct");
    auto name_id = tk.data;
    // skip base classes
    while(!tk.is_eof() && !tk.is_symbol(symbol_type::open_brace))
        tk = toks.next();
    if(tk.is_eof()) throw parse_error(tk, toks.line_number, "unexpected eof");

    object obj{name_id};

    size_t bracket_count = 0;
    while(!tk.is_eof()) {
        if(tk.is_keyword(keyword::el_m))
            obj.methods.push_back(parse_method());
        else if(tk.is_keyword(keyword::el_prop))
            obj.properties.push_back(parse_property());
        else if(tk.is_symbol(symbol_type::open_brace))
            bracket_count++;
        else if(tk.is_symbol(symbol_type::close_brace))
            bracket_count--;
        tk = toks.next();
        if(bracket_count == 0) break;
    }

    return obj;
}

std::pair<id, std::shared_ptr<cpptype>> parser::parse_typedef() {
    token tk = toks.next();
    if(!tk.is_keyword(keyword::using_))
        throw parse_error(tk, toks.line_number, "only support using= style typedefs");

    tk = toks.next();
    if(!tk.is_id()) throw parse_error(tk, toks.line_number, "expected name for typedef");
    id name = tk.data;

    check_next_symbol(symbol_type::eq, "expected equals in using statement");

    auto ty = parse_type();

    check_next_symbol(symbol_type::semicolon, "expected semicolon to end using statement");

    return {name, ty};
}

void parser::parse(world& ast) {
    token tk = toks.next();
    while(!tk.is_eof()) {
        if(tk.is_keyword(keyword::el_obj))
            ast.objects.emplace_back(parse_object());
        else if(tk.is_keyword(keyword::el_typedef))
            ast.typedefs.emplace(parse_typedef());
        tk = toks.next();
    }
}
