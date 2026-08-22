#include "expression/LanguageExpressionCortex.h"
#include "vision/VisionPerceptionCortex.h"
#include <iostream>

/**
 * @file test_expression_vision.cpp
 * @brief Test Phase E1-E4: Language Expression and Vision
 */

int main() {
  std::cout << "\n==========================================" << std::endl;
  std::cout << " NeuroForge Expression & Vision Test     " << std::endl;
  std::cout << " Phases E1-E4 Verification               " << std::endl;
  std::cout << "==========================================" << std::endl;

  int passed = 0;
  int failed = 0;

  // ========== Phase E1: Language Expression Cortex ==========
  std::cout << "\n[E1] Language Expression Cortex Tests" << std::endl;

  NeuroForge::Expression::LanguageExpressionCortex lec;

  // Register vocabulary
  lec.registerVocabulary(1, "quantum mechanics", 0.9f);
  lec.registerVocabulary(2, "artificial intelligence", 0.95f);
  lec.registerVocabulary(3, "neural networks", 0.85f);

  // Test 1: Simple description
  {
    auto intent = NeuroForge::Expression::ExpressionIntent::describe(1);
    auto plan = lec.express(intent);
    std::string text = plan.render();

    bool ok = plan.canExpress() &&
              text.find("quantum mechanics") != std::string::npos;
    std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] Description: " << text
              << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 2: Answer with multiple concepts
  {
    auto intent =
        NeuroForge::Expression::ExpressionIntent::answer("What is AI?", {2, 3});
    auto plan = lec.express(intent);
    std::string text = plan.render();

    bool ok = plan.canExpress() &&
              text.find("artificial intelligence") != std::string::npos;
    std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] Answer: " << text
              << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 3: Normative filter (blocked content)
  {
    lec.registerVocabulary(99, "medical advice for patient", 0.9f);
    auto intent = NeuroForge::Expression::ExpressionIntent::describe(99);
    auto plan = lec.express(intent);

    bool ok = plan.status == NeuroForge::Expression::NormativeStatus::BLOCKED;
    std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] Blocked content detected"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 4: Expression history (audit)
  {
    auto history = lec.getHistory();
    bool ok = history.size() >= 3; // We made 3 expressions
    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] Audit history: " << history.size() << " entries"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // ========== Phase E3: Video Vision ==========
  std::cout << "\n[E3] Video Vision Tests" << std::endl;

  NeuroForge::Vision::VisionPerceptionCortex vpc;

  // Test 5: Process video caption
  {
    auto obs = NeuroForge::Vision::VideoObservation::fromCaption(
        "abc123", "The transformer architecture revolutionized NLP", 1000);
    obs.video_title = "Machine Learning Tutorial";

    auto concepts = vpc.processVideo(obs);
    bool ok = !concepts.empty() &&
              concepts[0].label.find("transformer") != std::string::npos;
    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] Video caption processed: " << concepts.size() << " concepts"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 6: Block explicit video
  {
    NeuroForge::Vision::VideoObservation obs;
    obs.video_id = "xyz789";
    obs.video_title = "NSFW explicit content"; // Should be blocked

    auto concepts = vpc.processVideo(obs);
    bool ok = concepts.empty(); // Should be blocked
    std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] Explicit video blocked"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // ========== Phase E4: Camera Vision ==========
  std::cout << "\n[E4] Camera Vision Tests" << std::endl;

  // Test 7: Process safe camera observation
  {
    NeuroForge::Vision::CameraObservation obs;
    obs.frame_id = 1;
    obs.objects.push_back({"book", "textbook", 0.9f, false});
    obs.objects.push_back({"desk", "", 0.85f, false});

    auto concepts = vpc.processCamera(obs);
    bool ok = obs.canProcess() && concepts.size() == 2;
    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] Safe camera: " << concepts.size() << " objects"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 8: Block child detection
  {
    NeuroForge::Vision::CameraObservation obs;
    obs.frame_id = 2;
    obs.objects.push_back({"child", "", 0.95f, false});

    auto concepts = vpc.processCamera(obs);
    bool ok = obs.privacy_level == NeuroForge::Vision::PrivacyLevel::BLOCKED;
    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] Child detected - frame blocked" << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 9: Anonymize faces
  {
    NeuroForge::Vision::CameraObservation obs;
    obs.frame_id = 3;
    obs.objects.push_back({"face", "", 0.9f, false});
    obs.objects.push_back({"laptop", "", 0.85f, false});

    auto concepts = vpc.processCamera(obs);
    bool ok = obs.privacy_level == NeuroForge::Vision::PrivacyLevel::ANONYMIZED;
    std::cout << "  [" << (ok ? "PASS" : "FAIL") << "] Face anonymized"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // ========== Summary ==========
  std::cout << "\n==========================================" << std::endl;
  std::cout << "  RESULTS: " << passed << " passed, " << failed << " failed"
            << std::endl;
  std::cout << "==========================================" << std::endl;

  std::cout << "\nStatistics:" << std::endl;
  std::cout << "  LEC Vocabulary: " << lec.vocabularySize() << " entries"
            << std::endl;
  std::cout << "  LEC History: " << lec.getHistory().size() << " expressions"
            << std::endl;
  std::cout << "  Video Observations: " << vpc.videoObservations() << std::endl;
  std::cout << "  Camera Observations: " << vpc.cameraObservations()
            << std::endl;
  std::cout << "  Blocked Frames: " << vpc.blockedFrames() << std::endl;

  return failed == 0 ? 0 : 1;
}
