#include <iostream>
#include "emlisp.h"
#include "api.h"
using namespace emlisp;

void add_bindings(runtime* rt, void* cx);

int main(int argc, char* argv[]) {
    emlisp::runtime rt{ 1024*1024 };
    add_bindings(&rt, nullptr);

    counter c {
        .value = 10
    };

    try {
        rt.define_global("c", rt.make_extern_reference(&c));

        std::cout << "get/1\n";
        value x = rt.eval(rt.read("(counter/value c)"));
        assert(to_int(x) == 10);

        std::cout << "reset/1\n";
        rt.eval(rt.read("(counter/reset c)"));
        assert(c.value == 0);

        std::cout << "get/2\n";
        {value x = rt.eval(rt.read("(counter/value c)"));
            assert(to_int(x) == 0);}

        std::cout << "incr/1\n";
        {value x = rt.eval(rt.read("(counter/increment c 1)"));
            assert(c.value == 1);
            assert(to_int(x) == 1);}

        std::cout << "incr/2\n";
        {value x = rt.eval(rt.read("(counter/increment c 1)"));
            assert(c.value == 2);
            assert(to_int(x) == 2);}
    } catch(emlisp::type_mismatch_error e) {
        std::cout << "error: " << e.what() << "; actual = " << e.actual << ", expected = " << e.expected << "\n";
        exit(3);
    } catch (std::runtime_error e) {
        std::cout << "error: " << e.what() << "\n";
        exit(2);
    }

    return 0;
}
