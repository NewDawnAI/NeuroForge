#include <iostream>
#include <cassert>
#include "biases/AttachmentBias.h"

using namespace NeuroForge::Biases;

int main() {
    std::cout << "Testing AttachmentBias constructor..." << std::endl;
    
    try {
        // Test default constructor
        AttachmentBias::Config config;
        std::cout << "Config created successfully" << std::endl;
        
        AttachmentBias attachment(config);
        std::cout << "AttachmentBias created successfully" << std::endl;
        
        // Test basic functionality
        auto metrics = attachment.calculateAttachmentMetrics();
        std::cout << "Metrics calculated successfully" << std::endl;
        
        std::cout << "✅ Simple test passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception" << std::endl;
        return 1;
    }
}