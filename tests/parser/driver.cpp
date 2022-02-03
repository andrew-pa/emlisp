#include "emlisp.h"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    std::ifstream input(argv[1]);

    emlisp::runtime rt(1024*1024, false);

    std::string s;
    while(std::getline(input, s)) {
        emlisp::value v = rt.read(s);
        rt.write(std::cout, v);
        std::cout << "\n";
    }

    return 0;
}
