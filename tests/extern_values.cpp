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

int main() {
    runtime rt{1024*1024, false};

    {
        test t;
    }
    assert(test_destroyed);

    value lv = rt.make_owned_extern<std::vector<int>>();
    auto* cv = rt.get_extern_reference<std::vector<int>>(lv);
    rt.define_global("v", lv);

    rt.define_fn("test", [](runtime* rt, value args, void* cx) {
        auto* v = rt->get_extern_reference<std::vector<int>>(first(args));
        v->push_back(v->size());
        return NIL;
    }, nullptr);

    rt.eval(rt.read("(test v)"));
    assert(cv->size() == 1 && cv->at(0) == 0);

    std::cout << "0\n";
    rt.collect_garbage();

    rt.eval(rt.read("(test v)"));
    assert(cv->size() == 2 && cv->at(1) == 1);

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
