#pragma once
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

enum class symbol_type {
    open_paren,
    close_paren,
    open_brace,
    close_brace,
    open_sq,
    close_sq,
    open_angle,
    close_angle,
    colon,
    double_colon,
    semicolon,
    comma,
    ampersand,
    star,
    eq
};

enum class keyword {
    class_,
    struct_,
    const_,
    template_,
    typename_,
    using_,
    el_obj,
    el_prop,
    el_m,
    el_c,
    el_with_cx,
    read,
    readwrite,
    el_known_insts,
    el_typedef,
    el_always_shared
};

struct token {
    enum type_e { number, identifer, keyword, symbol, eof, str } type;

    size_t data;

    token(type_e type, size_t data) : type(type), data(data) {}

    token(symbol_type data) : type(symbol), data((size_t)data) {}

    token(enum keyword k) : type(keyword), data((size_t)k) {}

    inline bool is_number() const { return type == number; }

    inline bool is_id() const { return type == identifer; }

    inline bool is_symbol(symbol_type t) const { return type == symbol && data == (size_t)t; }

    inline bool is_keyword(enum keyword k) const { return type == keyword && data == (size_t)k; }

    inline bool is_eof() const { return type == eof; }

    inline bool is_str() const { return type == str; }
};

class tokenizer {
  public:
    std::vector<std::string> identifiers;
    std::vector<std::string> string_literals;
    size_t                   line_number;

  private:
    std::istream*        _in;
    std::optional<token> next_token;

    token                next_in_stream();
    std::optional<token> parse_symbol(char ch);
    token                parse_num(char ch);
    token                parse_str(char ch);
    token                parse_id(char ch);

  public:
    tokenizer(std::istream* input) : line_number(0), _in(input) {}

    void reset(std::istream* input) {
        _in         = input;
        line_number = 0;
        next_token  = std::optional<token>();
    }

    token next() {
        if(this->next_token.has_value()) {
            auto tok = this->next_token.value();
            this->next_token.reset();
            return tok;
        }
        auto t = this->next_in_stream();
        // std::cout << t.type << "; " << t.data;
        // if(t.is_id()) {
        //     std::cout << " " << identifiers[t.data];
        // }
        // std::cout << "\n";
        return t;
    }

    const token& peek() {
        if(!this->next_token.has_value()) next_token = this->next_in_stream();
        return this->next_token.value();
    }
};
