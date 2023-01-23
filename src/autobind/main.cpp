#include "emlisp_autobind.h"
#include "parser.h"
#include "token.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

std::string make_lisp_name(std::string_view s) {
    std::string ns(s);
    for(auto& c : ns)
        if(c == '_') c = '-';
    return ns;
}

const std::unordered_set<std::string> intlike_types
    = {"char",
       "short",
       "int",
       "long",
       "uint8_t",
       "uint16_t",
       "uint32_t",
       "uint64_t",
       "int8_t",
       "int16_t",
       "int32_t",
       "int64_t",
       "size_t",
       "intptr_t"};

const std::unordered_set<std::string> floatlike_types = {"float", "double"};

struct code_generator {
    size_t           next_tmp;
    std::ofstream    out;
    const tokenizer& toks;

    code_generator(const std::filesystem::path& output_path, const tokenizer& toks)
        : out(output_path), toks(toks), next_tmp(1) {}

    void out_define_fn(std::string_view name, const std::function<void()>& body) {
        out << "rt->define_fn(\"" << name << "\", [](runtime* rt, value args, void* cx) {\n";
        body();
        out << "}, cx);\n\n";
    }

    std::string new_tmp_var() { return "_" + std::to_string(next_tmp++); }

    void unpack_self(const object& ob) {
        out << "auto* self = rt->get_extern_reference<" << toks.identifiers[ob.name]
            << ">(first(args));\n";
    }

    std::string get_arg(size_t arg_index) {
        auto tmp = new_tmp_var();
        out << "auto " << tmp << " = nth(args, " << arg_index << ");\n";
        return tmp;
    }

    std::string get_field(const std::string& self, id field_name) {
        auto tmp = new_tmp_var();
        out << "const auto& " << tmp << " = " << self << "->" << toks.identifiers[field_name]
            << ";\n";
        return tmp;
    }

    void set_field(const std::string& self, id field_name, const std::string& value) {
        out << self << "->" << toks.identifiers[field_name] << " = " << value << ";\n";
    }

    void call_method_expr(
        const std::string& self, id method_name, const std::vector<std::string>& arg_vals
    ) {
        out << self << "->" << toks.identifiers[method_name] << "(";
        for(int i = 0; i < arg_vals.size(); ++i) {
            out << arg_vals[i];
            if(i < arg_vals.size() - 1) out << ", ";
        }
        out << ");\n";
    }

    std::string call_method(
        const std::string& self, id method_name, const std::vector<std::string>& arg_vals
    ) {
        auto tmp = new_tmp_var();
        out << "auto " << tmp << " = ";
        call_method_expr(self, method_name, arg_vals);
        return tmp;
    }

    std::string lisp_to_cpp(const std::string& lisp_value, std::shared_ptr<cpptype> type) {
        auto tmp = new_tmp_var();
        out << "auto " << tmp << " = ";

        // const T& is basically a value type
        auto ct = std::dynamic_pointer_cast<const_type>(type);
        if(ct != nullptr) {
            auto rt = std::dynamic_pointer_cast<ref_type>(ct->underlying);
            if(rt != nullptr) type = rt->deref;
        }

        auto pt = std::dynamic_pointer_cast<plain_type>(type);
        if(pt != nullptr) {
            if(intlike_types.find(toks.identifiers[pt->name]) != intlike_types.end()) {
                out << "to_int(" << lisp_value << ");\n";
                return tmp;
            }
            if(floatlike_types.find(toks.identifiers[pt->name]) != floatlike_types.end()) {
                out << "to_float(" << lisp_value << ");\n";
                return tmp;
            }
            if(toks.identifiers[pt->name] == "bool") {
                out << "to_bool(" << lisp_value << ");\n";
                return tmp;
            }
            if(toks.identifiers[pt->name] == "std::string"
               || toks.identifiers[pt->name] == "std::string_view") {
                out << "rt->to_str(" << lisp_value << ");\n";
                return tmp;
            }
        }

        auto tt = std::dynamic_pointer_cast<template_instance>(type);
        if(tt != nullptr) {
            const auto& name = toks.identifiers[tt->name];
            if(name == "std::vector") {
                out << "rt->to_vec(" << lisp_value << ");\n";
                return tmp;
            }
            if(name == "std::function") {
                auto fn = std::dynamic_pointer_cast<fn_type>(tt->args.at(0));
                if(fn == nullptr) throw std::runtime_error("std::function must have function type");
                //  TODO: this captures the value for the function, which *could* get garbage
                //  collected before the C++ closure is invoked. Ideally we would create a value
                //  handle that is moved into the closure.
                auto fvh = new_tmp_var();
                out << "auto " << fvh << " = rt->handle_for(" << lisp_value << ");\n";
                out << "[&rt," << fvh << " = std::move(" << fvh << ")](";
                std::vector<std::string> cpp_args;
                for(size_t i = 0; i < fn->arguments.size(); ++i) {
                    auto n = new_tmp_var();
                    fn->arguments[i]->print(out, toks) << " " << n;
                    cpp_args.emplace_back(n);
                    if(i < fn->arguments.size() - 1) out << ",";
                }
                out << ") -> ";
                fn->return_type->print(out, toks) << " {\n";
                std::vector<std::string> lisp_args;
                for(size_t i = 0; i < fn->arguments.size(); ++i)
                    lisp_args.emplace_back(cpp_to_lisp(cpp_args[i], fn->arguments[i]));
                out << "auto args = ";
                for(size_t i = 0; i < fn->arguments.size(); ++i)
                    out << "rt->cons(" << lisp_args[i] << ", ";
                out << "NIL";
                for(size_t i = 0; i < fn->arguments.size(); ++i)
                    out << ")";
                out << ";\n";
                out << "auto result = rt->apply(*" << fvh << ", "
                    << "args);\n";
                auto prt = std::dynamic_pointer_cast<plain_type>(fn->return_type);
                if(prt == nullptr || toks.identifiers[prt->name] != "void")
                    return_from_fn(lisp_to_cpp("result", fn->return_type));
                out << "};\n";
                return tmp;
            }
        }

        out << "*rt->get_extern_reference<";
        type->print(out, toks);
        out << ">(" << lisp_value << ");\n";
        return tmp;
    }

