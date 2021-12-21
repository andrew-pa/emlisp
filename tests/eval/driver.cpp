#include <iostream>
#include <fstream>
#include "emlisp.h"

int main(int argc, char* argv[]) {
    std::ifstream input(argv[1]);

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

    std::string s;
    while(std::getline(input, s)) {
        try {
            emlisp::value v = rt.read(s);
            v = rt.eval(v);
        }
        catch(std::runtime_error e) {
            std::cout << "error: " << e.what() << "\n";
            exit(2);
        }
    }

    return 0;
}
