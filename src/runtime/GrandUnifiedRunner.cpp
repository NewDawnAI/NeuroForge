#include "runtime/GrandUnifiedRunner.h"

// ── Subsystem Includes ──
#include "audio/AudioInputSystem.h"
#include "audio/AudioOutputSystem.h"
#include "biases/NoveltyBias.h"
#include "core/BrainPersistence.h"
#include "core/CompositionMetrics.h"
#include "core/LanguageSystem.h"
#include "core/MemoryDB.h"
#include "core/Phase7Reflection.h"
#include "core/Phase8GoalSystem.h"
#include "core/Phase9Metacognition.h"
#include "core/SelfModel.h"
#include "core/UniversalLearningSignal.h"
#include "language/LanguageAcquisitionLoop.h"
#include "memory/DreamProcessor.h"
#include "memory/EpisodicMemoryManager.h"
#include "memory/ProceduralMemory.h"
#include "memory/SemanticMemory.h"
#include "memory/SleepConsolidation.h"
#include "perception/world/WorldModelCortex.h"
#include "sandbox/GridWorld.h"
#include "sandbox/LinearActionModel.h"
#include "sandbox/LinearLatentDecoder.h"
#include "sandbox/WebSandbox.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#ifdef NF_HAVE_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#endif

namespace NeuroForge {
namespace Runtime {

// ────────────────────────────────────────────────────────────────────
// Helper: tokenize text and produce avg-pooled embedding
// ────────────────────────────────────────────────────────────────────
static void encode_text(const std::string &text,
                        NeuroForge::Core::LanguageSystem &lang,
                        std::vector<float> &out) {
  std::fill(out.begin(), out.end(), 0.0f);
  int token_count = 0;
  std::string tok;

  auto flush = [&](const std::string &t) {
    if (t.length() < 3)
      return;
    auto *tp = lang.getToken(t);
    if (!tp)
      return;
    const auto &emb = tp->embedding;
    if (emb.size() != out.size())
      return;
    for (std::size_t i = 0; i < emb.size(); ++i)
      out[i] += emb[i];
    ++token_count;
  };

  for (char c : text) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-') {
      tok += static_cast<char>(std::tolower(static_cast<int>(c)));
    } else {
      if (!tok.empty())
        flush(tok);
      tok.clear();
    }
  }
  if (!tok.empty())
    flush(tok);

  if (token_count > 0) {
    float scale = 1.0f / static_cast<float>(token_count);
    float norm_sq = 0.0f;
    for (float &f : out) {
      f *= scale;
      norm_sq += f * f;
    }
    float norm = std::sqrt(norm_sq);
    if (norm > 1e-6f) {
      for (float &f : out)
        f /= norm;
    }
  }
}

