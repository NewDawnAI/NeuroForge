#include "core/PhaseC.h"
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
 
 namespace fs = std::filesystem;
 
 namespace NeuroForge {
 
 // ----- PhaseCCSVLogger -----
 PhaseCCSVLogger::PhaseCCSVLogger(const fs::path& out_dir) : out_dir_(out_dir) {
     std::error_code ec;
     fs::create_directories(out_dir_, ec);
     timeline_csv_.open(out_dir_ / "timeline.csv", std::ios::out | std::ios::trunc);
     assemblies_csv_.open(out_dir_ / "assemblies.csv", std::ios::out | std::ios::trunc);
     if (timeline_csv_) {
         timeline_csv_ << "step,winner_id,winner_symbol,winner_score\n";
         timeline_csv_.flush();
     }
     if (assemblies_csv_) {
         assemblies_csv_ << "step,assembly_id,symbol,score\n";
         assemblies_csv_.flush();
     }
 }
 
 PhaseCCSVLogger::~PhaseCCSVLogger() {
     if (timeline_csv_.is_open()) timeline_csv_.close();
     if (assemblies_csv_.is_open()) assemblies_csv_.close();
     if (bindings_csv_.is_open()) bindings_csv_.close();
     if (sequence_csv_.is_open()) sequence_csv_.close();
     if (wm_csv_.is_open()) wm_csv_.close();
 }
 
 void PhaseCCSVLogger::ensureBindings() {
     if (!bindings_ready_) {
         bindings_csv_.open(out_dir_ / "bindings.csv", std::ios::out | std::ios::trunc);
         if (bindings_csv_) {
             bindings_csv_ << "step,role,filler,strength\n";
             bindings_ready_ = true;
         }
     }
 }
 
 void PhaseCCSVLogger::ensureSequence() {
     if (!sequence_ready_) {
         sequence_csv_.open(out_dir_ / "sequence.csv", std::ios::out | std::ios::trunc);
         if (sequence_csv_) {
             sequence_csv_ << "step,target,predicted,correct\n";
             sequence_ready_ = true;
         }
     }
 }
 
 void PhaseCCSVLogger::ensureWorkingMemory() {
     if (!wm_ready_) {
         wm_csv_.open(out_dir_ / "working_memory.csv", std::ios::out | std::ios::trunc);
         if (wm_csv_) {
             wm_csv_ << "step,role,filler,strength\n";
             wm_ready_ = true;
         }
     }
 }
 
 // Small helpers for JSON emission
 static inline long long nf_epoch_ms() {
     auto now = std::chrono::system_clock::now();
     return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
 }
static inline std::string nf_json_escape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o << '\\' << c; }
        else { o << c; }
    }
    return o.str();
}
static inline std::string nf_iso8601_utc_now() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// Emit survival modulation telemetry using the JSON sink
void PhaseCCSVLogger::logSurvivalMod(int step,
                        const std::string& symbol,
                        float base_coherence,
                        float modulated_coherence,
                        float hazard_probability,
                        float risk_score,
                        float arousal_level,
                        float avoidance_drive,
                        float approach_drive,
                        float weight) {
    if (!json_sink_) return;
    std::ostringstream js;
    js.setf(std::ios::fixed);
    js << "{\"version\":1,\"phase\":\"C\",\"event\":\"survival_mod\",\"time\":\"" << nf_iso8601_utc_now()
       << "\",\"step\":" << step
       << ",\"symbol\":\"" << nf_json_escape(symbol) << "\""
       << ",\"base\":" << std::setprecision(4) << base_coherence
       << ",\"modulated\":" << std::setprecision(4) << modulated_coherence
       << ",\"delta\":" << std::setprecision(4) << (modulated_coherence - base_coherence)
       << ",\"hazard_probability\":" << std::setprecision(4) << hazard_probability
       << ",\"risk_score\":" << std::setprecision(4) << risk_score
       << ",\"arousal_level\":" << std::setprecision(4) << arousal_level
       << ",\"avoidance_drive\":" << std::setprecision(4) << avoidance_drive
       << ",\"approach_drive\":" << std::setprecision(4) << approach_drive
       << ",\"weight\":" << std::setprecision(4) << weight
       << ",\"effective_weight\":" << std::setprecision(4) << weight
       << "}";
    json_sink_(js.str());
}

 void PhaseCCSVLogger::logSequence(const SequenceRow& row) {
     ensureSequence();
     if (!sequence_csv_) return;
     sequence_csv_ << row.step << "," << row.target << "," << row.predicted << "," << row.correct << "\n";
     if (json_sink_) {
         std::ostringstream js;
         js << "{\"version\":1,\"phase\":\"C\",\"event\":\"sequence\",\"time\":\"" << nf_iso8601_utc_now()
            << "\",\"step\":" << row.step
            << ",\"target\":\"" << nf_json_escape(row.target) << "\""
            << ",\"predicted\":\"" << nf_json_escape(row.predicted) << "\""
            << ",\"correct\":" << row.correct << "}";
         json_sink_(js.str());
     }
 }
 
 void PhaseCCSVLogger::logWorkingMemory(int step, const std::vector<WorkingMemoryItem>& items) {
     ensureWorkingMemory();
     if (!wm_csv_) return;
     for (const auto& it : items) {
         wm_csv_ << step << "," << it.role << "," << it.filler << "," << std::fixed << std::setprecision(4) << it.strength << "\n";
     }
     if (json_sink_) {
         std::ostringstream js;
         js.setf(std::ios::fixed);
         js << "{\"version\":1,\"phase\":\"C\",\"event\":\"working_memory\",\"time\":\"" << nf_iso8601_utc_now()
            << "\",\"step\":" << step << ",\"items\":[";
         for (size_t i = 0; i < items.size(); ++i) {
             if (i) js << ",";
             js << "{\"role\":\"" << nf_json_escape(items[i].role) << "\",";
             js << "\"filler\":\"" << nf_json_escape(items[i].filler) << "\",";
             js << "\"strength\":" << std::setprecision(4) << items[i].strength << "}";
         }
         js << "]}";
         json_sink_(js.str());
     }
 }
 
 // Add missing logger methods
 void PhaseCCSVLogger::logTimeline(int step, const Assembly& winner) {
     if (!timeline_csv_) return;
     timeline_csv_ << step << "," << winner.id << "," << winner.symbol << "," << std::fixed << std::setprecision(4) << winner.score << "\n";
     if (json_sink_) {
         std::ostringstream js;
         js.setf(std::ios::fixed);
         js << "{\"version\":1,\"phase\":\"C\",\"event\":\"timeline\",\"time\":\"" << nf_iso8601_utc_now()
            << "\",\"step\":" << step
            << ",\"winner\":{\"id\":" << winner.id
            << ",\"symbol\":\"" << nf_json_escape(winner.symbol) << "\""
            << ",\"score\":" << std::setprecision(4) << winner.score << "}}";
         json_sink_(js.str());
     }
 }
 
 void PhaseCCSVLogger::logAssemblies(int step, const std::vector<Assembly>& assemblies) {
     if (!assemblies_csv_) return;
     for (const auto& a : assemblies) {
         assemblies_csv_ << step << "," << a.id << "," << a.symbol << "," << std::fixed << std::setprecision(4) << a.score << "\n";
     }
     if (json_sink_) {
         std::ostringstream js;
         js.setf(std::ios::fixed);
         js << "{\"version\":1,\"phase\":\"C\",\"event\":\"assemblies\",\"time\":\"" << nf_iso8601_utc_now()
            << "\",\"step\":" << step << ",\"assemblies\":[";
         for (size_t i = 0; i < assemblies.size(); ++i) {
             if (i) js << ",";
             js << "{\"id\":" << assemblies[i].id
                << ",\"symbol\":\"" << nf_json_escape(assemblies[i].symbol) << "\""
                << ",\"score\":" << std::setprecision(4) << assemblies[i].score << "}";
         }
         js << "]}";
         json_sink_(js.str());
     }
 }
 
 void PhaseCCSVLogger::logBinding(const BindingRow& row) {
     ensureBindings();
     if (!bindings_csv_) return;
     bindings_csv_ << row.step << "," << row.role << "," << row.filler << "," << std::fixed << std::setprecision(4) << row.strength << "\n";
     if (json_sink_) {
         std::ostringstream js;
         js.setf(std::ios::fixed);
         js << "{\"version\":1,\"phase\":\"C\",\"event\":\"binding\",\"time\":\"" << nf_iso8601_utc_now()
            << "\",\"step\":" << row.step
            << ",\"role\":\"" << nf_json_escape(row.role) << "\""
            << ",\"filler\":\"" << nf_json_escape(row.filler) << "\""
            << ",\"strength\":" << std::setprecision(4) << row.strength << "}";
         json_sink_(js.str());
     }
 }
 
 // ----- GlobalWorkspacePhaseC -----
 GlobalWorkspacePhaseC::GlobalWorkspacePhaseC(std::mt19937::result_type seed, PhaseCCSVLogger& logger)
 : rng_(seed), logger_(logger) {}

 float GlobalWorkspacePhaseC::randUniform(float a, float b) {
     std::uniform_real_distribution<float> dist(a, b);
     return dist(rng_);
 }

 std::map<std::string, std::string> GlobalWorkspacePhaseC::perceiveBinding(int /*step*/) {
     std::map<std::string, std::string> stim;
     // random pick one color and one shape
     size_t ci = static_cast<size_t>(randUniform(0.0f, 0.999f) * static_cast<float>(colors_.size())) % colors_.size();
     size_t si = static_cast<size_t>(randUniform(0.0f, 0.999f) * static_cast<float>(shapes_.size())) % shapes_.size();
     stim["color"] = colors_[ci];
     stim["shape"] = shapes_[si];
     return stim;
 }

 std::map<std::string, std::string> GlobalWorkspacePhaseC::perceiveSequence(int step) {
     std::map<std::string, std::string> stim;
     stim["target"] = seq_tokens_[static_cast<size_t>(step % static_cast<int>(seq_tokens_.size()))];
     return stim;
 }

 std::vector<Assembly> GlobalWorkspacePhaseC::formAssembliesBinding(const std::map<std::string, std::string>& percept, int /*step*/) {
     std::vector<Assembly> out;
     auto color_true = percept.at("color");
     auto shape_true = percept.at("shape");
     auto noise = [this]() { return randUniform(0.0f, 0.25f); };
     for (const auto& c : colors_) {
         float base = (c == color_true) ? 0.8f : 0.3f;
         out.push_back(Assembly{static_cast<int>(out.size()), std::string("color:") + c, base + noise()});
     }
     for (const auto& s : shapes_) {
         float base = (s == shape_true) ? 0.8f : 0.3f;
         out.push_back(Assembly{static_cast<int>(out.size()), std::string("shape:") + s, base + noise()});
     }
     return out;
 }

 std::vector<Assembly> GlobalWorkspacePhaseC::formAssembliesSequence(const std::map<std::string, std::string>& percept, int /*step*/) {
     std::vector<Assembly> out;
     auto target = percept.at("target");
     auto noise = [this]() { return randUniform(0.0f, 0.25f); };
     for (const auto& t : seq_tokens_) {
         float base = (t == target) ? 0.8f : 0.25f;
         out.push_back(Assembly{static_cast<int>(out.size()), t, base + noise()});
     }
     return out;
 }

 Assembly GlobalWorkspacePhaseC::compete(const std::vector<Assembly>& assemblies) {
     if (assemblies.empty()) return Assembly{};
     size_t best = 0;
     for (size_t i = 1; i < assemblies.size(); ++i) {
         if (assemblies[i].score > assemblies[best].score) best = i;
     }
     return assemblies[best];
 }
 void GlobalWorkspacePhaseC::stepBinding(int step) {
     auto percept = perceiveBinding(step);
     auto assemblies = formAssembliesBinding(percept, step);
     auto winner = compete(assemblies);
 
     // Log broadcast timeline and assembly scores
     logger_.logTimeline(step, winner);
     logger_.logAssemblies(step, assemblies);
 
     // Soft binding strengths
     auto color_true = percept["color"];
     auto shape_true = percept["shape"];
     auto strength = [this](bool is_true) {
         float base_true = 0.9f, base_false = 0.1f;
         float jitter = randUniform(-0.03f, 0.03f);
         float v = (is_true ? base_true : base_false) + jitter;
         if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f; return v;
     };
 
     // Decay WM before writing new evidence
     working_memory_.decayStep();
 
     for (const auto& c : colors_) {
         logger_.logBinding(BindingRow{step, "color", c, strength(c == color_true)});
         working_memory_.write("color", c, strength(c == color_true));
     }
     for (const auto& s : shapes_) {
         logger_.logBinding(BindingRow{step, "shape", s, strength(s == shape_true)});
         working_memory_.write("shape", s, strength(s == shape_true));
     }

    // Snapshot WM after updates
    logger_.logWorkingMemory(step, working_memory_.snapshot());
 }
 
 void GlobalWorkspacePhaseC::stepSequence(int step) {
     auto percept = perceiveSequence(step);
     auto assemblies = formAssembliesSequence(percept, step);
     auto winner = compete(assemblies);
 
     // Log broadcast timeline and assembly scores
     logger_.logTimeline(step, winner);
     logger_.logAssemblies(step, assemblies);
 
     // Update WM with token beliefs
     working_memory_.decayStep();
     for (const auto& a : assemblies) {
         float s = std::clamp(a.score, 0.0f, 1.0f);
         working_memory_.write("token", a.symbol, s);
     }
     // If a sequence window is set, keep only the strongest N token entries
     if (seq_window_ > 0) {
         working_memory_.pruneRoleCapacity("token", seq_window_);
     }
     logger_.logWorkingMemory(step, working_memory_.snapshot());
 
     // Report target vs winner
     SequenceRow row;
     row.step = step;
     row.target = percept["target"];
     row.predicted = winner.symbol;
     row.correct = (row.target == row.predicted) ? 1 : 0;
     logger_.logSequence(row);
 }
 
 } // namespace NeuroForge
