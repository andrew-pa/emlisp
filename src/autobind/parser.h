#pragma once
#include "emlisp_autobind.h"
#include "token.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

struct cpptype {
    virtual std::ostream& print(std::ostream& out, const tokenizer& toks) const = 0;
    virtual ~cpptype()                                                          = default;
};

using id = size_t;

struct plain_type : public cpptype {
    id name;

    plain_type(id name) : name(name) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        return out << toks.identifiers[name];
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
};

struct const_type : public cpptype {
    std::shared_ptr<cpptype> underlying;

    const_type(std::shared_ptr<cpptype> u) : underlying(std::move(u)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        out << "const ";
        return underlying->print(out, toks);
    }
};

struct ref_type : public cpptype {
    std::shared_ptr<cpptype> deref;

    ref_type(std::shared_ptr<cpptype> u) : deref(std::move(u)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        return deref->print(out, toks) << "&";
    }
};

struct ptr_type : public cpptype {
    std::shared_ptr<cpptype> deref;

    ptr_type(std::shared_ptr<cpptype> u) : deref(std::move(u)) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const override {
        return deref->print(out, toks) << "*";
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
};

struct method {
    id                                                   name;
    std::shared_ptr<cpptype>                             return_type;
    std::vector<std::pair<std::shared_ptr<cpptype>, id>> args;
    bool                                                 with_cx;

    method(id name, std::shared_ptr<cpptype> return_type, bool with_cx)
        : name(name), return_type(std::move(return_type)), with_cx(with_cx) {}

    std::ostream& print(std::ostream& out, const tokenizer& toks) const {
        out << "M ";
        if(with_cx) out << "+X ";
        out << toks.identifiers[name] << "(";
        for(const auto& [ty, nm] : args) {
            out << toks.identifiers[nm] << ": ";
            ty->print(out, toks) << ", ";
        }
        out << ") -> ";
        return return_type->print(out, toks);
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
};

struct world {
    std::vector<object> objects;

    std::ostream& print(std::ostream& out, const tokenizer& toks) const {
        for(const auto& ob : objects)
            ob.print(out, toks) << "\n";
        return out;
    }
};

struct parse_error : public std::runtime_error {
    token  tk;
    size_t ln;

    parse_error(token tk, size_t ln, const std::string& s)
        : std::runtime_error(s), tk(tk), ln(ln) {}
};

struct parser {
    tokenizer& toks;

    parser(tokenizer& toks) : toks(toks) {}

    std::optional<object> next_object();

  private:
    void                     check_next_symbol(symbol_type s, const std::string& msg);
    std::shared_ptr<cpptype> parse_type();
    property                 parse_property();
    method                   parse_method();
};
