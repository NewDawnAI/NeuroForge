#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <optional>
#include <fstream>
#include <cmath>

#include "core/PhaseC.h"
#include "core/MemoryDB.h"
#include "core/LearningSystem.h"

using namespace NeuroForge;
using namespace NeuroForge::Core;

static inline std::int64_t epoch_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

// Very small helper to extract a JSON field value from PhaseC logger sink
static std::optional<std::string> extract_field(const std::string& js, const std::string& key) {
    // looks for "key":"value" and returns value; naive but sufficient
    std::string pat = std::string("\"") + key + "\":\"";
    auto pos = js.find(pat);
    if (pos == std::string::npos) return std::nullopt;
    pos += pat.size();
    auto end = js.find('"', pos);
    if (end == std::string::npos) return std::nullopt;
    return js.substr(pos, end - pos);
}

int main() {
    try {
        // Configure CSV logger and JSON sink to capture sequence events
        std::string last_sequence_json;
        std::mutex seq_mu;
        PhaseCCSVLogger logger("PhaseC_Logs/runner");
        logger.setJsonSink([&](const std::string& js){
            // capture the most recent sequence event for reward logging
            if (js.find("\"event\":\"sequence\"") != std::string::npos) {
                std::lock_guard<std::mutex> lk(seq_mu);
                last_sequence_json = js;
            }
        });

        // Initialize Phase C workspace
        std::mt19937::result_type seed = 42u;
        GlobalWorkspacePhaseC gw(seed, logger);
        gw.setWorkingMemoryParams(/*capacity*/ 6, /*decay*/ 0.90f);
        gw.setSequenceWindow(/*w*/ 4); // keep strongest recent tokens to emulate focus

        // Prepare MemoryDB
        MemoryDB db("phasec_runner.db");
        db.setDebug(true);
        if (!db.open()) {
            std::cerr << "[ERROR] Failed to open SQLite DB phasec_runner.db" << std::endl;
            return 1;
        }

        // Begin run with simple metadata
        std::int64_t run_id = 0;
        std::string meta = "{\"runner\":\"phase_c_runner\",\"mode\":\"sequence\",\"seed\":42}";
        if (!db.beginRun(meta, run_id)) {
            std::cerr << "[ERROR] Failed to begin run in MemoryDB" << std::endl;
            return 1;
        }

        // Multi-episode sweep
        const int episodes = 20;
        const std::uint64_t steps_per_episode = 200;
        std::uint64_t total_correct = 0;

        // Hz estimation variables
        auto last_hz_time = std::chrono::steady_clock::now();
        std::uint64_t steps_since = 0;

        // Collect self-model summary per episode
        struct EpSelfModel { int episode; double awareness; double confidence; double identity_similarity; double drift; std::string stage; };
        std::vector<EpSelfModel> self_summaries;
        struct OptSummary { int episode; std::int64_t option_id; double confidence; bool selected; double reward; int evaluations; double avg_score; };
        std::vector<OptSummary> option_summaries;

        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> conf_dist(0.50, 0.95);
        double prev_confidence = std::numeric_limits<double>::quiet_NaN();

        for (int ep = 0; ep < episodes; ++ep) {
            // Start an episode
            std::int64_t episode_id = 0;
            if (!db.insertEpisode("PhaseC-Sequence", epoch_ms(), run_id, episode_id)) {
                std::cerr << "[WARN] Failed to insert episode" << std::endl;
            }

            std::uint64_t correct = 0;
            NeuroForge::Core::LearningSystem::Statistics st{};
            st.active_synapses = 0; // sandbox: no substrate

            for (std::uint64_t step = 0; step < steps_per_episode; ++step) {
                gw.stepSequence(static_cast<int>(step));

                // Extract target/predicted from last JSON sink event
                std::string js;
                {
                    std::lock_guard<std::mutex> lk(seq_mu);
                    js = last_sequence_json;
                }
                auto target = extract_field(js, "target");
                auto predicted = extract_field(js, "predicted");
                int is_correct = (target && predicted && *target == *predicted) ? 1 : 0;
                if (is_correct) {
                    ++correct;
                    st.last_reward = 1.0f;
                    st.cumulative_reward += 1.0f;
                    st.reward_events += 1;
                    st.reward_updates += 1;
                } else {
                    st.last_reward = 0.0f;
                }

                // Reward: 1 for correct, 0 otherwise; log with minimal context
                std::int64_t reward_id = 0;
                std::string ctx = std::string("{\"target\":\"") + (target ? *target : "?") + "\",\"predicted\":\"" + (predicted ? *predicted : "?") + "\"}";
                db.insertRewardLog(epoch_ms(), step, is_correct ? 1.0 : 0.0, "PhaseCRunner", ctx, run_id, reward_id);

                // Compute processing_hz over a sliding window
                steps_since += 1;
                auto now = std::chrono::steady_clock::now();
                double hz = 0.0;
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hz_time).count();
                if (elapsed_ms > 0) {
                    hz = (steps_since * 1000.0) / static_cast<double>(elapsed_ms);
                }
                if (elapsed_ms >= 250) { // reset window every 250ms
                    steps_since = 0;
                    last_hz_time = now;
                }

                // Insert minimal learning_stats snapshot
                db.insertLearningStats(epoch_ms(), step, hz, st, run_id);

                // Gentle pacing to mimic runtime
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

            // End episode and record stats
            db.updateEpisodeEnd(episode_id, epoch_ms());
            double episode_return = static_cast<double>(correct);
            db.upsertEpisodeStats(episode_id, steps_per_episode, /*success*/ correct > steps_per_episode/2, episode_return);

            // Phase 6 hooks: Self-model snapshot per episode (with drift + stage)
            {
                const double awareness = static_cast<double>(correct) / static_cast<double>(steps_per_episode);
                const double identity_similarity = 1.0; // sandbox stable identity
                const double confidence = conf_dist(rng);
                double drift = std::isnan(prev_confidence) ? 0.0 : std::abs(confidence - prev_confidence);
                prev_confidence = confidence;
                const std::string stage = "concept";

                const std::int64_t ts = epoch_ms();
                const std::uint64_t step_mark = steps_per_episode;
                std::string state_json = std::string("{\"awareness\":") + std::to_string(awareness) +
                                         ",\"confidence\":" + std::to_string(confidence) +
                                         ",\"identity_similarity\":" + std::to_string(identity_similarity) +
                                         ",\"self_model_drift\":" + std::to_string(drift) +
                                         ",\"stage\":\"" + stage + "\"}";
                std::int64_t self_id = 0;
                db.insertSelfModel(ts, step_mark, state_json, confidence, run_id, self_id);
                self_summaries.push_back(EpSelfModel{ep, awareness, confidence, identity_similarity, drift, stage});
            }

            // Phase 6 hooks: Option logging per episode + outcome stats + verification
            {
                const std::int64_t ts = epoch_ms();
                const std::uint64_t step_mark = steps_per_episode;
                const double opt_conf = conf_dist(rng);
                const bool selected = true;
                std::string source = "Phase6ReasonerSandbox";
                std::string option_key = std::string("episode_choice_") + std::to_string(ep);
                std::string option_json = std::string("{\"option_key\":\"") + option_key +
                                          "\",\"decision\":\"continue\",\"episode_return\":" + std::to_string(episode_return) + "}";
                std::int64_t option_id = 0;
                db.insertOption(ts, step_mark, source, option_json, opt_conf, selected, run_id, option_id);
                // upsert evaluation stats based on simple reward ratio
                double reward_ratio = episode_return / static_cast<double>(steps_per_episode);
                int evaluations = 1;
                double avg_score = reward_ratio;
                db.upsertOptionStats(option_id, evaluations, avg_score, ts);

                // Insert an inferred fact for episode outcome
                std::int64_t fact_id = 0;
                std::string fact_json = std::string("{\"type\":\"episode_end\",\"episode\":") + std::to_string(ep) +
                                        ",\"reward_ratio\":" + std::to_string(reward_ratio) +
                                        ",\"option_id\":" + std::to_string(option_id) + "}";
                db.insertInferredFact(ts, fact_json, opt_conf, run_id, std::optional<std::int64_t>(option_id), fact_id);
                // Verification: contradiction true if reward <= 0.8
                bool contradiction = !(reward_ratio > 0.8);
                std::string details_json = std::string("{\"threshold\":0.8,\"reward_ratio\":") + std::to_string(reward_ratio) + "}";
                std::int64_t verification_id = 0;
                db.insertVerification(ts, fact_id, "episode_end", contradiction, details_json, run_id, verification_id);

                option_summaries.push_back(OptSummary{ep, option_id, opt_conf, selected, reward_ratio, evaluations, avg_score});
            }

            total_correct += correct;
        }

        // Emit self_model_summary.json for dashboard
        {
            std::ofstream ofs("PhaseC_Logs/runner/self_model_summary.json", std::ios::binary);
            ofs << "{\n  \"run_id\": " << run_id << ",\n  \"episodes\": [\n";
            for (size_t i = 0; i < self_summaries.size(); ++i) {
                const auto& s = self_summaries[i];
                ofs << "    {\"episode\": " << s.episode
                    << ", \"awareness\": " << s.awareness
                    << ", \"confidence\": " << s.confidence
                    << ", \"identity_similarity\": " << s.identity_similarity
                    << ", \"self_model_drift\": " << s.drift
                    << ", \"stage\": \"" << s.stage << "\"}";
                if (i + 1 < self_summaries.size()) ofs << ",\n"; else ofs << "\n";
            }
            ofs << "  ]\n}";
        }

        // Emit options_summary.json for dashboard
        {
            std::ofstream ofs("PhaseC_Logs/runner/options_summary.json", std::ios::binary);
            ofs << "{\n  \"run_id\": " << run_id << ",\n  \"episodes\": [\n";
            for (size_t i = 0; i < option_summaries.size(); ++i) {
                const auto& o = option_summaries[i];
                ofs << "    {\"episode\": " << o.episode
                    << ", \"option_id\": " << o.option_id
                    << ", \"confidence\": " << o.confidence
                    << ", \"selected\": " << (o.selected ? 1 : 0)
                    << ", \"reward\": " << o.reward
                    << ", \"evaluations\": " << o.evaluations
                    << ", \"avg_score\": " << o.avg_score << "}";
                if (i + 1 < option_summaries.size()) ofs << ",\n"; else ofs << "\n";
            }
            ofs << "  ]\n}";
        }

        // Close DB
        db.close();

        std::cout << "Phase C runner sweep completed: episodes=" << episodes
                  << ", steps/episode=" << steps_per_episode
                  << ", total_correct=" << total_correct
                  << ", db=phasec_runner.db" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[EXCEPTION] " << e.what() << std::endl;
        return 2;
    }
}