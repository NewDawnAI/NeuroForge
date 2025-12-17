#include <iostream>
#include <memory>
#include "connectivity/ConnectivityManager.h"
#include "core/HypergraphBrain.h"

using namespace NeuroForge::Core;
using namespace NeuroForge::Connectivity;

int main() {
    std::cout << "Starting M6 simple test..." << std::endl;
    
    try {
        std::cout << "Creating ConnectivityManager..." << std::endl;
        auto connectivity_manager = std::make_shared<ConnectivityManager>();
        std::cout << "ConnectivityManager created successfully" << std::endl;
        
        std::cout << "Creating HypergraphBrain..." << std::endl;
        auto brain = std::make_unique<HypergraphBrain>(connectivity_manager);
        std::cout << "HypergraphBrain created successfully" << std::endl;
        
        std::cout << "Test completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unknown exception occurred" << std::endl;
        return 1;
    }
}