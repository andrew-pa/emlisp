#include "parser.h"

void parser::check_next_symbol(symbol_type s, const std::string& msg) {
    token tk = toks.next();
    if(!tk.is_symbol(s)) throw parse_error(tk, toks.line_number, msg);
}

std::shared_ptr<cpptype> parser::parse_type() {
    auto tk = toks.next();
    // TODO: eof
    if(tk.is_keyword(keyword::const_)) return std::make_shared<const_type>(parse_type());

    if(tk.is_id()) {
        auto                     name = tk.data;
        std::shared_ptr<cpptype> ty   = std::make_shared<plain_type>(name);

        tk = toks.peek();
        if(tk.is_symbol(symbol_type::open_angle)) {
            toks.next();
            std::vector<std::shared_ptr<cpptype>> args;
            tk = toks.peek();
            if(tk.is_symbol(symbol_type::close_angle))
                toks.next();  // consume close angle, we will skip the loop

            while(!tk.is_symbol(symbol_type::close_angle)) {
                args.push_back(parse_type());
                tk = toks.next();
                if(tk.is_symbol(symbol_type::close_angle)) break;
                if(!tk.is_symbol(symbol_type::comma)) {
                    throw parse_error(
                        tk,
                        toks.line_number,
                        "expected comma to seperate types in template instance args"
                    );
                }
            }
            ty = std::make_shared<template_instance>(name, args);
        }

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

    throw parse_error(tk, toks.line_number, "parse type, expected id");
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

method parser::parse_method() {
    bool with_cx = false;
    if(toks.peek().is_keyword(keyword::el_with_cx)) {
        toks.next();
        with_cx = true;
    }

    auto ret_ty = parse_type();

    token tk = toks.next();
    if(!tk.is_id()) throw parse_error(tk, toks.line_number, "expected name of method");

    auto m = method{tk.data, ret_ty, with_cx};

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

std::optional<object> parser::next_object() {
    token tk = toks.next();
    while(!tk.is_eof() && !tk.is_keyword(keyword::el_obj))
        tk = toks.next();
    if(tk.is_eof()) return std::nullopt;
    tk = toks.next();
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
