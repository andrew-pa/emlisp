#include <iostream>
#include "emlisp.h"


int main(int argc, char* argv[]) {
	std::string line;
	auto rt = emlisp::runtime();
	while (true) {
		try {
			std::cout << "> ";
			std::getline(std::cin, line);
			auto v = rt.read(line);
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