    std::string cpp_to_lisp(const std::string& cpp_value, const std::shared_ptr<cpptype>& type) {
        auto tmp = new_tmp_var();

        auto tt = std::dynamic_pointer_cast<template_instance>(type);
        if(tt != nullptr) {
            const auto& name = toks.identifiers[tt->name];
            if(name == "std::vector") {
                auto tmp_vec = new_tmp_var();
                out << "std::vector<value> " << tmp_vec << ";\n";
                out << "for(const auto& x : " << cpp_value << "){\n";
                auto lisp_val = cpp_to_lisp("x", tt->args.at(0));
                out << tmp_vec << ".push_back(" << lisp_val << ");}\n";
                out << "auto " << tmp << " = rt->from_vec(" << tmp_vec << ");\n";
                return tmp;
            }
        }

        out << "auto " << tmp << " = ";
        auto pt = std::dynamic_pointer_cast<plain_type>(type);
        if(pt != nullptr) {
            if(intlike_types.find(toks.identifiers[pt->name]) != intlike_types.end()) {
                out << "rt->from_int(" << cpp_value << ");\n";
                return tmp;
            }
            if(floatlike_types.find(toks.identifiers[pt->name]) != floatlike_types.end()) {
                out << "rt->from_float(" << cpp_value << ");\n";
                return tmp;
            }
            if(toks.identifiers[pt->name] == "bool") {
                out << "rt->from_bool(" << cpp_value << ");\n";
                return tmp;
            }
            if(toks.identifiers[pt->name] == "std::string"
               || toks.identifiers[pt->name] == "std::string_view") {
                out << "rt->from_str(" << cpp_value << ");\n";
                return tmp;
            }
        }

        out << "rt->make_owned_extern<";
        type->print(out, toks);
        out << ">(" << cpp_value << ");\n";
        return tmp;
    }

    void return_from_fn(const std::string& val = "NIL") { out << "return " << val << ";\n"; }

    void check_for_arg(size_t arg_index) { out << "if(nth(args, " << arg_index << ") != NIL) {\n"; }

    void start_bindings(const std::vector<std::filesystem::path>& input_files) {
        out << "#include \"emlisp.h\"\n";
        for(const auto& i : input_files)
            out << "#include " << i << "\n";
        out << "using namespace emlisp;\n";
        out << "void add_bindings(runtime* rt, void* cx) {\n";
    }

    void start_object(const object& ob) {
        out << "/* object " << toks.identifiers[ob.name] << " */ {\n";
    }

    void end_block() { out << "}\n"; }
};

struct generate_visitor {
    code_generator   gen;
    const tokenizer& toks;

    generate_visitor(const std::filesystem::path& output_path, const tokenizer& toks)
        : gen(output_path, toks), toks(toks) {}

