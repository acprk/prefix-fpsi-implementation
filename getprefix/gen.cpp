#include <fstream>
#include <vector>
#include <random>
#include <iostream>
#include <cstdint>
#include <algorithm>

int main() {
    const int senderSize = 1 << 10; // 2^10 = 1024
    const int receiverSize = 1 << 10; // 2^10 = 1024
    const int intersectionSize = 100;
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<uint32_t> dist(1, 1000000); // 32-bit integers

    // Generate intersection elements
    std::vector<uint32_t> intersection(intersectionSize);
    for (int i = 0; i < intersectionSize; ++i) {
        intersection[i] = dist(rng);
    }

    // Generate sender set: 100 intersection + 924 unique elements
    std::vector<uint32_t> senderSet = intersection;
    while (senderSet.size() < senderSize) {
        uint32_t val = dist(rng);
        if (std::find(senderSet.begin(), senderSet.end(), val) == senderSet.end()) {
            senderSet.push_back(val);
        }
    }

    // Generate receiver set: 100 intersection + 924 unique elements
    std::vector<uint32_t> receiverSet = intersection;
    while (receiverSet.size() < receiverSize) {
        uint32_t val = dist(rng);
        if (std::find(receiverSet.begin(), receiverSet.end(), val) == receiverSet.end() &&
            std::find(senderSet.begin(), senderSet.end(), val) == senderSet.end()) {
            receiverSet.push_back(val);
        }
    }

    // Save sender set to file
    std::ofstream senderFile("sender_set.txt");
    if (!senderFile) {
        std::cerr << "Failed to open sender_set.txt" << std::endl;
        return 1;
    }
    for (const auto& val : senderSet) {
        senderFile << val << "\n";
    }
    senderFile.close();

    // Save receiver set to file
    std::ofstream receiverFile("receiver_set.txt");
    if (!receiverFile) {
        std::cerr << "Failed to open receiver_set.txt" << std::endl;
        return 1;
    }
    for (const auto& val : receiverSet) {
        receiverFile << val << "\n";
    }
    receiverFile.close();

    // Save intersection for verification
    std::ofstream intersectionFile("intersection.txt");
    if (!intersectionFile) {
        std::cerr << "Failed to open intersection.txt" << std::endl;
        return 1;
    }
    for (const auto& val : intersection) {
        intersectionFile << val << "\n";
    }
    intersectionFile.close();

    std::cout << "Datasets generated: sender_set.txt, receiver_set.txt, intersection.txt" << std::endl;
    return 0;
}