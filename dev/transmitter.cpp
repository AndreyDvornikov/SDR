#include <iostream>
#include <random>
#include <vector>

int main() {
    constexpr int SIZE = 16;

    std::vector<bool> bits(SIZE);

    std::mt19937 rng(std::random_device{}());
    std::bernoulli_distribution bit(0.5);

    for (int i = 0; i < SIZE; ++i) {
        bits[i] = bit(rng);
    }

    for (bool b : bits) {
        std::cout << b;
    }

    std::cout << '\n';
}
