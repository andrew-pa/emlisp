#pragma once
#include "emlisp_autobind.h"
#include "token.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using id                       = size_t;
using template_known_instances = std::vector<std::vector<std::shared_ptr<struct cpptype>>>;
using named_types_map          = std::unordered_map<id, std::shared_ptr<struct cpptype>>;

struct cpptype {
    virtual std::ostream&            print(std::ostream& out, const tokenizer& toks) const = 0;
    virtual std::shared_ptr<cpptype> substitute(const named_types_map& params)             = 0;
    virtual ~cpptype() = default;
};

struct plain_type : public cpptype,
                    std::enable_shared_from_this<plain_type> {
    id name;

    plain_type(id name) : name(name) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        return out << toks.identifiers[name];
    }

    std::shared_ptr<cpptype> substitute(const named_types_map& params) override {
        auto r = params.find(name);
        if(r != params.end()) return r->second;
        return std::dynamic_pointer_cast<cpptype>(this->shared_from_this());
    }
};

struct dependent_name_type : public cpptype,
                             std::enable_shared_from_this<dependent_name_type> {
    std::vector<id> names;

    dependent_name_type(std::vector<id> names) : names(std::move(names)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        out << "typename ";
        for(size_t i = 0; i < names.size(); ++i) {
            out << toks.identifiers[names[i]];
            if(i < names.size() - 1) out << "::";
        }
        return out;
    }

    std::shared_ptr<cpptype> substitute(const named_types_map& params) override {
        auto r = params.find(names[0]);
        if(r != params.end()) {
            std::vector<id> nnames{names};
            auto            pt = std::dynamic_pointer_cast<plain_type>(r->second);
            if(pt == nullptr) {
                throw std::runtime_error(
                    "must bind plain type to template parameter used in dependent type name"
                );
            }
            nnames[0] = pt->name;
            return std::make_shared<dependent_name_type>(nnames);
        }
        return std::dynamic_pointer_cast<cpptype>(this->shared_from_this());
    }
};

struct template_instance : public cpptype {
    id                                    name;
    std::vector<std::shared_ptr<cpptype>> args;

    template_instance(id name, std::vector<std::shared_ptr<cpptype>> args)
        : name(name), args(std::move(args)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        out << toks.identifiers[name] << "<";
        for(size_t i = 0; i < args.size(); ++i) {
            args[i]->print(out, toks);
            if(i < args.size() - 1) out << ", ";
        }
        return out << ">";
    }

    std::shared_ptr<cpptype> substitute(const named_types_map& params) override {
        std::vector<std::shared_ptr<cpptype>> new_args;
        new_args.reserve(args.size());
        for(const auto& a : args)
            new_args.emplace_back(a->substitute(params));
        return std::make_shared<template_instance>(name, new_args);
    }
};

struct const_type : public cpptype {
    std::shared_ptr<cpptype> underlying;

    const_type(std::shared_ptr<cpptype> u) : underlying(std::move(u)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        out << "const ";
        return underlying->print(out, toks);
    }

    std::shared_ptr<cpptype> substitute(const named_types_map& params) override {
        return std::make_shared<const_type>(underlying->substitute(params));
    }
};

struct ref_type : public cpptype {
    std::shared_ptr<cpptype> deref;

    ref_type(std::shared_ptr<cpptype> u) : deref(std::move(u)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        return deref->print(out, toks) << "&";
    }

    std::shared_ptr<cpptype> substitute(const named_types_map& params) override {
        return std::make_shared<ref_type>(deref->substitute(params));
    }
};

struct ptr_type : public cpptype {
    std::shared_ptr<cpptype> deref;

    ptr_type(std::shared_ptr<cpptype> u) : deref(std::move(u)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        return deref->print(out, toks) << "*";
    }

    std::shared_ptr<cpptype> substitute(const named_types_map& params) override {
        return std::make_shared<ptr_type>(deref->substitute(params));
    }
};

struct fn_type : public cpptype {
    std::shared_ptr<cpptype>              return_type;
    std::vector<std::shared_ptr<cpptype>> arguments;

    fn_type(std::shared_ptr<cpptype> ret_type, std::vector<std::shared_ptr<cpptype>> arguments)
        : return_type(std::move(ret_type)), arguments(std::move(arguments)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        return_type->print(out, toks) << "(";
        for(size_t i = 0; i < arguments.size(); ++i) {
            arguments[i]->print(out, toks);
            if(i < arguments.size() - 1) out << ",";
        }
        return out << ")";
    }

    std::shared_ptr<cpptype> substitute(const named_types_map& params) override {
        std::vector<std::shared_ptr<cpptype>> new_args;
        new_args.reserve(arguments.size());
        for(const auto& a : arguments)
            new_args.emplace_back(a->substitute(params));
        return std::make_shared<fn_type>(return_type->substitute(params), new_args);
    }
};

