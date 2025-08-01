#include <volePSI/RsPsi.h>
#include <cryptoTools/Common/CLP.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Network/Session.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <thread>
#include <cstdint>
#include <algorithm>

void loadSet(const std::string& filename, std::vector<osuCrypto::block>& set) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Failed to open " << filename << std::endl;
        exit(1);
    }
    uint32_t val;
    while (file >> val) {
        set.push_back(osuCrypto::toBlock(val));
    }
    file.close();
}

void runSender(const std::vector<osuCrypto::block>& senderSet, osuCrypto::Channel& chl) {
    osuCrypto::PRNG prng(osuCrypto::sysRandomSeed());
    volePSI::RsPsiSender sender;
    sender.init(senderSet.size(), 40, prng.get<osuCrypto::block>());
    sender.send(senderSet, chl);
}

void runReceiver(const std::vector<osuCrypto::block>& receiverSet, osuCrypto::Channel& chl, std::vector<osuCrypto::u64>& intersection) {
    osuCrypto::PRNG prng(osuCrypto::sysRandomSeed());
    volePSI::RsPsiReceiver receiver;
    receiver.init(receiverSet.size(), 40, prng.get<osuCrypto::block>());
    receiver.receive(receiverSet, chl, intersection);
}

int main() {
    // Load datasets
    std::vector<osuCrypto::block> senderSet, receiverSet;
    loadSet("sender_set.txt", senderSet);
    loadSet("receiver_set.txt", receiverSet);

    // Set up networking
    osuCrypto::IOService ios;
    osuCrypto::Session senderSession(ios, "localhost:1212", osuCrypto::SessionMode::Server);
    osuCrypto::Session receiverSession(ios, "localhost:1212", osuCrypto::SessionMode::Client);
    auto senderChl = senderSession.addChannel();
    auto receiverChl = receiverSession.addChannel();

    // Run PSI protocol
    std::vector<osuCrypto::u64> intersection;
    std::thread senderThread(runSender, senderSet, std::ref(senderChl));
    runReceiver(receiverSet, receiverChl, intersection);
    senderThread.join();

    // Load expected intersection for verification
    std::vector<uint32_t> expectedIntersection;
    std::ifstream intersectionFile("intersection.txt");
    if (!intersectionFile) {
        std::cerr << "Failed to open intersection.txt" << std::endl;
        return 1;
    }
    uint32_t val;
    while (intersectionFile >> val) {
        expectedIntersection.push_back(val);
    }
    intersectionFile.close();

    // Verify intersection
    std::vector<uint32_t> computedIntersection;
    for (auto idx : intersection) {
        computedIntersection.push_back(osuCrypto::block(receiverSet[idx]).get<uint32_t>()[0]);
    }
    std::sort(computedIntersection.begin(), computedIntersection.end());
    std::sort(expectedIntersection.begin(), expectedIntersection.end());

    bool correct = (computedIntersection.size() == expectedIntersection.size()) &&
                  std::equal(computedIntersection.begin(), computedIntersection.end(), expectedIntersection.begin());
    std::cout << "Intersection size: " << computedIntersection.size() << std::endl;
    std::cout << "Verification " << (correct ? "passed" : "failed") << std::endl;
    if (correct) {
        std::cout << "Intersection (first 10 elements): ";
        for (size_t i = 0; i < std::min<size_t>(10, computedIntersection.size()); ++i) {
            std::cout << computedIntersection[i] << " ";
        }
        std::cout << std::endl;
    }

    return correct ? 0 : 1;
}