#include "runtime/WebSandboxRunner.h"
#include "biases/NoveltyBias.h"
#include "core/LanguageSystem.h"
#include "core/MemoryDB.h"
#include "language/LanguageAcquisitionLoop.h"
#include "perception/world/WorldModelCortex.h"
#include "sandbox/WebSandbox.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace NeuroForge {
namespace Runtime {

int runWebSandbox(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                  NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
                  bool log_json, const WebSandboxSinks &sinks,
                  const WebSandboxConfig &cfg) {

  (void)sinks;
  (void)memdb_run_id;
  (void)log_json;
  (void)memdb;

  std::cout << "[Web] Initializing Internal Language System...\n";
  NeuroForge::Core::LanguageSystem::Config ls_cfg;
  ls_cfg.max_vocabulary_size = 50000;
  ls_cfg.embedding_dimension = 64; // Match WorldModel input
  auto language_system =
      std::make_unique<NeuroForge::Core::LanguageSystem>(ls_cfg);
  if (!language_system->initialize()) {
    std::cerr << "[Web] Failed to initialize LanguageSystem.\n";
    return 1;
  }

  std::cout << "[Web] Starting Language Acquisition Loop...\n";
  NeuroForge::Language::AcquisitionConfig acq_cfg;
  acq_cfg.enable_direct_tokenization = true;
  acq_cfg.min_occurrences_to_learn =
      1; // Aggressive learning for rapid bootstrapping
  // We don't have a full LivePerceptionLoop here yet, so we pass nullptr and
  // drive it manually via processWebContent.
  // Note: LanguageAcquisitionLoop typically expects a perception loop, but we
  // can bypass it for text-only direct feed.
  auto acq_loop =
      std::make_unique<NeuroForge::Language::LanguageAcquisitionLoop>(
          language_system.get(), nullptr, nullptr, nullptr, acq_cfg);

  if (!acq_loop->initialize()) {
    std::cerr << "[Web] Failed to initialize AcquisitionLoop.\n";
    return 1;
  }
  acq_loop->start(); // Sets running flag

  std::cout << "[Web] Creating Body (WebView2 Sandboxed Window)...\n";
  NeuroForge::Sandbox::WebSandbox sandbox;
  if (!sandbox.create(cfg.window_width, cfg.window_height,
                      "NeuroForge Observer")) {
    std::cerr
        << "[Web] Failed to create sandbox window. Is WebView2 installed?\n";
    return 1;
  }

  std::cout << "[Web] Navigating to seed: " << cfg.start_url << "\n";
  sandbox.navigate(cfg.start_url);

  std::ofstream log_csv(cfg.log_csv_path);
  if (log_csv.is_open()) {
    log_csv << "step,url,text_len,vocab_size,embedding_norm,surprise\n";
  }

  std::uint64_t step = 0;
  int idle_poll_count = 0;

  // The Observer Loop
  while (sandbox.isOpen()) {
    sandbox.poll();

    // Attempt extraction
    if (idle_poll_count % 20 == 0) { // Approx every 400ms
      sandbox.extractPageContent();
    }

    std::string current_url = sandbox.getCurrentUrl();
    std::string page_text = sandbox.getPageText();

    // Update Brain
    if (!page_text.empty()) {
      // 1. Feed the Language Acquisition Loop (Learning)
      acq_loop->processWebContent(page_text, current_url);

      // 2. Encode (Inference)
      // Retrieve embeddings for the known tokens in the text
      std::vector<float> linguistic_input(ls_cfg.embedding_dimension, 0.0f);
      int token_count = 0;

      // Simple whitespace tokenizer for inference (should match AcqLoop's logic
      // ideally)
      std::string current_token;
      auto process_token = [&](const std::string &t) {
        if (t.length() < 3)
          return;
        auto *token_ptr = language_system->getToken(t);
        if (token_ptr) {
          const auto &emb = token_ptr->embedding;
          if (emb.size() == linguistic_input.size()) {
            for (size_t i = 0; i < emb.size(); ++i) {
              linguistic_input[i] += emb[i];
            }
            token_count++;
          }
        }
      };

      for (char c : page_text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-') {
          current_token += static_cast<char>(std::tolower(static_cast<int>(c)));
        } else {
          if (!current_token.empty())
            process_token(current_token);
          current_token.clear();
        }
      }
      if (!current_token.empty())
        process_token(current_token);

      // Average pooling
      if (token_count > 0) {
        float scale =
            1.0f / std::sqrt(static_cast<float>(
                       token_count)); // Sqrt scaling often better for variance
        // But strict average is 1/N. Let's use 1/N for semantic centroid.
        scale = 1.0f / static_cast<float>(token_count);

        float l_norm_sq = 0.0f;
        for (float &f : linguistic_input) {
          f *= scale;
          l_norm_sq += f * f;
        }
        // Normalize the final vector to unit sphere for the cortex
        float l_norm = std::sqrt(l_norm_sq);
        if (l_norm > 1e-6f) {
          for (float &f : linguistic_input)
            f /= l_norm;
        }
      }

      // 3. Integrate Bias (Novelty/Curiosity)
      static NeuroForge::Biases::NoveltyBias::Config nb_cfg;
      static NeuroForge::Biases::NoveltyBias novelty_bias(nb_cfg);

      auto novelty_metrics = novelty_bias.calculateNovelty(linguistic_input);
      novelty_bias.updateExperienceBuffer(linguistic_input);

      // 4. Perceive (Observe Cycle)
      // WorldModelCortex::processCycle(visual, motion, temporal, social,
      // spatial, auditory, linguistic)
      world_model_cortex.processCycle({}, {}, {}, {}, {}, {}, linguistic_input);

      float surprise = world_model_cortex.getLastSurpriseLevel();
      size_t vocab_size = language_system->getActiveTokens(0.0f).size();

      if (log_csv.is_open() && step % 10 == 0) {
        float l_norm = 0.0f; // Re-calc if needed or track above
        log_csv << step << "," << current_url << "," << page_text.length()
                << "," << vocab_size << "," << l_norm << "," << surprise
                << "\n";
      }

      if (step % 50 == 0) {
        std::cout << "[Web] Step " << step
                  << " | Reading: " << page_text.length() << " chars"
                  << " | Vocab: " << vocab_size << " | Tokens: " << token_count
                  << " | Novelty: " << novelty_metrics.surprise_level
                  << " | Bonus: " << novelty_metrics.exploration_bonus << "\n";
      }

      // Attention Modulator (Simple Boredom Mechanism)
      static float boredom_level = 0.0f;
      float interest = novelty_metrics.exploration_bonus;

      if (interest < 0.1f) {
        boredom_level += 1.0f;
      } else {
        boredom_level = std::max(0.0f, boredom_level - 0.5f);
      }

      if (boredom_level > 20.0f) { // ~2 seconds of low interest
        std::cout << "[Web] Bored (Level " << boredom_level
                  << ")! Seeking novelty...\n";
        sandbox.navigate("https://en.wikipedia.org/wiki/Special:Random");
        boredom_level = 0.0f;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      } else {
        // If interested, read more
        if (interest > 0.3f) {
          sandbox.scroll(100);
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    step++;
    idle_poll_count++;

    // Manual brake for testing
    if (step > 10000)
      break;
  }

  std::cout << "[Web] Session ended.\n";
  acq_loop->stop();
  language_system->shutdown();
  return 0;
}

} // namespace Runtime
} // namespace NeuroForge
