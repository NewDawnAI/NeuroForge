#include <iostream>
#include <cassert>
#include <vector>

int main() {
    std::cout << "Starting minimal test..." << std::endl;
    
    try {
        // Test basic vector operations
        std::vector<float> test_vec = {1.0f, 2.0f, 3.0f};
        std::cout << "Vector created successfully" << std::endl;
        
        // Test basic assertions
        assert(test_vec.size() == 3);
        assert(test_vec[0] == 1.0f);
        std::cout << "Assertions passed" << std::endl;
        
        std::cout << "All basic tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        return 1;
    }
}