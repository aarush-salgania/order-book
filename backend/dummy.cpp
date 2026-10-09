#include <iostream>
#include <string>

int main() {
    std::string line;
    // Keep reading line by line from stdin
    while (std::getline(std::cin, line)) {
        // Echo back a mock trade or book state
        std::cout << "TRADE_EXECUTED: " << line << std::endl;
    }
    return 0;
}