#include "core/Region.h"
#include "core/Neuron.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace NeuroForge::Core;

bool is_close(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) < epsilon;
}

int main() {
    auto region = RegionFactory::createRegion("TestRegion", Region::Type::Cortical, Region::ActivationPattern::Asynchronous);
    region->initialize();

    // Create neurons
    std::shared_ptr<Neuron> n1 = NeuronFactory::createNeuron();
    std::shared_ptr<Neuron> n2 = NeuronFactory::createNeuron();

    // Ensure decay rate is non-zero
    n1->setDecayRate(0.1f);
    n2->setDecayRate(0.1f);

    std::cout << "Test 1: Add N1 and process" << std::endl;
    if (!region->addNeuron(n1)) return 1;

    n1->setActivation(0.5f);
    region->process(0.1f);

    if (is_close(n1->getActivation(), 0.5f)) {
        std::cerr << "FAIL: N1 did not change." << std::endl;
        return 1;
    }
    std::cout << "PASS: N1 processed. Val=" << n1->getActivation() << std::endl;

    std::cout << "Test 2: Add N2 and process" << std::endl;
    if (!region->addNeuron(n2)) return 1;

    n1->setActivation(0.5f);
    n2->setActivation(0.5f);

    region->process(0.1f);

    if (is_close(n1->getActivation(), 0.5f)) {
        std::cerr << "FAIL: N1 did not change." << std::endl;
        return 1;
    }
    if (is_close(n2->getActivation(), 0.5f)) {
        std::cerr << "FAIL: N2 did not change." << std::endl;
        return 1;
    }
    std::cout << "PASS: N1 and N2 processed." << std::endl;

    std::cout << "Test 3: Remove N1 and process" << std::endl;
    if (!region->removeNeuron(n1->getId())) {
        std::cerr << "FAIL: Could not remove N1." << std::endl;
        return 1;
    }

    n1->setActivation(0.5f);
    n2->setActivation(0.5f);

    region->process(0.1f);

    if (!is_close(n1->getActivation(), 0.5f)) {
        std::cerr << "FAIL: N1 was processed after removal! Current: " << n1->getActivation() << std::endl;
        return 1;
    }
    if (is_close(n2->getActivation(), 0.5f)) {
        std::cerr << "FAIL: N2 did not change." << std::endl;
        return 1;
    }
    std::cout << "PASS: N1 ignored, N2 processed." << std::endl;

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
