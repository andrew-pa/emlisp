#include <emlisp.h>
#include <iostream>
using namespace emlisp;

bool test_destroyed = false;

struct test {
    int x;

    test() : x(0xabcdef) {
        test_destroyed = false;
    }

    ~test() {
        std::cout << x << "\n";
        test_destroyed = true;
    }
};

struct thing {
    int x;
    int y;

    thing(int x, int y) : x(x), y(y) {}

    void test() {
        x *= y;
        y += 1;
    }
};

int main() {
    runtime rt{1024*1024, false};

    {
        test t;
    }
    assert(test_destroyed);

    value lv = rt.make_owned_extern<thing>(1, 1);
    auto hv = rt.handle_for(lv);
    rt.define_global("v", lv);

    rt.define_fn("test", [](runtime* rt, value args, void* cx) {
        auto* v = rt->get_extern_reference<thing>(first(args));
        std::cout << "Bx = " << v->x << " y = " << v->y << " v=" << std::hex << v << std::dec << "\n";
        v->test();
        std::cout << "Ax = " << v->x << " y = " << v->y << " v=" << std::hex << v << std::dec << "\n";
        return NIL;
    }, nullptr);

    rt.eval(rt.read("(test v)"));
    auto* cv = rt.get_extern_reference<thing>(*hv);
    assert(cv->x == 1 && cv->y == 2);

    std::cout << "0\n";
    rt.collect_garbage();

    rt.eval(rt.read("(test v)"));
    cv = rt.get_extern_reference<thing>(*hv);
    assert(cv->x == 2 && cv->y == 3);

    std::cout << "1\n";
    value t = rt.make_owned_extern<test>();
    assert(!test_destroyed);
    rt.collect_garbage();
    assert(test_destroyed);

    std::cout << "2\n";
    {
        value t2 = rt.make_owned_extern<test>();
        assert(!test_destroyed);
        test tt = rt.take_owned_extern<test>(t2);
        rt.collect_garbage();
        assert(!test_destroyed);
    }
    assert(test_destroyed);

    return 0;
}
