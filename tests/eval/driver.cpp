#include <iostream>
#include <fstream>
#include "emlisp.h"

std::string get_file_contents(const char* filename) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in) {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return(contents);
    }
    throw errno;
}

int main(int argc, char* argv[]) {
    auto source = get_file_contents(argv[1]);

    emlisp::runtime rt;

    rt.define_fn("assert!", [](emlisp::runtime* rt, emlisp::value args, void* d) {
        if (emlisp::first(args) != emlisp::TRUE) {
            std::cout << "assertion failed! value = ";
            rt->write(std::cout, emlisp::first(args));
            if (emlisp::second(args) != emlisp::NIL) {
                std::cout << ": ";
                rt->write(std::cout, emlisp::first(emlisp::second(args)));
            }
            std::cout << "\n";
            exit(1);
        }
        return emlisp::NIL;
	}, nullptr);

    rt.define_fn("assert-eq!", [](emlisp::runtime* rt, emlisp::value args, void* d) {
        auto a = emlisp::first(args);
        auto b = emlisp::first(emlisp::second(args));
        if (a != b) {
            std::cout << "assertion failed! ";
            rt->write(std::cout, a);
            std::cout << " != ";
            rt->write(std::cout, b);
            if (emlisp::second(emlisp::second(args)) != emlisp::NIL) {
                std::cout << ": ";
                rt->write(std::cout, emlisp::first(emlisp::second(emlisp::second(args))));
            }
            std::cout << "\n";
            exit(1);
        }
        return emlisp::NIL;
	}, nullptr);

    auto src_vals = rt.read_all(source);

    for(auto v : src_vals) {
        try {
            rt.write(std::cout, v);
            std::cout << "\n";
            rt.eval(v);
        }
        catch(std::runtime_error e) {
            std::cout << "error: " << e.what() << "\n";
            exit(2);
        }
    }

    return 0;
}
