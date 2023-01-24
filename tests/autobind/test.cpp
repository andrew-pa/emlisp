#include <iostream>
#include "emlisp.h"
#include "api.h"
using namespace emlisp;

void add_bindings(runtime* rt, void* cx);

int main(int argc, char* argv[]) {
    emlisp::runtime rt{ 1024*1024 };
    int cx = 0;
    add_bindings(&rt, (void*)&cx);

    counter c { 10 };

    test_fn f;

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

        std::cout << "testcx/2\n";
        {rt.eval(rt.read("(counter/test-context c)"));
            assert(cx == 2);}

        c.reset();
        std::cout << "t/1\n";
        {rt.eval(rt.read("(counter/test-templates<int> c 3)"));
            assert(c.value == 3);}
        std::cout << "t/2\n";
        {rt.eval(rt.read("(counter/test-templates<float> c 2.0)"));
            assert(c.value == 5);}

        std::cout << "testfn\n";
        c.reset();
        {rt.eval(rt.read("(test-fn/times (test-fn 3) 10 (lambda (i) (counter/increment c i)))"));
            assert(c.value == 45);}

        std::cout << "test constructors\n";
        {
            value x = rt.eval(rt.read("(let ([nc (counter 2)]) (counter/increment nc 3))"));
            assert(to_int(x) == 5);
        }
    } catch(emlisp::type_mismatch_error e) {
        std::cout << "error: " << e.what() << "; actual = " << e.actual << ", expected = " << e.expected << "\n";
        exit(3);
    } catch (std::runtime_error e) {
        std::cout << "error: " << e.what() << "\n";
        exit(2);
    }

    return 0;
}