    void generate_property_function(
        const object& ob, const std::string& prefix, const property& prop
    ) {
        auto fn_name = prefix + make_lisp_name(toks.identifiers[prop.name]);
        gen.out_define_fn(fn_name, [&]() {
            // convert lisp self value into C++ type
            gen.unpack_self(ob);
            // if we're writing a new value and the property is read/write:
            if(!prop.readonly) {
                gen.check_for_arg(1);
                //  get new value as C++ value
                auto new_val = gen.get_arg(1);
                new_val      = gen.lisp_to_cpp(new_val, prop.type);
                //  set C++ field
                gen.set_field("self", prop.name, new_val);
                gen.return_from_fn();
                gen.end_block();
            }
            // otherwise:
            //  get the C++ value of the field
            auto field = gen.get_field("self", prop.name);
            //  convert C++ value to lisp value
            field = gen.cpp_to_lisp(field, prop.type);
            gen.return_from_fn(field);
        });
    }

    void generate_method_function(const object& ob, const std::string& prefix, const method& m) {
        auto fn_name = prefix + make_lisp_name(toks.identifiers[m.name]);
        gen.out_define_fn(fn_name, [&]() {
            gen.unpack_self(ob);
            std::vector<std::string> arg_vals;
            size_t                   i = 1;
            for(const auto& [ty, nm] : m.args) {
                if(i == 1 && m.with_cx) {
                    std::ostringstream oss;
                    oss << "(";
                    ty->print(oss, toks);
                    oss << ")cx";
                    arg_vals.push_back(oss.str());
                    continue;
                }
                auto lisp_arg = gen.get_arg(i++);
                arg_vals.push_back(gen.lisp_to_cpp(lisp_arg, ty));
            }
            auto prt = std::dynamic_pointer_cast<plain_type>(m.return_type);
            if(prt != nullptr && toks.identifiers[prt->name] == "void") {
                gen.call_method_expr("self", m.name, arg_vals);
                gen.return_from_fn();
            } else {
                auto cpp_retval = gen.call_method("self", m.name, arg_vals);
                gen.return_from_fn(gen.cpp_to_lisp(cpp_retval, m.return_type));
            }
        });
    }

    void generate_bindings_for_object(const object& ob) {
        gen.start_object(ob);
        auto prefix = make_lisp_name(toks.identifiers[ob.name]) + "/";
        for(const auto& prop : ob.properties)
            generate_property_function(ob, prefix, prop);
        for(const auto& m : ob.methods)
            generate_method_function(ob, prefix, m);
        gen.end_block();
    }

    void generate_bindings(
        const world& ast, const std::vector<std::filesystem::path>& input_files
    ) {
        gen.start_bindings(input_files);
        for(const auto& ob : ast.objects)
            generate_bindings_for_object(ob);
        gen.end_block();
    }
};

int main(int argc, char* argv[]) {
    if(argc < 4) {
        std::cerr << "autobind usage:\nemlisp_autobind <output_file> <input directory> <input "
                     "files...>\n";
        return -1;
    }

    std::filesystem::path output_path(argv[1]);
    std::filesystem::path input_path(argv[2]);

    std::cout << output_path << " <- " << input_path << "\n";

    std::string_view                   input_files_str(argv[3]);
    std::vector<std::filesystem::path> input_files;
    size_t                             pos = 0;
    do {
        size_t next_pos = input_files_str.find_first_of(',', pos);
        auto   s        = input_files_str.substr(pos, next_pos - pos);
        if(s.empty()) break;
        input_files.push_back(input_path / s);
        if(next_pos == std::string_view::npos) break;
        pos = next_pos + 1;
    } while(true);

    // open each file and process it into the AST
    world     ast;
    tokenizer toks{nullptr};
    for(const auto& inp : input_files) {
        std::cout << "input " << inp << "\n";
        std::ifstream ins(inp);
        toks.reset(&ins);
        try {
            parser p{toks};
            while(true) {
                auto ob = p.next_object();
                if(!ob.has_value()) break;
                ast.objects.push_back(ob.value());
            }
        } catch(parse_error e) {
            std::cerr << "failed to parse " << inp << " at ln " << e.ln << ": " << e.what() << " ("
                      << e.tk.type << ", " << e.tk.data << ")";
            return -1;
        }
    }

    ast.print(std::cout, toks);

    // generate bindings by visiting AST nodes
    generate_visitor g(output_path, toks);
    g.generate_bindings(ast, input_files);

    return 0;
}

// TODO: what do we do about method overloads based on arg count?
