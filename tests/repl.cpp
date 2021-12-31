#include <iostream>
#include "emlisp.h"


int main(int argc, char* argv[]) {
	std::string line;
	auto rt = emlisp::runtime();
	rt.define_fn("debug-print", [](emlisp::runtime* rt, emlisp::value args, void* d) {
		std::cout << std::hex << emlisp::first(args) << std::dec << "\n";
		return emlisp::NIL;
	}, nullptr);
	int64_t counter = 0;
	rt.define_fn("inc-counter", [](emlisp::runtime* rt, emlisp::value args, void* d) {
		int64_t* counter = (int64_t*)d;
		*counter = *counter + 1;
		return rt->from_int(*counter);
	}, &counter);
	rt.define_fn("run-gc", [](emlisp::runtime* rt, emlisp::value args, void* d) {
		emlisp::heap_info ifo;
		rt->collect_garbage(&ifo);
		std::cout << "old heap had " << ifo.old_size << " bytes, new heap has " << ifo.new_size << " bytes\n"
			<< "collected " << (ifo.old_size - ifo.new_size) << " bytes\n";
		return emlisp::NIL;
	}, nullptr);
	while (true) {
		try {
			std::cout << "> ";
			std::getline(std::cin, line);
			auto v = rt.expand(rt.read(line));
			std::cout << " -> ";
			rt.write(std::cout, v);
			std::cout << "\n";

			auto ev = rt.eval(v);
			std::cout << " = ";
			rt.write(std::cout, ev);
			std::cout << "\n";
		} catch (emlisp::type_mismatch_error te) {
			std::cout << "type error: expected " << te.expected << ", found " << te.actual << ": " << te.what() << "\n";
		} catch (std::runtime_error e) {
			std::cout << "error: " << e.what() << "\n";
		}
	}
	return 0;
}