// ────────────────────────────────────────────────────────────────────
// The Genesis Loop
// ────────────────────────────────────────────────────────────────────
int runGrandUnified(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const GrandUnifiedSinks &sinks,
    const GrandUnifiedConfig &cfg, const std::atomic<bool> &abort_signal) {

  (void)sinks;
  (void)log_json;

  std::cout << "\n"
            << "╔══════════════════════════════════════════════╗\n"
            << "║     NEUROFORGE — GENESIS UNIFIED RUNNER      ║\n"
            << "║  Web + Grid + Language + Cognition + Memory  ║\n"
            << "╚══════════════════════════════════════════════╝\n\n";

  // ───────────────── 1. LANGUAGE SYSTEM ─────────────────
  std::cout << "[Genesis] Initializing Language System...\n";
  NeuroForge::Core::LanguageSystem::Config ls_cfg;
  ls_cfg.max_vocabulary_size = cfg.max_vocabulary;
  ls_cfg.embedding_dimension = cfg.embedding_dim;
  auto language_system =
      std::make_unique<NeuroForge::Core::LanguageSystem>(ls_cfg);
  if (!language_system->initialize()) {
    std::cerr << "[Genesis] FATAL: LanguageSystem init failed.\n";
    return 1;
  }

  NeuroForge::Language::AcquisitionConfig acq_cfg;
  acq_cfg.enable_direct_tokenization = true;
  acq_cfg.min_occurrences_to_learn = 1;
  auto acq_loop =
      std::make_unique<NeuroForge::Language::LanguageAcquisitionLoop>(
          language_system.get(), nullptr, nullptr, nullptr, acq_cfg);
  if (!acq_loop->initialize()) {
    std::cerr << "[Genesis] FATAL: AcquisitionLoop init failed.\n";
    return 1;
  }
  acq_loop->start();
  std::cout << "[Genesis] Language System ONLINE.\n";

  // ───────────────── 2. GRIDWORLD ─────────────────
  std::cout << "[Genesis] Spawning GridWorld (7x7)...\n";
  NeuroForge::Sandbox::GridWorld env(7, 7, cfg.seed);
  const std::size_t obs_dim = env.observe().size();
  NeuroForge::Sandbox::LinearActionModel obs_model(obs_dim, cfg.action_dim);
  NeuroForge::Sandbox::LinearLatentDecoder decoder(
      world_model_cortex.getCurrentState().latent.size(), 3);
  std::cout << "[Genesis] GridWorld ONLINE.\n";

  // ───────────────── 3. WEB SANDBOX (Optional) ─────────────────
  std::unique_ptr<NeuroForge::Sandbox::WebSandbox> sandbox;
  bool web_active = false;

  if (cfg.enable_web) {
    std::cout << "[Genesis] Creating WebView2 Sandbox...\n";
    sandbox = std::make_unique<NeuroForge::Sandbox::WebSandbox>();
    if (sandbox->create(cfg.window_width, cfg.window_height,
                        "NeuroForge Genesis")) {
      sandbox->navigate(cfg.start_url);
      web_active = true;
      std::cout << "[Genesis] Web Sandbox ONLINE → " << cfg.start_url << "\n";
    } else {
      std::cerr << "[Genesis] WebView2 unavailable — degrading to "
                   "GridWorld-only mode.\n";
      sandbox.reset();
    }
  } else {
    std::cout
        << "[Genesis] Web disabled (--no-webview). GridWorld-only mode.\n";
  }

  // ───────────────── 4. COGNITIVE STACK ─────────────────
  std::cout << "[Genesis] Wiring Cognitive Stack (Phase 7/8/9)...\n";

  // SelfModel
  std::unique_ptr<NeuroForge::Core::SelfModel> self_model;
  if (memdb) {
    self_model = std::make_unique<NeuroForge::Core::SelfModel>(*memdb);
    self_model->loadForRun(static_cast<std::int64_t>(memdb_run_id));
  }

  // Phase 8: Goal System
  std::shared_ptr<NeuroForge::Core::MemoryDB> memdb_shared;
  std::unique_ptr<NeuroForge::Core::Phase8GoalSystem> goal_system;
  if (memdb) {
    // Create a non-owning shared_ptr that won't delete memdb
    memdb_shared = std::shared_ptr<NeuroForge::Core::MemoryDB>(
        memdb, [](NeuroForge::Core::MemoryDB *) {});
    goal_system = std::make_unique<NeuroForge::Core::Phase8GoalSystem>(
        memdb_shared, static_cast<std::int64_t>(memdb_run_id));
  }

  // Phase 9: Metacognition
  std::unique_ptr<NeuroForge::Core::Phase9Metacognition> metacog;
  if (memdb) {
    metacog = std::make_unique<NeuroForge::Core::Phase9Metacognition>(
        memdb, static_cast<std::int64_t>(memdb_run_id));
  }

  // Phase 7: Reflection
  std::unique_ptr<NeuroForge::Core::Phase7Reflection> reflection;
  if (memdb) {
    reflection = std::make_unique<NeuroForge::Core::Phase7Reflection>(
        memdb, static_cast<std::int64_t>(memdb_run_id));
    // Wire cross-references
    if (goal_system)
      reflection->setPhase8Components(goal_system.get());
    if (metacog)
      reflection->setPhase9Metacognition(metacog.get());
    if (self_model)
      reflection->setSelfModel(self_model.get());
  }

  // Wire Phase 8 → Phase 9 / SelfModel
  if (goal_system) {
    if (metacog)
      goal_system->setPhase9Metacognition(metacog.get());
    if (self_model)
      goal_system->setSelfModel(self_model.get());
  }

  std::cout << "[Genesis] Cognitive Stack ONLINE.\n";

  // ───────────────── 4b. MEMORY SYSTEMS ─────────────────
  std::cout << "[Genesis] Initializing Memory Systems...\n";
  NeuroForge::Memory::EpisodicConfig episodic_cfg;
  episodic_cfg.pattern_dim = cfg.embedding_dim;
  auto episodic_memory =
      std::make_unique<NeuroForge::Memory::EpisodicMemoryManager>(episodic_cfg);

  NeuroForge::Memory::SemanticConfig semantic_cfg;
  semantic_cfg.concept_dim = cfg.embedding_dim;
  auto semantic_memory =
      std::make_unique<NeuroForge::Memory::SemanticMemory>(semantic_cfg);

  NeuroForge::Memory::ProceduralConfig procedural_cfg;
  procedural_cfg.context_dim = cfg.embedding_dim;
  procedural_cfg.action_dim = cfg.action_dim > 0 ? cfg.action_dim : 16;
  auto procedural_memory =
      std::make_unique<NeuroForge::Memory::ProceduralMemory>(procedural_cfg);

  NeuroForge::Memory::DreamProcessor::DreamConfig dream_cfg;
  auto dream_processor =
      std::make_unique<NeuroForge::Memory::DreamProcessor>(dream_cfg);
  dream_processor->registerEpisodicMemory(episodic_memory.get());
  dream_processor->registerSemanticMemory(semantic_memory.get());

  NeuroForge::Memory::ConsolidationConfig consol_cfg;
  auto sleep_consolidation =
      std::make_unique<NeuroForge::Memory::SleepConsolidation>(consol_cfg);
  sleep_consolidation->registerEpisodicMemory(episodic_memory.get());
  sleep_consolidation->registerSemanticMemory(semantic_memory.get());
  sleep_consolidation->registerProceduralMemory(procedural_memory.get());

  std::cout << "[Genesis] Memory Systems ONLINE (Episodic + Semantic + "
               "Procedural + Dreaming).\n";

  // ───────────────── 4c. BRAIN PERSISTENCE (AUTO-LOAD) ─────────────────
  std::uint64_t total_steps = 0;
  std::uint64_t episode_count = 0;

  if (cfg.auto_load) {
    auto load_result = NeuroForge::Core::BrainPersistence::loadState(
        cfg.brain_state_dir, *language_system, *episodic_memory,
        *semantic_memory, *procedural_memory);
    if (load_result.found && load_result.success) {
      total_steps = load_result.saved_step_count;
      std::cout << "[Genesis] \xe2\x9c\x93 Brain state restored from previous "
                   "session.\n";
    } else if (!load_result.found) {
      std::cout << "[Genesis] No prior brain state found — starting fresh.\n";
    } else {
      std::cerr << "[Genesis] WARNING: Failed to load brain state: "
                << load_result.error << "\n";
    }
  }

  // ───────────────── 5. BIAS SYSTEM ─────────────────
  NeuroForge::Biases::NoveltyBias::Config nb_cfg;
  NeuroForge::Biases::NoveltyBias novelty_bias(nb_cfg);

  // ───────────────── 5b. UNIVERSAL LEARNING SIGNAL ─────────────────
  std::cout << "[Genesis] Initializing Universal Learning Signal...\n";
  NeuroForge::Core::UniversalSignalConfig uls_cfg;
  uls_cfg.eta_base = 0.01f;
  uls_cfg.gamma = 0.99f;
  uls_cfg.alpha = 0.5f;
  uls_cfg.beta_sparse = 0.01f;
  NeuroForge::Core::UniversalLearningSignal universal_signal(uls_cfg);
  std::cout
      << "[Genesis] Universal Learning Signal ONLINE (Δw = η·δ·∇I_gain).\n";

  // ───────────────── 5c. COMPOSITION METRICS (Stage 3) ─────────────────
  NeuroForge::Core::CompositionConfig comp_cfg;
  comp_cfg.composition_threshold = 0.3f;
  comp_cfg.noise_sigma = 0.05f;
  NeuroForge::Core::CompositionMetrics composition_metrics(comp_cfg);
  std::cout << "[Genesis] Composition Metrics ONLINE (K(whole|parts) proxy).\n";
  dream_processor->registerCompositionMetrics(&composition_metrics);
  std::cout
      << "[Genesis] Phase 5: CompositionMetrics wired into DreamProcessor.\n";

  // ───────────────── 6. LOGGING ─────────────────
  std::ofstream log_csv(cfg.log_csv_path);
  if (log_csv.is_open()) {
    log_csv << "step,episode,vocab,surprise,novelty,boredom,web_chars,"
               "grid_action,goal_coherence,self_trust,"
               "delta,info_gain,eta_effective,reward_total\n";
  }

  auto now_ms = []() -> std::int64_t {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  };

  // ───────────────── 7. MAIN LOOP STATE ─────────────────
  std::uint64_t steps_in_episode = 0;
  std::uint32_t seed = cfg.seed;
  float boredom_level = 0.0f;
  bool needs_new_goal = true;
  bool is_sleeping = cfg.start_asleep;
  std::int64_t last_cycle_change = now_ms();

  std::vector<float> linguistic_input(cfg.embedding_dim, 0.0f);
  std::mutex ling_mutex;

  // ───────────────── 8. CONVERSATIONAL INTERFACE ─────────────────
  std::unique_ptr<NeuroForge::Audio::AudioInputSystem> audio_in;
  std::unique_ptr<NeuroForge::Audio::AudioOutputSystem> audio_out;
  if (cfg.conversational_mode) {
    std::cout
        << "[Genesis] Initializing Conversational Interface (STT/TTS)...\n";
    audio_in = std::make_unique<NeuroForge::Audio::AudioInputSystem>();
    audio_out = std::make_unique<NeuroForge::Audio::AudioOutputSystem>();

    if (audio_in->initialize() && audio_out->initialize()) {
      audio_in->start();
      audio_out->speak("NeuroForge audio interface online.");
      std::cout << "[Genesis] Conversational Interface ONLINE.\n";
    } else {
      std::cerr << "[Genesis] WARNING: Failed to initialize audio components. "
                   "Chat disabled.\n";
      audio_in.reset();
      audio_out.reset();
    }
  }

  std::int64_t last_decay_ms = now_ms();

  std::cout << "\n[Genesis] ═══ ENTERING MAIN LOOP ═══\n"
            << "[Genesis] Mode: "
            << (cfg.infinite_mode ? "INFINITE" : "BOUNDED")
            << " | Web: " << (web_active ? "ON" : "OFF")
            << " | Cognition: " << (memdb ? "ON" : "OFF") << " | Threading: "
            << (cfg.enable_parallel ? "PARALLEL" : "SEQUENTIAL") << "\n\n";

  // ════════════════════════════════════════════════════════════════
  //  PARALLEL CLOSED LOOP (Stage 2)
  // ════════════════════════════════════════════════════════════════
  if (cfg.enable_parallel) {
    // ── Shared State (atomic for lock-free cross-thread reads) ──
    struct SharedState {
      std::atomic<float> surprise{0.0f};
      std::atomic<float> info_gain{0.0f};
      std::atomic<float> interest{0.0f};
      std::atomic<float> eta_effective{0.01f};
      std::atomic<float> boredom{0.0f};
      std::atomic<std::uint64_t> total_steps{0};
      std::atomic<std::uint64_t> episode_count{0};
      std::atomic<bool> needs_new_goal{true};
      std::atomic<bool> is_sleeping{false}; // Circadian Rhythm (Phase 3)
    } shared;

    // Seed shared state from persistence
    shared.total_steps.store(total_steps, std::memory_order_relaxed);
    shared.episode_count.store(episode_count, std::memory_order_relaxed);
    shared.is_sleeping.store(cfg.start_asleep, std::memory_order_relaxed);

    // Thread-safe log
    std::mutex log_mutex;

    auto hz_to_ms = [](int hz) -> int { return hz > 0 ? 1000 / hz : 1000; };

    // ── THREAD 1: Perception (WorldModel + Universal Signal) ──
    std::thread t_perception([&]() {
      std::cout << "[Genesis/T1] Perception thread started @ "
                << cfg.perception_hz << "Hz\n";
      auto interval = std::chrono::milliseconds(hz_to_ms(cfg.perception_hz));
      std::vector<float> vis_input;

#ifdef NF_HAVE_OPENCV
      cv::VideoCapture cap;
      if (cfg.enable_camera) {
        cap.open(0);
        if (!cap.isOpened()) {
          std::cerr << "[Genesis/T1] Warning: Failed to open camera 0\n";
        } else {
          std::cout << "[Genesis/T1] Camera 0 opened successfully for real "
                       "vision grounding.\n";
        }
      }
#endif

      while (!abort_signal.load()) {
        if (shared.is_sleeping.load(std::memory_order_relaxed)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue; // Pause simulation
        }
        auto obs = env.observe();
        vis_input.assign(obs.begin(), obs.end());

#ifdef NF_HAVE_OPENCV
        if (cfg.enable_camera && cap.isOpened()) {
          cv::Mat frame;
          cap >> frame;
          if (!frame.empty()) {
            cv::Mat resized, gray;
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
            cv::resize(
                gray, resized,
                cv::Size(8, 8)); // 64 pixels to match visual_input_dim = 64
            resized.convertTo(resized, CV_32F, 1.0 / 255.0);
            if (resized.isContinuous()) {
              vis_input.assign((float *)resized.datastart,
                               (float *)resized.dataend);
            }
          }
        }
#endif

        std::vector<float> current_ling;
        {
          std::lock_guard<std::mutex> lk(ling_mutex);
          current_ling = linguistic_input;
        }
        world_model_cortex.processCycle(vis_input, {}, {}, {}, {}, {},
                                        current_ling);

        float s = world_model_cortex.getLastSurpriseLevel();
        shared.surprise.store(s, std::memory_order_relaxed);

        // Universal signal compute
        auto nm = novelty_bias.calculateNovelty(current_ling);
        novelty_bias.updateExperienceBuffer(current_ling);
        shared.interest.store(nm.exploration_bonus, std::memory_order_relaxed);
        shared.info_gain.store(nm.information_gain, std::memory_order_relaxed);

        float task_r = shared.needs_new_goal.load() ? -0.01f : 0.0f;
        auto usig =
            universal_signal.compute(s, nm.information_gain, task_r, 0.05f);
        shared.eta_effective.store(usig.eta_effective,
                                   std::memory_order_relaxed);

        shared.total_steps.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(interval);
      }
      std::cout << "[Genesis/T1] Perception thread stopped.\n";
    });

    // ── THREAD 2: Grid Motor (GridWorld actions) ──
    std::thread t_grid([&]() {
      std::cout << "[Genesis/T2] Grid motor thread started @ " << cfg.grid_hz
                << "Hz\n";
      auto interval = std::chrono::milliseconds(hz_to_ms(cfg.grid_hz));
      std::uint32_t local_seed = cfg.seed;
      std::uint64_t steps_in_ep = 0;
      while (!abort_signal.load()) {
        if (shared.is_sleeping.load(std::memory_order_relaxed)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue; // Pause motor outputs
        }
        if (shared.needs_new_goal.load(std::memory_order_relaxed)) {
          shared.episode_count.fetch_add(1, std::memory_order_relaxed);
          local_seed += 13;
          env.reset(local_seed);
          steps_in_ep = 0;
          shared.needs_new_goal.store(false, std::memory_order_relaxed);
        }

        auto action_idx = env.greedyActionToGoal();
        auto result = env.step(static_cast<NeuroForge::Sandbox::GridAction>(
            static_cast<int>(action_idx)));

        steps_in_ep++;
        if (result.reached_goal || steps_in_ep > 200) {
          shared.needs_new_goal.store(true, std::memory_order_relaxed);
        }

        // Logging (every 50 steps)
        std::uint64_t ts = shared.total_steps.load(std::memory_order_relaxed);
        if (ts % 50 == 0 && log_csv.is_open()) {
          std::lock_guard<std::mutex> lg(log_mutex);
          std::size_t vocab_sz = language_system->getActiveTokens(0.0f).size();
          double coh = goal_system ? goal_system->getLastCoherence() : 0.0;
          double trust_val = metacog ? metacog->getSelfTrust() : 0.5;
          log_csv << ts << "," << shared.episode_count.load() << "," << vocab_sz
                  << "," << shared.surprise.load() << ","
                  << shared.info_gain.load() << "," << shared.boredom.load()
                  << ",0," << static_cast<int>(action_idx) << "," << coh << ","
                  << trust_val << "," << universal_signal.getLatest().delta
                  << "," << universal_signal.getLatest().info_gain << ","
                  << universal_signal.getLatest().eta_effective << ","
                  << universal_signal.getLatest().reward_total << "\n";
        }

        std::this_thread::sleep_for(interval);
      }
      std::cout << "[Genesis/T2] Grid motor thread stopped.\n";
    });

    // ── THREAD 3: Web Language Acquisition ──
    std::thread t_web([&]() {
      if (!web_active || !sandbox) {
        std::cout << "[Genesis/T3] Web thread skipped (no sandbox).\n";
        return;
      }
      std::cout << "[Genesis/T3] Web acquisition thread started @ "
                << cfg.web_hz << "Hz\n";
      auto interval = std::chrono::milliseconds(hz_to_ms(cfg.web_hz));
      while (!abort_signal.load()) {
        if (shared.is_sleeping.load(std::memory_order_relaxed)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue; // Pause web scrolling/reading
        }
        sandbox->poll();
        sandbox->extractPageContent();
        std::string page_text = sandbox->getPageText();
        if (!page_text.empty()) {
          std::string url = sandbox->getCurrentUrl();
          acq_loop->processWebContent(page_text, url);
          std::vector<float> local_ling;
          encode_text(page_text, *language_system, local_ling);
          {
            std::lock_guard<std::mutex> lk(ling_mutex);
            linguistic_input = local_ling;
          }
        }

        // Boredom-driven navigation
        float cur_interest = shared.interest.load(std::memory_order_relaxed);
        float bored = shared.boredom.load(std::memory_order_relaxed);
        if (cur_interest < 0.1f) {
          shared.boredom.store(bored + 1.0f, std::memory_order_relaxed);
        } else {
          shared.boredom.store(std::max(0.0f, bored - 0.5f),
                               std::memory_order_relaxed);
        }
        if (bored > 30.0f) {
          sandbox->navigate("https://en.wikipedia.org/wiki/Special:Random");
          shared.boredom.store(0.0f, std::memory_order_relaxed);
        } else if (cur_interest > 0.3f) {
          sandbox->scroll(100);
        }

        std::this_thread::sleep_for(interval);
      }
      std::cout << "[Genesis/T3] Web acquisition thread stopped.\n";
    });

    // ── THREAD 4: Cognition (Memory Recall + Reflection + Goals + Metacog) ──
    std::thread t_cognition([&]() {
      std::cout << "[Genesis/T4] Cognition thread started @ "
                << cfg.cognition_hz << "Hz"
                << (memdb ? " (with MemoryDB)"
                          : " (no MemoryDB — memory recall only)")
                << "\n";
      auto interval = std::chrono::milliseconds(hz_to_ms(cfg.cognition_hz));
      std::int64_t last_decay = now_ms();
      while (!abort_signal.load()) {
        if (shared.is_sleeping.load(std::memory_order_relaxed)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue; // Pause goal decay and reflection during sleep
        }
        std::uint64_t ep = shared.episode_count.load(std::memory_order_relaxed);

        // ── Parallel Memory Recall ──
        // Run episodic/semantic/procedural recall on this thread,
        // offloading from the main loop. Each memory system is mutex-protected.
        {
          std::vector<float> current_ling;
          {
            std::lock_guard<std::mutex> lk(ling_mutex);
            current_ling = linguistic_input;
          }

          // Episodic: pattern-completion recall with current context as cue
          if (episodic_memory && !current_ling.empty()) {
            episodic_memory->recallByCue(current_ling, 3);
          }

          // Semantic: settle into nearest attractor basin for current input
          if (semantic_memory && !current_ling.empty()) {
            semantic_memory->settle(current_ling);
          }

          // Procedural: recall action weights for current context
          if (procedural_memory && !current_ling.empty()) {
            procedural_memory->recall(current_ling);
          }
        }

        // Phase 7: Reflection (requires memdb)
        if (reflection &&
            ep % static_cast<std::uint64_t>(cfg.reflect_every_n_episodes) ==
                0) {
          float cur_interest = shared.interest.load(std::memory_order_relaxed);
          float cur_surprise = shared.surprise.load(std::memory_order_relaxed);
          double contradiction_rate = 1.0 - static_cast<double>(cur_interest);
          double avg_reward = cur_interest > 0.5 ? 1.0 : -0.1;
          reflection->maybeReflect(static_cast<std::int64_t>(ep),
                                   contradiction_rate, avg_reward,
                                   static_cast<double>(cur_interest),
                                   static_cast<double>(cur_surprise));
        }

        // Phase 9: Metacognition (requires memdb)
        if (metacog && goal_system) {
          double actual_coh = goal_system->getLastCoherence();
          metacog->resolveActuals(actual_coh, 0.0);
        }

        // Phase 8: Goal decay (requires memdb)
        if (goal_system) {
          std::int64_t now = now_ms();
          double dt = static_cast<double>(now - last_decay) / 1000.0;
          if (dt > 0.1) {
            goal_system->decayStability(dt);
            last_decay = now;
          }
        }

        std::this_thread::sleep_for(interval);
      }
      std::cout << "[Genesis/T4] Cognition thread stopped.\n";
    });

    // ── THREAD 5: Consolidation (dream/noise injection) ──
    // ── THREAD 5: Consolidation (Circadian Rhythm + Dream) ──
    std::thread t_consolidation([&]() {
      std::cout << "[Genesis/T5] Consolidation thread started (Wake/Sleep "
                   "Cycle Control)\n";
      std::int64_t last_cycle_change = now_ms();

      while (!abort_signal.load()) {
        bool is_sleeping = shared.is_sleeping.load(std::memory_order_relaxed);
        std::int64_t now = now_ms();
        double seconds_in_state =
            static_cast<double>(now - last_cycle_change) / 1000.0;

        if (!is_sleeping && seconds_in_state >= cfg.wake_duration_s) {
          std::cout << "\n[Genesis/T5] === FALLING ASLEEP ("
                    << cfg.sleep_duration_s << "s) ===\n";
          if (audio_out)
            audio_out->speak(
                "I am feeling tired. Falling asleep to process memories.");
          shared.is_sleeping.store(true, std::memory_order_relaxed);
          last_cycle_change = now;
        } else if (is_sleeping && seconds_in_state >= cfg.sleep_duration_s) {
          std::cout << "\n[Genesis/T5] === WAKING UP ===\n";
          if (audio_out)
            audio_out->speak("I am awake. My sleep cycles are complete.");
          shared.is_sleeping.store(false, std::memory_order_relaxed);
          last_cycle_change = now;
        }

        if (is_sleeping) {
          // Perform Dream Processing & Sleep Consolidation
          std::cout << "[Genesis/T5] Dreaming... Pruning weak memories and "
                       "consolidating.\n";
          if (dream_processor)
            dream_processor->processREMDreams(5000); // 5 sec REM burst
          if (sleep_consolidation) {
            std::size_t consolidated =
                sleep_consolidation->transferEpisodicToSemantic(10);
            std::size_t integrated =
                sleep_consolidation->performCrossModalIntegration();
            std::cout << "[Genesis/T5] Consolidated " << consolidated
                      << " episodes and integrated " << integrated
                      << " cross-modal patterns.\n";
          }
          std::this_thread::sleep_for(
              std::chrono::seconds(2)); // Tick dream processor every 2s
        } else {
          // Waking Status print
          std::uint64_t ts = shared.total_steps.load(std::memory_order_relaxed);
          std::uint64_t ep =
              shared.episode_count.load(std::memory_order_relaxed);
          std::size_t vocab_sz = language_system->getActiveTokens(0.0f).size();
          std::cout << "[Genesis/T5] Active @ Step " << ts << " | Episode "
                    << ep << " | Vocab " << vocab_sz << " | Surprise "
                    << shared.surprise.load() << " | η "
                    << shared.eta_effective.load() << "\n";
          std::this_thread::sleep_for(std::chrono::seconds(5));
        }
      }
      std::cout << "[Genesis/T5] Consolidation thread stopped.\n";
    });

    // ── THREAD 6: Conversation (STT/TTS loop) ──
    std::thread t_conversation([&]() {
      if (!audio_in || !audio_out) {
        std::cout
            << "[Genesis/T6] Conversational thread skipped (mode disabled).\n";
        return;
      }
      std::cout << "[Genesis/T6] Conversational thread started.\n";
      while (!abort_signal.load()) {
        if (shared.is_sleeping.load(std::memory_order_relaxed)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          auto transcriptions =
              audio_in->fetch(); // Drain the pipe so it doesn't backlog while
                                 // sleeping
          continue;
        }
        auto transcriptions = audio_in->fetch();
        for (const auto &text : transcriptions) {
          std::cout << "\n[User (STT)] " << text << "\n";

          std::string reply = "I heard you, but my semantic memory is empty.";
          if (semantic_memory) {
            std::vector<float> local_ling;
            encode_text(text, *language_system, local_ling);
            {
              std::lock_guard<std::mutex> lk(ling_mutex);
              linguistic_input = local_ling;
            }
            auto results =
                semantic_memory->findSimilarConcepts(local_ling, 1, 0.1f);
            if (!results.empty()) {
              reply = "I am thinking about: " + results[0].first.label;
            }
          }
          std::cout << "[NeuroForge] " << reply << "\n";
          audio_out->speak(reply);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
      std::cout << "[Genesis/T6] Conversational thread stopped.\n";
    });

    // ── Wait for all threads ──
    t_perception.join();
    t_grid.join();
    t_web.join();
    t_cognition.join();
    t_consolidation.join();
    t_conversation.join();

    total_steps = shared.total_steps.load();
    episode_count = shared.episode_count.load();

  } else {
    // ════════════════════════════════════════════════════════════════
    //  SEQUENTIAL GENESIS LOOP (original)
    // ════════════════════════════════════════════════════════════════
    while (!abort_signal.load()) {
      if (!cfg.infinite_mode &&
          total_steps >= static_cast<std::uint64_t>(cfg.max_steps)) {
        break;
      }

      // Sequential Circadian Rhythm
      std::int64_t now = now_ms();
      double seconds_in_state =
          static_cast<double>(now - last_cycle_change) / 1000.0;

      if (!is_sleeping && seconds_in_state >= cfg.wake_duration_s) {
        std::cout << "\n[Genesis] === FALLING ASLEEP (" << cfg.sleep_duration_s
                  << "s) ===\n";
        if (audio_out)
          audio_out->speak(
              "I am feeling tired. Falling asleep to process memories.");
        is_sleeping = true;
        last_cycle_change = now;
      } else if (is_sleeping && seconds_in_state >= cfg.sleep_duration_s) {
        std::cout << "\n[Genesis] === WAKING UP ===\n";
        if (audio_out)
          audio_out->speak("I am awake. My sleep cycles are complete.");
        is_sleeping = false;
        last_cycle_change = now;
      }

      if (is_sleeping) {
        std::cout << "[Genesis] Dreaming... Pruning weak memories and "
                     "consolidating.\n";
        if (dream_processor)
          dream_processor->processREMDreams(5000); // 5 sec REM burst
        if (sleep_consolidation) {
          std::size_t consolidated =
              sleep_consolidation->transferEpisodicToSemantic(10);
          std::size_t integrated =
              sleep_consolidation->performCrossModalIntegration();
          std::cout << "[Genesis] Consolidated " << consolidated
                    << " episodes and integrated " << integrated
                    << " cross-modal patterns.\n";
        }
        if (audio_in) {
          auto toss = audio_in->fetch();
        } // Drain pipe
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue; // Skip sensory/motor processing
      }

      // ── A. GRID: Episode Management ──
      if (needs_new_goal) {
        episode_count++;
        seed += 13;
        env.reset(seed);
        steps_in_episode = 0;
        needs_new_goal = false;

        if (total_steps > 0 && episode_count % 20 == 0) {
          std::cout << "[Genesis] New Episode " << episode_count << "\n";
        }
      }

      // ── B. WEB TICK (every 20 steps) ──
      std::string page_text;
      if (web_active && sandbox) {
        sandbox->poll();

        if (total_steps % 20 == 0) {
          sandbox->extractPageContent();
        }

        page_text = sandbox->getPageText();

        if (!page_text.empty()) {
          // Feed language acquisition
          std::string url = sandbox->getCurrentUrl();
          acq_loop->processWebContent(page_text, url);

          // Encode to embedding
          encode_text(page_text, *language_system, linguistic_input);
        }
      }

      // ── C. GRID TICK ──
      auto obs = env.observe();
      auto action_idx = env.greedyActionToGoal();
      auto result = env.step(static_cast<NeuroForge::Sandbox::GridAction>(
          static_cast<int>(action_idx)));

      // ── D. PERCEIVE: Feed WorldModel ──
      // Visual channel = grid observation, Linguistic channel = web text
      std::vector<float> visual_input(obs.begin(), obs.end());
      world_model_cortex.processCycle(visual_input, {}, {}, {}, {}, {},
                                      linguistic_input);

      float surprise = world_model_cortex.getLastSurpriseLevel();

      // ── E. NOVELTY BIAS ──
      auto novelty_metrics = novelty_bias.calculateNovelty(linguistic_input);
      novelty_bias.updateExperienceBuffer(linguistic_input);
      float interest = novelty_metrics.exploration_bonus;

      // ── E2. UNIVERSAL SIGNAL COMPUTE ──
      // This is THE equation: Δw = η · δ · ∇_w I(gain)
      float task_reward = result.reached_goal ? 1.0f : -0.01f;
      auto usig = universal_signal.compute(
          surprise,                         // δ: prediction error
          novelty_metrics.information_gain, // I_gain: information gain
          task_reward,                      // r_ext: external reward
          0.05f                             // sparsity_ratio (~5% active)
      );

      // ── F. ATTENTION MODULATOR (Boredom / Web Navigation) ──
      if (web_active && sandbox) {
        if (interest < 0.1f) {
          boredom_level += 1.0f;
        } else {
          boredom_level = std::max(0.0f, boredom_level - 0.5f);
        }

        if (boredom_level > 30.0f) {
          std::cout << "[Genesis] Bored (Level " << boredom_level
                    << ") — seeking novelty...\n";
          sandbox->navigate("https://en.wikipedia.org/wiki/Special:Random");
          boredom_level = 0.0f;
          std::this_thread::sleep_for(std::chrono::milliseconds(300));
        } else if (interest > 0.3f) {
          sandbox->scroll(100);
        }
      }

      // ── G. EPISODE BOUNDARY ──
      if (result.reached_goal || steps_in_episode > 200) {
        // Phase 7: Reflection
        if (reflection && episode_count % cfg.reflect_every_n_episodes == 0) {
          double contradiction_rate = 1.0 - static_cast<double>(interest);
          double avg_reward =
              result.reached_goal ? 1.0 : -0.1 * steps_in_episode;
          double valence = static_cast<double>(interest);
          double arousal = static_cast<double>(surprise);

          reflection->maybeReflect(static_cast<std::int64_t>(episode_count),
                                   contradiction_rate, avg_reward, valence,
                                   arousal);
        }

        // Phase 9: Metacognition — resolve actuals
        if (metacog && goal_system) {
          double actual_coherence = goal_system->getLastCoherence();
          double goal_shift = 0.0; // simplified
          metacog->resolveActuals(actual_coherence, goal_shift);
        }

        needs_new_goal = true;
      }

      // ── H. PHASE 8: Goal Stability Decay ──
      if (goal_system) {
        std::int64_t now = now_ms();
        double dt = static_cast<double>(now - last_decay_ms) / 1000.0;
        if (dt > 0.1) { // Decay at most every 100ms
          goal_system->decayStability(dt);
          last_decay_ms = now;
        }
      }

      // ── H2. CONVERSATIONAL TICK ──
      if (audio_in && audio_out) {
        auto transcriptions = audio_in->fetch();
        for (const auto &text : transcriptions) {
          std::cout << "\n[User (STT)] " << text << "\n";

          std::string reply = "My semantic memory is empty.";
          if (semantic_memory) {
            encode_text(text, *language_system, linguistic_input);
            auto results =
                semantic_memory->findSimilarConcepts(linguistic_input, 1, 0.1f);
            if (!results.empty()) {
              reply = "Thinking about: " + results[0].first.label;
            }
          }
          std::cout << "[NeuroForge] " << reply << "\n";
          audio_out->speak(reply);
        }
      }

      // ── I. LOGGING ──
      if (log_csv.is_open() && total_steps % 50 == 0) {
        std::size_t vocab_size = language_system->getActiveTokens(0.0f).size();
        double coherence = goal_system ? goal_system->getLastCoherence() : 0.0;
        double trust = metacog ? metacog->getSelfTrust() : 0.5;

        log_csv << total_steps << "," << episode_count << "," << vocab_size
                << "," << surprise << "," << novelty_metrics.surprise_level
                << "," << boredom_level << ","
                << (page_text.empty() ? 0 : page_text.length()) << ","
                << static_cast<int>(action_idx) << "," << coherence << ","
                << trust << "," << usig.delta << "," << usig.info_gain << ","
                << usig.eta_effective << "," << usig.reward_total << "\n";
      }

      // ── J. STATUS PRINT ──
      if (total_steps % 500 == 0) {
        std::size_t vocab_size = language_system->getActiveTokens(0.0f).size();
        std::cout << "[Genesis] Step " << total_steps << " | Episode "
                  << episode_count << " | Vocab " << vocab_size
                  << " | Surprise " << surprise << " | Novelty " << interest;
        if (metacog) {
          std::cout << " | SelfTrust " << metacog->getSelfTrust();
        }
        if (web_active) {
          std::cout << " | WebChars " << page_text.length();
        }
        std::cout << "\n";
      }

      steps_in_episode++;
      total_steps++;

      // Small sleep to avoid CPU spin (20ms = ~50Hz tick rate)
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  } // end else (sequential)

  // ═══ SHUTDOWN ═══
  std::cout << "\n[Genesis] ═══ SHUTTING DOWN ═══\n";
  std::cout << "[Genesis] Total Steps: " << total_steps
            << " | Episodes: " << episode_count << "\n";

  if (metacog) {
    std::cout << "[Genesis] Final Self-Trust: " << metacog->getSelfTrust()
              << "\n";
  }

  std::size_t final_vocab = language_system->getActiveTokens(0.0f).size();
  std::cout << "[Genesis] Final Vocabulary: " << final_vocab << " tokens\n";

  // ───────────────── BRAIN PERSISTENCE (AUTO-SAVE) ─────────────────
  if (cfg.auto_save) {
    std::cout << "[Genesis] Saving brain state...\n";
    auto save_result = NeuroForge::Core::BrainPersistence::saveState(
        cfg.brain_state_dir, *language_system, *episodic_memory,
        *semantic_memory, *procedural_memory, total_steps, episode_count);
    if (save_result.success) {
      std::cout << "[Genesis] \xe2\x9c\x93 Brain state saved successfully.\n";
    } else {
      std::cerr << "[Genesis] WARNING: Failed to save brain state: "
                << save_result.error << "\n";
    }
  }

  acq_loop->stop();
  language_system->shutdown();

  std::cout << "[Genesis] Goodbye.\n";
  return 0;
}

} // namespace Runtime
} // namespace NeuroForge
