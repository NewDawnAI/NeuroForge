/**
 * @file test_survival_bias.cpp
 * @brief Unit test for SurvivalBias modulation and metrics
 *
 * Validates that SurvivalBias reduces coherence under risk and responds to
 * variance sensitivity configuration changes.
 */

#include "biases/SurvivalBias.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

using NeuroForge::Biases::SurvivalBias;

namespace {
    bool nearlyEqual(float a, float b, float eps = 1e-5f) {
        return std::fabs(a - b) <= eps;
    }
}

class SurvivalBiasTestSuite {
public:
    bool runAllTests() {
        std::cout << "=== NeuroForge SurvivalBias Test Suite ===\n\n";

        bool all_passed = true;
        all_passed &= testBasicModulation();
        all_passed &= testVarianceSensitivityEffect();
        all_passed &= testHazardProbabilitySignal();
        all_passed &= testDynamicScalingResponds();
        all_passed &= testDynamicScalingClamp();

        std::cout << "\n=== Test Suite Summary ===\n";
        if (all_passed) {
            std::cout << "✅ All tests PASSED!\n";
        } else {
            std::cout << "❌ Some tests FAILED!\n";
        }
        return all_passed;
    }

private:
    bool testBasicModulation() {
        std::cout << "Test 1: Basic coherence down-modulation under risk... ";

        try {
            SurvivalBias::Config cfg;
            cfg.hazard_threshold = 0.7f;
            cfg.hazard_coherence_weight = 0.2f;
            cfg.variance_sensitivity = 1.0f;
            SurvivalBias bias(cfg);

            // High-variance activation pattern with spike above threshold
            std::vector<float> pattern = {0.1f, 0.95f, 0.05f, 0.9f, 0.02f, 0.85f, 0.1f, 0.8f};
            float base = 0.9f;
            float modulated = bias.applyCoherenceBias(base, pattern);

            auto m = bias.getLastMetrics();
            bool ok = (modulated < base) && (m.risk_score > 0.0f);
            if (!ok) {
                std::cout << "FAILED (base=" << base
                          << ", modulated=" << std::setprecision(6) << modulated
                          << ", risk=" << std::setprecision(6) << m.risk_score << ")\n";
                return false;
            }

            // Low-variance activation pattern below threshold should yield smaller (or zero) modulation
            std::vector<float> pattern_low = {0.3f, 0.35f, 0.32f, 0.33f, 0.31f, 0.34f, 0.33f, 0.32f};
            float modulated_low = bias.applyCoherenceBias(base, pattern_low);
            auto m_low = bias.getLastMetrics();

            bool ok2 = (modulated_low <= base) && (m_low.risk_score <= m.risk_score);
            if (!ok2) {
                std::cout << "FAILED (expected lower risk for low-variance pattern; got risk="
                          << m_low.risk_score << ")\n";
                return false;
            }

            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }

    bool testVarianceSensitivityEffect() {
        std::cout << "Test 2: Variance sensitivity increases modulation on same pattern... ";

        try {
            SurvivalBias::Config cfg;
            cfg.hazard_threshold = 0.7f;
            cfg.hazard_coherence_weight = 0.2f;
            cfg.variance_sensitivity = 0.5f; // start lower
            SurvivalBias bias(cfg);

            // Same pattern used with different variance_sensitivity
            std::vector<float> pattern = {0.1f, 0.95f, 0.05f, 0.9f, 0.02f, 0.85f, 0.1f, 0.8f};
            float base = 0.9f;

            float mod_low = bias.applyCoherenceBias(base, pattern);
            auto m_low = bias.getLastMetrics();

            cfg.variance_sensitivity = 2.0f; // increase sensitivity
            bias.updateConfig(cfg);

            float mod_high = bias.applyCoherenceBias(base, pattern);
            auto m_high = bias.getLastMetrics();

            bool ok = (mod_high < mod_low) && (m_high.risk_score >= m_low.risk_score);
            if (!ok) {
                std::cout << "FAILED (expected stronger down-modulation; mod_low="
                          << std::setprecision(6) << mod_low << ", mod_high="
                          << std::setprecision(6) << mod_high << ")\n";
                return false;
            }

            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }

    bool testHazardProbabilitySignal() {
        std::cout << "Test 3: Hazard probability reflects spike relative to threshold... ";

        try {
            SurvivalBias::Config cfg;
            cfg.hazard_threshold = 0.8f;
            cfg.hazard_coherence_weight = 0.2f;
            cfg.variance_sensitivity = 1.0f;
            SurvivalBias bias(cfg);

            // Below-threshold spike
            std::vector<float> pattern_low = {0.1f, 0.6f, 0.4f, 0.5f, 0.7f};
            bias.analyze(pattern_low);
            auto m_low = bias.getLastMetrics();

            // Above-threshold spike
            std::vector<float> pattern_high = {0.1f, 0.95f, 0.4f, 0.5f, 0.7f};
            bias.analyze(pattern_high);
            auto m_high = bias.getLastMetrics();

            bool ok = (m_high.hazard_probability > m_low.hazard_probability);
            if (!ok) {
                std::cout << "FAILED (hazard prob not increasing: low="
                          << std::setprecision(6) << m_low.hazard_probability
                          << ", high=" << std::setprecision(6) << m_high.hazard_probability << ")\n";
                return false;
            }

            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }

    bool testDynamicScalingResponds() {
        std::cout << "Test 4: Adaptive scaling responds to external hazard (alpha) ... ";

        try {
            SurvivalBias::Config cfg;
            cfg.hazard_threshold = 0.7f;
            cfg.hazard_coherence_weight = 0.2f;
            cfg.hazard_alpha = 1.0f; // scale entirely by external hazard
            cfg.hazard_beta = 0.0f;
            cfg.variance_sensitivity = 1.0f;
            SurvivalBias bias(cfg);

            // Consistent high-risk pattern
            std::vector<float> pattern = {0.1f, 0.95f, 0.05f, 0.9f, 0.02f, 0.85f, 0.1f, 0.8f};
            float base = 0.9f;

            // Low external hazard -> scale ~0 => minimal modulation
            bias.setExternalHazard(0.0f);
            float mod_lowH = bias.applyCoherenceBias(base, pattern);
            auto w_lowH = bias.getLastAppliedWeight();

            // High external hazard -> scale ~1 => full baseline modulation
            bias.setExternalHazard(1.0f);
            float mod_highH = bias.applyCoherenceBias(base, pattern);
            auto w_highH = bias.getLastAppliedWeight();

            bool ok = (mod_highH < mod_lowH) && nearlyEqual(w_highH, cfg.hazard_coherence_weight);
            if (!ok) {
                std::cout << "FAILED (mod_lowH=" << std::setprecision(6) << mod_lowH
                          << ", mod_highH=" << std::setprecision(6) << mod_highH
                          << ", w_lowH=" << std::setprecision(6) << w_lowH
                          << ", w_highH=" << std::setprecision(6) << w_highH << ")\n";
                return false;
            }
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }

    bool testDynamicScalingClamp() {
        std::cout << "Test 5: Adaptive scaling clamps to [0,1] under alpha+beta > 1 ... ";

        try {
            SurvivalBias::Config cfg;
            cfg.hazard_threshold = 0.6f;
            cfg.hazard_coherence_weight = 0.3f;
            cfg.hazard_alpha = 0.8f;
            cfg.hazard_beta = 0.5f; // alpha+beta may exceed 1; ensures clamp
            cfg.variance_sensitivity = 1.0f;
            SurvivalBias bias(cfg);

            std::vector<float> pattern = {0.2f, 0.9f, 0.7f, 0.85f, 0.1f, 0.8f};
            float base = 0.9f;

            // Build up arousal via repeated high-risk analyses to approximate arousal -> 1
            bias.setExternalHazard(1.0f);
            for (int i = 0; i < 5; ++i) {
                (void)bias.applyCoherenceBias(base, pattern);
            }
            float mod = bias.applyCoherenceBias(base, pattern);
            auto w_applied = bias.getLastAppliedWeight();

            // Clamp ensures applied weight <= base weight
            bool ok = (w_applied <= cfg.hazard_coherence_weight) && (mod <= base);
            if (!ok) {
                std::cout << "FAILED (w_applied=" << std::setprecision(6) << w_applied
                          << ", base_weight=" << std::setprecision(6) << cfg.hazard_coherence_weight
                          << ", modulated=" << std::setprecision(6) << mod
                          << ", base=" << std::setprecision(6) << base << ")\n";
                return false;
            }
            std::cout << "PASSED\n";
            return true;
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
};

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    SurvivalBiasTestSuite suite;
    bool ok = suite.runAllTests();
    return ok ? 0 : 1;
}