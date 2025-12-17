#pragma once

#include <vector>
#include <string>

namespace NeuroForge {
namespace Core {

// Lightweight global context hooks used by metacognition/ethics modules
// NF_InitContext configures sampling gain and update cadence; window controls
// how many recent samples are retained for export.
void NF_InitContext(double gain, int update_ms, int window);

// Sample current context signal. The label is a human-readable tag for the
// sample (e.g., "metacog heartbeat" or "post-resolution"). Returns a value
// in [0,1]. Internally records the sample in a ring buffer of size 'window'.
double NF_SampleContext(const std::string& label);

// Return the most recent samples (up to 'window'). Most recent last.
std::vector<double> NF_GetRecentContextSamples();

struct NFContextConfig {
    double gain{1.0};
    int update_ms{1000};
    int window{5};
};

// Current configuration
NFContextConfig NF_GetContextConfig();

// Peer context APIs: register named peers with independent configs and optional coupling.
struct NFPeerConfig {
    double gain{1.0};
    int update_ms{1000};
    int window{5};
};

// Register or update a named context peer with its config.
void NF_RegisterContextPeer(const std::string& name, double gain, int update_ms, int window);

// Sample a specific peer stream; returns value in [0,1] and records in its window.
double NF_SampleContextPeer(const std::string& peer, const std::string& label);

// Get recent samples for a named peer (most recent last).
std::vector<double> NF_GetRecentContextPeerSamples(const std::string& peer);

// List all registered peer names.
std::vector<std::string> NF_ListContextPeers();

// Get peer config if registered; default values if absent.
NFPeerConfig NF_GetPeerConfig(const std::string& peer);

// Set coupling weight from src→dst in [0,1]. Multiple sources can couple into a dst.
void NF_SetContextCoupling(const std::string& src, const std::string& dst, double weight);

// Inspect current couplings as triples (src, dst, weight).
std::vector<std::tuple<std::string, std::string, double>> NF_GetContextCouplings();

} // namespace Core
} // namespace NeuroForge
