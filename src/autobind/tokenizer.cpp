#include "token.h"
#include <istream>

std::optional<token> tokenizer::parse_symbol(char ch) {
    switch(ch) {
        case '{': return {symbol_type::open_brace};
        case '}': return {symbol_type::close_brace};
        case '(': return {symbol_type::open_paren};
        case ')': return {symbol_type::close_paren};
        case '[': return {symbol_type::open_sq};
        case ']': return {symbol_type::close_sq};
        case '<': return {symbol_type::open_angle};
        case '>': return {symbol_type::close_angle};
        case ':':
            // if (_in->peek() == ':') {
            //     _in->get();
            //     return {symbol_type::double_colon};
            // }
            return {symbol_type::colon};
        case ';': return {symbol_type::semicolon};
        case ',': return {symbol_type::comma};
        case '&': return {symbol_type::ampersand};
        case '*': return {symbol_type::star};
        case '=': return {symbol_type::eq};
        default: return std::nullopt;
    }
}

token tokenizer::parse_num(char ch) {
    int sign = 1;
    if(ch == '-') {
        ch   = _in->get();
        sign = -1;
    }
    intptr_t value = ch - '0';
    while((_in != nullptr) && (isdigit(_in->peek()) != 0))
        value = value * 10 + (_in->get() - '0');
    return {token::number, (size_t)value * sign};
}

token tokenizer::parse_str(char ch) {
    std::string str;
    while(*_in && _in->peek() != '"') {
        ch = _in->get();
        if(ch == '\\') {
            auto nch = _in->get();
            if(nch < 0) break;
            if(nch == '\\')
                ch = '\\';
            else if(nch == 'n')
                ch = '\n';
            else if(nch == 't')
                ch = '\t';
            else if(nch == '"')
                ch = '"';
            str += ch;
        } else if(ch > 0)
            str += ch;
        else
            break;
    }
    _in->get();
    auto id = string_literals.size();
    string_literals.push_back(str);
    return {token::str, id};
}

const std::map<std::string_view, keyword> keywords = {
    {"class",   keyword::class_   },
    {"struct",  keyword::struct_  },
    {"const",   keyword::const_   },
    {"EL_OBJ",  keyword::el_obj   },
    {"EL_PROP", keyword::el_prop  },
    {"EL_M",    keyword::el_m     },
    {"r",       keyword::read     },
    {"rw",      keyword::readwrite},
};

token tokenizer::parse_id(char ch) {
    std::string id;
    id += ch;
    while(*_in && ((isalnum(_in->peek()) != 0) || _in->peek() == '_' || _in->peek() == ':')) {
        ch = _in->get();
        if(ch > 0)
            id += ch;
        else
            break;
    }
    auto kwd = keywords.find(id);
    if(kwd != keywords.end()) return {kwd->second};
    auto   idc = std::find(this->identifiers.begin(), this->identifiers.end(), id);
    size_t index;
    if(idc != this->identifiers.end()) {
        index = std::distance(this->identifiers.begin(), idc);
    } else {
        index = this->identifiers.size();
        this->identifiers.push_back(id);
    }
    return {token::identifer, index};
}

token tokenizer::next_in_stream() {
    if(!*_in) return {token::eof, 0};
    auto ch = _in->get();
    if(ch < 0) return {token::eof, 0};

    while(isspace(ch) != 0) {
        if(ch == '\n') line_number++;
        ch = _in->get();
        if(ch < 0) return {token::eof, 0};
    }

    auto sym = parse_symbol(ch);
    if(sym.has_value()) return sym.value();

    if((isdigit(ch) != 0) || (ch == '-' && (isdigit(_in->peek()) != 0))) return parse_num(ch);
    if(ch == '"') return parse_str(ch);

    return parse_id(ch);
}