struct property {
    id                       name;
    std::shared_ptr<cpptype> type;
    bool                     readonly;

    property(std::shared_ptr<cpptype> type, id name, bool readonly)
        : name(name), type(std::move(type)), readonly(readonly) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const {
        out << "P " << toks.identifiers[name] << ": ";
        return type->print(out, toks) << (readonly ? " (ro)" : " (rw)");
    }

    void substitute_in_place(const named_types_map& subs) { type = type->substitute(subs); }
};

struct method {
    id                                                   name;
    std::shared_ptr<cpptype>                             return_type;
    std::vector<std::pair<std::shared_ptr<cpptype>, id>> args;
    bool                                                 with_cx;
    std::optional<std::tuple<std::vector<id>, template_known_instances>>
        template_names_and_known_instances;

    method(
        id                                                                   name,
        std::shared_ptr<cpptype>                                             return_type,
        bool                                                                 with_cx,
        std::optional<std::tuple<std::vector<id>, template_known_instances>> t = {}
    )
        : name(name), return_type(std::move(return_type)), with_cx(with_cx),
          template_names_and_known_instances(std::move(t)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const {
        out << "M ";
        if(with_cx) out << "+X ";
        out << toks.identifiers[name] << "(";
        for(const auto& [ty, nm] : args) {
            out << toks.identifiers[nm] << ": ";
            ty->print(out, toks) << ", ";
        }
        out << ") -> ";
        return_type->print(out, toks);
        if(template_names_and_known_instances.has_value()) {
            const auto& [names, known_insts] = template_names_and_known_instances.value();
            out << "\ttemplate<";
            for(const auto& n : names)
                out << toks.identifiers[n] << ", ";
            out << "> = { ";
            for(const auto& ki : known_insts) {
                out << "(";
                for(const auto& t : ki)
                    t->print(out, toks) << ", ";
                out << ") ";
            }
            out << "}";
        }
        return out;
    }

    void substitute_in_place(const named_types_map& subs) {
        return_type = return_type->substitute(subs);
        for(auto& [t, _] : args)
            t = t->substitute(subs);
        if(template_names_and_known_instances.has_value()) {
            auto& m = std::get<1>(template_names_and_known_instances.value());
            for(auto& ts : m)
                for(auto& t : ts)
                    t = t->substitute(subs);
        }
    }
};

struct object {
    id                    name;
    std::vector<property> properties;
    std::vector<method>   methods;

    object(id name) : name(name) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const {
        out << "object " << toks.identifiers[name] << " {\n";
        for(const auto& prop : properties) {
            out << "\t";
            prop.print(out, toks) << "\n";
        }
        for(const auto& m : methods) {
            out << "\t";
            m.print(out, toks) << "\n";
        }
        return out << "}";
    }

    void substitute_in_place(const named_types_map& subs) {
        for(auto& p : properties)
            p.substitute_in_place(subs);
        for(auto& m : methods)
            m.substitute_in_place(subs);
    }
};

struct world {
    std::vector<object> objects;
    named_types_map     typedefs;

    std::ostream& print(std::ostream& out, const tokenizer& toks) const {
        for(const auto& ob : objects)
            ob.print(out, toks) << "\n";
        for(const auto& [name, ty] : typedefs) {
            out << "using " << toks.identifiers[name] << " as ";
            ty->print(out, toks) << "\n";
        }
        return out;
    }
};

struct parse_error : public std::runtime_error {
    token  tk;
    size_t ln;

    parse_error(token tk, size_t ln, const std::string& s)
        : std::runtime_error(s), tk(tk), ln(ln) {}
};

struct template_error : public std::runtime_error {
    size_t ln;

    template_error(size_t ln, const std::string& s) : std::runtime_error(s), ln(ln) {}
};

struct parser {
    tokenizer& toks;

    parser(tokenizer& toks, id constructor_id) : toks(toks), constructor_id(constructor_id) {}

    void parse(world& ast);

  private:
    id                       constructor_id;
    void                     check_next_symbol(symbol_type s, const std::string& msg);
    std::shared_ptr<cpptype> parse_type();
    property                 parse_property();
    method                   parse_method();
    method                   parse_constructor(const object& ob);
    void   parse_signature(std::vector<std::pair<std::shared_ptr<cpptype>, id>>& args);
    object parse_object();
    std::pair<id, std::shared_ptr<cpptype>>               parse_typedef();
    std::vector<std::shared_ptr<cpptype>>                 parse_template_param_list();
    template_known_instances                              parse_known_instance_map();
    std::tuple<std::vector<id>, template_known_instances> parse_template_def();
};
