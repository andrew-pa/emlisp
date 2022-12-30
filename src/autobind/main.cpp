#include <iostream>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>
#include <fstream>
#include <memory>

#include "emlisp_autobind.h"
#include "token.h"
#include "parser.h"

void generate_bindings(const std::filesystem::path& output_path, const world& ast) {
    // std::ofstream out(output_path);
}

int main(int argc, char* argv[]) {
    if(argc < 4) {
        std::cerr << "autobind usage:\nemlisp_autobind <output_file> <input directory> <input files...>\n";
        return -1;
    }

    std::filesystem::path output_path(argv[1]);
    std::filesystem::path input_path(argv[2]);

    std::cout << output_path << " <- " << input_path << "\n";

    std::string_view input_files_str(argv[3]);
    std::vector<std::filesystem::path> input_files;
    size_t pos = 0;
    do {
        size_t next_pos = input_files_str.find_first_of(',', pos);
        auto s = input_files_str.substr(pos, next_pos-pos);
        if(s.empty()) break;
        input_files.push_back(input_path / s);
        if(next_pos == std::string_view::npos) break;
        pos = next_pos + 1;
    } while(true);

    // open each file and process it into the AST
    world ast;
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
            std::cerr << "failed to parse " << inp << " at ln " << e.ln <<
                ": " << e.what() << " (" << e.tk.type << ", " << e.tk.data << ")";
            return -1;
        }
    }

    ast.print(std::cout, toks);

    // generate bindings by visiting AST nodes
    generate_bindings(output_path, ast);

    return 0;
}

// TODO: what do we do about method overloads based on arg count?
