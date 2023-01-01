#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    // std::cout << argv[1] << "\n" << argv[2] << "\n" << argv[3] << "\n";
    if(argc < 4) return -1;
    std::ofstream output(argv[1]);
    std::ifstream template_input(argv[2]);
    std::ifstream source(argv[3]);
    std::string   line;
    while(std::getline(template_input, line)) {
        if(line == "%%%%") {
            while(std::getline(source, line)) {
                for(size_t i = 0; i < line.size();) {
                    if(line[0] == ';') break;
                    if(std::isspace(line[i]) != 0) {
                        while(i < line.size() && (std::isspace(line[i]) != 0))
                            i++;
                        output << ' ';
                    } else {
                        output << line[i++];
                    }
                }
                output << ' ';
            }
            continue;
        }
        output << line << "\n";
    }
    return 0;
}
