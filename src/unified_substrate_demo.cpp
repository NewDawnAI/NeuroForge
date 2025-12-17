// Unified substrate demo: runs Phase A (language/mimicry), Substrate WM, Phase C, and SurvivalBias together
#include "core/HypergraphBrain.h"
#include "connectivity/ConnectivityManager.h"
#include "core/SubstrateWorkingMemory.h"
#include "core/SubstratePhaseC.h"
#include "core/SubstrateLanguageIntegration.h"
#include "core/LanguageSystem.h"
#include "core/MemoryDB.h"
#include "core/LearningSystem.h"
#include "biases/SurvivalBias.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <fstream>
#include <string>

using namespace NeuroForge::Core;

static bool parseOnOff(const std::string& v, bool& out) {
    if (v == "on") { out = true; return true; }
    if (v == "off") { out = false; return true; }
    return false;
}

int main(int argc, char** argv) {
    try {
        bool lang_bias = true;
        std::uint32_t seed = 42u;
        int steps = 200;
        int log_every = 10;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i] ? std::string(argv[i]) : std::string();
            if (arg.rfind("--lang-bias=", 0) == 0) {
                const std::string v = arg.substr(std::string("--lang-bias=").size());
                if (!parseOnOff(v, lang_bias)) {
                    std::cerr << "ERROR: invalid --lang-bias value (use on|off)\n";
                    return 1;
                }
            } else if (arg.rfind("--seed=", 0) == 0) {
                seed = static_cast<std::uint32_t>(std::stoul(arg.substr(std::string("--seed=").size())));
            } else if (arg.rfind("--steps=", 0) == 0) {
                steps = std::stoi(arg.substr(std::string("--steps=").size()));
            } else if (arg.rfind("--log-every=", 0) == 0) {
                log_every = std::max(1, std::stoi(arg.substr(std::string("--log-every=").size())));
            }
        }

        auto conn_mgr = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
        auto brain = std::make_shared<HypergraphBrain>(conn_mgr);
        brain->setProcessingMode(HypergraphBrain::ProcessingMode::Sequential);
        brain->setRandomSeed(seed);
        if (!brain->initialize()) {
            std::cerr << "ERROR: HypergraphBrain initialize failed" << std::endl;
            return 2;
        }

        {
            NeuroForge::Core::LearningSystem::Config lconf{};
            lconf.global_learning_rate = 0.01f;
            lconf.hebbian_rate = 0.0005f;
            lconf.stdp_rate = 0.0005f;
            lconf.stdp_rate_multiplier = 1.5f;
            lconf.p_gate = 0.05f;

            lconf.enable_attention_modulation = true;
            lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::ExternalMap;
            lconf.attention_boost_factor = 1.25f;
            lconf.attention_Amin = 1.0f;
            lconf.attention_Amax = 2.0f;
            lconf.attention_anneal_ms = 500;
            lconf.update_interval = std::chrono::milliseconds(10);

            (void)brain->initializeLearning(lconf);
            brain->setLearningEnabled(true);
            if (auto* ls = brain->getLearningSystem()) {
                ls->setAutoEligibilityAccumulation(false);
                ls->setRandomSeed(seed);
            }
        }

        brain->setHippocampalEnabled(false);

        // Optional MemoryDB wiring (path from env NF_TELEMETRY_DB or default phasec_mem.db)
        std::string db_path;
        const char* env_db = std::getenv("NF_TELEMETRY_DB");
        if (env_db && std::string(env_db) == "off") {
            db_path.clear();
        } else {
            db_path = env_db ? std::string(env_db) : std::string("phasec_mem.db");
        }
        std::shared_ptr<NeuroForge::Core::MemoryDB> memdb;
        std::int64_t memdb_run_id = 0;
        if (!db_path.empty()) {
            memdb = std::make_shared<NeuroForge::Core::MemoryDB>(db_path);
            if (memdb->open()) {
                std::string meta = std::string("{\"unified_demo\":true,\"notes\":\"WM+PhaseC+Language+Bias unified run\"}");
                if (memdb->beginRun(meta, memdb_run_id)) {
                    brain->setMemoryDB(memdb, memdb_run_id);
                    std::cout << "MemoryDB connected at '" << db_path << "' (run=" << memdb_run_id << ")" << std::endl;
                } else {
                    std::cerr << "Warning: beginRun failed; continuing without DB run id" << std::endl;
                }
            } else {
                std::cerr << "Info: MemoryDB unavailable or failed to open at '" << db_path << "'" << std::endl;
            }
        }

        // Initialize Substrate Working Memory
        SubstrateWorkingMemory::Config wm_cfg;
        wm_cfg.working_memory_regions = 4;
        wm_cfg.neurons_per_region = 64;
        auto wm = std::make_shared<SubstrateWorkingMemory>(brain, wm_cfg);
        if (!wm->initialize()) {
            std::cerr << "ERROR: SubstrateWorkingMemory initialize failed" << std::endl;
            return 3;
        }

        // Initialize Substrate Phase C
        SubstratePhaseC::Config pc_cfg;
        pc_cfg.binding_regions = 4;
        pc_cfg.sequence_regions = 3;
        pc_cfg.neurons_per_region = 64;
        auto phaseC = std::make_unique<SubstratePhaseC>(brain, wm, pc_cfg);
        if (!phaseC->initialize()) {
            std::cerr << "ERROR: SubstratePhaseC initialize failed" << std::endl;
            return 4;
        }

        // Attach SurvivalBias
        auto survival_bias = std::make_shared<NeuroForge::Biases::SurvivalBias>();
        phaseC->setSurvivalBias(survival_bias);
        phaseC->setEmitSurvivalRewards(true);
        phaseC->setSurvivalRewardScale(1.0f);

        // Initialize Language Substrate Integration
        // Create a basic LanguageSystem instance for integration
        LanguageSystem::Config ls_cfg{};
        auto language_system = std::make_shared<LanguageSystem>(ls_cfg);
        language_system->setRandomSeed(seed);
        if (!language_system->initialize()) {
            std::cerr << "ERROR: LanguageSystem initialize failed" << std::endl;
            return 5;
        }
        SubstrateLanguageIntegration::Config lang_cfg;
        lang_cfg.language_region_neurons = 256;
        lang_cfg.proto_word_region_neurons = 128;
        lang_cfg.prosodic_region_neurons = 64;
        lang_cfg.grounding_region_neurons = 192;
        lang_cfg.max_concurrent_patterns = 20;
        auto lang = std::make_shared<SubstrateLanguageIntegration>(language_system, brain, lang_cfg);
        if (!lang->initialize()) {
            std::cerr << "ERROR: SubstrateLanguageIntegration initialize failed" << std::endl;
            return 6;
        }
        if (!lang_bias) {
            language_system->setNeuronBiasCallback(nullptr);
        }

        // Run a short unified loop
        const float dt = 0.01f;
        auto last_hz_time = std::chrono::steady_clock::now();
        std::uint64_t steps_since = 0;

        const auto start_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const std::string bias_label = lang_bias ? "on" : "off";
        const std::string csv_path = "lang_bias_metrics_" + bias_label + "_seed" + std::to_string(seed) + "_ts" + std::to_string(start_ts_ms) + ".csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "step,processing_hz,avg_weight_change,hebbian_updates,stdp_updates,attention_events,mean_attention_weight,"
                   "total_neural_tokens,avg_binding_strength,substrate_language_coherence,"
                   "active_vocab,total_vocab,avg_token_activation,vocab_diversity,token_activation_entropy,avg_cluster_stability,tokens_stable_over_0_5\n";
            csv.flush();
        }

        for (int s = 0; s < steps; ++s) {
            const auto step_start = std::chrono::steady_clock::now();

            brain->processStep(dt);
            const auto after_brain = std::chrono::steady_clock::now();

            phaseC->processStep(s, dt);
            const auto after_phasec = std::chrono::steady_clock::now();

            lang->processSubstrateLanguageStep(dt);
            const auto after_lang = std::chrono::steady_clock::now();

            if (((s + 1) % 10) == 0) {
                const auto brain_ms = std::chrono::duration_cast<std::chrono::milliseconds>(after_brain - step_start).count();
                const auto phasec_ms = std::chrono::duration_cast<std::chrono::milliseconds>(after_phasec - after_brain).count();
                const auto lang_ms = std::chrono::duration_cast<std::chrono::milliseconds>(after_lang - after_phasec).count();
                std::cout << "[Progress] step=" << (s + 1)
                          << " brain_ms=" << brain_ms
                          << " phasec_ms=" << phasec_ms
                          << " lang_ms=" << lang_ms
                          << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            steps_since += 1;
            auto now = std::chrono::steady_clock::now();
            double hz = 0.0;
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hz_time).count();
            if (elapsed_ms > 0) {
                hz = (steps_since * 1000.0) / static_cast<double>(elapsed_ms);
            }
            if (elapsed_ms >= 250) {
                steps_since = 0;
                last_hz_time = now;
            }

            if (memdb && memdb_run_id > 0) {
                auto st_opt = brain->getLearningStatistics();
                NeuroForge::Core::LearningSystem::Statistics st{};
                if (st_opt.has_value()) st = *st_opt;
                const auto step_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                memdb->insertLearningStats(static_cast<std::int64_t>(step_ts_ms), static_cast<std::uint64_t>(s + 1), hz, st, memdb_run_id);
            }

            if (csv.is_open() && ((s + 1) % log_every == 0)) {
                auto st_opt = brain->getLearningStatistics();
                NeuroForge::Core::LearningSystem::Statistics st{};
                if (st_opt.has_value()) st = *st_opt;
                auto lang_stats = lang->getStatistics();
                auto ls_stats = language_system->getStatistics();
                csv << (s + 1) << "," << hz << ","
                    << st.average_weight_change << "," << st.hebbian_updates << "," << st.stdp_updates << ","
                    << st.attention_modulation_events << "," << st.mean_attention_weight << ","
                    << lang_stats.total_neural_tokens << "," << lang_stats.average_binding_strength << "," << lang_stats.substrate_language_coherence << ","
                    << ls_stats.active_vocabulary_size << "," << ls_stats.total_vocabulary_size << ","
                    << ls_stats.average_token_activation << "," << ls_stats.vocabulary_diversity << ","
                    << ls_stats.token_activation_entropy << "," << ls_stats.average_cluster_stability << "," << ls_stats.tokens_stable_over_0_5
                    << "\n";
                csv.flush();
            }

            // Periodic metrics summary (every 250 steps)
            if (((s + 1) % 250) == 0) {
                auto pc_stats = phaseC->getStatistics();
                auto l_stats = lang->getStatistics();
                auto assemblies = phaseC->getCurrentAssemblies();
                std::vector<std::size_t> asm_sizes; asm_sizes.reserve(assemblies.size());
                for (const auto& a : assemblies) {
                    asm_sizes.push_back(a.neurons.size());
                }
                std::sort(asm_sizes.begin(), asm_sizes.end(), std::greater<>());
                std::size_t topk1 = asm_sizes.size() > 0 ? asm_sizes[0] : 0;
                std::size_t topk2 = asm_sizes.size() > 1 ? asm_sizes[1] : 0;
                std::cout << "[Unified Metrics] step=" << (s + 1)
                          << " assemblies=" << pc_stats.assemblies_formed
                          << " avg_coherence=" << pc_stats.average_coherence
                          << " topK_sizes=" << topk1 << "," << topk2
                          << " | language_coherence=" << l_stats.substrate_language_coherence
                          << " binding_strength_avg=" << l_stats.average_binding_strength
                          << " tokens=" << l_stats.total_neural_tokens
                          << " patterns=" << l_stats.active_neural_patterns
                          << std::endl;
            }
        }

        // Summaries
        auto stats = phaseC->getStatistics();
        auto lstats = lang->getStatistics();
        std::cout << "=== Unified Substrate Demo Summary ===\n";
        std::cout << "Phase C: assemblies=" << stats.assemblies_formed
                  << " bindings=" << stats.bindings_created
                  << " sequences=" << stats.sequences_predicted
                  << " goals=" << stats.goals_achieved
                  << " avg_coherence=" << stats.average_coherence << "\n";
        std::cout << "Language: substrate_language_coherence=" << lstats.substrate_language_coherence
                  << " binding_strength_avg=" << lstats.average_binding_strength
                  << " integration_efficiency=" << lstats.integration_efficiency << "\n";
        std::cout << "Lang bias: " << (lang_bias ? "on" : "off") << " | seed=" << seed << " | metrics_csv=" << csv_path << "\n";

        wm->shutdown();
        phaseC->shutdown();
        lang->shutdown();
        brain->shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Unified substrate demo error: " << e.what() << std::endl;
        return 1;
    }
}
