#include "core/ContextHooks.h"
#include <mutex>
#include <deque>
#include <chrono>
#include <random>
#include <unordered_map>
#include <tuple>

namespace NeuroForge {
namespace Core {

static std::mutex g_ctx_m;
static NFContextConfig g_cfg;
static std::deque<double> g_recent;
static std::mt19937 g_rng{std::random_device{}()};

// Peer contexts: name -> {config, recent deque, last_sample}
struct PeerState {
    NFPeerConfig cfg;
    std::deque<double> recent;
    double last{0.0};
};
static std::unordered_map<std::string, PeerState> g_peers;
// Couplings: (src,dst) -> weight
struct PairHash {
    std::size_t operator()(const std::pair<std::string,std::string>& p) const noexcept {
        return std::hash<std::string>{}(p.first) ^ (std::hash<std::string>{}(p.second) << 1);
    }
};
static std::unordered_map<std::pair<std::string,std::string>, double, PairHash> g_couplings;

void NF_InitContext(double gain, int update_ms, int window) {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    if (window <= 0) window = 1;
    g_cfg.gain = gain;
    g_cfg.update_ms = update_ms;
    g_cfg.window = window;
    g_recent.clear();
    // Do not clear peers on init to allow independent lifecycle; keep explicit via register calls.
}

// Simple synthetic sampler: combines time-based jitter with label hashing.
// In a real system, this would pull from substrate/WM signals.
static double sample_raw(const std::string& label) {
    using namespace std::chrono;
    auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    std::hash<std::string> H;
    uint64_t h = static_cast<uint64_t>(H(label)) ^ static_cast<uint64_t>(now);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double jitter = dist(g_rng);
    // Mix hashed signal with jitter; bound to [0,1]
    double base = ((h & 0xFFFFull) / 65535.0) * 0.7 + jitter * 0.3;
    if (base < 0.0) base = 0.0; if (base > 1.0) base = 1.0;
    return base;
}

double NF_SampleContext(const std::string& label) {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    double raw = sample_raw(label);
    double v = raw * g_cfg.gain;
    if (v < 0.0) v = 0.0; if (v > 1.0) v = 1.0;
    g_recent.push_back(v);
    while (static_cast<int>(g_recent.size()) > g_cfg.window) g_recent.pop_front();
    return v;
}

std::vector<double> NF_GetRecentContextSamples() {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    return std::vector<double>(g_recent.begin(), g_recent.end());
}

NFContextConfig NF_GetContextConfig() {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    return g_cfg;
}

void NF_RegisterContextPeer(const std::string& name, double gain, int update_ms, int window) {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    if (window <= 0) window = 1;
    PeerState& ps = g_peers[name];
    ps.cfg.gain = gain;
    ps.cfg.update_ms = update_ms;
    ps.cfg.window = window;
    if (static_cast<int>(ps.recent.size()) > window) {
        while (static_cast<int>(ps.recent.size()) > window) ps.recent.pop_front();
    }
}

double NF_SampleContextPeer(const std::string& peer, const std::string& label) {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    // Ensure peer exists with default config if not registered
    if (g_peers.find(peer) == g_peers.end()) {
        g_peers.emplace(peer, PeerState{});
    }
    PeerState& ps = g_peers[peer];
    if (ps.cfg.window <= 0) ps.cfg.window = 1;
    double raw = sample_raw(label);
    double v = raw * ps.cfg.gain;
    // Apply coupling contributions from sources into this peer
    for (const auto& kv : g_couplings) {
        const auto& pr = kv.first; double w = kv.second;
        if (pr.second == peer && w != 0.0) {
            auto it = g_peers.find(pr.first);
            double src_last = (it != g_peers.end()) ? it->second.last : 0.0;
            v += w * src_last;
        }
    }
    if (v < 0.0) v = 0.0; if (v > 1.0) v = 1.0;
    ps.last = v;
    ps.recent.push_back(v);
    while (static_cast<int>(ps.recent.size()) > ps.cfg.window) ps.recent.pop_front();
    return v;
}

std::vector<double> NF_GetRecentContextPeerSamples(const std::string& peer) {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    auto it = g_peers.find(peer);
    if (it == g_peers.end()) return {};
    const auto& dq = it->second.recent;
    return std::vector<double>(dq.begin(), dq.end());
}

std::vector<std::string> NF_ListContextPeers() {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    std::vector<std::string> names;
    names.reserve(g_peers.size());
    for (const auto& kv : g_peers) names.push_back(kv.first);
    return names;
}

NFPeerConfig NF_GetPeerConfig(const std::string& peer) {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    auto it = g_peers.find(peer);
    if (it == g_peers.end()) return NFPeerConfig{};
    return it->second.cfg;
}

void NF_SetContextCoupling(const std::string& src, const std::string& dst, double weight) {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    if (weight == 0.0) {
        g_couplings.erase({src, dst});
        return;
    }
    if (weight < 0.0) weight = 0.0; if (weight > 1.0) weight = 1.0;
    g_couplings[{src, dst}] = weight;
}

std::vector<std::tuple<std::string, std::string, double>> NF_GetContextCouplings() {
    std::lock_guard<std::mutex> lg(g_ctx_m);
    std::vector<std::tuple<std::string, std::string, double>> edges;
    edges.reserve(g_couplings.size());
    for (const auto& kv : g_couplings) edges.emplace_back(kv.first.first, kv.first.second, kv.second);
    return edges;
}

} // namespace Core
} // namespace NeuroForge
