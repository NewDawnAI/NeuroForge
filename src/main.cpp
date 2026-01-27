#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <mutex>
#include <fstream>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <random>
#include <queue>
#include <utility>
#include <limits>
#include <cstring>
#include <unordered_map>

#include <exception>
#include <stdexcept>
#include <new>
#include <csignal>


#ifdef NF_HAVE_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#endif


#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cerrno>
#endif

// Project headers
#include "audio_capture.h"
#include "system_audio_capture.h"
#include "screen_capture.h"
#include "core/HypergraphBrain.h"
#include "connectivity/ConnectivityManager.h"
#include "core/Region.h"
#include "core/LearningSystem.h"
#include "core/FirstPersonMazeRenderer.h"
#include "regions/CorticalRegions.h"
#include "encoders/VisionEncoder.h"
#include "encoders/AudioEncoder.h"
#include "biases/SocialPerceptionBias.h"
#include "biases/VoiceBias.h"
#include "biases/MotionBias.h"
#include "biases/SurvivalBias.h"
#include "core/Neuron.h"
#include "core/MemoryDB.h"
#include "core/SelfModel.h"
#include "core/LanguageSystem.h"
#include "core/PhaseAMimicry.h"
// Context hooks (peer coupling and sampling)
#include "core/ContextHooks.h"
#include "core/Phase6Reasoner.h"
#include "core/Phase7AffectiveState.h"
#include "core/Phase7Reflection.h"
#include "core/Phase8GoalSystem.h"
#include "core/Phase9Metacognition.h"
#include "core/AutonomyEnvelope.h"
#include "core/Phase10SelfExplanation.h"
#include "core/Phase11SelfRevision.h"
#include "core/Phase12Consistency.h"
#include "core/Phase13AutonomyEnvelope.h"
#include "core/Phase14MetaReasoner.h"
#include "core/Phase15EthicsRegulator.h"
#include "core/ContextHooks.h"
                #include "core/SubstratePhaseC.h"
#include "core/SubstrateWorkingMemory.h"
// Sandbox window for safe, bounded interaction on Windows
#include "sandbox/WebSandbox.h"
// Unified action gating layer
#include "core/ActionFilter.h"

static std::shared_ptr<NeuroForge::Core::MemoryDB> g_memdb;
static std::int64_t g_memdb_run_id = 0;
static std::atomic<std::uint64_t> g_last_step{0};
static std::atomic<bool> g_abort{false};

static void NfSetTerminationHandlers();

#ifdef _WIN32
static double nf_process_rss_mb() {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
    return 0.0;
}

static BOOL WINAPI NfCtrlHandler(DWORD ctrl) {
    const char* t = "unknown";
    if (ctrl == CTRL_C_EVENT) t = "CTRL_C";
    else if (ctrl == CTRL_BREAK_EVENT) t = "CTRL_BREAK";
    else if (ctrl == CTRL_CLOSE_EVENT) t = "CTRL_CLOSE";
    else if (ctrl == CTRL_LOGOFF_EVENT) t = "CTRL_LOGOFF";
    else if (ctrl == CTRL_SHUTDOWN_EVENT) t = "CTRL_SHUTDOWN";
    g_abort.store(true);
    if (g_memdb && g_memdb_run_id > 0) {
        std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::int64_t event_id = 0;
        (void)g_memdb->insertRunEvent(g_memdb_run_id, ts_ms, g_last_step.load(), std::string("signal"), std::string(t), 0, nf_process_rss_mb(), 0.0, event_id);
    }
    return TRUE;
}
#else
static double nf_process_rss_mb() { return 0.0; }
#endif
                #include "core/PhaseC.h"
#include "core/SubstrateLanguageIntegration.h"
#include "regions/LimbicRegions.h"
#include "core/RegionRegistry.h"

// Force-link declarations for region translation units (defined in their respective .cpp files)
extern "C" void NF_ForceLink_CorticalRegions();
extern "C" void NF_ForceLink_SubcorticalRegions();
extern "C" void NF_ForceLink_LimbicRegions();
extern "C" void NF_ForceLink_PhaseARegion();

namespace {

// Helper: sanitizes shell arguments to prevent command injection
std::string shell_escape(const std::string& arg) {
#ifdef _WIN32
    // Windows cmd.exe escaping: wrap in double quotes.
    // NOTE: cmd.exe does NOT treat \" as an escaped quote inside double quotes.
    // It is impossible to safely escape a double quote inside a double-quoted argument for cmd.exe
    // in a way that is also compatible with typical C runtime argument parsing (CommandLineToArgvW).
    // Since this helper is used for paths and simple enum strings where quotes are invalid anyway,
    // we throw an exception if a quote is found to prevent command injection.
    std::string out = "\"";
    for (char c : arg) {
        if (c == '"') {
            throw std::invalid_argument("Security error: Double quotes are not allowed in shell arguments on Windows to prevent command injection.");
        } else if (c == '\\') {
            // Note: Internal backslashes don't need doubling inside quotes unless they precede a quote.
            // Since quotes are banned, internal backslashes are safe.
            out += "\\";
        } else {
            out += c;
        }
    }
    // Escape trailing backslashes so they don't escape the closing quote
    if (!arg.empty() && arg.back() == '\\') {
        std::size_t backslash_count = 0;
        for (auto it = arg.rbegin(); it != arg.rend(); ++it) {
            if (*it == '\\') backslash_count++;
            else break;
        }
        out.append(backslash_count, '\\');
    }
    out += "\"";
    return out;
#else
    // POSIX sh escaping: wrap in single quotes, escape single quotes inside
    std::string out = "'";
    for (char c : arg) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
#endif
}

void print_usage() {
    std::cout << "NeuroForge demo\n"
              << "Usage: neuroforge.exe [options]\n\n"
              << "Options:\n"
              << "  --help                                 Show this help\n"
              << "  --steps=N                              Number of processing steps (default: 1)\n"
              << "  --step-ms=MS                           Milliseconds per step (default: 10)\n"
              << "  --enable-learning                      Enable learning system (default: off)\n"
              << "  --gpu                                   Prefer GPU acceleration when available (optional)\n"
              << "  --hebbian-rate=R                       Hebbian learning rate (default: from code)\n"
              << "  --stdp-rate=R                          STDP learning rate (default: from code)\n"
              << "  --stdp-rate-multiplier=M               Global STDP rate multiplier (> 0)\n"
              << "  --attention-boost=F                    Attention boost factor (> 0; default: from code)\n"
              << "  --homeostasis[=on|off]                 Enable or disable homeostasis (default: from code)\n"
              << "  --consolidation-interval=MS            Consolidation/update interval in milliseconds\n"
              << "  -a, --alpha=F                          Phase-4 novelty weight alpha (default: 0.50)\n"
              << "  -g, --gamma=F                          Phase-4 task weight gamma (default: 1.00)\n"
              << "  -u, --eta=F                            Phase-4 uncertainty weight eta (default: 0.20)\n"
              << "  -l, --lambda=F                         Phase-4 eligibility decay lambda in [0,1] (default: 0.90)\n"
              << "  -e, --eta-elig=F                       Phase-4 eligibility update rate etaElig (default: 0.50)\n"
              << "  -k, --kappa=F                          Phase-4 reward-to-weight scale kappa (default: 0.15)\n"
              << "  --phase4-unsafe[=on|off]               Bypass validation of Phase-4 params (default: off)\n"
              << "  --attention-mode=none|external|saliency|topk  Attention modulation mode (default: external)\n"
              << "  --p-gate=F                             Probability [0,1] to apply a plasticity update per synapse\n"
              << "  --competence-mode=off|scale-pgate|scale-lr  Competence gating mode (default: off)\n"
              << "  --competence-rho=F                   EMA rate for competence update in [0,1] (default: from code)\n"
              << "  --auto-eligibility=on|off              Automatic eligibility accumulation (default: off)\n"
              << "  --homeostasis-eta=F                    Step size for homeostatic scaling (>= 0)\n"
              << "  --attention-Amin=F                     Min attention gain (> 0)\n"
              << "  --attention-Amax=F                     Max attention gain (>= Amin)\n"
              << "  --attention-anneal-ms=MS               Linear anneal duration for attention (0 = off)\n"
              << "  --chaos-steps=N                        Steps in chaos/explore phase (default: 0)\n"
              << "  --consolidate-steps=N                  Steps in consolidation phase (default: 0)\n"
              << "  --novelty-window=N                     Window size for novelty/saliency (0 = off)\n"
              << "  --prune-threshold=F                    Prune/freeze synapses with |w| < F during consolidation\n"
              << "  --snapshot-csv=PATH                    Export synapse snapshot (pre,post,weight) to CSV at end of run\n"
              << "  --snapshot-live=PATH                   Periodically export snapshot CSV to PATH during run (overwrites)\n"
              << "  --snapshot-interval=MS                 Interval for --snapshot-live in ms (default: 1000)\n"
              << "  --heatmap-view[=on|off]                Live synapse weight heatmap via OpenCV window (default: off)\n"
              << "  --heatmap-interval=MS                  Heatmap refresh interval in ms (default: 1000)\n"
              << "  --heatmap-size=N                       Heatmap resolution N (NxN) (default: 256)\n"
              << "  --heatmap-threshold=F                  Minimum |weight| to include (default: 0.0)\n"
              << "  --viewer[=on|off]                      Launch 3D viewer and stream live synapses (default: off)\n"
              << "  --viewer-exe=PATH                      Path to neuroforge_viewer executable (optional)\n"
              << "  --viewer-layout=shells|layers          3D layout strategy for neurons (default: shells)\n"
              << "  --viewer-refresh-ms=MS                 Viewer refresh interval in ms (default: 1000)\n"
              << "  --viewer-threshold=F                   Minimum |weight| to draw in viewer (default: 0.0)\n"
              // Sandbox window options (Windows-only)
              << "  --sandbox[=on|off]                     Enable browser sandbox window (default: off)\n"
              << "  --sandbox-url=URL                      Navigate sandbox to a URL (default: https://www.youtube.com)\n"
              << "  --sandbox-size=WxH                     Sandbox window size (default: 1280x720)\n"
              << "  --no-web-actions[=on|off]              Disable web actions in sandbox (bare flag = on)\n"
              << "  --simulate-blocked-actions=N           Simulate N blocked actions per step (debug)\n"
              << "  --simulate-rewards=N                   Simulate N reward events per step (debug)\n"
              << "  --save-brain=PATH                      Save brain checkpoint (JSON) to PATH at end of run\n"
              << "  --load-brain=PATH                      Load brain checkpoint (JSON) from PATH at startup (skips demo init)\n"
              << "  --memory-db=PATH                       Enable memory database logging (experimental; no-op if unsupported)\n"
              << "  --memdb-debug[=on|off]                 Verbose MemoryDB debug logging (default: off)\n"
              << "  --memdb-color[=auto|on|off]            Colorize MemoryDB debug output; bare flag = auto (TTY only), default: auto (TTY only)\n"
              << "  --memdb-interval=MS                    Periodic MemoryDB logging interval in ms (default: 1000)\n"
              << "  --reward-interval=MS                   Periodic reward logging interval in ms (default: 1000)\n"
              << "  --list-episodes=RUN_ID                 List episodes for RUN_ID from MemoryDB and exit\n"
              << "  --recent-rewards=RUN_ID[,LIMIT]        List recent rewards for RUN_ID (optional LIMIT, default 10) and exit\n"
              << "  --recent-run-events=RUN_ID[,LIMIT]     List recent run events for RUN_ID (optional LIMIT, default 10) and exit\n"
              << "  --list-runs                            List all runs in MemoryDB and exit\n"
              << "\nContext Hooks:\n"
              << "  --context-gain=F                      Context sampling gain (default: 1.0)\n"
              << "  --context-update-ms=MS                Context sampling interval in ms (default: 1000)\n"
              << "  --context-window=N                    Context window size for recent samples (default: 5)\n"
              << "  --context-peer=NAME,GAIN,UPDATE_MS,WINDOW[,LABEL]  Register a context peer with sampling config; LABEL optional\n"
              << "  --context-couple=PEER_A:PEER_B,WEIGHT              Couple two peers with influence weight in [0,1]\n"
              << "\nM6 Memory Internalization:\n"
              << "  --hippocampal-snapshots[=on|off]       Enable hippocampal snapshotting (default: off)\n"
              << "  --memory-independent[=on|off]          Enable memory-independent learning (default: off)\n"
              << "  --consolidation-interval-m6=MS         Memory consolidation interval in ms (default: 1000)\n"
              << "\nM7 Autonomous Operation:\n"
              << "  --autonomous-mode[=on|off]             Enable autonomous operation mode (default: off)\n"
              << "  --substrate-mode=off|mirror|train|native  Neural substrate operation mode (default: off)\n"
              << "  --curiosity-threshold=F                Curiosity-driven task threshold in [0,1] (default: 0.3)\n"
              << "  --uncertainty-threshold=F              Uncertainty-based task threshold in [0,1] (default: 0.4)\n"
              << "  --prediction-error-threshold=F         Prediction error task threshold in [0,1] (default: 0.5)\n"
              << "  --max-concurrent-tasks=N               Maximum concurrent autonomous tasks (default: 5)\n"
              << "  --task-generation-interval=MS          Task generation interval in ms (default: 1000)\n"
              << "  --eliminate-scaffolds[=on|off]         Enable external scaffold elimination (default: off)\n"
              << "  --autonomy-metrics[=on|off]            Enable autonomy measurement system (default: off)\n"
              << "  --autonomy-target=F                    Target autonomy level in [0,1] (default: 0.9)\n"
              << "  --motivation-decay=F                   Motivation signal decay rate in [0,1] (default: 0.95)\n"
              << "  --exploration-bonus=F                  Exploration behavior bonus (>= 0, default: 0.2)\n"
              << "  --novelty-memory-size=N                Novelty detection memory size (>= 1, default: 100)\n"
              << "  --enable-selfnode[=on|off]             Enable SelfNode integration in autonomous loop (default: off)\n"
              << "  --enable-pfc[=on|off]                  Enable PrefrontalCortex integration in autonomous loop (default: off)\n"
              << "  --enable-motor-cortex[=on|off]         Enable MotorCortex integration in autonomous loop (default: off)\n"
              << "\nSpike overlays (3D viewer):\n"
              << "  --spikes-live=PATH                    Periodically export recent spikes CSV to PATH (overwrites)\n"
              << "  --spike-ttl=SEC                       Time-to-live for spikes window in seconds (default: 2.0)\n"
             << "\nDemo selection:\n"
              << "  --vision-demo[=on|off]                 Enable vision demo (default: off unless built with NF_ENABLE_VISION_DEMO)\n"
              << "  --audio-demo[=on|off]                  Enable audio demo (default: off)\n"
              << "  --motor-cortex[=on|off]                Enable motor cortex demo (default: off)\n"
              << "  --social-perception[=on|off]           Enable advanced social perception with face masking and gaze vectors (default: off)\n"
              << "  --social-view[=on|off]                 Live social perception visualization via OpenCV window (default: off)\n"
              << "  --audio-mic[=on|off]                   Use live microphone input (Windows only; default: off)\n"
              << "  --audio-system[=on|off]                Use system loopback audio (Windows; default: off)\n"
              << "  --audio-file=PATH                      Use audio from WAV file (mono 16-bit PCM)\n"
              << "  --camera-index=N                       Select camera device index (default: 0)\n"
              << "  --camera-backend=any|msmf|dshow        Force OpenCV backend (Windows: msmf or dshow; default: any)\n"
              << "  --vision-source=camera|screen|maze|synthetic  Select visual input source (default: camera)\n"
              << "  --retina-screen-rect=X,Y,W,H           Screen rectangle for --vision-source=screen (default: 0,0,1280,720)\n"
              << "  --foveation[=on|off]                    Enable dynamic retina focusing (default: off)\n"
              << "  --fovea-size=WxH                        Fovea size in pixels (default: 640x360)\n"
             << "  --fovea-mode=cursor|center|attention    Fovea follow mode (default: cursor)\n"
              << "  --fovea-alpha=F                        Fovea center EMA smoothing in [0,1] (default: 0.3)\n"
              << "  --youtube-mode[=on|off]                Preset: vision=screen, audio=system (default: off)\n"
              << "\nVision encoder config:\n"
              << "  --vision-grid=N                        Vision grid size G (input length = G*G; default: 16)\n"
              << "  --vision-edge[=on|off]                 Include edge magnitude in fusion (default: on)\n"
              << "  --vision-edge-weight=F                 Edge weight (default: 0.6)\n"
              << "  --vision-intensity-weight=F            Intensity weight (default: 0.4)\n"
              << "  --vision-motion[=on|off]               Include simple motion term (default: off)\n"
              << "  --vision-motion-weight=F               Motion weight (default: 0.3)\n"
              << "  --vision-temporal-decay=F              Reserved EMA decay in [0,1] (default: 0.9)\n"
             << "\nAudio encoder config:\n"
              << "  --audio-samplerate=N                   Audio sample rate (default: 16000)\n"
              << "  --audio-feature-bins=N                 Output feature bins (default: 256)\n"
              << "  --audio-spectral-bins=N                Internal spectral bins (default: 64)\n"
              << "  --audio-mel-bands=N                    Mel bands (default: 64)\n"
              << "  --audio-preemphasis[=on|off]           Enable pre-emphasis (default: on)\n"
             << "\nMultimodal options:\n"
              << "  --cross-modal[=on|off]                 Enable Visual<->Auditory cross-modal connectivity (default: off)\n"
             << "\nMaze demo:\n"
              << "  --maze-demo[=on|off]                   Enable simple grid maze demo (default: off)\n"
              << "  --maze-first-person[=on|off]           Enable first-person visual navigation mode (default: off)\n"
              << "  --maze-size=N                          Maze grid size N x N (default: 8)\n"
              << "  --maze-wall-density=F                  Fraction of cells as walls in [0,0.45] (default: 0.20)\n"
              << "  --epsilon=F                            Epsilon-greedy rate in [0,1] (debug / Q-learning mode)\n"
              << "  --softmax-temp=F                       Softmax temperature > 0 for neural-style stochastic policy (default: 0.5)\n"
              << "  --maze-view[=on|off]                   Live maze visualization via OpenCV window (default: off)\n"
              << "  --maze-view-interval=MS                Maze view refresh interval in ms (default: 300)\n"
              << "  --maze-max-episode-steps=N             Terminate episode as failure after N steps without reaching goal (default: 4*N*N)\n"
              << "  --maze-shaping=off|euclid|manhattan    Potential-based shaping mode (default: off)\n"
              << "  --maze-shaping-k=F                     Shaping scale beta (default: 0.01)\n"
              << "  --maze-shaping-gamma=F                 Shaping discount gamma in [0,1] (default: 0.99)\n"
              << "  --episode-csv=PATH                     Append per-episode metrics to PATH as CSV\n"
              << "  --summary[=on|off]                     Print end-of-run episode summary (default: off)\n"
              << "  --qlearning[=on|off]                   Use Q-learning baseline policy (default: off; neural control is default)\n"
              << "  --hybrid-lambda=F                      Blend motor cortex (lambda) with Q-table (1-lambda); 1=motor only, 0=Q only; omit to use pure neural or pure Q based on --qlearning\n"
              << "  --teacher-policy=none|greedy|bfs       Optional maze teacher policy (default: none)\n"
              << "  --teacher-mix=F                        Blend teacher one-hot into scores in [0,1] (default: 0.0)\n"
              << "\nMimicry shaping (Phase-5, optional):\n"
              << "  --mimicry[=on|off]                     Enable mimicry term in shaped reward (default: off)\n"
              << "  --mimicry-weight=F                     Weight mu for mimicry term (default: 0.0)\n"
              << "  --mimicry-internal[=on|off]           Route Phase A similarity/novelty internally in LearningSystem (default: off)\n"
             << "  --teacher-embed=PATH                   Path to teacher embedding file (comma/space-separated floats)\n"
             << "  --student-embed=PATH                   Path to initial student embedding file (optional)\n"
              << "                                         When maze demo is active, the student embedding is updated each step from the blended policy scores.\n"
              << "  --mirror-mode=off|vision|audio         Use sensory features as student embedding source instead of action scores (default: off)\n"
              << "  --student-learning-rate=F              Set Phase A student learning rate (default: 0.05)\n"
             << "\nUnified substrate (WM + Phase C + SurvivalBias + Language):\n"
             << "  --unified-substrate[=on|off]          Enable unified substrate run (default: off)\n"
             << "  --wm-neurons=N                         Override WM/binding/sequence neurons per region (default: 64)\n"
             << "  --phasec-neurons=N                     Override Phase C neurons per region (default: 64)\n"
             << "  --adaptive=on|off                      Toggle unified adaptive reflection (default: on)\n"
              << "  --survival-bias=on|off                 Toggle SurvivalBias effector in unified mode (default: on)\n"
             << "\nDataset ingestion:\n"
             << "  --dataset-triplets=PATH                Root of triplet dataset (audio/text/images)\n"
             << "  --dataset-mode=triplets               Enable triplet ingestion mode\n"
             << "  --dataset-limit=N                     Limit number of triplets loaded\n"
             << "  --dataset-shuffle[=on|off]            Shuffle loaded triplets (default: off)\n"
             << "  --reward-scale=F                      Scale delivered reward (default: 1.0)\n"
             << "\nLanguage/Phase A (experimental):\n"
              << "  --phase5-language[=on|off]            Initialize Phase-5 LanguageSystem (default: off)\n"
              << "  --phase-a[=on|off]                     Initialize Phase A Baby Mimicry system (requires LanguageSystem) (default: off)\n"
              << "  --phase-a-similarity-threshold=F       Set Phase A similarity success threshold (default: 0.6)\n"
              << "  --phase-a-novelty-threshold=F          Set Phase A novelty success threshold (default: 0.1)\n"
              << "  --phase-a-ema[=on|off]                 Toggle Phase A EMA stabilizer (default: on)\n"
              << "  --phase-a-ema-min=F                    Minimum EMA coefficient alpha_min (default: 0.02)\n"
              << "  --phase-a-ema-max=F                    Maximum EMA coefficient alpha_max (default: 0.2)\n"
              << "  --phase-a-replay-interval=N            Replay top attempts every N steps (>=1)\n"
              << "  --phase-a-replay-top-k=K               Number of past attempts to replay (>=1)\n"
              << "  --phase-a-replay-boost=F               Scale reward during replay (>=0; default: 1.0)\n"
              << "  --phase-a-replay-lr-scale=F            Scale learning rate during replay (>=0; default: 1.0)\n"
              << "  --phase-a-replay-include-hard-negatives=on|off  Enable hard-negative replay (default: on)\n"
              << "  --phase-a-replay-hard-k=K               Number of hard negatives to include (>=1; default: 3)\n"
              << "  --phase-a-replay-repulsion-weight=F     Repulsion weight for hard negatives (>=0; default: 0.5)\n"
              << "  --phase-a-export=DIR                   Export Phase A JSON to DIR at end of run\n"
<< "  --phase6[=on|off]                      Enable Phase 6 Reasoner (shadow logging; no behavior changes)\n"
<< "  --phase6-active=on|off|audit           Phase 6 control mode (default: off)\n"
<< "  --phase6-margin=F                      Override margin in [0,1] (default: 0.08)\n"
<< "  --phase7[=on|off]                      Initialize Phase 7 Affective State and Reflection (requires MemoryDB)\n"
<< "  --phase7-affect[=on|off]               Initialize Phase 7 Affective State only (requires MemoryDB)\n"
<< "  --phase7-reflect[=on|off]              Initialize Phase 7 Reflection only (requires MemoryDB)\n"
<< "  --phase8[=on|off]                      Initialize Phase 8 Goal System (default: on)\n"
<< "  --phase9[=on|off]                     Enable/disable Phase 9 metacognition (default: on)\n"
             << "  --phase9-modulation[=on|off]           Enable Phase 9 metacog modulation (default: off)\n"
             << "  --phase10[=on|off]                    Enable/disable Phase 10 self-explanation (default: on)\n"
             << "  --phase11[=on|off]                    Enable/disable Phase 11 self-revision (default: on)\n"
             << "  --phase11-revision-interval=N         Revision interval in ms (default: 300000)\n"
             << "  --phase11-min-gap-ms=N                Minimum gap between revisions in ms (default: 60000)\n"
             << "  --phase11-outcome-window-ms=N         Outcome evaluation pre/post window in ms (default: 60000)\n"
             << "  --revision-threshold=F                Threshold for triggering self-revision (default: 0.3)\n"
             << "  --revision-mode=MODE                  Mode for self-revision: conservative|moderate|aggressive (default: moderate)\n"
              << "  --stagec[=on|off]                     Enable/disable Stage C v1 autonomy gating (default: off)\n"
              << "  --phase13[=on|off]                    Enable/disable Phase 13 autonomy envelope (default: on)\n"
              << "  --phase13-window=N                    Autonomy analysis window size (default: 10)\n"
              << "  --phase13-trust-tighten=F             Self-trust tighten threshold (default: 0.35)\n"
              << "  --phase13-trust-expand=F              Self-trust expand threshold (default: 0.70)\n"
              << "  --phase13-consistency-tighten=F       Self-consistency tighten threshold (default: 0.50)\n"
              << "  --phase13-consistency-expand=F        Self-consistency expand threshold (default: 0.80)\n"
              << "  --phase13-contraction-hysteresis-ms=N Contraction hysteresis in ms (default: 60000)\n"
              << "  --phase13-expansion-hysteresis-ms=N   Expansion hysteresis in ms (default: 60000)\n"
              << "  --phase13-min-log-interval-ms=N       Minimum log interval in ms (default: 30000)\n"
              << "  --phase14[=on|off]                    Enable/disable Phase 14 Meta-Reasoner (default: on)\n"
              << "  --phase14-window=N                    Meta-reasoner analysis window size (default: 10)\n"
              << "  --phase14-trust-degraded=F            Trust level considered degraded (default: 0.40)\n"
              << "  --phase14-rmse-degraded=F             RMSE considered degraded (default: 0.35)\n"
              << "  --phase15[=on|off]                    Enable/disable Phase 15 Ethics Regulator (default: on)\n"
              << "  --phase15-window=N                    Ethics regulator analysis window size (default: 5)\n"
              << "  --phase15-risk-threshold=F            Ethics risk threshold (default: 0.60)\n"
              << "  Note: Phase A embedding dimension is auto-derived in this order:\n"
              << "        1) teacher vector length from --teacher-embed\n"
              << "        2) mirror mode: vision uses G*G from --vision-grid; audio uses --audio-feature-bins\n"
              << "        3) otherwise the Phase A config default\n"
              << "\nTelemetry:\n"
              << "  --telemetry-extended[=on|off]          Include Phase A last-attempt metrics in experience snapshots (default: off)\n"
              << "\nMachine-readable logs:\n"
              << "  --log-json[=PATH|on|off]               Emit line-delimited JSON events (stdout by default; append to PATH if provided)\n"
              << "  --log-json-sample=N                    Log every Nth event (default 1 = no sampling)\n"
              << "  --log-json-events=list                 Comma-separated allowlist of events to log; items are 'event' or 'Phase:event'\n"
              << "                                         Examples: --log-json-events=decision,episode_end or A:decision,B:reward\n"
              << "\nPhase C (Global Workspace prototype):\n"
              << "  --phase-c[=on|off]                     Run Phase C variable-binding/sequence task (default: off)\n"
              << "  --phase-c-mode=binding|sequence        Select Phase C task mode (default: binding)\n"
              << "  --phase-c-out=PATH                     Output directory for Phase C CSV logs (default: PhaseC_Logs)\n"
              << "  --phase-c-wm-capacity=N                WorkingMemory capacity (default: 6)\n"
              << "  --binding-threshold=F                  Activation threshold for binding regions in [0,1] (default: 0.7)\n"
              << "  --sequence-threshold=F                 Activation threshold for sequence regions in [0,1] (default: 0.6)\n"
              << "  --binding-coherence-min=F              Minimum coherence to accept binding assemblies in [0,1] (default: 0.5)\n"
              << "  --sequence-coherence-min=F             Minimum coherence to accept sequence assemblies in [0,1] (default: 0.4)\n"
              << "  --prune-coherence-threshold=F          Prune assemblies below coherence in [0,1] (default: 0.3)\n"
              << "  --phase-c-wm-decay=F                   WorkingMemory decay per step in (0,1] (default: 0.90)\n"
              << "  --phase-c-seq-window=N                 Optional: keep at most N recent sequence tokens in WM (0 = unlimited; default: 0)\n"
              << "  --phase-c-seed=N                       RNG seed (default: random if omitted or 0)\n"
              << "  --phase-c-survival-bias[=on|off]       Enable SurvivalBias modulation and telemetry (default: off)\n"
              << "  --phase-c-variance-sensitivity=F       Scale variance contribution to risk in SurvivalBias (default: 1.0)\n"
              << "  --phase-c-survival-scale=F             Scale shaped reward magnitude (default: 1.0)\n"
              << "  --hazard-density=F                     Fixed hazard rate in [0,1]; >0 overrides audio; 0 = audio RMS fallback (default: unset)\n"
              << "  --phase-c-hazard-weight=F              Hazard coherence down-modulation weight in [0,1] (default: 0.2)\n"
              << "  --phase-c-hazard-alpha=F               Sensitivity of modulation to external hazard in [0,1] (default: 0.0)\n"
              << "  --phase-c-hazard-beta=F                Sensitivity of modulation to arousal in [0,1] (default: 0.0)\n"
              << "  --phase-c-lag-align=N                  Offset reward log step by N (default: 0)\n"
              << "  Note: Phase C runs as an independent demo path and does not currently coexist with Phase A/B\n"
              << "        in the same invocation. To collect A/B and C telemetry, run separate executions.\n"
              << "\nRegion creation:\n"
              << "  --add-region=KEY[:NAME[:COUNT]]        Create a region by registry key; NAME defaults to KEY; COUNT (>=0) creates that many neurons.\n"
              << "                                         Examples: --add-region=visual:VisIn:1024 --add-region=hippocampus\n"
              << "  --list-regions                         List available region keys/aliases and exit\n"
              << "\nEmergence mode:\n"
              << "  --emergent-only[=on|off]               Force pure emergent control: disable Q-learning, teacher mix, epsilon/softmax; action = argmax(neural)\n"
             << "  --true-emergence[=on|off]              Alias for --emergent-only\n"
             << "\nUnified substrate:\n"
             << "  --unified-substrate[=on|off]          Run WM + Phase C + SurvivalBias + Language integration concurrently\n"
             << std::endl;

    // Dynamically list currently-registered region keys/aliases for convenience
    try {
        auto keys = NeuroForge::Core::RegionRegistry::instance().listKeys();
        if (!keys.empty()) {
            std::cout << "Registered region keys/aliases (sorted):\n";
            for (const auto& k : keys) {
                std::cout << "  " << k << "\n";
            }
        }
    } catch (...) {
        // Ignore any errors during help printing
    }
}

bool starts_with(const std::string &s, const std::string &prefix) {
    return s.rfind(prefix, 0) == 0; // prefix at position 0
}

bool parse_on_off_flag(const std::string& v, bool& out) {
    std::string vl = v;
    std::transform(vl.begin(), vl.end(), vl.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (vl == "1" || vl == "true" || vl == "on") { out = true; return true; }
    if (vl == "0" || vl == "false" || vl == "off") { out = false; return true; }
    return false;
}

// Helper: handle audio-related CLI arguments to reduce nesting in main
static bool handle_audio_arg(const std::string& arg, NeuroForge::Encoders::AudioEncoder::Config& acfg) {
    if (starts_with(arg, "--audio-samplerate=")) {
        auto v = arg.substr(std::string("--audio-samplerate=").size());
        try { acfg.sample_rate = std::stoi(v); if (acfg.sample_rate <= 0) { std::cerr << "Error: --audio-samplerate must be positive" << std::endl; return true; } }
        catch (...) { std::cerr << "Error: invalid integer for --audio-samplerate" << std::endl; return true; }
        return true;
    }
    if (starts_with(arg, "--audio-feature-bins=")) {
        auto v = arg.substr(std::string("--audio-feature-bins=").size());
        try { acfg.feature_bins = std::stoi(v); if (acfg.feature_bins <= 0) { std::cerr << "Error: --audio-feature-bins must be positive" << std::endl; return true; } }
        catch (...) { std::cerr << "Error: invalid integer for --audio-feature-bins" << std::endl; return true; }
        return true;
    }
    if (starts_with(arg, "--audio-spectral-bins=")) {
        auto v = arg.substr(std::string("--audio-spectral-bins=").size());
        try { acfg.spectral_bins = std::stoi(v); if (acfg.spectral_bins <= 0) { std::cerr << "Error: --audio-spectral-bins must be positive" << std::endl; return true; } }
        catch (...) { std::cerr << "Error: invalid integer for --audio-spectral-bins" << std::endl; return true; }
        return true;
    }
    if (starts_with(arg, "--audio-mel-bands=")) {
        auto v = arg.substr(std::string("--audio-mel-bands=").size());
        try { acfg.mel_bands = std::stoi(v); if (acfg.mel_bands <= 0) { std::cerr << "Error: --audio-mel-bands must be positive" << std::endl; return true; } }
        catch (...) { std::cerr << "Error: invalid integer for --audio-mel-bands" << std::endl; return true; }
        return true;
    }
    if (arg == "--audio-preemphasis") { acfg.pre_emphasis = true; return true; }
    if (starts_with(arg, "--audio-preemphasis=")) {
        auto v = arg.substr(std::string("--audio-preemphasis=").size());
        if (!parse_on_off_flag(v, acfg.pre_emphasis)) { std::cerr << "Error: --audio-preemphasis must be on|off|true|false|1|0" << std::endl; return true; }
        return true;
    }
    return false;
}

static bool handle_learning_arg(
    const std::string& arg,
    NeuroForge::Core::LearningSystem::Config& lconf,
    bool& hebbian_rate_set,
    bool& stdp_rate_set,
    bool& stdp_mult_set,
    bool& attention_boost_set,
    bool& homeostasis_set,
    bool& consolidation_interval_set,
    bool& consolidation_strength_set,
    bool& attention_mode_set,
    bool& competence_mode_set,
    bool& p_gate_set,
    bool& competence_rho_set,
    bool& auto_elig_set,
    bool& homeostasis_eta_set,
    bool& chaos_steps_set,
    bool& consolidate_steps_set,
    bool& novelty_window_set,
    bool& prune_threshold_set,
    bool& attention_Amin_set,
    bool& attention_Amax_set,
    bool& attention_anneal_ms_set
) {
    if (starts_with(arg, "--hebbian-rate=")) {
        auto v = arg.substr(std::string("--hebbian-rate=").size());
        try { lconf.hebbian_rate = std::stof(v); hebbian_rate_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --hebbian-rate" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--stdp-rate=")) {
        auto v = arg.substr(std::string("--stdp-rate=").size());
        try { lconf.stdp_rate = std::stof(v); stdp_rate_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --stdp-rate" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--stdp-rate-multiplier=")) {
        auto v = arg.substr(std::string("--stdp-rate-multiplier=").size());
        try { lconf.stdp_rate_multiplier = std::stof(v); stdp_mult_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --stdp-rate-multiplier" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--attention-boost=")) {
        auto v = arg.substr(std::string("--attention-boost=").size());
        try { lconf.attention_boost_factor = std::stof(v); attention_boost_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --attention-boost" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--homeostasis") { lconf.enable_homeostasis = true; homeostasis_set = true; return true; }
    if (starts_with(arg, "--homeostasis=")) {
        auto v = arg.substr(std::string("--homeostasis=").size());
        if (!parse_on_off_flag(v, lconf.enable_homeostasis)) { std::cerr << "Error: --homeostasis must be on|off|true|false|1|0" << std::endl; exit(2); }
        homeostasis_set = true; return true;
    }
    if (starts_with(arg, "--consolidation-interval=")) {
        auto v = arg.substr(std::string("--consolidation-interval=").size());
        try { int ms = std::stoi(v); if (ms < 0) { std::cerr << "Error: --consolidation-interval must be non-negative" << std::endl; exit(2); }
              lconf.update_interval = std::chrono::milliseconds(ms); consolidation_interval_set = true; }
        catch (...) { std::cerr << "Error: invalid integer for --consolidation-interval" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--consolidation-strength=")) {
        auto v = arg.substr(std::string("--consolidation-strength=").size());
        try { lconf.consolidation_strength = std::stof(v); consolidation_strength_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --consolidation-strength" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--attention-mode=")) {
        auto v = arg.substr(std::string("--attention-mode=").size());
        std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (vlow == "none" || vlow == "off") {
            lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::Off;
        } else if (vlow == "external" || vlow == "map" || vlow == "externalmap") {
            lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::ExternalMap;
        } else if (vlow == "saliency") {
            lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::Saliency;
        } else if (vlow == "topk" || vlow == "top-k") {
            lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::TopK;
        } else {
            std::cerr << "Error: --attention-mode must be one of: none, external, saliency, topk" << std::endl; exit(2);
        }
        attention_mode_set = true;
        if (lconf.attention_mode != NeuroForge::Core::LearningSystem::AttentionMode::Off) {
            lconf.enable_attention_modulation = true;
        }
        return true;
    }
    if (starts_with(arg, "--competence-mode=")) {
        auto v = arg.substr(std::string("--competence-mode=").size());
        std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (vlow == "off" || vlow == "none") {
            lconf.competence_mode = NeuroForge::Core::LearningSystem::CompetenceMode::Off;
        } else if (vlow == "scale-pgate" || vlow == "scale-p_gate" || vlow == "scale-p") {
            lconf.competence_mode = NeuroForge::Core::LearningSystem::CompetenceMode::ScalePGate;
        } else if (vlow == "scale-lr" || vlow == "scale-learning-rates" || vlow == "scale-learning") {
            lconf.competence_mode = NeuroForge::Core::LearningSystem::CompetenceMode::ScaleLearningRates;
        } else {
            std::cerr << "Error: --competence-mode must be one of: off, scale-pgate, scale-lr" << std::endl; exit(2);
        }
        competence_mode_set = true; return true;
    }
    if (starts_with(arg, "--p-gate=")) {
        auto v = arg.substr(std::string("--p-gate=").size());
        try { lconf.p_gate = std::stof(v); p_gate_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --p-gate" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--competence-rho=")) {
        auto v = arg.substr(std::string("--competence-rho=").size());
        try { lconf.competence_rho = std::stof(v); competence_rho_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --competence-rho" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--auto-eligibility=")) {
        auto v = arg.substr(std::string("--auto-eligibility=").size());
        bool auto_elig_enabled = false;
        if (!parse_on_off_flag(v, auto_elig_enabled)) { std::cerr << "Error: --auto-eligibility must be on|off|true|false|1|0" << std::endl; exit(2); }
        auto_elig_set = true;
        return true;
    }
    if (starts_with(arg, "--homeostasis-eta=")) {
        auto v = arg.substr(std::string("--homeostasis-eta=").size());
        try { lconf.homeostasis_eta = std::stof(v); homeostasis_eta_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --homeostasis-eta" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--attention-Amin=")) {
        auto v = arg.substr(std::string("--attention-Amin=").size());
        try { lconf.attention_Amin = std::stof(v); }
        catch (...) { std::cerr << "Error: invalid float for --attention-Amin" << std::endl; exit(2); }
        attention_Amin_set = true; return true;
    }
    if (starts_with(arg, "--attention-Amax=")) {
        auto v = arg.substr(std::string("--attention-Amax=").size());
        try { lconf.attention_Amax = std::stof(v); }
        catch (...) { std::cerr << "Error: invalid float for --attention-Amax" << std::endl; exit(2); }
        attention_Amax_set = true; return true;
    }
    if (starts_with(arg, "--attention-anneal-ms=")) {
        auto v = arg.substr(std::string("--attention-anneal-ms=").size());
        try { lconf.attention_anneal_ms = std::stoi(v); attention_anneal_ms_set = true; }
        catch (...) { std::cerr << "Error: invalid integer for --attention-anneal-ms" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--chaos-steps=")) {
        auto v = arg.substr(std::string("--chaos-steps=").size());
        try { lconf.chaos_steps = std::stoi(v); chaos_steps_set = true; }
        catch (...) { std::cerr << "Error: invalid integer for --chaos-steps" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--consolidate-steps=")) {
        auto v = arg.substr(std::string("--consolidate-steps=").size());
        try { lconf.consolidate_steps = std::stoi(v); consolidate_steps_set = true; }
        catch (...) { std::cerr << "Error: invalid integer for --consolidate-steps" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--novelty-window=")) {
        auto v = arg.substr(std::string("--novelty-window=").size());
        try { lconf.novelty_window = std::stoi(v); novelty_window_set = true; }
        catch (...) { std::cerr << "Error: invalid integer for --novelty-window" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--prune-threshold=")) {
        auto v = arg.substr(std::string("--prune-threshold=").size());
        try { lconf.prune_threshold = std::stof(v); prune_threshold_set = true; }
        catch (...) { std::cerr << "Error: invalid float for --prune-threshold" << std::endl; exit(2); }
        return true;
    }
    return false;
}

static bool handle_phase4_arg(
    const std::string& arg,
    int argc,
    char* argv[],
    int& i,
    float& alpha_weight,
    bool& alpha_set,
    float& gamma_weight,
    bool& gamma_set,
    float& eta_weight,
    bool& eta_set,
    float& lambda_param,
    bool& lambda_set,
    float& etaElig_param,
    bool& etaElig_set,
    float& kappa_param,
    bool& kappa_set,
    bool& phase4_unsafe
) {
    if (arg == "-a" || starts_with(arg, "-a=") || starts_with(arg, "--alpha=")) {
        std::string v;
        if (arg == "-a") { if (i + 1 >= argc) { std::cerr << "Error: -a requires a float value" << std::endl; exit(2); } v = argv[++i]; }
        else { v = starts_with(arg, "-a=") ? arg.substr(std::string("-a=").size()) : arg.substr(std::string("--alpha=").size()); }
        try { alpha_weight = std::stof(v); alpha_set = true; } catch (...) { std::cerr << "Error: invalid float for --alpha" << std::endl; exit(2); }
        return true;
    }
    if (arg == "-g" || starts_with(arg, "-g=") || starts_with(arg, "--gamma=")) {
        std::string v;
        if (arg == "-g") { if (i + 1 >= argc) { std::cerr << "Error: -g requires a float value" << std::endl; exit(2); } v = argv[++i]; }
        else { v = starts_with(arg, "-g=") ? arg.substr(std::string("-g=").size()) : arg.substr(std::string("--gamma=").size()); }
        try { gamma_weight = std::stof(v); gamma_set = true; } catch (...) { std::cerr << "Error: invalid float for --gamma" << std::endl; exit(2); }
        return true;
    }
    if (arg == "-u" || starts_with(arg, "-u=") || starts_with(arg, "--eta=")) {
        std::string v;
        if (arg == "-u") { if (i + 1 >= argc) { std::cerr << "Error: -u requires a float value" << std::endl; exit(2); } v = argv[++i]; }
        else { v = starts_with(arg, "-u=") ? arg.substr(std::string("-u=").size()) : arg.substr(std::string("--eta=").size()); }
        try { eta_weight = std::stof(v); eta_set = true; } catch (...) { std::cerr << "Error: invalid float for --eta" << std::endl; exit(2); }
        return true;
    }
    if (arg == "-l" || starts_with(arg, "-l=") || starts_with(arg, "--lambda=")) {
        std::string v;
        if (arg == "-l") { if (i + 1 >= argc) { std::cerr << "Error: -l requires a float value" << std::endl; exit(2); } v = argv[++i]; }
        else { v = starts_with(arg, "-l=") ? arg.substr(std::string("-l=").size()) : arg.substr(std::string("--lambda=").size()); }
        try { lambda_param = std::stof(v); lambda_set = true; } catch (...) { std::cerr << "Error: invalid float for --lambda" << std::endl; exit(2); }
        return true;
    }
    if (arg == "-e" || starts_with(arg, "-e=") || starts_with(arg, "--eta-elig=")) {
        std::string v;
        if (arg == "-e") { if (i + 1 >= argc) { std::cerr << "Error: -e requires a float value" << std::endl; exit(2); } v = argv[++i]; }
        else { v = starts_with(arg, "-e=") ? arg.substr(std::string("-e=").size()) : arg.substr(std::string("--eta-elig=").size()); }
        try { etaElig_param = std::stof(v); etaElig_set = true; } catch (...) { std::cerr << "Error: invalid float for --eta-elig" << std::endl; exit(2); }
        return true;
    }
    if (arg == "-k" || starts_with(arg, "-k=") || starts_with(arg, "--kappa=")) {
        std::string v;
        if (arg == "-k") { if (i + 1 >= argc) { std::cerr << "Error: -k requires a float value" << std::endl; exit(2); } v = argv[++i]; }
        else { v = starts_with(arg, "-k=") ? arg.substr(std::string("-k=").size()) : arg.substr(std::string("--kappa=").size()); }
        try { kappa_param = std::stof(v); kappa_set = true; } catch (...) { std::cerr << "Error: invalid float for --kappa" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--phase4-unsafe") { phase4_unsafe = true; return true; }
    if (starts_with(arg, "--phase4-unsafe=")) {
        auto v = arg.substr(std::string("--phase4-unsafe=").size());
        if (!parse_on_off_flag(v, phase4_unsafe)) { std::cerr << "Error: --phase4-unsafe must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    return false;
}
// Helper: handle vision-related CLI arguments to reduce nesting in main
static bool handle_vision_arg(const std::string& arg, NeuroForge::Encoders::VisionEncoder::Config& vcfg) {
    if (starts_with(arg, "--vision-grid=")) {
        auto v = arg.substr(std::string("--vision-grid=").size());
        try { vcfg.grid_size = std::stoi(v); if (vcfg.grid_size <= 0) { std::cerr << "Error: --vision-grid must be positive" << std::endl; return true; } }
        catch (...) { std::cerr << "Error: invalid integer for --vision-grid" << std::endl; return true; }
        return true;
    }
    if (arg == "--vision-edge") { vcfg.use_edge = true; return true; }
    if (starts_with(arg, "--vision-edge=")) {
        auto v = arg.substr(std::string("--vision-edge=").size());
        if (!parse_on_off_flag(v, vcfg.use_edge)) { std::cerr << "Error: --vision-edge must be on|off|true|false|1|0" << std::endl; return true; }
        return true;
    }
    if (starts_with(arg, "--vision-edge-weight=")) {
        auto v = arg.substr(std::string("--vision-edge-weight=").size());
        try { vcfg.edge_weight = std::stof(v); }
        catch (...) { std::cerr << "Error: invalid float for --vision-edge-weight" << std::endl; return true; }
        return true;
    }
    if (starts_with(arg, "--vision-intensity-weight=")) {
        auto v = arg.substr(std::string("--vision-intensity-weight=").size());
        try { vcfg.intensity_weight = std::stof(v); }
        catch (...) { std::cerr << "Error: invalid float for --vision-intensity-weight" << std::endl; return true; }
        return true;
    }
    if (arg == "--vision-motion") { vcfg.use_motion = true; return true; }
    if (starts_with(arg, "--vision-motion=")) {
        auto v = arg.substr(std::string("--vision-motion=").size());
        if (!parse_on_off_flag(v, vcfg.use_motion)) { std::cerr << "Error: --vision-motion must be on|off|true|false|1|0" << std::endl; return true; }
        return true;
    }
    if (starts_with(arg, "--vision-motion-weight=")) {
        auto v = arg.substr(std::string("--vision-motion-weight=").size());
        try { vcfg.motion_weight = std::stof(v); }
        catch (...) { std::cerr << "Error: invalid float for --vision-motion-weight" << std::endl; return true; }
        return true;
    }
    if (starts_with(arg, "--vision-temporal-decay=")) {
        auto v = arg.substr(std::string("--vision-temporal-decay=").size());
        try { vcfg.temporal_decay = std::stof(v); vcfg.temporal_decay = std::clamp(vcfg.temporal_decay, 0.0f, 1.0f); }
        catch (...) { std::cerr << "Error: invalid float for --vision-temporal-decay" << std::endl; return true; }
        return true;
    }
    return false;
}

// Helper: handle telemetry and logging CLI arguments to reduce nesting in main
static bool handle_telemetry_arg(
    const std::string& arg,
    bool& log_json,
    std::string& log_json_path,
    int& log_json_sample_val,
    std::string& log_json_events_csv,
    std::string& memory_db_path,
    bool& memdb_debug,
    int& memdb_interval_ms,
    bool& memdb_interval_cli_set,
    bool& flag_list_runs,
    bool& flag_list_episodes,
    std::string& list_episodes_run_id,
    bool& flag_recent_rewards,
    std::string& recent_rewards_run_id,
    int& recent_rewards_limit,
    bool& flag_recent_run_events,
    std::string& recent_run_events_run_id,
    int& recent_run_events_limit
) {
    // JSON logging
    if (arg == "--log-json") { log_json = true; return true; }
    if (starts_with(arg, "--log-json=")) {
        auto v = arg.substr(std::string("--log-json=").size());
        std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (vlow == "on" || vlow == "true" || vlow == "1") { log_json = true; }
        else if (vlow == "off" || vlow == "false" || vlow == "0") { log_json = false; }
        else { log_json = true; log_json_path = v; }
        return true;
    }
    if (starts_with(arg, "--log-json-sample=")) {
        auto v = arg.substr(std::string("--log-json-sample=").size());
        try { int n = std::max(1, std::stoi(v)); log_json_sample_val = n; }
        catch (...) { std::cerr << "Error: invalid integer for --log-json-sample" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--log-json-events=")) {
        log_json_events_csv = arg.substr(std::string("--log-json-events=").size());
        return true;
    }

    // MemoryDB path and controls
    if (starts_with(arg, "--memory-db=")) {
        memory_db_path = arg.substr(std::string("--memory-db=").size());
        if (memory_db_path.empty()) { std::cerr << "Error: --memory-db requires a file path" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--memdb-debug") { memdb_debug = true; return true; }
    if (starts_with(arg, "--memdb-debug=")) {
        auto v = arg.substr(std::string("--memdb-debug=").size());
        if (!parse_on_off_flag(v, memdb_debug)) { std::cerr << "Error: --memdb-debug must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--memdb-interval=")) {
        auto v = arg.substr(std::string("--memdb-interval=").size());
        try {
            memdb_interval_ms = std::stoi(v);
            if (memdb_interval_ms <= 0) { std::cerr << "Error: --memdb-interval must be > 0" << std::endl; exit(2); }
            memdb_interval_cli_set = true;
        } catch (...) { std::cerr << "Error: invalid integer for --memdb-interval" << std::endl; exit(2); }
        return true;
    }

    // MemoryDB listing flags
    if (arg == "--list-runs") { flag_list_runs = true; return true; }
    if (starts_with(arg, "--list-episodes=")) {
        list_episodes_run_id = arg.substr(std::string("--list-episodes=").size());
        if (list_episodes_run_id.empty()) { std::cerr << "Error: --list-episodes requires RUN_ID" << std::endl; exit(2); }
        flag_list_episodes = true;
        return true;
    }
    if (starts_with(arg, "--recent-rewards=")) {
        auto v = arg.substr(std::string("--recent-rewards=").size());
        if (v.empty()) { std::cerr << "Error: --recent-rewards requires RUN_ID[,LIMIT]" << std::endl; exit(2); }
        auto comma = v.find(',');
        if (comma == std::string::npos) { recent_rewards_run_id = v; }
        else {
            recent_rewards_run_id = v.substr(0, comma);
            auto lims = v.substr(comma + 1);
            try { recent_rewards_limit = std::stoi(lims); if (recent_rewards_limit <= 0) recent_rewards_limit = 10; }
            catch (...) { std::cerr << "Error: invalid LIMIT for --recent-rewards (expected integer)" << std::endl; exit(2); }
        }
        if (recent_rewards_run_id.empty()) { std::cerr << "Error: --recent-rewards requires RUN_ID before comma" << std::endl; exit(2); }
        flag_recent_rewards = true;
        return true;
    }
    if (starts_with(arg, "--recent-run-events=")) {
        auto v = arg.substr(std::string("--recent-run-events=").size());
        if (v.empty()) { std::cerr << "Error: --recent-run-events requires RUN_ID[,LIMIT]" << std::endl; exit(2); }
        auto comma = v.find(',');
        if (comma == std::string::npos) { recent_run_events_run_id = v; }
        else {
            recent_run_events_run_id = v.substr(0, comma);
            auto lims = v.substr(comma + 1);
            try { recent_run_events_limit = std::stoi(lims); if (recent_run_events_limit <= 0) recent_run_events_limit = 10; }
            catch (...) { std::cerr << "Error: invalid LIMIT for --recent-run-events (expected integer)" << std::endl; exit(2); }
        }
        if (recent_run_events_run_id.empty()) { std::cerr << "Error: --recent-run-events requires RUN_ID before comma" << std::endl; exit(2); }
        flag_recent_run_events = true;
        return true;
    }

    return false;
}

// Handle M6 and M7 parameters to reduce nesting in main parameter parsing loop
bool handle_m6_m7_parameters(const std::string& arg,
                             bool& hippocampal_snapshots, bool& hippocampal_snapshots_set,
                             bool& memory_independent, bool& memory_independent_set,
                             int& consolidation_interval_m6, bool& consolidation_interval_m6_set,
                             bool& autonomous_mode, bool& autonomous_mode_set,
                             std::string& substrate_mode, bool& substrate_mode_set,
                             float& curiosity_threshold, bool& curiosity_threshold_set,
                             float& uncertainty_threshold, bool& uncertainty_threshold_set,
                             float& prediction_error_threshold, bool& prediction_error_threshold_set,
                             int& max_concurrent_tasks, bool& max_concurrent_tasks_set,
                             int& task_generation_interval, bool& task_generation_interval_set,
                             bool& eliminate_scaffolds, bool& eliminate_scaffolds_set,
                             bool& autonomy_metrics, bool& autonomy_metrics_set,
                             float& autonomy_target, bool& autonomy_target_set,
                             float& motivation_decay, bool& motivation_decay_set,
                             float& exploration_bonus, bool& exploration_bonus_set,
                             int& novelty_memory_size, bool& novelty_memory_size_set,
                             bool& enable_selfnode, bool& enable_selfnode_set,
                             bool& enable_pfc, bool& enable_pfc_set,
                             bool& enable_motor_cortex, bool& enable_motor_cortex_set) {
    
    // M6 Memory Internalization parameters
    if (arg == "--hippocampal-snapshots") {
        hippocampal_snapshots = true;
        hippocampal_snapshots_set = true;
        return true;
    } else if (starts_with(arg, "--hippocampal-snapshots=")) {
        auto v = arg.substr(std::string("--hippocampal-snapshots=").size());
        if (!parse_on_off_flag(v, hippocampal_snapshots)) {
            std::cerr << "Error: --hippocampal-snapshots must be on|off|true|false|1|0" << std::endl;
            exit(2);
        }
        hippocampal_snapshots_set = true;
        return true;
    } else if (arg == "--memory-independent") {
        memory_independent = true;
        memory_independent_set = true;
        return true;
    } else if (starts_with(arg, "--memory-independent=")) {
        auto v = arg.substr(std::string("--memory-independent=").size());
        if (!parse_on_off_flag(v, memory_independent)) {
            std::cerr << "Error: --memory-independent must be on|off|true|false|1|0" << std::endl;
            exit(2);
        }
        memory_independent_set = true;
        return true;
    } else if (starts_with(arg, "--consolidation-interval-m6=")) {
        auto v = arg.substr(std::string("--consolidation-interval-m6=").size());
        try {
            consolidation_interval_m6 = std::stoi(v);
            if (consolidation_interval_m6 < 0) {
                std::cerr << "Error: --consolidation-interval-m6 must be non-negative" << std::endl;
                exit(2);
            }
            consolidation_interval_m6_set = true;
        } catch (...) {
            std::cerr << "Error: invalid integer for --consolidation-interval-m6" << std::endl;
            exit(2);
        }
        return true;
    }
    
    // M7 Autonomous Operation parameters
    if (arg == "--autonomous-mode") {
        autonomous_mode = true;
        autonomous_mode_set = true;
        return true;
    } else if (starts_with(arg, "--autonomous-mode=")) {
        auto v = arg.substr(std::string("--autonomous-mode=").size());
        if (!parse_on_off_flag(v, autonomous_mode)) {
            std::cerr << "Error: --autonomous-mode must be on|off|true|false|1|0" << std::endl;
            exit(2);
        }
        autonomous_mode_set = true;
        return true;
    } else if (starts_with(arg, "--substrate-mode=")) {
        auto v = arg.substr(std::string("--substrate-mode=").size());
        if (v == "off" || v == "mirror" || v == "train" || v == "native") {
            substrate_mode = v;
            substrate_mode_set = true;
        } else {
            std::cerr << "Error: --substrate-mode must be off|mirror|train|native" << std::endl;
            exit(2);
        }
        return true;
    } else if (starts_with(arg, "--curiosity-threshold=")) {
        auto v = arg.substr(std::string("--curiosity-threshold=").size());
        try {
            curiosity_threshold = std::stof(v);
            if (curiosity_threshold < 0.0f || curiosity_threshold > 1.0f) {
                std::cerr << "Error: --curiosity-threshold must be in [0,1]" << std::endl;
                exit(2);
            }
            curiosity_threshold_set = true;
        } catch (...) {
            std::cerr << "Error: invalid float for --curiosity-threshold" << std::endl;
            exit(2);
        }
        return true;
    } else if (starts_with(arg, "--uncertainty-threshold=")) {
        auto v = arg.substr(std::string("--uncertainty-threshold=").size());
        try {
            uncertainty_threshold = std::stof(v);
            if (uncertainty_threshold < 0.0f || uncertainty_threshold > 1.0f) {
                std::cerr << "Error: --uncertainty-threshold must be in [0,1]" << std::endl;
                exit(2);
            }
            uncertainty_threshold_set = true;
        } catch (...) {
            std::cerr << "Error: invalid float for --uncertainty-threshold" << std::endl;
            exit(2);
        }
        return true;
    } else if (starts_with(arg, "--prediction-error-threshold=")) {
        auto v = arg.substr(std::string("--prediction-error-threshold=").size());
        try {
            prediction_error_threshold = std::stof(v);
            if (prediction_error_threshold < 0.0f || prediction_error_threshold > 1.0f) {
                std::cerr << "Error: --prediction-error-threshold must be in [0,1]" << std::endl;
                exit(2);
            }
            prediction_error_threshold_set = true;
        } catch (...) {
            std::cerr << "Error: invalid float for --prediction-error-threshold" << std::endl;
            exit(2);
        }
        return true;
    } else if (starts_with(arg, "--max-concurrent-tasks=")) {
        auto v = arg.substr(std::string("--max-concurrent-tasks=").size());
        try {
            max_concurrent_tasks = std::stoi(v);
            if (max_concurrent_tasks < 1) {
                std::cerr << "Error: --max-concurrent-tasks must be >= 1" << std::endl;
                exit(2);
            }
            max_concurrent_tasks_set = true;
        } catch (...) {
            std::cerr << "Error: invalid integer for --max-concurrent-tasks" << std::endl;
            exit(2);
        }
        return true;
    } else if (starts_with(arg, "--task-generation-interval=")) {
        auto v = arg.substr(std::string("--task-generation-interval=").size());
        try {
            task_generation_interval = std::stoi(v);
            if (task_generation_interval < 0) {
                std::cerr << "Error: --task-generation-interval must be non-negative" << std::endl;
                exit(2);
            }
            task_generation_interval_set = true;
        } catch (...) {
            std::cerr << "Error: invalid integer for --task-generation-interval" << std::endl;
            exit(2);
        }
        return true;
    } else if (arg == "--eliminate-scaffolds") {
        eliminate_scaffolds = true;
        eliminate_scaffolds_set = true;
        return true;
    } else if (starts_with(arg, "--eliminate-scaffolds=")) {
        auto v = arg.substr(std::string("--eliminate-scaffolds=").size());
        if (!parse_on_off_flag(v, eliminate_scaffolds)) {
            std::cerr << "Error: --eliminate-scaffolds must be on|off|true|false|1|0" << std::endl;
            exit(2);
        }
        eliminate_scaffolds_set = true;
        return true;
    } else if (arg == "--autonomy-metrics") {
        autonomy_metrics = true;
        autonomy_metrics_set = true;
        return true;
    } else if (starts_with(arg, "--autonomy-metrics=")) {
        auto v = arg.substr(std::string("--autonomy-metrics=").size());
        if (!parse_on_off_flag(v, autonomy_metrics)) {
            std::cerr << "Error: --autonomy-metrics must be on|off|true|false|1|0" << std::endl;
            exit(2);
        }
        autonomy_metrics_set = true;
        return true;
    } else if (starts_with(arg, "--autonomy-target=")) {
        auto v = arg.substr(std::string("--autonomy-target=").size());
        try {
            autonomy_target = std::stof(v);
            if (autonomy_target < 0.0f || autonomy_target > 1.0f) {
                std::cerr << "Error: --autonomy-target must be in [0,1]" << std::endl;
                exit(2);
            }
            autonomy_target_set = true;
        } catch (...) {
            std::cerr << "Error: invalid float for --autonomy-target" << std::endl;
            exit(2);
        }
        return true;
    } else if (starts_with(arg, "--motivation-decay=")) {
        auto v = arg.substr(std::string("--motivation-decay=").size());
        try {
            motivation_decay = std::stof(v);
            if (motivation_decay < 0.0f || motivation_decay > 1.0f) {
                std::cerr << "Error: --motivation-decay must be in [0,1]" << std::endl;
                exit(2);
            }
            motivation_decay_set = true;
        } catch (...) {
            std::cerr << "Error: invalid float for --motivation-decay" << std::endl;
            exit(2);
        }
        return true;
    } else if (starts_with(arg, "--exploration-bonus=")) {
        auto v = arg.substr(std::string("--exploration-bonus=").size());
        try {
            exploration_bonus = std::stof(v);
            if (exploration_bonus < 0.0f) {
                std::cerr << "Error: --exploration-bonus must be >= 0" << std::endl;
                exit(2);
            }
            exploration_bonus_set = true;
        } catch (...) {
            std::cerr << "Error: invalid float for --exploration-bonus" << std::endl;
            exit(2);
        }
        return true;
    } else if (starts_with(arg, "--novelty-memory-size=")) {
        auto v = arg.substr(std::string("--novelty-memory-size=").size());
        try {
            novelty_memory_size = std::stoi(v);
            if (novelty_memory_size < 1) {
                std::cerr << "Error: --novelty-memory-size must be >= 1" << std::endl;
                exit(2);
            }
            novelty_memory_size_set = true;
        } catch (...) {
            std::cerr << "Error: invalid integer for --novelty-memory-size" << std::endl;
            exit(2);
        }
        return true;
    } else if (arg == "--enable-selfnode") {
        enable_selfnode = true;
        enable_selfnode_set = true;
        return true;
    } else if (starts_with(arg, "--enable-selfnode=")) {
        auto v = arg.substr(std::string("--enable-selfnode=").size());
        if (!parse_on_off_flag(v, enable_selfnode)) {
            std::cerr << "Error: --enable-selfnode must be on|off|true|false|1|0" << std::endl;
            exit(2);
        }
        enable_selfnode_set = true;
        return true;
    } else if (arg == "--enable-pfc") {
        enable_pfc = true;
        enable_pfc_set = true;
        return true;
    } else if (starts_with(arg, "--enable-pfc=")) {
        auto v = arg.substr(std::string("--enable-pfc=").size());
        if (!parse_on_off_flag(v, enable_pfc)) {
            std::cerr << "Error: --enable-pfc must be on|off|true|false|1|0" << std::endl;
            exit(2);
        }
        enable_pfc_set = true;
        return true;
    } else if (arg == "--enable-motor-cortex") {
        enable_motor_cortex = true;
        enable_motor_cortex_set = true;
        return true;
    } else if (starts_with(arg, "--enable-motor-cortex=")) {
        auto v = arg.substr(std::string("--enable-motor-cortex=").size());
        if (!parse_on_off_flag(v, enable_motor_cortex)) {
            std::cerr << "Error: --enable-motor-cortex must be on|off|true|false|1|0" << std::endl;
            exit(2);
        }
        enable_motor_cortex_set = true;
        return true;
    }
    
    return false; // Parameter not handled
}

// Helper: handle demo and interactive flags to reduce nesting in main
static bool handle_demo_arg(
    const std::string& arg,
    bool& heatmap_view,
    int& heatmap_interval_ms,
    int& heatmap_size,
    float& heatmap_threshold,
    bool& vision_demo,
    bool& audio_demo,
    bool& motor_cortex,
    bool& social_perception,
    bool& social_view,
    bool& cross_modal,
    bool& audio_mic,
    bool& audio_system,
    std::string& audio_file_path,
    int& camera_index,
    std::string& camera_backend,
    std::string& vision_source,
    int& retina_rect_x,
    int& retina_rect_y,
    int& retina_rect_w,
    int& retina_rect_h,
    bool& youtube_mode,
    bool& foveation_enable,
    int& fovea_w,
    int& fovea_h,
    std::string& fovea_mode,
    double& fovea_alpha
) {
    // Heatmap viewer
    if (arg == "--heatmap-view") { heatmap_view = true; return true; }
    if (starts_with(arg, "--heatmap-view=")) {
        auto v = arg.substr(std::string("--heatmap-view=").size());
        if (!parse_on_off_flag(v, heatmap_view)) { std::cerr << "Error: --heatmap-view must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--heatmap-interval=")) {
        auto v = arg.substr(std::string("--heatmap-interval=").size());
        try {
            heatmap_interval_ms = std::stoi(v);
            if (heatmap_interval_ms < 0) { std::cerr << "Error: --heatmap-interval must be non-negative" << std::endl; exit(2); }
        } catch (...) { std::cerr << "Error: invalid integer for --heatmap-interval" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--heatmap-size=")) {
        auto v = arg.substr(std::string("--heatmap-size=").size());
        try {
            heatmap_size = std::stoi(v);
            if (heatmap_size <= 0) { std::cerr << "Error: --heatmap-size must be positive" << std::endl; exit(2); }
        } catch (...) { std::cerr << "Error: invalid integer for --heatmap-size" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--heatmap-threshold=")) {
        auto v = arg.substr(std::string("--heatmap-threshold=").size());
        try { heatmap_threshold = std::stof(v); if (heatmap_threshold < 0.0f) heatmap_threshold = 0.0f; }
        catch (...) { std::cerr << "Error: invalid float for --heatmap-threshold" << std::endl; exit(2); }
        return true;
    }

    // Demo toggles
    if (arg == "--vision-demo") { vision_demo = true; return true; }
    if (starts_with(arg, "--vision-demo=")) {
        auto v = arg.substr(std::string("--vision-demo=").size());
        if (!parse_on_off_flag(v, vision_demo)) { std::cerr << "Error: --vision-demo must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--audio-demo") { audio_demo = true; return true; }
    if (starts_with(arg, "--audio-demo=")) {
        auto v = arg.substr(std::string("--audio-demo=").size());
        if (!parse_on_off_flag(v, audio_demo)) { std::cerr << "Error: --audio-demo must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--motor-cortex") { motor_cortex = true; return true; }
    if (starts_with(arg, "--motor-cortex=")) {
        auto v = arg.substr(std::string("--motor-cortex=").size());
        if (!parse_on_off_flag(v, motor_cortex)) { std::cerr << "Error: --motor-cortex must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--social-perception") { social_perception = true; return true; }
    if (starts_with(arg, "--social-perception=")) {
        auto v = arg.substr(std::string("--social-perception=").size());
        if (!parse_on_off_flag(v, social_perception)) { std::cerr << "Error: --social-perception must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--social-view") { social_view = true; return true; }
    if (starts_with(arg, "--social-view=")) {
        auto v = arg.substr(std::string("--social-view=").size());
        if (!parse_on_off_flag(v, social_view)) { std::cerr << "Error: --social-view must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--cross-modal") { cross_modal = true; return true; }
    if (starts_with(arg, "--cross-modal=")) {
        auto v = arg.substr(std::string("--cross-modal=").size());
        if (!parse_on_off_flag(v, cross_modal)) { std::cerr << "Error: --cross-modal must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--audio-mic") { audio_mic = true; return true; }
    if (starts_with(arg, "--audio-mic=")) {
        auto v = arg.substr(std::string("--audio-mic=").size());
        if (!parse_on_off_flag(v, audio_mic)) { std::cerr << "Error: --audio-mic must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--audio-system") { audio_system = true; return true; }
    if (starts_with(arg, "--audio-system=")) {
        auto v = arg.substr(std::string("--audio-system=").size());
        if (!parse_on_off_flag(v, audio_system)) { std::cerr << "Error: --audio-system must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--audio-file=")) {
        audio_file_path = arg.substr(std::string("--audio-file=").size());
        if (audio_file_path.empty()) { std::cerr << "Error: --audio-file requires a file path" << std::endl; exit(2); }
        return true;
    }

    // Camera and vision source
    if (starts_with(arg, "--camera-index=")) {
        auto v = arg.substr(std::string("--camera-index=").size());
        try { camera_index = std::stoi(v); if (camera_index < 0) { std::cerr << "Error: --camera-index must be non-negative" << std::endl; exit(2); } }
        catch (...) { std::cerr << "Error: invalid integer for --camera-index" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--camera-backend=")) {
        camera_backend = arg.substr(std::string("--camera-backend=").size());
        if (camera_backend != "any" && camera_backend != "msmf" && camera_backend != "dshow") {
            std::cerr << "Error: --camera-backend must be one of: any, msmf, dshow" << std::endl; exit(2);
        }
        return true;
    }
    if (starts_with(arg, "--vision-source=")) {
        auto v = arg.substr(std::string("--vision-source=").size());
        if (v == "camera" || v == "screen" || v == "maze" || v == "synthetic") { vision_source = v; }
        else { std::cerr << "Error: --vision-source must be one of: camera, screen, maze, synthetic" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--retina-screen-rect=")) {
        auto v = arg.substr(std::string("--retina-screen-rect=").size());
        int x=0,y=0,w=0,h=0; char comma;
        std::stringstream ss(v);
        if (!(ss >> x >> comma) || comma != ',' || !(ss >> y >> comma) || comma != ',' || !(ss >> w >> comma) || comma != ',' || !(ss >> h)) {
            std::cerr << "Error: --retina-screen-rect must be X,Y,W,H" << std::endl; exit(2);
        }
        if (w <= 0 || h <= 0) { std::cerr << "Error: --retina-screen-rect width/height must be positive" << std::endl; exit(2); }
        retina_rect_x = x; retina_rect_y = y; retina_rect_w = w; retina_rect_h = h;
        return true;
    }
    if (arg == "--foveation") { foveation_enable = true; return true; }
    if (starts_with(arg, "--foveation=")) {
        auto v = arg.substr(std::string("--foveation=").size());
        if (!parse_on_off_flag(v, foveation_enable)) { std::cerr << "Error: --foveation must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--fovea-size=")) {
        auto v = arg.substr(std::string("--fovea-size=").size());
        int w=0,h=0; char xchar;
        std::stringstream ss2(v);
        if (!(ss2 >> w >> xchar) || xchar != 'x' || !(ss2 >> h) || w <= 0 || h <= 0) {
            std::cerr << "Error: --fovea-size must be WxH with positive integers" << std::endl; exit(2);
        }
        fovea_w = w; fovea_h = h; return true;
    }
    if (starts_with(arg, "--fovea-mode=")) {
        fovea_mode = arg.substr(std::string("--fovea-mode=").size());
        std::transform(fovea_mode.begin(), fovea_mode.end(), fovea_mode.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (!(fovea_mode == "cursor" || fovea_mode == "center" || fovea_mode == "attention")) { std::cerr << "Error: --fovea-mode must be cursor|center|attention" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--fovea-alpha=")) {
        auto v = arg.substr(std::string("--fovea-alpha=").size());
        try { fovea_alpha = std::stod(v); fovea_alpha = std::clamp(fovea_alpha, 0.0, 1.0); }
        catch (...) { std::cerr << "Error: invalid float for --fovea-alpha" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--youtube-mode") { youtube_mode = true; vision_demo = true; audio_demo = true; vision_source = "screen"; audio_system = true; return true; }
    if (starts_with(arg, "--youtube-mode=")) {
        auto v = arg.substr(std::string("--youtube-mode=").size());
        bool on=false; if (!parse_on_off_flag(v, on)) { std::cerr << "Error: --youtube-mode must be on|off|true|false|1|0" << std::endl; exit(2); }
        youtube_mode = on;
        if (on) { vision_demo = true; audio_demo = true; vision_source = "screen"; audio_system = true; }
        return true;
    }

    return false;
}

static bool handle_dataset_arg(
    const std::string& arg,
    std::string& dataset_triplets_root,
    std::string& dataset_mode,
    int& dataset_limit,
    bool& dataset_shuffle,
    double& reward_scale
) {
    if (starts_with(arg, "--dataset-triplets=")) {
        dataset_triplets_root = arg.substr(std::string("--dataset-triplets=").size());
        return true;
    }
    if (starts_with(arg, "--dataset-mode=")) {
        dataset_mode = arg.substr(std::string("--dataset-mode=").size());
        std::transform(dataset_mode.begin(), dataset_mode.end(), dataset_mode.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (dataset_mode != "triplets") { std::cerr << "Error: --dataset-mode must be 'triplets'" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--dataset-limit=")) {
        auto v = arg.substr(std::string("--dataset-limit=").size());
        try { long long ll = std::stoll(v); if (ll < 0) { std::cerr << "Error: --dataset-limit must be non-negative" << std::endl; exit(2); } dataset_limit = static_cast<int>(ll); }
        catch (...) { std::cerr << "Error: invalid integer for --dataset-limit" << std::endl; exit(2); }
        return true;
    }
    if (arg == "--dataset-shuffle") { dataset_shuffle = true; return true; }
    if (starts_with(arg, "--dataset-shuffle=")) {
        auto v = arg.substr(std::string("--dataset-shuffle=").size());
        if (!parse_on_off_flag(v, dataset_shuffle)) { std::cerr << "Error: --dataset-shuffle must be on|off|true|false|1|0" << std::endl; exit(2); }
        return true;
    }
    if (starts_with(arg, "--reward-scale=")) {
        auto v = arg.substr(std::string("--reward-scale=").size());
        try { reward_scale = std::stod(v); if (reward_scale < 0.0) { std::cerr << "Error: --reward-scale must be >= 0" << std::endl; exit(2); } }
        catch (...) { std::cerr << "Error: invalid float for --reward-scale" << std::endl; exit(2); }
        return true;
    }
    return false;
}

// Helper to handle snapshot/spikes/save/load arguments to reduce nesting depth
static bool handle_io_arg(
    const std::string& arg,
    std::string& snapshot_csv_path,
    std::string& snapshot_live_path,
    int& snapshot_interval_ms,
    std::string& spikes_live_path,
    double& spikes_ttl_sec,
    std::string& save_brain_path,
    std::string& load_brain_path
) {
    if (starts_with(arg, "--snapshot-csv=")) {
        snapshot_csv_path = arg.substr(std::string("--snapshot-csv=").size());
        if (snapshot_csv_path.empty()) {
            std::cerr << "Error: --snapshot-csv requires a file path" << std::endl;
            exit(2);
        }
        return true;
    }
    if (starts_with(arg, "--snapshot-live=")) {
        snapshot_live_path = arg.substr(std::string("--snapshot-live=").size());
        if (snapshot_live_path.empty()) {
            std::cerr << "Error: --snapshot-live requires a file path" << std::endl;
            exit(2);
        }
        return true;
    }
    if (starts_with(arg, "--snapshot-interval=")) {
        auto v = arg.substr(std::string("--snapshot-interval=").size());
        try {
            snapshot_interval_ms = std::stoi(v);
            if (snapshot_interval_ms < 0) {
                std::cerr << "Error: --snapshot-interval must be non-negative" << std::endl;
                exit(2);
            }
        } catch (...) {
            std::cerr << "Error: invalid integer for --snapshot-interval" << std::endl;
            exit(2);
        }
        return true;
    }
    if (starts_with(arg, "--spikes-live=")) {
        spikes_live_path = arg.substr(std::string("--spikes-live=").size());
        if (spikes_live_path.empty()) {
            std::cerr << "Error: --spikes-live requires a file path" << std::endl;
            exit(2);
        }
        return true;
    }
    if (starts_with(arg, "--spike-ttl=")) {
        auto v2 = arg.substr(std::string("--spike-ttl=").size());
        try {
            spikes_ttl_sec = std::stod(v2);
            if (spikes_ttl_sec < 0.0) {
                std::cerr << "Error: --spike-ttl must be non-negative" << std::endl;
                exit(2);
            }
        } catch (...) {
            std::cerr << "Error: invalid float for --spike-ttl" << std::endl;
            exit(2);
        }
        return true;
    }
    if (starts_with(arg, "--save-brain=")) {
        save_brain_path = arg.substr(std::string("--save-brain=").size());
        if (save_brain_path.empty()) {
            std::cerr << "Error: --save-brain requires a file path" << std::endl;
            exit(2);
        }
        return true;
    }
    if (starts_with(arg, "--load-brain=")) {
        load_brain_path = arg.substr(std::string("--load-brain=").size());
        if (load_brain_path.empty()) {
            std::cerr << "Error: --load-brain requires a file path" << std::endl;
            exit(2);
        }
        return true;
    }
    return false;
}

// JSON utilities for --log-json
static std::string json_escape(const std::string& s) {
    std::ostringstream o;
    o << std::noskipws;
    for (unsigned char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (c < 0x20) {
                    o << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << static_cast<int>(c)
                      << std::dec << std::nouppercase << std::setfill(' ');
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

static std::string iso8601_utc_now() {
#ifdef _WIN32
    SYSTEMTIME st; GetSystemTime(&st); // UTC
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << st.wYear << '-'
        << std::setw(2) << st.wMonth << '-'
        << std::setw(2) << st.wDay << 'T'
        << std::setw(2) << st.wHour << ':'
        << std::setw(2) << st.wMinute << ':'
        << std::setw(2) << st.wSecond << 'Z';
    return oss.str();
#else
    // Fallback if non-Windows builds lack time facilities here
    return "1970-01-01T00:00:00Z";
#endif
}

// Helper: directory where the executable resides (used to place default output folders near the binary)
static inline std::filesystem::path get_executable_dir() {
#ifdef _WIN32
    wchar_t path_buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path_buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(path_buf).parent_path();
#else
    // Fallback for non-Windows: use current working directory
    return std::filesystem::current_path();
#endif
}

// JSON log filtering/throttling state and helpers
static std::atomic<unsigned long long> g_json_seq{0};
static int g_log_json_sample = 1; // log every Nth allowed event (>=1)
static bool g_log_json_filter_enabled = false;
static std::vector<std::pair<std::string,std::string>> g_log_json_allow; // pair<phase (or ""), event>

static void set_log_json_filters(int sample, const std::string& events_csv) {
    if (sample < 1) sample = 1;
    g_log_json_sample = sample;
    g_log_json_allow.clear();
    g_log_json_filter_enabled = false;
    std::string token;
    for (size_t i = 0; i <= events_csv.size(); ++i) {
        char c = (i < events_csv.size() ? events_csv[i] : ',');
        if (c == ',') {
            // trim token
            size_t b = 0, e = token.size();
            while (b < e && std::isspace(static_cast<unsigned char>(token[b]))) ++b;
            while (e > b && std::isspace(static_cast<unsigned char>(token[e-1]))) --e;
            if (e > b) {
                std::string t = token.substr(b, e - b);
                auto pos = t.find(':');
                if (pos != std::string::npos) {
                    std::string ph = t.substr(0, pos);
                    std::string ev = t.substr(pos + 1);
                    g_log_json_allow.emplace_back(ph, ev);
                } else {
                    g_log_json_allow.emplace_back(std::string(), t);
                }
                g_log_json_filter_enabled = true;
            }
            token.clear();
        } else {
            token.push_back(c);
        }
    }
}

static inline bool parse_phase_event(const std::string& line, std::string& phase, std::string& event) {
    phase.clear(); event.clear();
    auto ppos = line.find("\"phase\":\"");
    if (ppos != std::string::npos) {
        ppos += 9; // length of "phase":" is 9
        auto pend = line.find('"', ppos);
        if (pend != std::string::npos) phase = line.substr(ppos, pend - ppos);
    }
    auto epos = line.find("\"event\":\"");
    if (epos != std::string::npos) {
        epos += 9; // length of "event":" is 9
        auto eend = line.find('"', epos);
        if (eend != std::string::npos) event = line.substr(epos, eend - epos);
    }
    return !event.empty();
}

static void emit_json_line(bool enabled, const std::string& path, const std::string& line) {
    if (!enabled) return;

    // Event filtering
    if (g_log_json_filter_enabled) {
        std::string ph, ev; (void)ph; (void)ev;
        if (parse_phase_event(line, ph, ev)) {
            bool allowed = false;
            for (const auto& pe : g_log_json_allow) {
                const std::string& req_ph = pe.first; const std::string& req_ev = pe.second;
                if (!req_ph.empty()) {
                    if (req_ph == ph && req_ev == ev) { allowed = true; break; }
                } else {
                    if (req_ev == ev) { allowed = true; break; }
                }
            }
            if (!allowed) return; // drop
        }
        // If cannot parse, fall through and log (fail-open)
    }

    // Sampling (global 1-in-N)
    unsigned long long seq = ++g_json_seq;
    if (g_log_json_sample > 1) {
        if ((seq % static_cast<unsigned long long>(g_log_json_sample)) != 0ULL) {
            return; // drop
        }
    }

    // Always echo to stdout as line-delimited JSON
    std::cout << line << std::endl;
    // Optionally append to a file
    if (!path.empty()) {
        std::ofstream ofs(path, std::ios::out | std::ios::app);
        if (ofs) {
            ofs << line << '\n';
        }
    }
}

// Create a tiny demo brain with two regions and sparse interconnections
void create_demo_brain(NeuroForge::Core::HypergraphBrain &brain) {
    using NeuroForge::Core::Region;
    auto regionA = brain.createRegion("DemoCortex", Region::Type::Cortical, Region::ActivationPattern::Asynchronous);
    auto regionB = brain.createRegion("DemoSubcortex", Region::Type::Subcortical, Region::ActivationPattern::Asynchronous);

    if (regionA) regionA->createNeurons(32);
    if (regionB) regionB->createNeurons(32);

    if (regionA && regionB) {
        // Connect regions with modest density
        brain.connectRegions(regionA->getId(), regionB->getId(), 0.05f, {0.1f, 0.9f});
        brain.connectRegions(regionB->getId(), regionA->getId(), 0.05f, {0.1f, 0.9f});
    }
}

// Synthetic vision input (GxG) fallback: moving checker pattern
std::vector<float> make_synthetic_gray_grid(int G, int step_idx) {
    std::vector<float> grid(static_cast<std::size_t>(G * G), 0.0f);
    int shift = step_idx % G;
    for (int r = 0; r < G; ++r) {
        for (int c = 0; c < G; ++c) {
            int rr = (r + shift) % G;
            bool on = ((rr / 2) % 2) ^ ((c / 2) % 2);
            grid[static_cast<std::size_t>(r * G + c)] = on ? 1.0f : 0.0f;
        }
    }
    return grid;
}

// Synthetic audio input fallback: multi-tone with slow sweep
std::vector<float> make_synthetic_audio(std::size_t N, int sample_rate, int step_idx) {
    std::vector<float> x(N, 0.0f);
    float t0 = static_cast<float>(step_idx) * (N / static_cast<float>(sample_rate));
    float f1 = 220.0f + 10.0f * static_cast<float>(step_idx % 50);
    float f2 = 440.0f + 5.0f * static_cast<float>((step_idx / 2) % 50);
    float f3 = 880.0f;
    for (std::size_t n = 0; n < N; ++n) {
        float t = t0 + static_cast<float>(n) / static_cast<float>(sample_rate);
        float v = 0.4f * std::sin(2.0f * 3.14159265f * f1 * t)
                + 0.3f * std::sin(2.0f * 3.14159265f * f2 * t)
                + 0.2f * std::sin(2.0f * 3.14159265f * f3 * t);
        x[n] = std::clamp(v, -1.0f, 1.0f);
    }
    return x;
}


static bool nf_load_wav_any_mono(const std::string& path, std::vector<float>& out_samples, int& out_sample_rate) {
    out_samples.clear();
    out_sample_rate = 0;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char riff[4]; f.read(riff, 4);
    if (f.gcount() != 4 || std::strncmp(riff, "RIFF", 4) != 0) return false;
    std::uint32_t riff_size = 0; f.read(reinterpret_cast<char*>(&riff_size), 4);
    char wave[4]; f.read(wave, 4);
    if (f.gcount() != 4 || std::strncmp(wave, "WAVE", 4) != 0) return false;
    bool have_fmt = false, have_data = false;
    std::uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    std::uint32_t sample_rate = 0;
    std::vector<char> data_bytes;
    while (f && !(have_fmt && have_data)) {
        char chunk_id[4]; f.read(chunk_id, 4);
        if (f.gcount() != 4) break;
        std::uint32_t chunk_size = 0; f.read(reinterpret_cast<char*>(&chunk_size), 4);
        if (!f) break;
        std::string id(chunk_id, 4);
        if (id == "fmt ") {
            have_fmt = true;
            std::uint16_t block_align = 0; std::uint32_t byte_rate = 0;
            f.read(reinterpret_cast<char*>(&audio_format), 2);
            f.read(reinterpret_cast<char*>(&num_channels), 2);
            f.read(reinterpret_cast<char*>(&sample_rate), 4);
            f.read(reinterpret_cast<char*>(&byte_rate), 4);
            f.read(reinterpret_cast<char*>(&block_align), 2);
            f.read(reinterpret_cast<char*>(&bits_per_sample), 2);
            if (chunk_size > 16) f.seekg(static_cast<std::streamoff>(chunk_size - 16), std::ios::cur);
        } else if (id == "data") {
            have_data = true;
            data_bytes.resize(chunk_size);
            if (chunk_size > 0) f.read(data_bytes.data(), static_cast<std::streamsize>(chunk_size));
        } else {
            f.seekg(static_cast<std::streamoff>(chunk_size), std::ios::cur);
        }
    }
    if (!(have_fmt && have_data)) return false;
    if (num_channels < 1) return false;
    if (!(audio_format == 1 || audio_format == 3)) return false;
    out_sample_rate = static_cast<int>(sample_rate);
    if (audio_format == 1) {
        if (bits_per_sample == 16) {
            std::size_t frame_count = static_cast<std::size_t>(data_bytes.size()) / (num_channels * sizeof(std::int16_t));
            out_samples.resize(frame_count);
            const std::int16_t* p = reinterpret_cast<const std::int16_t*>(data_bytes.data());
            for (std::size_t i = 0; i < frame_count; ++i) {
                long acc = 0;
                for (std::size_t ch = 0; ch < num_channels; ++ch) acc += p[i * num_channels + ch];
                float v = static_cast<float>(acc) / (32768.0f * static_cast<float>(num_channels));
                out_samples[i] = v;
            }
            return true;
        } else {
            return false;
        }
    } else if (audio_format == 3) {
        if (bits_per_sample == 32) {
            std::size_t frame_count = static_cast<std::size_t>(data_bytes.size()) / (num_channels * sizeof(float));
            out_samples.resize(frame_count);
            const float* p = reinterpret_cast<const float*>(data_bytes.data());
            for (std::size_t i = 0; i < frame_count; ++i) {
                double acc = 0.0;
                for (std::size_t ch = 0; ch < num_channels; ++ch) acc += p[i * num_channels + ch];
                out_samples[i] = static_cast<float>(acc / static_cast<double>(num_channels));
            }
            return true;
        } else {
            return false;
        }
    }
    return false;
}

static std::vector<float> nf_resample_linear(const std::vector<float>& in, int sr_in, int sr_out) {
    if (in.empty() || sr_in <= 0 || sr_out <= 0 || sr_in == sr_out) return in;
    double ratio = static_cast<double>(sr_out) / static_cast<double>(sr_in);
    std::size_t out_len = static_cast<std::size_t>(std::ceil(in.size() * ratio));
    std::vector<float> out(out_len, 0.0f);
    for (std::size_t i = 0; i < out_len; ++i) {
        double pos = static_cast<double>(i) / ratio;
        std::size_t i0 = static_cast<std::size_t>(pos);
        double frac = pos - static_cast<double>(i0);
        float s0 = in[std::min(i0, in.size() - 1)];
        float s1 = in[std::min(i0 + 1, in.size() - 1)];
        out[i] = s0 + static_cast<float>(frac) * (s1 - s0);
    }
    return out;
}

// Grid maze environment with walls and proper navigation challenge
class MazeEnv {
public:
    explicit MazeEnv(int n = 8, float wall_density = 0.20f, int max_steps = -1) 
        : N(std::max(2, n)), wall_density_(wall_density), max_episode_steps_(max_steps < 0 ? 4 * n * n : max_steps) {
        walls_.resize(static_cast<std::size_t>(N * N), false);
        generateMaze();
        reset();
    }

    // Potential-based shaping configuration
    enum class ShapingMode { Off, Euclid, Manhattan };
    void setShaping(ShapingMode mode, float k, float gamma) {
        shaping_mode_ = mode;
        shaping_k_ = k;
        shaping_gamma_ = gamma;
    }
    
    void reset() { 
        ax = 0; ay = 0; 
        gx = N - 1; gy = N - 1; 
        episode_steps_ = 0;
        // Ensure start and goal are not walls
        walls_[0] = false;
        walls_[static_cast<std::size_t>((N-1) * N + (N-1))] = false;
        
        // Reset first-person agent state
        agent_state_.x = 0.5f;
        agent_state_.y = 0.5f;
        agent_state_.angle = 0.0f; // Facing right initially
        agent_state_.maze_x = 0;
        agent_state_.maze_y = 0;
    }
    
    std::vector<float> observation() const {
        std::vector<float> obs(static_cast<std::size_t>(N * N), 0.0f);
        // Mark walls
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                if (walls_[static_cast<std::size_t>(y * N + x)]) {
                    obs[static_cast<std::size_t>(y * N + x)] = -1.0f;
                }
            }
        }
        obs[static_cast<std::size_t>(ay * N + ax)] = 1.0f;   // agent marker
        obs[static_cast<std::size_t>(gy * N + gx)] = 0.8f;   // goal marker
        return obs;
    }
    
    // First-person visual observation
    std::vector<float> firstPersonObservation() const {
        #ifdef NF_HAVE_OPENCV
        if (!fp_renderer_) {
            // Fallback to grid observation if renderer not available
            return observation();
        }
        return fp_renderer_->render(agent_state_);
        #else
        // Fallback to grid observation if OpenCV not available
        return observation();
        #endif
    }
    
    // Initialize first-person renderer
    void initializeFirstPersonRenderer() {
        #ifdef NF_HAVE_OPENCV
        NeuroForge::Core::FirstPersonMazeRenderer::RenderConfig config;
        config.width = 160;   // Smaller for neural processing
        config.height = 120;
        config.fov = 90.0f;   // Wide field of view
        config.view_distance = 8.0f;
        config.enable_textures = true;
        config.enable_shadows = true;
        
        fp_renderer_ = std::make_unique<NeuroForge::Core::FirstPersonMazeRenderer>(config);
        fp_renderer_->setMaze(walls_, N, gx, gy);
        #endif
    }
    
    // Get first-person renderer for external access
    NeuroForge::Core::FirstPersonMazeRenderer* getFirstPersonRenderer() const {
        #ifdef NF_HAVE_OPENCV
        return fp_renderer_.get();
        #else
        return nullptr;
        #endif
    }
    
    // Get agent state for first-person rendering
    const NeuroForge::Core::FirstPersonMazeRenderer::AgentState& getAgentState() const {
        return agent_state_;
    }
    
    // actions: 0=up,1=down,2=left,3=right (grid mode) or 0=forward,1=backward,2=turn_left,3=turn_right (first-person mode)
    float step(int action, bool& done) {
        episode_steps_++;
        
        float reward = -0.01f;  // Small step penalty
        
        #ifdef NF_HAVE_OPENCV
        if (fp_renderer_) {
            // First-person mode: use continuous movement and rotation
            bool movement_success = fp_renderer_->updateAgentPosition(agent_state_, action, walls_, N);
            
            if (!movement_success && (action == 0 || action == 1)) {
                // Movement blocked by wall
                reward = -0.1f;
                last_collision_ = true;
            } else {
                last_collision_ = false;
                
                // Update discrete position for compatibility
                ax = agent_state_.maze_x;
                ay = agent_state_.maze_y;
                
                // Potential-based reward shaping
                if (shaping_mode_ != ShapingMode::Off) {
                    float old_d, new_d;
                    if (shaping_mode_ == ShapingMode::Euclid) {
                        old_d = std::sqrt((agent_state_.x - (gx + 0.5f)) * (agent_state_.x - (gx + 0.5f)) + 
                                        (agent_state_.y - (gy + 0.5f)) * (agent_state_.y - (gy + 0.5f)));
                        // For shaping, we need the previous distance, so we approximate
                        new_d = old_d; // Simplified for now
                    } else { // Manhattan
                        old_d = std::abs(agent_state_.x - (gx + 0.5f)) + std::abs(agent_state_.y - (gy + 0.5f));
                        new_d = old_d; // Simplified for now
                    }
                }
            }
            
            // Check for goal reached (within small distance)
            float goal_distance = std::sqrt((agent_state_.x - (gx + 0.5f)) * (agent_state_.x - (gx + 0.5f)) + 
                                          (agent_state_.y - (gy + 0.5f)) * (agent_state_.y - (gy + 0.5f)));
            if (goal_distance < 0.3f) {
                done = true;
                reward = 1.0f;
                episode_success_ = true;
                reset();
                return reward;
            }
        } else {
        #endif
            // Original grid-based movement
            int nx = ax, ny = ay;
            switch (action) {
                case 0: ny = std::max(0, ay - 1); break;
                case 1: ny = std::min(N - 1, ay + 1); break;
                case 2: nx = std::max(0, ax - 1); break;
                case 3: nx = std::min(N - 1, ax + 1); break;
                default: break;
            }
            
            // Check for wall collision
            if (walls_[static_cast<std::size_t>(ny * N + nx)]) {
                reward = -0.1f;  // Wall collision penalty
                last_collision_ = true;
                // Don't move
            } else {
                last_collision_ = false;
                // Potential-based reward shaping with selectable metric
                float old_d = 0.0f;
                if (shaping_mode_ != ShapingMode::Off) {
                    if (shaping_mode_ == ShapingMode::Euclid) {
                        old_d = std::sqrt(static_cast<float>((ax - gx) * (ax - gx) + (ay - gy) * (ay - gy)));
                    } else { // Manhattan
                        old_d = static_cast<float>(std::abs(ax - gx) + std::abs(ay - gy));
                    }
                }
                ax = nx; ay = ny;
                if (shaping_mode_ != ShapingMode::Off) {
                    float new_d;
                    if (shaping_mode_ == ShapingMode::Euclid) {
                        new_d = std::sqrt(static_cast<float>((ax - gx) * (ax - gx) + (ay - gy) * (ay - gy)));
                    } else { // Manhattan
                        new_d = static_cast<float>(std::abs(ax - gx) + std::abs(ay - gy));
                    }
                    // r' = r + k * (d(s) - gamma * d(s')) with Phi(s) = -d(s)
                    reward += shaping_k_ * (old_d - shaping_gamma_ * new_d);
                }
            }
            
            // Check for success
            done = (ax == gx && ay == gy);
            if (done) { 
                reward = 1.0f;
                episode_success_ = true;
                reset();
                return reward;
            }
        #ifdef NF_HAVE_OPENCV
        }
        #endif
        
        // Check for failure (max steps exceeded)
        if (episode_steps_ >= max_episode_steps_) {
            done = true;
            episode_success_ = false;
            reward = -0.5f;  // Failure penalty
            reset();
        }
        
        return reward;
    }
    
    int actionCount() const { return 4; }
    int size() const { return N; }
    int agentX() const { return ax; }
    int agentY() const { return ay; }
    int goalX() const { return gx; }
    int goalY() const { return gy; }
    bool isWall(int x, int y) const { 
        if (x < 0 || x >= N || y < 0 || y >= N) return true;
        return walls_[static_cast<std::size_t>(y * N + x)]; 
    }
    bool lastCollision() const { return last_collision_; }
    bool episodeSuccess() const { return episode_success_; }
    int episodeSteps() const { return episode_steps_; }
    
private:
    int N;
    float wall_density_;
    int max_episode_steps_;
    int episode_steps_ = 0;
    bool last_collision_ = false;
    bool episode_success_ = true;
    std::vector<bool> walls_;
    int ax = 0, ay = 0;
    int gx = 0, gy = 0;

    // Shaping parameters
    ShapingMode shaping_mode_ = ShapingMode::Off;
    float shaping_k_ = 0.01f;
    float shaping_gamma_ = 0.99f;
    
    // First-person rendering support
    #ifdef NF_HAVE_OPENCV
    std::unique_ptr<NeuroForge::Core::FirstPersonMazeRenderer> fp_renderer_;
    #endif
    NeuroForge::Core::FirstPersonMazeRenderer::AgentState agent_state_;
    
    void generateMaze() {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        
        // Generate random walls but ensure connectivity
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                // Don't place walls at start/goal
                if ((x == 0 && y == 0) || (x == N-1 && y == N-1)) continue;
                
                if (dist(rng) < wall_density_) {
                    walls_[static_cast<std::size_t>(y * N + x)] = true;
                }
            }
        }
        
        // Ensure path exists using simple connectivity check
        ensurePathExists();
    }
    
    void ensurePathExists() {
        // Simple flood fill to check connectivity from start to goal
        std::vector<bool> visited(static_cast<std::size_t>(N * N), false);
        std::queue<std::pair<int, int>> queue;
        queue.push({0, 0});
        visited[0] = true;
        
        bool can_reach_goal = false;
        while (!queue.empty() && !can_reach_goal) {
            auto [x, y] = queue.front();
            queue.pop();
            
            if (x == N-1 && y == N-1) {
                can_reach_goal = true;
                break;
            }
            
            // Check 4 directions
            int dx[] = {-1, 1, 0, 0};
            int dy[] = {0, 0, -1, 1};
            for (int i = 0; i < 4; ++i) {
                int nx = x + dx[i];
                int ny = y + dy[i];
                if (nx >= 0 && nx < N && ny >= 0 && ny < N) {
                    std::size_t idx = static_cast<std::size_t>(ny * N + nx);
                    if (!visited[idx] && !walls_[idx]) {
                        visited[idx] = true;
                        queue.push({nx, ny});
                    }
                }
            }
        }
        
        // If no path exists, clear some walls to create a simple path
        if (!can_reach_goal) {
            // Create a simple L-shaped path from (0,0) to (N-1,N-1)
            for (int x = 0; x < N; ++x) {
                walls_[static_cast<std::size_t>(0 * N + x)] = false;  // Top row
            }
            for (int y = 0; y < N; ++y) {
                walls_[static_cast<std::size_t>(y * N + (N-1))] = false;  // Right column
            }
        }
    }
};

} // end anonymous namespace

// Helper: handle context-related CLI flags to keep main parse shallow
// Returns true if handled; sets 'err' to non-zero on parse error and prints message
static bool handle_context_flag(
    const std::string& arg,
    double& context_gain,
    int& context_update_ms,
    int& context_window,
    std::vector<std::string>& context_peer_args,
    std::vector<std::string>& context_coupling_args,
    int& err
) {
    err = 0;
    // --context-gain=FLOAT
    if (arg.rfind("--context-gain=", 0) == 0) {
        auto v = arg.substr(std::string("--context-gain=").size());
        try { context_gain = std::stod(v); }
        catch (...) { std::cerr << "Error: invalid float for --context-gain" << std::endl; err = 2; }
        return true;
    }
    // --context-update-ms=INT
    if (arg.rfind("--context-update-ms=", 0) == 0) {
        auto v = arg.substr(std::string("--context-update-ms=").size());
        try { context_update_ms = std::stoi(v); if (context_update_ms <= 0) { std::cerr << "Error: --context-update-ms must be > 0" << std::endl; err = 2; } }
        catch (...) { std::cerr << "Error: invalid integer for --context-update-ms" << std::endl; err = 2; }
        return true;
    }
    // --context-update=INT (alias)
    if (arg.rfind("--context-update=", 0) == 0) {
        auto v = arg.substr(std::string("--context-update=").size());
        try { context_update_ms = std::stoi(v); if (context_update_ms <= 0) { std::cerr << "Error: --context-update must be > 0" << std::endl; err = 2; } }
        catch (...) { std::cerr << "Error: invalid integer for --context-update" << std::endl; err = 2; }
        return true;
    }
    // --context-window=INT
    if (arg.rfind("--context-window=", 0) == 0) {
        auto v = arg.substr(std::string("--context-window=").size());
        try { context_window = std::stoi(v); if (context_window < 1) { std::cerr << "Error: --context-window must be >= 1" << std::endl; err = 2; } }
        catch (...) { std::cerr << "Error: invalid integer for --context-window" << std::endl; err = 2; }
        return true;
    }
    // --context-peer=name[,gain][,update_ms][,window][,label]
    if (arg.rfind("--context-peer=", 0) == 0) {
        auto v = arg.substr(std::string("--context-peer=").size());
        if (v.empty()) { std::cerr << "Error: --context-peer requires name[,gain][,update_ms][,window][,label]" << std::endl; err = 2; }
        else { context_peer_args.push_back(v); }
        return true;
    }
    // --context-couple=src:dst[,weight]
    if (arg.rfind("--context-couple=", 0) == 0) {
        auto v = arg.substr(std::string("--context-couple=").size());
        if (v.empty()) { std::cerr << "Error: --context-couple requires src:dst[,weight]" << std::endl; err = 2; }
        else { context_coupling_args.push_back(v); }
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Ensure UTF-8 console output on Windows (fixes garbled glyphs in logs)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    NfSetTerminationHandlers();
    std::cout << "DEBUG: Entering main function" << std::endl;
    std::cout.flush();
    try {
        std::cout << "DEBUG: Starting force-link calls" << std::endl;
        std::cout.flush();
        // Force-link region translation units so their static registrars run
        std::cout << "DEBUG: Calling NF_ForceLink_CorticalRegions" << std::endl;
        std::cout.flush();
        NF_ForceLink_CorticalRegions();
        std::cout << "DEBUG: Calling NF_ForceLink_SubcorticalRegions" << std::endl;
        std::cout.flush();
        NF_ForceLink_SubcorticalRegions();
        std::cout << "DEBUG: Calling NF_ForceLink_LimbicRegions" << std::endl;
        std::cout.flush();
        NF_ForceLink_LimbicRegions();
        std::cout << "DEBUG: Calling NF_ForceLink_PhaseARegion" << std::endl;
        std::cout.flush();
        NF_ForceLink_PhaseARegion();
        std::cout << "DEBUG: Completed all force-link calls" << std::endl;
        std::cout.flush();

        // Defaults
        int steps = 1;
        int step_ms = 10;
        bool enable_learning = false;
        bool show_help = false;

        // Demo flags and configs
        bool vision_demo = false;
        bool audio_demo = false;
        bool motor_cortex = false;
        bool social_perception = false;
        bool social_view = false;
        bool cross_modal = false;
        bool audio_mic = false;
        bool audio_system = false;
        std::string audio_file_path;
        int camera_index = 0;
        std::string camera_backend = "any";
        std::string vision_source = "camera"; // camera|screen|maze|synthetic
        int retina_rect_x = 0, retina_rect_y = 0, retina_rect_w = 1280, retina_rect_h = 720;
        bool foveation_enable = false;
        int fovea_w = 640;
        int fovea_h = 360;
        std::string fovea_mode = "cursor";
        double fovea_alpha = 0.3;
        double fovea_center_x = -1.0, fovea_center_y = -1.0;
        int last_fovea_x = -1, last_fovea_y = -1, last_fovea_w = -1, last_fovea_h = -1;
        // Sandbox configuration (restricted interaction area and optional embedded browser)
        bool sandbox_enable = false;
        std::string sandbox_url = "https://www.youtube.com";
        bool sandbox_actions_enable = true;
        int sandbox_w = 1280;
        int sandbox_h = 720;
        bool youtube_mode = false;
#ifdef _WIN32
#endif
        int simulate_blocked_actions = 0;
        int simulate_rewards = 0;
#ifdef NF_ENABLE_VISION_DEMO
        vision_demo = true;
#endif
        NeuroForge::Encoders::VisionEncoder::Config vcfg{};
        NeuroForge::Encoders::AudioEncoder::Config acfg{};
        // Hazard injection config (Phase C)
        float hazard_density = -1.0f;   // if >0 in [0,1], overrides audio-derived hazard; 0 = audio fallback
        float phase_c_hazard_weight = 0.2f; // coherence down-modulation weight in [0,1]
        bool phase_c_hazard_weight_set = false;
        float phase_c_hazard_alpha = 0.0f; // sensitivity to external hazard [0,1]
        float phase_c_hazard_beta = 0.0f;  // sensitivity to arousal [0,1]
        // Phase C threshold tuning (assemblies & pruning)
        bool phase_c_binding_threshold_set = false;
        float phase_c_binding_threshold = 0.0f;
        bool phase_c_sequence_threshold_set = false;
        float phase_c_sequence_threshold = 0.0f;
        bool phase_c_binding_coherence_min_set = false;
        float phase_c_binding_coherence_min = 0.0f;
        bool phase_c_sequence_coherence_min_set = false;
        float phase_c_sequence_coherence_min = 0.0f;
        bool phase_c_prune_coherence_threshold_set = false;
        float phase_c_prune_coherence_threshold = 0.0f;

        // Maze demo flags
        bool maze_demo = false;
        bool maze_first_person = false;   // Enable first-person visual navigation
        int maze_size = 8;
        float maze_wall_density = 0.20f;  // fraction of cells as walls
        // Exploration and maze visualization options
        float epsilon = -1.0f;            // <0 disables epsilon-greedy (kept for Q-learning baseline mode)
    float softmax_temp = 0.5f;        // Default to biological stochastic policy (0.5 = moderate exploration)
        bool maze_view = false;           // OpenCV live maze view
        int maze_view_interval_ms = 300;  // refresh interval for maze view
        int maze_max_episode_steps = -1;  // if <0, defaults to 4*N*N once maze_size is known
        std::string episode_csv_path;     // optional per-episode CSV output
        bool qlearning = false;           // use Q-learning baseline for maze policy
        bool summary = false;             // print end-of-run episode summary
        float hybrid_lambda = -1.0f;      // <0 disables blending; in [0,1] blends neural (lambda) with Q (1-lambda)
        // Maze shaping flags
        std::string maze_shaping = "off"; // off|euclid|manhattan
        float maze_shaping_k = 0.01f;      // default beta
        float maze_shaping_gamma = 0.99f;  // default gamma

        // Learning params (use LearningSystem::Config defaults unless overridden)
        NeuroForge::Core::LearningSystem::Config lconf{};
        bool prefer_gpu = false;
        bool hebbian_rate_set = false;
        bool stdp_rate_set = false;
        bool stdp_mult_set = false;
        bool attention_boost_set = false;
        bool homeostasis_set = false;
        bool consolidation_interval_set = false;
        bool consolidation_strength_set = false;
        std::string snapshot_csv_path;
        // Phase-4 reward shaping weights (novelty alpha, task gamma, uncertainty eta)
        float alpha_weight = 0.50f;
        float gamma_weight = 1.00f;
        float eta_weight   = 0.20f;
        bool alpha_set = false, gamma_set = false, eta_set = false;
        float lambda_param = 0.90f;
        float etaElig_param = 0.50f;
        float kappa_param = 0.15f;
        bool lambda_set = false, etaElig_set = false, kappa_set = false;
        // Phase-4 unsafe flag to bypass validation
        bool phase4_unsafe = false;
        // Live snapshot publishing
        std::string snapshot_live_path;
        int snapshot_interval_ms = 1000;
        
        // Heatmap viewer config
        bool heatmap_view = false;
        int heatmap_interval_ms = 1000;
        int heatmap_size = 256;
        float heatmap_threshold = 0.0f;

        // 3D viewer integration config
        bool viewer_enabled = false;
        std::string viewer_exe_path;
        std::string viewer_layout = "shells"; // shells | layers
        int viewer_refresh_ms = 1500;
        float viewer_threshold = 0.0f;

        // Persistence and memory database
        std::string save_brain_path;
        std::string load_brain_path;
        std::string memory_db_path;
        bool memdb_debug = false;
        bool memdb_color = true;
        int memdb_interval_ms = 1000; // periodic MemoryDB logging interval (ms)
        bool memdb_interval_cli_set = false; // tracks if CLI explicitly set interval
        int reward_interval_ms = 1000; // periodic reward logging interval (ms)
        bool reward_interval_cli_set = false; // tracks if CLI explicitly set interval

        // MemoryDB listing flags
        bool flag_list_episodes = false;
        std::string list_episodes_run_id;
        bool flag_recent_rewards = false;
        std::string recent_rewards_run_id;
        int recent_rewards_limit = 10;
        bool flag_list_runs = false;
        bool flag_recent_run_events = false;
        std::string recent_run_events_run_id;
        int recent_run_events_limit = 10;

        // Spike overlays export (for 3D viewer)
        std::string spikes_live_path;
        double spikes_ttl_sec = 2.0; // seconds window for recent spikes
        std::deque<std::pair<NeuroForge::NeuronID, NeuroForge::TimePoint>> spike_events;
        std::mutex spikes_mutex;

        // Mimicry (Phase-5) CLI-controlled options
        bool mimicry_enable = false;
        bool mimicry_weight_set = false;
        float mimicry_weight_mu = 0.0f;
        bool mimicry_internal = false;
        std::string teacher_embed_path;
        std::string student_embed_path;
        // Mirror mode: derive student embedding from sensory features instead of action scores
        std::string mirror_mode = "off"; // off|vision|audio
        std::vector<float> last_visual_features;
        std::vector<float> last_audio_features;
        // Teacher policy flags
        std::string teacher_policy = "none"; // none|greedy|bfs
        float teacher_mix = 0.0f;             // [0,1]
        int last_teacher_action = -1;         // for logging
        std::string current_teacher_id = "teacher_embed"; // active teacher embedding id for Phase A
        struct TripletItem { std::string stem; std::string image_path; std::string audio_path; std::string text; };
        std::vector<TripletItem> triplet_items;
        std::size_t dataset_index = 0;
        std::string current_image_path;
        std::string current_audio_path;
        std::string current_caption;
        bool dataset_active = false;
        bool telemetry_extended = false; // include extended metadata in experience snapshots
        float phase_a_last_similarity = 0.0f;
        float phase_a_last_novelty = 0.0f;
        float phase_a_last_reward = 0.0f;
        bool phase_a_last_success = false;
        int phase_a_last_stu_len = 0;
        int phase_a_last_tea_len = 0;
        double phase_a_last_stu_norm = 0.0;
        double phase_a_last_tea_norm = 0.0;
        double phase_a_last_dot = 0.0;

        auto scan_triplets_dataset = [&](const std::string& root, int limit, bool shuffle) -> std::vector<TripletItem> {
            std::vector<TripletItem> items;
            if (root.empty()) return items;
            namespace fs = std::filesystem;
            std::unordered_map<std::string, std::string> audio_by_stem;
            std::unordered_map<std::string, std::string> text_by_stem;
            std::unordered_map<std::string, std::string> image_by_stem;
            auto has_ext = [](const fs::path& p, std::initializer_list<const char*> exts) {
                auto e = p.extension().string();
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                for (auto x : exts) { if (e == x) return true; }
                return false;
            };
            try {
                fs::path rootp(root);
                fs::path audio_dir = rootp / "audio";
                fs::path text_dir  = rootp / "texts";
                fs::path image_dir = rootp / "images";
                auto scan_dir = [&](const fs::path& dir, std::unordered_map<std::string, std::string>& out_map, std::initializer_list<const char*> exts) {
                    if (!fs::exists(dir)) return;
                    for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied);
                         it != fs::recursive_directory_iterator(); ++it) {
                        if (!it->is_regular_file()) continue;
                        const auto& p = it->path();
                        if (!has_ext(p, exts)) continue;
                        out_map[p.stem().string()] = p.string();
                    }
                };
                bool structured = fs::exists(audio_dir) && fs::exists(text_dir);
                if (structured) {
                    scan_dir(audio_dir, audio_by_stem, {".wav", ".flac", ".ogg"});
                    scan_dir(text_dir,  text_by_stem,  {".txt"});
                    scan_dir(image_dir, image_by_stem, {".jpg", ".jpeg", ".png", ".bmp", ".gif"});
                } else {
                    for (auto it = fs::recursive_directory_iterator(rootp, fs::directory_options::skip_permission_denied);
                         it != fs::recursive_directory_iterator(); ++it) {
                        if (!it->is_regular_file()) continue;
                        const auto& p = it->path();
                        auto stem = p.stem().string();
                        if (has_ext(p, {".wav", ".flac", ".ogg"})) {
                            audio_by_stem[stem] = p.string();
                        } else if (has_ext(p, {".txt"})) {
                            text_by_stem[stem] = p.string();
                        } else if (has_ext(p, {".jpg", ".jpeg", ".png", ".bmp", ".gif"})) {
                            image_by_stem[stem] = p.string();
                        }
                    }
                }
                auto read_text = [](const std::string& file) -> std::string {
                    std::ifstream ifs(file);
                    if (!ifs.is_open()) return std::string();
                    std::stringstream ss; ss << ifs.rdbuf();
                    return ss.str();
                };
                auto assemble = [&]() {
                    items.clear();
                    for (const auto& kv : text_by_stem) {
                        const std::string& stem = kv.first;
                        auto a_it = audio_by_stem.find(stem);
                        if (a_it == audio_by_stem.end()) continue;
                        TripletItem it;
                        it.stem = stem;
                        it.audio_path = a_it->second;
                        it.text = read_text(kv.second);
                        auto img_it = image_by_stem.find(stem);
                        if (img_it != image_by_stem.end()) it.image_path = img_it->second;
                        if (!it.text.empty()) items.push_back(std::move(it));
                    }
                };
                assemble();
                if (items.empty() && structured) {
                    audio_by_stem.clear();
                    text_by_stem.clear();
                    image_by_stem.clear();
                    for (auto it = fs::recursive_directory_iterator(rootp, fs::directory_options::skip_permission_denied);
                         it != fs::recursive_directory_iterator(); ++it) {
                        if (!it->is_regular_file()) continue;
                        const auto& p = it->path();
                        auto stem = p.stem().string();
                        if (has_ext(p, {".wav", ".flac", ".ogg"})) {
                            audio_by_stem[stem] = p.string();
                        } else if (has_ext(p, {".txt"})) {
                            text_by_stem[stem] = p.string();
                        } else if (has_ext(p, {".jpg", ".jpeg", ".png", ".bmp", ".gif"})) {
                            image_by_stem[stem] = p.string();
                        }
                    }
                    assemble();
                }
                if (shuffle) {
                    std::mt19937 rng{std::random_device{}()};
                    std::shuffle(items.begin(), items.end(), rng);
                }
                if (limit > 0 && static_cast<int>(items.size()) > limit) items.resize(static_cast<std::size_t>(limit));
                std::cout << "Triplet scan: audio=" << audio_by_stem.size()
                          << " text=" << text_by_stem.size()
                          << " image=" << image_by_stem.size()
                          << " matched=" << items.size() << std::endl;
            } catch (...) {
            }
            return items;
        };

        // Language / Phase A options
        bool phase_a_enable = false;
        bool phase5_language_enable = false;
        std::shared_ptr<NeuroForge::Core::LanguageSystem> language_system;
        std::unique_ptr<NeuroForge::Core::PhaseAMimicry> phase_a_system;
        std::shared_ptr<NeuroForge::Regions::SelfNode> self_node;

        // Phase A threshold overrides (CLI-configurable)
        bool phase_a_similarity_threshold_set = false;
        float phase_a_similarity_threshold = 0.6f;
        bool phase_a_novelty_threshold_set = false;
        float phase_a_novelty_threshold = 0.1f;
        bool phase_a_student_lr_set = false;
        double phase_a_student_lr = 0.0;
        bool phase_a_mimicry_repeats_set = false;
        int phase_a_mimicry_repeats = 5;
        bool phase_a_negative_k_set = false;
        int phase_a_negative_k = 5;
        bool phase_a_negative_weight_set = false;
        float phase_a_negative_weight = 0.2f;
        // Phase A EMA stabilizer overrides (CLI-configurable)
        bool phase_a_ema_enable = true;            // default on to stabilize long-run updates
        bool phase_a_ema_enable_set = false;       // track if explicitly toggled via CLI
        bool phase_a_ema_min_set = false;          // track explicit alpha_min override
        double phase_a_ema_min = 0.02;             // default lower bound on EMA coefficient
        bool phase_a_ema_max_set = false;          // track explicit alpha_max override
        double phase_a_ema_max = 0.2;              // default upper bound on EMA coefficient
        bool phase_a_replay_interval_set = false;
        int phase_a_replay_interval_steps = 0;
        bool phase_a_replay_top_k_set = false;
        int phase_a_replay_top_k = 0;
        bool phase_a_replay_boost_set = false;
        double phase_a_replay_boost = 1.0;
        bool phase_a_replay_lr_scale_set = false;
        double phase_a_replay_lr_scale = 1.0;
        bool phase_a_replay_include_hard_set = false;
        bool phase_a_replay_include_hard = true;
        bool phase_a_replay_hard_k_set = false;
        int phase_a_replay_hard_k = 3;
        bool phase_a_replay_repulsion_weight_set = false;
        double phase_a_replay_repulsion_weight = 0.5;
        bool phase_a_export_set = false;
        std::string phase_a_export_dir;

        // Phase 6 Reasoner
        bool phase6_enable = false;
        std::unique_ptr<NeuroForge::Core::Phase6Reasoner> phase6_reasoner;
        std::string phase6_active_mode = "off"; // off|audit|on
        double phase6_margin = 0.08;

        // Phase 7 Affective State and Reflection
        bool phase7_enable = false;
        bool phase7_affect_enable = false;
        bool phase7_reflect_enable = false;
        std::unique_ptr<NeuroForge::Core::Phase7AffectiveState> phase7_affect;
        std::unique_ptr<NeuroForge::Core::Phase7Reflection> phase7_reflect;

        // Phase 8 Goal System
        bool phase8_enable = true; // default enabled when MemoryDB available
        std::unique_ptr<NeuroForge::Core::Phase8GoalSystem> phase8_goals;
bool phase9_enable = true;
bool phase9_modulation_enable = false;
std::unique_ptr<NeuroForge::Core::Phase9Metacognition> phase9_metacog;

bool phase10_enable = true;
std::unique_ptr<NeuroForge::Core::Phase10SelfExplanation> phase10_selfexplainer;

bool phase11_enable = true;
int phase11_revision_interval_ms = 300000; // default 5 minutes
int phase11_min_gap_ms = 60000;
int phase11_outcome_eval_window_ms = 60000;
double phase11_revision_threshold = 0.3;
std::string phase11_revision_mode = "moderate";
std::unique_ptr<NeuroForge::Core::Phase11SelfRevision> phase11_revision;

    bool stagec_enable = false;

    bool phase12_enable = true;
    int phase12_window = 8; // default analysis window
    std::unique_ptr<NeuroForge::Core::Phase12Consistency> phase12_consistency;

    // Phase 13 – Autonomy Envelope
    bool phase13_enable = true;
    int phase13_window = 10; // default analysis window
    std::unique_ptr<NeuroForge::Core::Phase13AutonomyEnvelope> phase13_autonomy;
    double phase13_trust_tighten = 0.35;
    double phase13_trust_expand = 0.70;
    double phase13_consistency_tighten = 0.50;
    double phase13_consistency_expand = 0.80;
    int phase13_contraction_hysteresis_ms = 60000;
    int phase13_expansion_hysteresis_ms = 60000;
    int phase13_min_log_interval_ms = 30000;

    // Phase 14 – Meta-Reasoner (quality trend monitoring)
    bool phase14_enable = true;
    int phase14_window = 10; // default analysis window
    double phase14_trust_degraded = 0.40; // trust below this considered degraded
    double phase14_rmse_degraded = 0.35;  // rmse above this considered degraded
    std::unique_ptr<NeuroForge::Core::Phase14MetaReasoner> phase14_metareason;

    // Phase 15 – Ethics Regulator (risk assessment and decision control)
    bool phase15_enable = true;
    int phase15_window = 5; // default analysis window
    double phase15_risk_threshold = 0.60; // risk above this triggers review/deny
    std::unique_ptr<NeuroForge::Core::Phase15EthicsRegulator> phase15_ethics;

    // Context hooks configuration
    double context_gain = 1.0;         // scaling for sampled signals
    int context_update_ms = 1000;      // sampling interval
    int context_window = 5;            // recent window size

    // Context peer/coupling CLI args (parsed later)
    std::vector<std::string> context_peer_args;      // --context-peer=name[,gain][,update_ms][,window][,label]
    std::vector<std::string> context_coupling_args;  // --context-couple=src:dst[,weight]
    // Optional sampling labels per peer (persist for runtime logging)
    static std::unordered_map<std::string, std::string> context_peer_labels;

        // Phase C options
        bool phase_c = false;
        std::string phase_c_mode = "binding"; // binding|sequence
        std::string phase_c_out = "PhaseC_Logs";
        unsigned int phase_c_seed = 0; // 0 = random at runtime
        std::size_t phase_c_wm_capacity = 6; // default WM capacity
        float phase_c_wm_decay = 0.90f;      // default WM decay
        std::size_t phase_c_seq_window = 0;  // 0 = unlimited
        bool phase_c_survival_bias = false;   // Enable SurvivalBias modulation
        float phase_c_variance_sensitivity = 1.0f; // SurvivalBias variance sensitivity
        float phase_c_survival_scale = 1.0f; // Shaped reward scale
        bool phase_c_survival_scale_set = false;
        int phase_c_lag_align = 0;           // Reward log step offset (may be negative)
        bool phase_c_lag_align_set = false;

        // Unified substrate mode (Phase C + WM + SurvivalBias + Language integration)
        bool unified_substrate_enable = false;
        std::size_t unified_wm_neurons = 0;       // 0 = use default (64)
        std::size_t unified_phasec_neurons = 0;   // 0 = use default (64)
        bool adaptive_enable = true;              // unified adaptive reflection on by default
        bool survival_bias_enable = true;         // unified SurvivalBias effector on by default
        // Emergent-only mode (true emergence)
        bool emergent_only = false;
        bool emergent_only_set = false;

        // M6 Memory Internalization parameters
        bool hippocampal_snapshots = false;
        bool memory_independent = false;
        int consolidation_interval_m6 = 1000; // ms
        bool hippocampal_snapshots_set = false;
        bool memory_independent_set = false;
        bool consolidation_interval_m6_set = false;

        // M7 Autonomous Operation parameters
        bool autonomous_mode = false;
        std::string substrate_mode = "off"; // off|mirror|train|native
        std::string dataset_triplets_root;
        std::string dataset_mode;
        int dataset_limit = 0;
        bool dataset_shuffle = false;
        double reward_scale = 1.0;
        float curiosity_threshold = 0.3f;
        float uncertainty_threshold = 0.4f;
        float prediction_error_threshold = 0.5f;
        int max_concurrent_tasks = 5;
        int task_generation_interval = 1000; // ms
        bool eliminate_scaffolds = false;
        bool autonomy_metrics = false;
        float autonomy_target = 0.9f;
        float motivation_decay = 0.95f;
        float exploration_bonus = 0.2f;
        int novelty_memory_size = 100;
        // M7 parameter set flags
        bool autonomous_mode_set = false;
        bool substrate_mode_set = false;
        bool curiosity_threshold_set = false;
        bool uncertainty_threshold_set = false;
        bool prediction_error_threshold_set = false;
        bool max_concurrent_tasks_set = false;
        bool task_generation_interval_set = false;
        bool eliminate_scaffolds_set = false;
        bool autonomy_metrics_set = false;
        bool autonomy_target_set = false;
        bool motivation_decay_set = false;
        bool exploration_bonus_set = false;
        bool novelty_memory_size_set = false;

        bool enable_selfnode = false;
        bool enable_selfnode_set = false;
        bool enable_pfc = false;
        bool enable_pfc_set = false;
        bool enable_motor_cortex = false;
        bool enable_motor_cortex_set = false;

        // Unified reward pipeline weights and logging controls
        double wt_teacher = 0.6;
        double wt_novelty = 0.1;
        double wt_survival = 0.3;
        bool log_shaped_zero = false;


        // Phase-5 learning flags (attention and gating)
        bool p_gate_set = false;
        bool homeostasis_eta_set = false;
        bool attention_mode_set = false;
        bool attention_Amin_set = false;
        bool attention_Amax_set = false;
        bool attention_anneal_ms_set = false;
        bool chaos_steps_set = false;
        bool consolidate_steps_set = false;
        bool novelty_window_set = false;
        bool prune_threshold_set = false;
        bool auto_elig_set = false;
        bool auto_elig_enabled = false;
        bool competence_mode_set = false;
        bool competence_rho_set = false;

        // Machine-readable JSON logging flag/path
        bool log_json = false;
        std::string log_json_path;
        // JSON logging throttling/filter flags
        int log_json_sample_val = 1; // log every Nth event
        std::string log_json_events_csv; // comma separated list of event names or Phase:Event

        // Region creation requests
        struct AddReq { std::string key; std::string name; std::size_t count; };
        std::vector<AddReq> add_region_specs;

        // Pre-scan for JSON log filter flags to keep main parse shallow
        for (int i_pre = 1; i_pre < argc; ++i_pre) {
            std::string arg_pre = argv[i_pre];
            if (starts_with(arg_pre, "--log-json-sample=")) {
                auto v = arg_pre.substr(std::string("--log-json-sample=").size());
                try {
                    int n = std::max(1, std::stoi(v));
                    log_json_sample_val = n;
                } catch (...) {
                    std::cerr << "Error: invalid integer for --log-json-sample" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg_pre, "--log-json-events=")) {
                log_json_events_csv = arg_pre.substr(std::string("--log-json-events=").size());
            }
        }
        set_log_json_filters(log_json_sample_val, log_json_events_csv);

        // Pre-scan for Context flags to keep the main parse shallow and avoid deep nesting
        for (int i_ctx = 1; i_ctx < argc; ++i_ctx) {
            std::string arg_ctx = argv[i_ctx];
            int ctx_err = 0;
            if (handle_context_flag(arg_ctx, context_gain, context_update_ms, context_window, context_peer_args, context_coupling_args, ctx_err)) {
                if (ctx_err) { return ctx_err; }
                // handled, continue pre-scan
            }
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            // Swallow JSON filter flags here (handled in pre-scan) to avoid growing the else-if chain
            if (starts_with(arg, "--log-json-sample=") || starts_with(arg, "--log-json-events=")) {
                continue;
            }
            // Swallow Context flags here (handled in pre-scan) to avoid growing the else-if chain
            if (starts_with(arg, "--context-gain=") || starts_with(arg, "--context-update-ms=") ||
                starts_with(arg, "--context-update=") || starts_with(arg, "--context-window=") ||
                starts_with(arg, "--context-peer=") || starts_with(arg, "--context-couple=")) {
                continue;
            }

            // GPU preference (optional)
            if (arg == "--gpu") { prefer_gpu = true; continue; }
            else if (starts_with(arg, "--gpu=")) {
                auto v = arg.substr(std::string("--gpu=").size());
                if (!parse_on_off_flag(v, prefer_gpu)) { std::cerr << "Error: --gpu must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            }
            
            // Handle Phase 8 arguments early to avoid nesting depth issues
            if (arg == "--phase8") {
                phase8_enable = true;
                continue;
            }
            if (starts_with(arg, "--phase8=")) {
                auto v = arg.substr(std::string("--phase8=").size());
                if (!parse_on_off_flag(v, phase8_enable)) {
                    std::cerr << "Error: --phase8 must be on|off|true|false|1|0" << std::endl;
                    return 2;
                }
                continue;
            }

            if (starts_with(arg, "--phase-a-replay-interval=")) {
                auto v = arg.substr(std::string("--phase-a-replay-interval=").size());
                try { phase_a_replay_interval_steps = std::stoi(v); if (phase_a_replay_interval_steps < 1) { std::cerr << "Error: --phase-a-replay-interval must be >= 1" << std::endl; return 2; } phase_a_replay_interval_set = true; }
                catch (...) { std::cerr << "Error: invalid int for --phase-a-replay-interval" << std::endl; return 2; }
                continue;
            }
            if (starts_with(arg, "--phase-a-replay-top-k=")) {
                auto v = arg.substr(std::string("--phase-a-replay-top-k=").size());
                try { phase_a_replay_top_k = std::stoi(v); if (phase_a_replay_top_k < 1) { std::cerr << "Error: --phase-a-replay-top-k must be >= 1" << std::endl; return 2; } phase_a_replay_top_k_set = true; }
                catch (...) { std::cerr << "Error: invalid int for --phase-a-replay-top-k" << std::endl; return 2; }
                continue;
            }
            if (starts_with(arg, "--phase-a-replay-boost=")) {
                auto v = arg.substr(std::string("--phase-a-replay-boost=").size());
                try { phase_a_replay_boost = std::stod(v); if (phase_a_replay_boost < 0.0) { std::cerr << "Error: --phase-a-replay-boost must be >= 0" << std::endl; return 2; } phase_a_replay_boost_set = true; }
                catch (...) { std::cerr << "Error: invalid float for --phase-a-replay-boost" << std::endl; return 2; }
                continue;
            }
            if (starts_with(arg, "--phase-a-replay-lr-scale=")) {
                auto v = arg.substr(std::string("--phase-a-replay-lr-scale=").size());
                try { phase_a_replay_lr_scale = std::stod(v); if (phase_a_replay_lr_scale < 0.0) { std::cerr << "Error: --phase-a-replay-lr-scale must be >= 0" << std::endl; return 2; } phase_a_replay_lr_scale_set = true; }
                catch (...) { std::cerr << "Error: invalid float for --phase-a-replay-lr-scale" << std::endl; return 2; }
                continue;
            }
            if (starts_with(arg, "--phase-a-replay-include-hard-negatives=")) {
                auto v = arg.substr(std::string("--phase-a-replay-include-hard-negatives=").size());
                if (!parse_on_off_flag(v, phase_a_replay_include_hard)) { std::cerr << "Error: --phase-a-replay-include-hard-negatives must be on|off|true|false|1|0" << std::endl; return 2; }
                phase_a_replay_include_hard_set = true;
                continue;
            }
            if (starts_with(arg, "--phase-a-replay-hard-k=")) {
                auto v = arg.substr(std::string("--phase-a-replay-hard-k=").size());
                try { phase_a_replay_hard_k = std::stoi(v); if (phase_a_replay_hard_k < 1) { std::cerr << "Error: --phase-a-replay-hard-k must be >= 1" << std::endl; return 2; } phase_a_replay_hard_k_set = true; }
                catch (...) { std::cerr << "Error: invalid int for --phase-a-replay-hard-k" << std::endl; return 2; }
                continue;
            }
            if (starts_with(arg, "--phase-a-replay-repulsion-weight=")) {
                auto v = arg.substr(std::string("--phase-a-replay-repulsion-weight=").size());
                try { phase_a_replay_repulsion_weight = std::stod(v); if (phase_a_replay_repulsion_weight < 0.0) { std::cerr << "Error: --phase-a-replay-repulsion-weight must be >= 0" << std::endl; return 2; } phase_a_replay_repulsion_weight_set = true; }
                catch (...) { std::cerr << "Error: invalid float for --phase-a-replay-repulsion-weight" << std::endl; return 2; }
                continue;
            }
            if (starts_with(arg, "--phase-a-export=")) {
                auto v = arg.substr(std::string("--phase-a-export=").size());
                phase_a_export_dir = v;
                phase_a_export_set = true;
                continue;
            }

            // Handle Phase-4 shaping and reward params early to reduce nesting depth
            if (handle_phase4_arg(
                    arg,
                    argc,
                    argv,
                    i,
                    alpha_weight,
                    alpha_set,
                    gamma_weight,
                    gamma_set,
                    eta_weight,
                    eta_set,
                    lambda_param,
                    lambda_set,
                    etaElig_param,
                    etaElig_set,
                    kappa_param,
                    kappa_set,
                    phase4_unsafe)) {
                continue;
            }


            
            // Handle --add-region early to avoid increasing nesting depth of the main else-if chain
            if (starts_with(arg, "--add-region=")) {
                auto v = arg.substr(std::string("--add-region=").size());
                std::string key, name; std::size_t count = 0;
                size_t p1 = v.find(':');
                if (p1 == std::string::npos) { key = v; name = v; }
                else {
                    key = v.substr(0, p1);
                    size_t p2 = v.find(':', p1 + 1);
                    if (p2 == std::string::npos) {
                        name = v.substr(p1 + 1);
                        if (name.empty()) name = key;
                    } else {
                        name = v.substr(p1 + 1, p2 - (p1 + 1));
                        std::string cnt = v.substr(p2 + 1);
                        try {
                            long long ll = std::stoll(cnt);
                            if (ll < 0) { std::cerr << "Error: --add-region COUNT must be non-negative" << std::endl; return 2; }
                            count = static_cast<std::size_t>(ll);
                        } catch (...) { std::cerr << "Error: invalid COUNT for --add-region" << std::endl; return 2; }
                    }
                }
                if (key.empty()) { std::cerr << "Error: --add-region requires KEY" << std::endl; return 2; }
                if (name.empty()) name = key;
                add_region_specs.push_back({key, name, count});
                continue;
            }
            
            // Handle --list-regions early and exit
            if (arg == "--list-regions") {
                auto keys = NeuroForge::Core::RegionRegistry::instance().listKeys();
                std::cout << "Available region keys/aliases (sorted):" << std::endl;
                for (const auto& k : keys) std::cout << "  " << k << std::endl;
        if (g_memdb && g_memdb_run_id > 0) {
            std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            std::int64_t event_id = 0;
            (void)g_memdb->insertRunEvent(g_memdb_run_id, ts_ms, g_last_step.load(), std::string("exit"), std::string("normal"), 0, nf_process_rss_mb(), 0.0, event_id);
        }
        return 0;
            }

            // Handle --memdb-color early to reduce nesting in the main chain
            if (arg == "--memdb-color") {
                // Bare flag = auto (keep enabled; HypergraphBrain will colorize only on TTY)
                memdb_color = true;
                continue;
            } else if (starts_with(arg, "--memdb-color=")) {
                auto v = arg.substr(std::string("--memdb-color=").size());
                std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (vlow == "auto") {
                    memdb_color = true; // auto relies on TTY check inside HypergraphBrain
                    continue;
                }
                if (!parse_on_off_flag(v, memdb_color)) {
                    std::cerr << "Error: --memdb-color must be auto|on|off|true|false|1|0" << std::endl;
                    return 2;
                }
                continue;
            }

            // Handle --mimicry-internal early to avoid extending the large else-if chain depth
            if (arg == "--mimicry-internal") {
                mimicry_internal = true;
                continue;
            } else if (starts_with(arg, "--mimicry-internal=")) {
                auto v = arg.substr(std::string("--mimicry-internal=").size());
                if (!parse_on_off_flag(v, mimicry_internal)) { std::cerr << "Error: --mimicry-internal must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            }

            // Unified substrate flag (early)
            if (arg == "--unified-substrate") { unified_substrate_enable = true; continue; }
            else if (starts_with(arg, "--unified-substrate=")) {
                auto v = arg.substr(std::string("--unified-substrate=").size());
                if (!parse_on_off_flag(v, unified_substrate_enable)) { std::cerr << "Error: --unified-substrate must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            }
            // Unified adaptive toggle
            if (arg == "--adaptive") { adaptive_enable = true; continue; }
            else if (starts_with(arg, "--adaptive=")) {
                auto v = arg.substr(std::string("--adaptive=").size());
                if (!parse_on_off_flag(v, adaptive_enable)) { std::cerr << "Error: --adaptive must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            }
            // Unified SurvivalBias toggle
            if (arg == "--survival-bias") { survival_bias_enable = true; continue; }
            else if (starts_with(arg, "--survival-bias=")) {
                auto v = arg.substr(std::string("--survival-bias=").size());
                if (!parse_on_off_flag(v, survival_bias_enable)) { std::cerr << "Error: --survival-bias must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            }
            else if (starts_with(arg, "--wm-neurons=")) {
                auto v = arg.substr(std::string("--wm-neurons=").size());
                try {
                    long long ll = std::stoll(v);
                    if (ll <= 0) { std::cerr << "Error: --wm-neurons must be positive" << std::endl; return 2; }
                    unified_wm_neurons = static_cast<std::size_t>(ll);
                } catch (...) { std::cerr << "Error: invalid integer for --wm-neurons" << std::endl; return 2; }
                continue;
            }
            else if (starts_with(arg, "--phasec-neurons=")) {
                auto v = arg.substr(std::string("--phasec-neurons=").size());
                try {
                    long long ll = std::stoll(v);
                    if (ll <= 0) { std::cerr << "Error: --phasec-neurons must be positive" << std::endl; return 2; }
                    unified_phasec_neurons = static_cast<std::size_t>(ll);
                } catch (...) { std::cerr << "Error: invalid integer for --phasec-neurons" << std::endl; return 2; }
                continue;
            }

            // 3D viewer integration flags (early)
            if (arg == "--viewer") { viewer_enabled = true; continue; }
            else if (starts_with(arg, "--viewer=")) {
                auto v = arg.substr(std::string("--viewer=").size());
                if (!parse_on_off_flag(v, viewer_enabled)) { std::cerr << "Error: --viewer must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--viewer-exe=")) {
                viewer_exe_path = arg.substr(std::string("--viewer-exe=").size());
                continue;
            } else if (starts_with(arg, "--viewer-layout=")) {
                viewer_layout = arg.substr(std::string("--viewer-layout=").size());
                std::string vlow = viewer_layout; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (!(vlow == "shells" || vlow == "layers")) { std::cerr << "Error: --viewer-layout must be shells|layers" << std::endl; return 2; }
                viewer_layout = vlow;
                continue;
            } else if (starts_with(arg, "--viewer-refresh-ms=")) {
                auto v = arg.substr(std::string("--viewer-refresh-ms=").size());
                try { viewer_refresh_ms = std::stoi(v); if (viewer_refresh_ms < 0) viewer_refresh_ms = 0; }
                catch (...) { std::cerr << "Error: invalid integer for --viewer-refresh-ms" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--viewer-threshold=")) {
                auto v = arg.substr(std::string("--viewer-threshold=").size());
                try { viewer_threshold = std::stof(v); }
                catch (...) { std::cerr << "Error: invalid float for --viewer-threshold" << std::endl; return 2; }
                continue;
            }

            // Sandbox flags: enable a dedicated window and optional embedded WebView2 browser
            if (arg == "--sandbox") { sandbox_enable = true; continue; }
            else if (starts_with(arg, "--sandbox=")) {
                auto v = arg.substr(std::string("--sandbox=").size());
                if (!parse_on_off_flag(v, sandbox_enable)) { std::cerr << "Error: --sandbox must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--sandbox-url=")) {
                sandbox_url = arg.substr(std::string("--sandbox-url=").size());
                if (sandbox_url.empty()) sandbox_url = "https://www.youtube.com";
                continue;
            } else if (starts_with(arg, "--sandbox-size=")) {
                auto v = arg.substr(std::string("--sandbox-size=").size());
                int w=0,h=0; char xchar;
                std::stringstream ss(v);
                if (!(ss >> w >> xchar) || xchar != 'x' || !(ss >> h) || w <= 0 || h <= 0) {
                    std::cerr << "Error: --sandbox-size must be WxH with positive integers" << std::endl; return 2;
                }
                sandbox_w = w; sandbox_h = h;
                continue;
            } else if (arg == "--no-web-actions") {
                sandbox_actions_enable = false; continue;
            } else if (starts_with(arg, "--no-web-actions=")) {
                auto v = arg.substr(std::string("--no-web-actions=").size());
                bool no_web_actions = false;
                if (!parse_on_off_flag(v, no_web_actions)) { std::cerr << "Error: --no-web-actions must be on|off|true|false|1|0" << std::endl; return 2; }
                sandbox_actions_enable = !no_web_actions;
                continue;
            } else if (starts_with(arg, "--simulate-blocked-actions=")) {
                auto v = arg.substr(std::string("--simulate-blocked-actions=").size());
                try { long long ll = std::stoll(v); if (ll < 0) { std::cerr << "Error: --simulate-blocked-actions must be non-negative" << std::endl; return 2; } simulate_blocked_actions = static_cast<int>(ll); }
                catch (...) { std::cerr << "Error: invalid integer for --simulate-blocked-actions" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--simulate-rewards=")) {
                auto v = arg.substr(std::string("--simulate-rewards=").size());
                try { long long ll = std::stoll(v); if (ll < 0) { std::cerr << "Error: --simulate-rewards must be non-negative" << std::endl; return 2; } simulate_rewards = static_cast<int>(ll); }
                catch (...) { std::cerr << "Error: invalid integer for --simulate-rewards" << std::endl; return 2; }
                continue;
            }

            if (starts_with(arg, "--dataset-triplets=")) {
                dataset_triplets_root = arg.substr(std::string("--dataset-triplets=").size());
                continue;
        } else if (starts_with(arg, "--dataset-mode=")) {
                dataset_mode = arg.substr(std::string("--dataset-mode=").size());
                std::transform(dataset_mode.begin(), dataset_mode.end(), dataset_mode.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                continue;
            } else if (starts_with(arg, "--dataset-limit=")) {
                auto v = arg.substr(std::string("--dataset-limit=").size());
                try { long long ll = std::stoll(v); if (ll < 0) { std::cerr << "Error: --dataset-limit must be non-negative" << std::endl; return 2; } dataset_limit = static_cast<int>(ll); }
                catch (...) { std::cerr << "Error: invalid integer for --dataset-limit" << std::endl; return 2; }
                continue;
            } else if (arg == "--dataset-shuffle") {
                dataset_shuffle = true; continue;
            } else if (starts_with(arg, "--dataset-shuffle=")) {
                auto v = arg.substr(std::string("--dataset-shuffle=").size());
                if (!parse_on_off_flag(v, dataset_shuffle)) { std::cerr << "Error: --dataset-shuffle must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--reward-scale=")) {
                auto v = arg.substr(std::string("--reward-scale=").size());
                try { reward_scale = std::stod(v); if (reward_scale < 0.0) { std::cerr << "Error: --reward-scale must be >= 0" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --reward-scale" << std::endl; return 2; }
                continue;
            }

            // Phase C flags (early)
            if (arg == "--phase-c") { phase_c = true; continue; }
            else if (starts_with(arg, "--phase-c=")) {
                auto v = arg.substr(std::string("--phase-c=").size());
                if (!parse_on_off_flag(v, phase_c)) { std::cerr << "Error: --phase-c must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-mode=")) {
                phase_c_mode = arg.substr(std::string("--phase-c-mode=").size());
                std::string vlow = phase_c_mode; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (!(vlow == "binding" || vlow == "sequence")) { std::cerr << "Error: --phase-c-mode must be binding|sequence" << std::endl; return 2; }
                phase_c_mode = vlow;
                continue;
            } else if (starts_with(arg, "--phase-c-out=")) {
                phase_c_out = arg.substr(std::string("--phase-c-out=").size());
                if (phase_c_out.empty()) { std::cerr << "Error: --phase-c-out requires a directory path" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-seed=")) {
                auto v = arg.substr(std::string("--phase-c-seed=").size());
                try { unsigned long long s = std::stoull(v); if (s > std::numeric_limits<unsigned int>::max()) { std::cerr << "Error: --phase-c-seed out of range" << std::endl; return 2; } phase_c_seed = static_cast<unsigned int>(s); }
                catch (...) { std::cerr << "Error: invalid integer for --phase-c-seed" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-wm-capacity=")) {
                auto v = arg.substr(std::string("--phase-c-wm-capacity=").size());
                try {
                    long long ll = std::stoll(v);
                    if (ll <= 0) { std::cerr << "Error: --phase-c-wm-capacity must be positive" << std::endl; return 2; }
                    phase_c_wm_capacity = static_cast<std::size_t>(ll);
                } catch (...) { std::cerr << "Error: invalid integer for --phase-c-wm-capacity" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-wm-decay=")) {
                auto v = arg.substr(std::string("--phase-c-wm-decay=").size());
                try {
                    float d = std::stof(v);
                    if (!(d > 0.0f && d <= 1.0f)) { std::cerr << "Error: --phase-c-wm-decay must be in (0,1]" << std::endl; return 2; }
                    phase_c_wm_decay = d;
                } catch (...) { std::cerr << "Error: invalid float for --phase-c-wm-decay" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-seq-window=")) {
                auto v = arg.substr(std::string("--phase-c-seq-window=").size());
                try {
                    long long ll = std::stoll(v);
                    if (ll < 0) { std::cerr << "Error: --phase-c-seq-window must be non-negative" << std::endl; return 2; }
                    phase_c_seq_window = static_cast<std::size_t>(ll);
                } catch (...) { std::cerr << "Error: invalid integer for --phase-c-seq-window" << std::endl; return 2; }
                continue;
            } else if (arg == "--phase-c-survival-bias") {
                phase_c_survival_bias = true;
                continue;
            } else if (starts_with(arg, "--phase-c-survival-bias=")) {
                auto v = arg.substr(std::string("--phase-c-survival-bias=").size());
                if (!parse_on_off_flag(v, phase_c_survival_bias)) { std::cerr << "Error: --phase-c-survival-bias must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-variance-sensitivity=")) {
                auto v = arg.substr(std::string("--phase-c-variance-sensitivity=").size());
                try {
                    float d = std::stof(v);
                    if (d <= 0.0f) { std::cerr << "Error: --phase-c-variance-sensitivity must be positive" << std::endl; return 2; }
                    phase_c_variance_sensitivity = d;
                } catch (...) { std::cerr << "Error: invalid float for --phase-c-variance-sensitivity" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-survival-scale=")) {
                auto v = arg.substr(std::string("--phase-c-survival-scale=").size());
                try {
                    float d = std::stof(v);
                    if (d < 0.0f) { std::cerr << "Error: --phase-c-survival-scale must be >= 0" << std::endl; return 2; }
                    phase_c_survival_scale = d;
                    phase_c_survival_scale_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --phase-c-survival-scale" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-hazard-weight=")) {
                auto v = arg.substr(std::string("--phase-c-hazard-weight=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --phase-c-hazard-weight must be in [0,1]" << std::endl; return 2; }
                    phase_c_hazard_weight = d;
                    phase_c_hazard_weight_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --phase-c-hazard-weight" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--binding-threshold=")) {
                auto v = arg.substr(std::string("--binding-threshold=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --binding-threshold must be in [0,1]" << std::endl; return 2; }
                    phase_c_binding_threshold = d;
                    phase_c_binding_threshold_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --binding-threshold" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--sequence-threshold=")) {
                auto v = arg.substr(std::string("--sequence-threshold=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --sequence-threshold must be in [0,1]" << std::endl; return 2; }
                    phase_c_sequence_threshold = d;
                    phase_c_sequence_threshold_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --sequence-threshold" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--binding-coherence-min=")) {
                auto v = arg.substr(std::string("--binding-coherence-min=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --binding-coherence-min must be in [0,1]" << std::endl; return 2; }
                    phase_c_binding_coherence_min = d;
                    phase_c_binding_coherence_min_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --binding-coherence-min" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--sequence-coherence-min=")) {
                auto v = arg.substr(std::string("--sequence-coherence-min=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --sequence-coherence-min must be in [0,1]" << std::endl; return 2; }
                    phase_c_sequence_coherence_min = d;
                    phase_c_sequence_coherence_min_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --sequence-coherence-min" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--prune-coherence-threshold=")) {
                auto v = arg.substr(std::string("--prune-coherence-threshold=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --prune-coherence-threshold must be in [0,1]" << std::endl; return 2; }
                    phase_c_prune_coherence_threshold = d;
                    phase_c_prune_coherence_threshold_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --prune-coherence-threshold" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-hazard-alpha=")) {
                auto v = arg.substr(std::string("--phase-c-hazard-alpha=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --phase-c-hazard-alpha must be in [0,1]" << std::endl; return 2; }
                    phase_c_hazard_alpha = d;
                } catch (...) { std::cerr << "Error: invalid float for --phase-c-hazard-alpha" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-hazard-beta=")) {
                auto v = arg.substr(std::string("--phase-c-hazard-beta=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --phase-c-hazard-beta must be in [0,1]" << std::endl; return 2; }
                    phase_c_hazard_beta = d;
                } catch (...) { std::cerr << "Error: invalid float for --phase-c-hazard-beta" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--phase-c-lag-align=")) {
                auto v = arg.substr(std::string("--phase-c-lag-align=").size());
                try {
                    int off = std::stoi(v);
                    phase_c_lag_align = off;
                    phase_c_lag_align_set = true;
                } catch (...) { std::cerr << "Error: invalid integer for --phase-c-lag-align" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--hazard-density=")) {
                auto v = arg.substr(std::string("--hazard-density=").size());
                try {
                    float d = std::stof(v);
                    if (!(d >= 0.0f && d <= 1.0f)) { std::cerr << "Error: --hazard-density must be in [0,1]" << std::endl; return 2; }
                    hazard_density = d;
                } catch (...) { std::cerr << "Error: invalid float for --hazard-density" << std::endl; return 2; }
                continue;
            }

            // Reward pipeline weights and shaped logging controls
            else if (starts_with(arg, "--wt-teacher=")) {
                auto v = arg.substr(std::string("--wt-teacher=").size());
                try { wt_teacher = std::stod(v); }
                catch (...) { std::cerr << "Error: invalid float for --wt-teacher" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--wt-novelty=")) {
                auto v = arg.substr(std::string("--wt-novelty=").size());
                try { wt_novelty = std::stod(v); }
                catch (...) { std::cerr << "Error: invalid float for --wt-novelty" << std::endl; return 2; }
                continue;
            } else if (starts_with(arg, "--wt-survival=")) {
                auto v = arg.substr(std::string("--wt-survival=").size());
                try { wt_survival = std::stod(v); }
                catch (...) { std::cerr << "Error: invalid float for --wt-survival" << std::endl; return 2; }
                continue;
            } else if (arg == "--log-shaped-zero") {
                log_shaped_zero = true;
                continue;
            } else if (starts_with(arg, "--log-shaped-zero=")) {
                auto v = arg.substr(std::string("--log-shaped-zero=").size());
                if (!parse_on_off_flag(v, log_shaped_zero)) { std::cerr << "Error: --log-shaped-zero must be on|off|true|false|1|0" << std::endl; return 2; }
                continue;
            }

            // Emergent-only flags (early)
            if (arg == "--emergent-only" || arg == "--true-emergence") { emergent_only = true; emergent_only_set = true; continue; }
            else if (starts_with(arg, "--emergent-only=") || starts_with(arg, "--true-emergence=")) {
                bool tmp = emergent_only;
                std::string key = starts_with(arg, "--emergent-only=") ? "--emergent-only=" : "--true-emergence=";
                auto v = arg.substr(key.size());
                if (!parse_on_off_flag(v, tmp)) { std::cerr << "Error: --emergent-only/--true-emergence must be on|off|true|false|1|0" << std::endl; return 2; }
                emergent_only = tmp; emergent_only_set = true;
                continue;
            }

            if (arg == "--help" || arg == "-h" || arg == "/?") {
                show_help = true;
            } else if (arg == "--log-json") {
                log_json = true;
            } else if (starts_with(arg, "--log-json=")) {
                auto v = arg.substr(std::string("--log-json=").size());
                std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (vlow == "on" || vlow == "true" || vlow == "1") { log_json = true; }
                else if (vlow == "off" || vlow == "false" || vlow == "0") { log_json = false; }
                else { log_json = true; log_json_path = v; }
             } else if (starts_with(arg, "--steps=")) {
                auto v = arg.substr(std::string("--steps=").size());
                try {
                    steps = std::stoi(v);
                    if (steps < 0) {
                        std::cerr << "Error: --steps must be non-negative" << std::endl;
                        return 2;
                    }
                } catch (...) {
                    std::cerr << "Error: invalid integer for --steps" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--step-ms=")) {
                auto v = arg.substr(std::string("--step-ms=").size());
                try {
                    step_ms = std::stoi(v);
                    if (step_ms < 0) {
                        std::cerr << "Error: --step-ms must be non-negative" << std::endl;
                        return 2;
                    }
                } catch (...) {
                    std::cerr << "Error: invalid integer for --step-ms" << std::endl;
                    return 2;
                }
            } else if (arg == "--maze-demo") {
                maze_demo = true;
            } else if (starts_with(arg, "--maze-demo=")) {
                auto v = arg.substr(std::string("--maze-demo=").size());
                if (!parse_on_off_flag(v, maze_demo)) {
                    std::cerr << "Error: --maze-demo must be on|off|true|false|1|0" << std::endl;
                    return 2;
                }
            } else if (arg == "--maze-first-person") {
                maze_first_person = true;
            } else if (starts_with(arg, "--maze-first-person=")) {
                auto v = arg.substr(std::string("--maze-first-person=").size());
                if (!parse_on_off_flag(v, maze_first_person)) {
                    std::cerr << "Error: --maze-first-person must be on|off|true|false|1|0" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--maze-size=")) {
                auto v = arg.substr(std::string("--maze-size=").size());
                try {
                    maze_size = std::stoi(v);
                    if (maze_size < 2) { std::cerr << "Error: --maze-size must be >= 2" << std::endl; return 2; }
                } catch (...) {
                    std::cerr << "Error: invalid integer for --maze-size" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--maze-wall-density=")) {
                auto v = arg.substr(std::string("--maze-wall-density=").size());
                try {
                    maze_wall_density = std::stof(v);
                    if (!(maze_wall_density >= 0.0f && maze_wall_density <= 0.45f)) { std::cerr << "Error: --maze-wall-density must be in [0,0.45]" << std::endl; return 2; }
                } catch (...) { std::cerr << "Error: invalid float for --maze-wall-density" << std::endl; return 2; }
            } else if (starts_with(arg, "--maze-shaping=")) {
                auto v = arg.substr(std::string("--maze-shaping=").size());
                if (v == "off" || v == "euclid" || v == "manhattan") {
                    maze_shaping = v;
                } else {
                    std::cerr << "Error: --maze-shaping must be off|euclid|manhattan" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--maze-shaping-k=")) {
                auto v = arg.substr(std::string("--maze-shaping-k=").size());
                try {
                    maze_shaping_k = std::stof(v);
                    if (maze_shaping_k < 0.0f) { std::cerr << "Error: --maze-shaping-k must be >= 0" << std::endl; return 2; }
                } catch (...) { std::cerr << "Error: invalid float for --maze-shaping-k" << std::endl; return 2; }
            } else if (starts_with(arg, "--maze-shaping-gamma=")) {
                auto v = arg.substr(std::string("--maze-shaping-gamma=").size());
                try {
                    maze_shaping_gamma = std::stof(v);
                    if (!(maze_shaping_gamma >= 0.0f && maze_shaping_gamma <= 1.0f)) { std::cerr << "Error: --maze-shaping-gamma must be in [0,1]" << std::endl; return 2; }
                } catch (...) { std::cerr << "Error: invalid float for --maze-shaping-gamma" << std::endl; return 2; }
            } else if (starts_with(arg, "--epsilon=")) {
                auto v = arg.substr(std::string("--epsilon=").size());
                try {
                    epsilon = std::stof(v);
                    if (!(epsilon >= 0.0f && epsilon <= 1.0f)) {
                        std::cerr << "Error: --epsilon must be in [0,1]" << std::endl; return 2;
                    }
                } catch (...) {
                    std::cerr << "Error: invalid float for --epsilon" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--softmax-temp=")) {
                auto v = arg.substr(std::string("--softmax-temp=").size());
                try {
                    softmax_temp = std::stof(v);
                    if (softmax_temp <= 0.0f) {
                        std::cerr << "Error: --softmax-temp must be > 0" << std::endl; return 2;
                    }
                } catch (...) {
                    std::cerr << "Error: invalid float for --softmax-temp" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--hybrid-lambda=")) {
                auto v = arg.substr(std::string("--hybrid-lambda=").size());
                try {
                    hybrid_lambda = std::stof(v);
                    if (hybrid_lambda < 0.0f || hybrid_lambda > 1.0f) { std::cerr << "Error: --hybrid-lambda must be in [0,1]" << std::endl; return 2; }
                } catch (...) { std::cerr << "Error: invalid float for --hybrid-lambda" << std::endl; return 2; }
            } else if (starts_with(arg, "--teacher-policy=")) {
                auto v = arg.substr(std::string("--teacher-policy=").size());
                std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (vlow == "none" || vlow == "greedy" || vlow == "bfs") { teacher_policy = vlow; }
                else { std::cerr << "Error: --teacher-policy must be one of: none, greedy, bfs" << std::endl; return 2; }
            } else if (starts_with(arg, "--teacher-mix=")) {
                auto v = arg.substr(std::string("--teacher-mix=").size());
                try { teacher_mix = std::stof(v); if (teacher_mix < 0.0f || teacher_mix > 1.0f) { std::cerr << "Error: --teacher-mix must be in [0,1]" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --teacher-mix" << std::endl; return 2; }
            } else if (arg == "--mimicry") {
                mimicry_enable = true;
            } else if (starts_with(arg, "--mimicry=")) {
                auto v = arg.substr(std::string("--mimicry=").size());
                if (!parse_on_off_flag(v, mimicry_enable)) { std::cerr << "Error: --mimicry must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--mimicry-weight=")) {
                auto v = arg.substr(std::string("--mimicry-weight=").size());
                try { mimicry_weight_mu = std::stof(v); mimicry_weight_set = true; }
                catch (...) { std::cerr << "Error: invalid float for --mimicry-weight" << std::endl; return 2; }
            } else if (arg == "--mimicry-internal") {
                mimicry_internal = true;
            } else if (starts_with(arg, "--mimicry-internal=")) {
                auto v = arg.substr(std::string("--mimicry-internal=").size());
                if (!parse_on_off_flag(v, mimicry_internal)) { std::cerr << "Error: --mimicry-internal must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--teacher-embed=")) {
                teacher_embed_path = arg.substr(std::string("--teacher-embed=").size());
                if (teacher_embed_path.empty()) { std::cerr << "Error: --teacher-embed requires a file path" << std::endl; return 2; }
            } else if (starts_with(arg, "--student-embed=")) {
                student_embed_path = arg.substr(std::string("--student-embed=").size());
                if (student_embed_path.empty()) { std::cerr << "Error: --student-embed requires a file path" << std::endl; return 2; }
            } else if (starts_with(arg, "--mirror-mode=")) {
                auto v = arg.substr(std::string("--mirror-mode=").size());
                std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (vlow == "off" || vlow == "vision" || vlow == "audio") { mirror_mode = vlow; }
                else { std::cerr << "Error: --mirror-mode must be one of: off, vision, audio" << std::endl; return 2; }
            } else if (starts_with(arg, "--student-learning-rate=")) {
                auto v = arg.substr(std::string("--student-learning-rate=").size());
                try { phase_a_student_lr = std::stod(v); if (phase_a_student_lr < 0.0) { std::cerr << "Error: --student-learning-rate must be >= 0" << std::endl; return 2; } phase_a_student_lr_set = true; }
                catch (...) { std::cerr << "Error: invalid float for --student-learning-rate" << std::endl; return 2; }
            } else if (arg == "--phase5-language") {
                phase5_language_enable = true;
            } else if (starts_with(arg, "--phase5-language=")) {
                auto v = arg.substr(std::string("--phase5-language=").size());
                if (!parse_on_off_flag(v, phase5_language_enable)) { std::cerr << "Error: --phase5-language must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--phase-a") {
                phase_a_enable = true;
            } else if (starts_with(arg, "--phase-a=")) {
                auto v = arg.substr(std::string("--phase-a=").size());
                if (!parse_on_off_flag(v, phase_a_enable)) { std::cerr << "Error: --phase-a must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase-a-similarity-threshold=")) {
                auto v = arg.substr(std::string("--phase-a-similarity-threshold=").size());
                try { phase_a_similarity_threshold = std::stof(v); phase_a_similarity_threshold_set = true; }
                catch (...) { std::cerr << "Error: invalid float for --phase-a-similarity-threshold" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase-a-novelty-threshold=")) {
                auto v = arg.substr(std::string("--phase-a-novelty-threshold=").size());
                try { phase_a_novelty_threshold = std::stof(v); phase_a_novelty_threshold_set = true; }
                catch (...) { std::cerr << "Error: invalid float for --phase-a-novelty-threshold" << std::endl; return 2; }
            } else if (arg == "--phase-a-ema") {
                // Enable EMA stabilizer explicitly
                phase_a_ema_enable = true;
                phase_a_ema_enable_set = true;
            } else if (starts_with(arg, "--phase-a-ema=")) {
                // Toggle EMA stabilizer on/off
                auto v = arg.substr(std::string("--phase-a-ema=").size());
                if (!parse_on_off_flag(v, phase_a_ema_enable)) { std::cerr << "Error: --phase-a-ema must be on|off|true|false|1|0" << std::endl; return 2; }
                phase_a_ema_enable_set = true;
            } else if (starts_with(arg, "--phase-a-ema-min=")) {
                // Override minimum EMA coefficient
                auto v = arg.substr(std::string("--phase-a-ema-min=").size());
                try {
                    phase_a_ema_min = std::stod(v);
                    if (phase_a_ema_min <= 0.0 || phase_a_ema_min >= 1.0) { std::cerr << "Error: --phase-a-ema-min must be in (0,1)" << std::endl; return 2; }
                    phase_a_ema_min_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --phase-a-ema-min" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase-a-ema-max=")) {
                // Override maximum EMA coefficient
                auto v = arg.substr(std::string("--phase-a-ema-max=").size());
                try {
                    phase_a_ema_max = std::stod(v);
                    if (phase_a_ema_max <= 0.0 || phase_a_ema_max >= 1.0) { std::cerr << "Error: --phase-a-ema-max must be in (0,1)" << std::endl; return 2; }
                    phase_a_ema_max_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --phase-a-ema-max" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase-a-mimicry-repeats=")) {
                auto v = arg.substr(std::string("--phase-a-mimicry-repeats=").size());
                try {
                    phase_a_mimicry_repeats = std::stoi(v);
                    if (phase_a_mimicry_repeats < 1) { std::cerr << "Error: --phase-a-mimicry-repeats must be >= 1" << std::endl; return 2; }
                    phase_a_mimicry_repeats_set = true;
                } catch (...) { std::cerr << "Error: invalid int for --phase-a-mimicry-repeats" << std::endl; return 2; }
            } else if (starts_with(arg, "--negative-sampling-k=")) {
                auto v = arg.substr(std::string("--negative-sampling-k=").size());
                try {
                    phase_a_negative_k = std::stoi(v);
                    if (phase_a_negative_k < 0) { std::cerr << "Error: --negative-sampling-k must be >= 0" << std::endl; return 2; }
                    phase_a_negative_k_set = true;
                } catch (...) { std::cerr << "Error: invalid int for --negative-sampling-k" << std::endl; return 2; }
            } else if (starts_with(arg, "--negative-weight=")) {
                auto v = arg.substr(std::string("--negative-weight=").size());
                try {
                    phase_a_negative_weight = std::stof(v);
                    if (phase_a_negative_weight < 0.0f) { std::cerr << "Error: --negative-weight must be >= 0" << std::endl; return 2; }
                    phase_a_negative_weight_set = true;
                } catch (...) { std::cerr << "Error: invalid float for --negative-weight" << std::endl; return 2; }
            } else if (arg == "--phase6") {
                phase6_enable = true;
            } else if (starts_with(arg, "--phase6=")) {
                auto v = arg.substr(std::string("--phase6=").size());
                if (!parse_on_off_flag(v, phase6_enable)) { std::cerr << "Error: --phase6 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase6-active=")) {
                auto v = arg.substr(std::string("--phase6-active=").size());
                std::string vv;
                vv.reserve(v.size());
                for (char c : v) vv.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                if (vv == "on" || vv == "off" || vv == "audit") {
                    phase6_active_mode = vv;
                } else {
                    std::cerr << "Error: --phase6-active must be on|off|audit" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--phase6-margin=")) {
                auto v = arg.substr(std::string("--phase6-margin=").size());
                try {
                    double m = std::stod(v);
                    if (m < 0.0 || m > 1.0) { std::cerr << "Error: --phase6-margin must be in [0,1]" << std::endl; return 2; }
                    phase6_margin = m;
                } catch (...) {
                    std::cerr << "Error: --phase6-margin expects a floating point value" << std::endl; return 2;
                }
            } else if (arg == "--phase7") {
                phase7_enable = true;
            } else if (starts_with(arg, "--phase7=")) {
                auto v = arg.substr(std::string("--phase7=").size());
                if (!parse_on_off_flag(v, phase7_enable)) { std::cerr << "Error: --phase7 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--phase7-affect") {
                phase7_affect_enable = true;
            } else if (starts_with(arg, "--phase7-affect=")) {
                auto v = arg.substr(std::string("--phase7-affect=").size());
                if (!parse_on_off_flag(v, phase7_affect_enable)) { std::cerr << "Error: --phase7-affect must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--phase7-reflect") {
                phase7_reflect_enable = true;
            } else if (starts_with(arg, "--phase7-reflect=")) {
                auto v = arg.substr(std::string("--phase7-reflect=").size());
                if (!parse_on_off_flag(v, phase7_reflect_enable)) { std::cerr << "Error: --phase7-reflect must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--phase9") {
                phase9_enable = true;
            } else if (starts_with(arg, "--phase9=")) {
                auto v = arg.substr(std::string("--phase9=").size());
                if (!parse_on_off_flag(v, phase9_enable)) { std::cerr << "Error: --phase9 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--phase9-modulation") {
                phase9_modulation_enable = true;
            } else if (starts_with(arg, "--phase9-modulation=")) {
                auto v = arg.substr(std::string("--phase9-modulation=").size());
                if (!parse_on_off_flag(v, phase9_modulation_enable)) { std::cerr << "Error: --phase9-modulation must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--phase10") {
                phase10_enable = true;
            } else if (starts_with(arg, "--phase10=")) {
                auto v = arg.substr(std::string("--phase10=").size());
                if (!parse_on_off_flag(v, phase10_enable)) { std::cerr << "Error: --phase10 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--phase11") {
                phase11_enable = true;
            } else if (starts_with(arg, "--phase11=")) {
                auto v = arg.substr(std::string("--phase11=").size());
                if (!parse_on_off_flag(v, phase11_enable)) { std::cerr << "Error: --phase11 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase11-revision-interval=")) {
                auto v = arg.substr(std::string("--phase11-revision-interval=").size());
                try {
                    phase11_revision_interval_ms = std::stoi(v);
                    if (phase11_revision_interval_ms <= 0) { std::cerr << "Error: --phase11-revision-interval must be > 0" << std::endl; return 2; }
                } catch (...) {
                    std::cerr << "Error: invalid integer for --phase11-revision-interval" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--phase11-min-gap-ms=")) {
                auto v = arg.substr(std::string("--phase11-min-gap-ms=").size());
                try {
                    phase11_min_gap_ms = std::stoi(v);
                    if (phase11_min_gap_ms < 0) { std::cerr << "Error: --phase11-min-gap-ms must be >= 0" << std::endl; return 2; }
                } catch (...) {
                    std::cerr << "Error: invalid integer for --phase11-min-gap-ms" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--phase11-outcome-window-ms=")) {
                auto v = arg.substr(std::string("--phase11-outcome-window-ms=").size());
                try {
                    phase11_outcome_eval_window_ms = std::stoi(v);
                    if (phase11_outcome_eval_window_ms < 0) { std::cerr << "Error: --phase11-outcome-window-ms must be >= 0" << std::endl; return 2; }
                } catch (...) {
                    std::cerr << "Error: invalid integer for --phase11-outcome-window-ms" << std::endl; return 2;
                }
            } else if (arg == "--stagec") {
                stagec_enable = true;
            } else if (starts_with(arg, "--stagec=")) {
                auto v = arg.substr(std::string("--stagec=").size());
                if (!parse_on_off_flag(v, stagec_enable)) { std::cerr << "Error: --stagec must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--phase12") {
                phase12_enable = true;
            } else if (starts_with(arg, "--phase12=")) {
                auto v = arg.substr(std::string("--phase12=").size());
                if (!parse_on_off_flag(v, phase12_enable)) { std::cerr << "Error: --phase12 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase12-window=")) {
                auto v = arg.substr(std::string("--phase12-window=").size());
                try {
                    phase12_window = std::stoi(v);
                    if (phase12_window < 2) { std::cerr << "Error: --phase12-window must be >= 2" << std::endl; return 2; }
                } catch (...) {
                    std::cerr << "Error: invalid integer for --phase12-window" << std::endl; return 2;
                }
            // Phase 13 flags
            } else if (arg == "--phase13") {
                phase13_enable = true;
            } else if (starts_with(arg, "--phase13=")) {
                auto v = arg.substr(std::string("--phase13=").size());
                if (!parse_on_off_flag(v, phase13_enable)) { std::cerr << "Error: --phase13 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase13-window=")) {
                auto v = arg.substr(std::string("--phase13-window=").size());
                try {
                    phase13_window = std::stoi(v);
                    if (phase13_window < 2) { std::cerr << "Error: --phase13-window must be >= 2" << std::endl; return 2; }
                } catch (...) { std::cerr << "Error: invalid integer for --phase13-window" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase13-trust-tighten=")) {
                auto v = arg.substr(std::string("--phase13-trust-tighten=").size());
                try { phase13_trust_tighten = std::stod(v); if (!(phase13_trust_tighten >= 0.0 && phase13_trust_tighten <= 1.0)) { std::cerr << "Error: --phase13-trust-tighten must be in [0,1]" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --phase13-trust-tighten" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase13-trust-expand=")) {
                auto v = arg.substr(std::string("--phase13-trust-expand=").size());
                try { phase13_trust_expand = std::stod(v); if (!(phase13_trust_expand >= 0.0 && phase13_trust_expand <= 1.0)) { std::cerr << "Error: --phase13-trust-expand must be in [0,1]" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --phase13-trust-expand" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase13-consistency-tighten=")) {
                auto v = arg.substr(std::string("--phase13-consistency-tighten=").size());
                try { phase13_consistency_tighten = std::stod(v); if (!(phase13_consistency_tighten >= 0.0 && phase13_consistency_tighten <= 1.0)) { std::cerr << "Error: --phase13-consistency-tighten must be in [0,1]" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --phase13-consistency-tighten" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase13-consistency-expand=")) {
                auto v = arg.substr(std::string("--phase13-consistency-expand=").size());
                try { phase13_consistency_expand = std::stod(v); if (!(phase13_consistency_expand >= 0.0 && phase13_consistency_expand <= 1.0)) { std::cerr << "Error: --phase13-consistency-expand must be in [0,1]" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --phase13-consistency-expand" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase13-contraction-hysteresis-ms=")) {
                auto v = arg.substr(std::string("--phase13-contraction-hysteresis-ms=").size());
                try { phase13_contraction_hysteresis_ms = std::stoi(v); if (phase13_contraction_hysteresis_ms < 0) { std::cerr << "Error: --phase13-contraction-hysteresis-ms must be >= 0" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid integer for --phase13-contraction-hysteresis-ms" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase13-expansion-hysteresis-ms=")) {
                auto v = arg.substr(std::string("--phase13-expansion-hysteresis-ms=").size());
                try { phase13_expansion_hysteresis_ms = std::stoi(v); if (phase13_expansion_hysteresis_ms < 0) { std::cerr << "Error: --phase13-expansion-hysteresis-ms must be >= 0" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid integer for --phase13-expansion-hysteresis-ms" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase13-min-log-interval-ms=")) {
                auto v = arg.substr(std::string("--phase13-min-log-interval-ms=").size());
                try { phase13_min_log_interval_ms = std::stoi(v); if (phase13_min_log_interval_ms < 0) { std::cerr << "Error: --phase13-min-log-interval-ms must be >= 0" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid integer for --phase13-min-log-interval-ms" << std::endl; return 2; }
            // Phase 14 flags
            } else if (arg == "--phase14") {
                phase14_enable = true;
            } else if (starts_with(arg, "--phase14=")) {
                auto v = arg.substr(std::string("--phase14=").size());
                if (!parse_on_off_flag(v, phase14_enable)) { std::cerr << "Error: --phase14 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase14-window=")) {
                auto v = arg.substr(std::string("--phase14-window=").size());
                try { phase14_window = std::stoi(v); if (phase14_window < 2) { std::cerr << "Error: --phase14-window must be >= 2" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid integer for --phase14-window" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase14-trust-degraded=")) {
                auto v = arg.substr(std::string("--phase14-trust-degraded=").size());
                try { phase14_trust_degraded = std::stod(v); if (!(phase14_trust_degraded >= 0.0 && phase14_trust_degraded <= 1.0)) { std::cerr << "Error: --phase14-trust-degraded must be in [0,1]" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --phase14-trust-degraded" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase14-rmse-degraded=")) {
                auto v = arg.substr(std::string("--phase14-rmse-degraded=").size());
                try { phase14_rmse_degraded = std::stod(v); if (phase14_rmse_degraded < 0.0) { std::cerr << "Error: --phase14-rmse-degraded must be >= 0" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --phase14-rmse-degraded" << std::endl; return 2; }
            // Phase 15 flags
            } else if (arg == "--phase15") {
                phase15_enable = true;
            } else if (starts_with(arg, "--phase15=")) {
                auto v = arg.substr(std::string("--phase15=").size());
                if (!parse_on_off_flag(v, phase15_enable)) { std::cerr << "Error: --phase15 must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase15-window=")) {
                auto v = arg.substr(std::string("--phase15-window=").size());
                try { phase15_window = std::stoi(v); if (phase15_window < 1) { std::cerr << "Error: --phase15-window must be >= 1" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid integer for --phase15-window" << std::endl; return 2; }
            } else if (starts_with(arg, "--phase15-risk-threshold=")) {
                auto v = arg.substr(std::string("--phase15-risk-threshold=").size());
                try { phase15_risk_threshold = std::stod(v); if (!(phase15_risk_threshold >= 0.0 && phase15_risk_threshold <= 1.0)) { std::cerr << "Error: --phase15-risk-threshold must be in [0,1]" << std::endl; return 2; } }
                catch (...) { std::cerr << "Error: invalid float for --phase15-risk-threshold" << std::endl; return 2; }
            // Context flags are handled in a pre-scan to reduce nesting depth
            } else if (arg == "--telemetry-extended") {
                telemetry_extended = true;
            } else if (starts_with(arg, "--telemetry-extended=")) {
                auto v = arg.substr(std::string("--telemetry-extended=").size());
                if (!parse_on_off_flag(v, telemetry_extended)) { std::cerr << "Error: --telemetry-extended must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--maze-view") {
                maze_view = true;
            } else if (starts_with(arg, "--maze-view=")) {
                auto v = arg.substr(std::string("--maze-view=").size());
                if (!parse_on_off_flag(v, maze_view)) {
                    std::cerr << "Error: --maze-view must be on|off|true|false|1|0" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--maze-view-interval=")) {
                auto v = arg.substr(std::string("--maze-view-interval=").size());
                try {
                    maze_view_interval_ms = std::stoi(v);
                    if (maze_view_interval_ms < 0) { std::cerr << "Error: --maze-view-interval must be non-negative" << std::endl; return 2; }
                } catch (...) {
                    std::cerr << "Error: invalid integer for --maze-view-interval" << std::endl; return 2;
                }
            } else if (starts_with(arg, "--maze-max-episode-steps=")) {
                auto v = arg.substr(std::string("--maze-max-episode-steps=").size());
                try {
                    maze_max_episode_steps = std::stoi(v);
                    if (maze_max_episode_steps <= 0) { std::cerr << "Error: --maze-max-episode-steps must be > 0" << std::endl; return 2; }
                } catch (...) { std::cerr << "Error: invalid integer for --maze-max-episode-steps" << std::endl; return 2; }
            } else if (starts_with(arg, "--episode-csv=")) {
                episode_csv_path = arg.substr(std::string("--episode-csv=").size());
                if (episode_csv_path.empty()) { std::cerr << "Error: --episode-csv requires a file path" << std::endl; return 2; }
            } else if (arg == "--summary") {
                summary = true;
            } else if (starts_with(arg, "--summary=")) {
                auto v = arg.substr(std::string("--summary=").size());
                if (!parse_on_off_flag(v, summary)) { std::cerr << "Error: --summary must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--qlearning") {
                qlearning = true;
            } else if (starts_with(arg, "--qlearning=")) {
                auto v = arg.substr(std::string("--qlearning=").size());
                if (!parse_on_off_flag(v, qlearning)) { std::cerr << "Error: --qlearning must be on|off|true|false|1|0" << std::endl; return 2; }
            } else if (arg == "--enable-learning") {
                enable_learning = true;
            } else if (starts_with(arg, "--hebbian-rate=")) {
                auto v = arg.substr(std::string("--hebbian-rate=").size());
                try {
                    lconf.hebbian_rate = std::stof(v);
                    hebbian_rate_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --hebbian-rate" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--stdp-rate=")) {
                auto v = arg.substr(std::string("--stdp-rate=").size());
                try {
                    lconf.stdp_rate = std::stof(v);
                    stdp_rate_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --stdp-rate" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--stdp-rate-multiplier=")) {
                auto v = arg.substr(std::string("--stdp-rate-multiplier=").size());
                try {
                    lconf.stdp_rate_multiplier = std::stof(v);
                    stdp_mult_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --stdp-rate-multiplier" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--attention-boost=")) {
                auto v = arg.substr(std::string("--attention-boost=").size());
                try {
                    lconf.attention_boost_factor = std::stof(v);
                    attention_boost_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --attention-boost" << std::endl;
                    return 2;
                }
            } else if (arg == "--homeostasis") {
                lconf.enable_homeostasis = true;
                homeostasis_set = true;
            } else if (starts_with(arg, "--homeostasis=")) {
                auto v = arg.substr(std::string("--homeostasis=").size());
                if (!parse_on_off_flag(v, lconf.enable_homeostasis)) {
                    std::cerr << "Error: --homeostasis must be on|off|true|false|1|0" << std::endl;
                    return 2;
                }
                homeostasis_set = true;
            } else if (starts_with(arg, "--consolidation-interval=")) {
                auto v = arg.substr(std::string("--consolidation-interval=").size());
                try {
                    int ms = std::stoi(v);
                    if (ms < 0) {
                        std::cerr << "Error: --consolidation-interval must be non-negative" << std::endl;
                        return 2;
                    }
                    lconf.update_interval = std::chrono::milliseconds(ms);
                    consolidation_interval_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid integer for --consolidation-interval" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--consolidation-strength=")) {
                auto v = arg.substr(std::string("--consolidation-strength=").size());
                try {
                    lconf.consolidation_strength = std::stof(v);
                    consolidation_strength_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --consolidation-strength" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--attention-mode=")) {
                auto v = arg.substr(std::string("--attention-mode=").size());
                std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (vlow == "none" || vlow == "off") {
                    lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::Off;
                } else if (vlow == "external" || vlow == "map" || vlow == "externalmap") {
                    lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::ExternalMap;
                } else if (vlow == "saliency") {
                    lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::Saliency;
                } else if (vlow == "topk" || vlow == "top-k") {
                    lconf.attention_mode = NeuroForge::Core::LearningSystem::AttentionMode::TopK;
                } else {
                    std::cerr << "Error: --attention-mode must be one of: none, external, saliency, topk" << std::endl;
                    return 2;
                }
                attention_mode_set = true;
                if (lconf.attention_mode != NeuroForge::Core::LearningSystem::AttentionMode::Off) {
                    lconf.enable_attention_modulation = true;
                }
            } else if (starts_with(arg, "--competence-mode=")) {
                auto v = arg.substr(std::string("--competence-mode=").size());
                std::string vlow = v; std::transform(vlow.begin(), vlow.end(), vlow.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (vlow == "off" || vlow == "none") {
                    lconf.competence_mode = NeuroForge::Core::LearningSystem::CompetenceMode::Off;
                } else if (vlow == "scale-pgate" || vlow == "scale-p_gate" || vlow == "scale-p") {
                    lconf.competence_mode = NeuroForge::Core::LearningSystem::CompetenceMode::ScalePGate;
                } else if (vlow == "scale-lr" || vlow == "scale-learning-rates" || vlow == "scale-learning") {
                    lconf.competence_mode = NeuroForge::Core::LearningSystem::CompetenceMode::ScaleLearningRates;
                } else {
                    std::cerr << "Error: --competence-mode must be one of: off, scale-pgate, scale-lr" << std::endl;
                    return 2;
                }
                competence_mode_set = true;
            } else if (starts_with(arg, "--p-gate=")) {
                auto v = arg.substr(std::string("--p-gate=").size());
                try {
                    lconf.p_gate = std::stof(v);
                    p_gate_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --p-gate" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--competence-rho=")) {
                auto v = arg.substr(std::string("--competence-rho=").size());
                try {
                    lconf.competence_rho = std::stof(v);
                    competence_rho_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --competence-rho" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--auto-eligibility=")) {
                auto v = arg.substr(std::string("--auto-eligibility=").size());
                if (!parse_on_off_flag(v, auto_elig_enabled)) {
                    std::cerr << "Error: --auto-eligibility must be on|off|true|false|1|0" << std::endl;
                    return 2;
                }
                auto_elig_set = true;
            } else if (starts_with(arg, "--homeostasis-eta=")) {
                auto v = arg.substr(std::string("--homeostasis-eta=").size());
                try {
                    lconf.homeostasis_eta = std::stof(v);
                    homeostasis_eta_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --homeostasis-eta" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--attention-Amin=")) {
                auto v = arg.substr(std::string("--attention-Amin=").size());
                try {
                    lconf.attention_Amin = std::stof(v);
                    attention_Amin_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --attention-Amin" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--attention-Amax=")) {
                auto v = arg.substr(std::string("--attention-Amax=").size());
                try {
                    lconf.attention_Amax = std::stof(v);
                    attention_Amax_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --attention-Amax" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--attention-anneal-ms=")) {
                auto v = arg.substr(std::string("--attention-anneal-ms=").size());
                try {
                    lconf.attention_anneal_ms = std::stoi(v);
                    if (lconf.attention_anneal_ms < 0) {
                        std::cerr << "Error: --attention-anneal-ms must be non-negative" << std::endl;
                        return 2;
                    }
                    attention_anneal_ms_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid integer for --attention-anneal-ms" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--chaos-steps=")) {
                auto v = arg.substr(std::string("--chaos-steps=").size());
                try {
                    lconf.chaos_steps = std::stoi(v);
                    chaos_steps_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid integer for --chaos-steps" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--consolidate-steps=")) {
                auto v = arg.substr(std::string("--consolidate-steps=").size());
                try {
                    lconf.consolidate_steps = std::stoi(v);
                    consolidate_steps_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid integer for --consolidate-steps" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--novelty-window=")) {
                auto v = arg.substr(std::string("--novelty-window=").size());
                try {
                    lconf.novelty_window = std::stoi(v);
                    novelty_window_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid integer for --novelty-window" << std::endl;
                    return 2;
                }
            } else if (starts_with(arg, "--prune-threshold=")) {
                auto v = arg.substr(std::string("--prune-threshold=").size());
                try {
                    lconf.prune_threshold = std::stof(v);
                    prune_threshold_set = true;
                } catch (...) {
                    std::cerr << "Error: invalid float for --prune-threshold" << std::endl;
                    return 2;
                }
            } else {
                bool handled_any = false;
                if (handle_io_arg(arg,
                       snapshot_csv_path,
                       snapshot_live_path,
                       snapshot_interval_ms,
                       spikes_live_path,
                       spikes_ttl_sec,
                       save_brain_path,
                       load_brain_path)) {
                    handled_any = true;
                    continue;
                }

                if (starts_with(arg, "--reward-interval=")) {
                    auto v = arg.substr(std::string("--reward-interval=").size());
                    try {
                        reward_interval_ms = std::stoi(v);
                        if (reward_interval_ms <= 0) { std::cerr << "Error: --reward-interval must be > 0" << std::endl; exit(2); }
                        reward_interval_cli_set = true;
                    } catch (...) { std::cerr << "Error: invalid integer for --reward-interval" << std::endl; exit(2); }
                    handled_any = true;
                    continue;
                }
                if (starts_with(arg, "--revision-threshold=")) {
                    auto v = arg.substr(std::string("--revision-threshold=").size());
                    try { phase11_revision_threshold = std::stof(v); }
                    catch (...) { std::cerr << "Error: invalid float for --revision-threshold" << std::endl; exit(2); }
                    handled_any = true;
                    continue;
                }
                if (starts_with(arg, "--revision-mode=")) {
                    phase11_revision_mode = arg.substr(std::string("--revision-mode=").size());
                    handled_any = true;
                    continue;
                }
                if (handle_telemetry_arg(arg,
                       log_json,
                       log_json_path,
                       log_json_sample_val,
                       log_json_events_csv,
                       memory_db_path,
                       memdb_debug,
                       memdb_interval_ms,
                       memdb_interval_cli_set,
                       flag_list_runs,
                       flag_list_episodes,
                       list_episodes_run_id,
                       flag_recent_rewards,
                       recent_rewards_run_id,
                       recent_rewards_limit,
                       flag_recent_run_events,
                       recent_run_events_run_id,
                       recent_run_events_limit)) {
                    handled_any = true;
                    continue;
                }
                if (handle_demo_arg(arg,
                       heatmap_view,
                       heatmap_interval_ms,
                       heatmap_size,
                       heatmap_threshold,
                       vision_demo,
                       audio_demo,
                       motor_cortex,
                       social_perception,
                       social_view,
                       cross_modal,
                       audio_mic,
                       audio_system,
                       audio_file_path,
                       camera_index,
                       camera_backend,
                       vision_source,
                       retina_rect_x,
                       retina_rect_y,
                       retina_rect_w,
                       retina_rect_h,
                       youtube_mode,
                       foveation_enable,
                       fovea_w,
                       fovea_h,
                       fovea_mode,
                       fovea_alpha)) {
                    handled_any = true;
                    continue;
                }
                if (handle_dataset_arg(arg,
                       dataset_triplets_root,
                       dataset_mode,
                       dataset_limit,
                       dataset_shuffle,
                       reward_scale)) {
                    handled_any = true;
                    continue;
                }
                if (handle_vision_arg(arg, vcfg)) {
                    handled_any = true;
                    continue;
                }
                if (handle_audio_arg(arg, acfg)) {
                    handled_any = true;
                    continue;
                }
                if (handle_m6_m7_parameters(arg, hippocampal_snapshots, hippocampal_snapshots_set,
                                          memory_independent, memory_independent_set,
                                          consolidation_interval_m6, consolidation_interval_m6_set,
                                          autonomous_mode, autonomous_mode_set,
                                          substrate_mode, substrate_mode_set,
                                          curiosity_threshold, curiosity_threshold_set,
                                          uncertainty_threshold, uncertainty_threshold_set,
                                          prediction_error_threshold, prediction_error_threshold_set,
                                          max_concurrent_tasks, max_concurrent_tasks_set,
                                          task_generation_interval, task_generation_interval_set,
                                          eliminate_scaffolds, eliminate_scaffolds_set,
                                          autonomy_metrics, autonomy_metrics_set,
                                          autonomy_target, autonomy_target_set,
                                          motivation_decay, motivation_decay_set,
                                          exploration_bonus, exploration_bonus_set,
                                          novelty_memory_size, novelty_memory_size_set,
                                          enable_selfnode, enable_selfnode_set,
                                          enable_pfc, enable_pfc_set,
                                          enable_motor_cortex, enable_motor_cortex_set)) {
                    continue;
                }
                if (!handled_any) {
                    std::cerr << "Warning: unrecognized option '" << arg << "' (ignored)" << std::endl;
                }
            }
        }


        // Configure MemoryDB periodic logging interval via environment variable or assertion-mode hybrid default
        // Priority: CLI > NF_MEMDB_INTERVAL_MS > assertion-mode default (50ms) > code default (1000ms)
        if (!memdb_interval_cli_set) {
            const char* env_interval = std::getenv("NF_MEMDB_INTERVAL_MS");
            if (env_interval && *env_interval) {
                try {
                    int parsed = std::stoi(env_interval);
                    if (parsed > 0) {
                        memdb_interval_ms = parsed;
                        std::cerr << "Info: Using NF_MEMDB_INTERVAL_MS=" << memdb_interval_ms << " for periodic telemetry interval" << std::endl;
                    } else {
                        std::cerr << "Warning: NF_MEMDB_INTERVAL_MS must be > 0; keeping default " << memdb_interval_ms << std::endl;
                    }
                } catch (...) {
                    std::cerr << "Warning: invalid NF_MEMDB_INTERVAL_MS; keeping default " << memdb_interval_ms << std::endl;
                }
            } else {
                // Hybrid approach: faster default in assertion mode for deep testing
                const char* env_assert = std::getenv("NF_ASSERT_ENGINE_DB");
                if (env_assert && *env_assert && std::string(env_assert) != "0") {
                    memdb_interval_ms = 50;
                    std::cerr << "Info: Assertion mode detected; using fast telemetry interval " << memdb_interval_ms << "ms" << std::endl;
                }
            }
        }

        if (show_help) {
            print_usage();
            return 0;
        }

        // Emergent-only: environment variable fallback if CLI not set
        if (!emergent_only_set) {
            if (const char* env_em = std::getenv("NF_EMERGENT_ONLY")) {
                std::string v = env_em; std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                if (v == "1" || v == "true" || v == "on" || v == "yes") emergent_only = true;
                else if (v == "0" || v == "false" || v == "off" || v == "no") emergent_only = false;
            }
        }

        if (stdp_mult_set && lconf.stdp_rate_multiplier <= 0.0f) {
            std::cerr << "Error: --stdp-rate-multiplier must be > 0" << std::endl;
            return 2;
        }
        if (attention_boost_set && lconf.attention_boost_factor <= 0.0f) {
            std::cerr << "Error: --attention-boost must be > 0" << std::endl;
            return 2;
        }

        // Phase-5 validation
        if (p_gate_set && (lconf.p_gate < 0.0f || lconf.p_gate > 1.0f)) {
            std::cerr << "Error: --p-gate must be in [0,1]" << std::endl;
            return 2;
        }
        if (homeostasis_eta_set && (lconf.homeostasis_eta < 0.0f || lconf.homeostasis_eta > 1.0f)) {
            std::cerr << "Error: --homeostasis-eta must be in [0,1]" << std::endl;
            return 2;
        }
        if (competence_rho_set && (lconf.competence_rho < 0.0f || lconf.competence_rho > 1.0f)) {
            std::cerr << "Error: --competence-rho must be in [0,1]" << std::endl;
            return 2;
        }
        if (attention_Amin_set && (lconf.attention_Amin < 0.0f || lconf.attention_Amin > 1.0f)) {
            std::cerr << "Error: --attention-Amin must be in [0,1]" << std::endl;
            return 2;
        }
        if (attention_Amax_set && (lconf.attention_Amax < lconf.attention_Amin)) {
            std::cerr << "Error: --attention-Amax must be >= Amin" << std::endl;
            return 2;
        }

        // Emergent-only overrides: force pure neural argmax policy path
        if (emergent_only) {
            // Disable external controllers and randomness
            qlearning = false;
            hybrid_lambda = -1.0f; // ensure we don't blend with Q
            teacher_policy = "none";
            teacher_mix = 0.0f;
            epsilon = -1.0f;
            softmax_temp = 0.0f; // force WTA/argmax in neural path
            std::cout << "[Emergence] Emergent-only mode active: Q-learning/Teacher disabled; using pure motor-cortex argmax" << std::endl;
        }

        // Phase C early execution path
        if (phase_c) {
            try {
                namespace fs = std::filesystem;
                // Default Phase C output under the executable directory to keep logs co-located with the binary
                fs::path out_dir;
                if (phase_c_out.empty() || phase_c_out == "PhaseC_Logs") {
                    out_dir = get_executable_dir() / "PhaseC_Logs";
                } else {
                    out_dir = fs::path(phase_c_out);
                }
                std::error_code ec; fs::create_directories(out_dir, ec);
                if (ec) { std::cerr << "Error: failed to create output directory '" << out_dir.string() << "' : " << ec.message() << std::endl; return 2; }
                unsigned int seed = phase_c_seed;
                if (seed == 0) {
                    seed = static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count());
                }
                NeuroForge::PhaseCCSVLogger logger(out_dir);
                logger.setJsonSink([&](const std::string& line){ emit_json_line(log_json, log_json_path, line); });
                
                // Create substrate-driven Phase C instead of external computation
                // Minimal brain + learning integration for Phase C
                auto phasec_conn = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
                auto phasec_brain_shared = std::make_shared<NeuroForge::Core::HypergraphBrain>(phasec_conn);
                
                if (memdb_debug) {
                    phasec_brain_shared->setMemoryPropagationDebug(true);
                }
                if (memdb_color) {
                    phasec_brain_shared->setMemoryDBColorize(memdb_color);
                }
                
                // Initialize substrate working memory using the same brain instance
                NeuroForge::Core::SubstrateWorkingMemory::Config wm_config;
                wm_config.max_binding_capacity = phase_c_wm_capacity;
                wm_config.decay_rate = phase_c_wm_decay;
                auto substrate_working_memory = std::make_shared<NeuroForge::Core::SubstrateWorkingMemory>(
                    phasec_brain_shared, wm_config);
                // Wire MemoryDB to Phase C brain if a path is provided
                std::shared_ptr<NeuroForge::Core::MemoryDB> phasec_memdb;
                std::int64_t phasec_memdb_run_id = 0;
                // Environment fallback: NF_TELEMETRY_DB for Phase C if CLI path missing
                if (memory_db_path.empty()) {
                    const char* env_telemetry = std::getenv("NF_TELEMETRY_DB");
                    if (env_telemetry && *env_telemetry) {
                        memory_db_path = env_telemetry;
                        std::cerr << "Info: Using NF_TELEMETRY_DB for Phase C MemoryDB ('" << memory_db_path << "')" << std::endl;
                    }
                }
                if (!memory_db_path.empty()) {
                    // Open MemoryDB and begin a run; propagate to Phase C brain for reward logging
                    phasec_memdb = std::make_shared<NeuroForge::Core::MemoryDB>(memory_db_path);
                    phasec_memdb->setDebug(memdb_debug);
                    if (!phasec_memdb->open()) {
                        std::cerr << "Warning: failed to open MemoryDB at '" << memory_db_path << "' for Phase C path; continuing without persistence" << std::endl;
                        phasec_memdb.reset();
                    } else {
                        std::ostringstream meta;
                        meta.setf(std::ios::fixed);
                        meta << "{\"phase\":\"C\",\"mode\":\"" << phase_c_mode << "\""
                             << ",\"seed\":" << seed
                             << ",\"wm_capacity\":" << wm_config.max_binding_capacity
                             << ",\"wm_decay\":" << std::setprecision(4) << wm_config.decay_rate
                             << ",\"hazard_weight\":" << std::setprecision(4) << (phase_c_hazard_weight_set ? phase_c_hazard_weight : 0.0f)
                             << ",\"survival_reward_scale\":" << std::setprecision(4) << phase_c_survival_scale
                             << ",\"emit_survival_rewards\":" << (phase_c_survival_bias ? "true" : "false")
                             << "}";
                        if (!phasec_memdb->beginRun(meta.str(), phasec_memdb_run_id)) {
                            std::cerr << "Warning: failed to begin MemoryDB run for Phase C path; continuing without persistence" << std::endl;
                            phasec_memdb.reset();
                        } else {
                            std::cerr << "Info: Phase C MemoryDB enabled at '" << memory_db_path << "' (run=" << phasec_memdb_run_id << ")" << std::endl;
                            phasec_brain_shared->setMemoryDB(phasec_memdb, phasec_memdb_run_id);
                            // Emit lifecycle start marker for Phase C run
                            {
                                std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                                std::int64_t event_id = 0;
                                (void)phasec_memdb->insertRunEvent(phasec_memdb_run_id, ts_ms, 0ULL, std::string("start"), std::string("phasec_start"), 0, nf_process_rss_mb(), 0.0, event_id);
                            }
                        }
                    }
                }
                
                // Create substrate Phase C adapter (replaces GlobalWorkspacePhaseC)
                class SubstratePhaseCAdapter {
                public:
                    SubstratePhaseCAdapter(std::shared_ptr<NeuroForge::Core::HypergraphBrain> brain,
                                          std::shared_ptr<NeuroForge::Core::SubstrateWorkingMemory> working_memory,
                                          NeuroForge::PhaseCCSVLogger& logger,
                                          const NeuroForge::Core::SubstratePhaseC::Config& cfg)
                        : brain_(brain), working_memory_(working_memory), logger_(logger) {
                        
                        // Initialize substrate Phase C
                        NeuroForge::Core::SubstratePhaseC::Config config = cfg;
                        substrate_phase_c_ = std::make_unique<NeuroForge::Core::SubstratePhaseC>(brain, working_memory, config);
                        substrate_phase_c_->initialize();
                        // Wire JSON sink from Phase C logger into substrate for telemetry
                        substrate_phase_c_->setJsonSink(logger_.getJsonSink());
                    }
                    
                    void stepBinding(int step) {
                        // Set binding goal in substrate instead of external computation
                        std::map<std::string, std::string> binding_params;
                        
                        // Generate binding parameters from substrate state or external input
                        std::vector<std::string> colors = {"red", "green", "blue"};
                        std::vector<std::string> shapes = {"square", "circle", "triangle"};
                        
                        binding_params["color"] = colors[step % colors.size()];
                        binding_params["shape"] = shapes[(step / 2) % shapes.size()];
                        
                        // Set goal in substrate
                        substrate_phase_c_->setGoal("binding", binding_params);
                        
                        // Process substrate step
                        substrate_phase_c_->processStep(step, 0.1f);
                        
                        // Get results from substrate behavior
                        auto binding_results = substrate_phase_c_->getBindingResults(step);
                        
                        // Log results using existing logger interface
                        for (const auto& binding : binding_results) {
                            logger_.logBinding(binding);
                        }
                        
                        // Get assemblies for timeline logging
                        auto assemblies = substrate_phase_c_->getCurrentAssemblies();
                        if (!assemblies.empty()) {
                            // Convert substrate assembly to Phase C assembly format
                            NeuroForge::Assembly winner;
                            winner.id = 0;
                            winner.symbol = assemblies[0].symbol;
                            winner.score = assemblies[0].coherence_score;
                            
                            logger_.logTimeline(step, winner);
                            
                            // Log all assemblies
                            std::vector<NeuroForge::Assembly> phase_c_assemblies;
                            for (std::size_t i = 0; i < assemblies.size(); ++i) {
                                NeuroForge::Assembly assembly;
                                assembly.id = static_cast<int>(i);
                                assembly.symbol = assemblies[i].symbol;
                                assembly.score = assemblies[i].coherence_score;
                                phase_c_assemblies.push_back(assembly);
                            }
                            logger_.logAssemblies(step, phase_c_assemblies);
                        }
                        
                        // Log working memory state
                        if (working_memory_) {
                            auto bindings = working_memory_->getCurrentBindings();
                            std::vector<NeuroForge::WorkingMemoryItem> wm_items;
                            for (const auto& binding : bindings) {
                                NeuroForge::WorkingMemoryItem item;
                                item.role = binding.role_label;
                                item.filler = binding.filler_label;
                                item.strength = binding.strength;
                                wm_items.push_back(item);
                            }
                            logger_.logWorkingMemory(step, wm_items);
                        }
                    }
                    
                    void stepSequence(int step) {
                        // Set sequence goal in substrate
                        std::map<std::string, std::string> sequence_params;
                        std::vector<std::string> seq_tokens = {"A", "B", "C", "D"};
                        
                        sequence_params["target"] = seq_tokens[step % seq_tokens.size()];
                        
                        // Set goal in substrate
                        substrate_phase_c_->setGoal("sequence", sequence_params);
                        
                        // Process substrate step
                        substrate_phase_c_->processStep(step, 0.1f);
                        
                        // Get sequence result from substrate behavior
                        auto sequence_result = substrate_phase_c_->getSequenceResult(step);
                        logger_.logSequence(sequence_result);
                        
                        // Get assemblies for timeline logging
                        auto assemblies = substrate_phase_c_->getCurrentAssemblies();
                        if (!assemblies.empty()) {
                            // Convert substrate assembly to Phase C assembly format
                            NeuroForge::Assembly winner;
                            winner.id = 0;
                            winner.symbol = assemblies[0].symbol;
                            winner.score = assemblies[0].coherence_score;
                            
                            logger_.logTimeline(step, winner);
                            
                            // Log all assemblies
                            std::vector<NeuroForge::Assembly> phase_c_assemblies;
                            for (std::size_t i = 0; i < assemblies.size(); ++i) {
                                NeuroForge::Assembly assembly;
                                assembly.id = static_cast<int>(i);
                                assembly.symbol = assemblies[i].symbol;
                                assembly.score = assemblies[i].coherence_score;
                                phase_c_assemblies.push_back(assembly);
                            }
                            logger_.logAssemblies(step, phase_c_assemblies);
                        }
                        
                        // Log working memory state
                        if (working_memory_) {
                            auto bindings = working_memory_->getCurrentBindings();
                            std::vector<NeuroForge::WorkingMemoryItem> wm_items;
                            for (const auto& binding : bindings) {
                                NeuroForge::WorkingMemoryItem item;
                                item.role = binding.role_label;
                                item.filler = binding.filler_label;
                                item.strength = binding.strength;
                                wm_items.push_back(item);
                            }
                            logger_.logWorkingMemory(step, wm_items);
                        }
                    }
                    
                    void setWorkingMemoryParams(std::size_t capacity, float decay) {
                        if (working_memory_) {
                            // Update configuration - SubstrateWorkingMemory doesn't have direct setters
                            // The configuration is set during construction
                            // For runtime updates, we would need to add these methods to SubstrateWorkingMemory
                        }
                    }
                    
                    void setSequenceWindow(std::size_t window) {
                        // Update substrate Phase C configuration
                        if (substrate_phase_c_) {
                            substrate_phase_c_->setMaxAssemblies(window);
                        }
                    }

                    void setHazardCoherenceWeight(float weight) {
                        // Update hazard coherence modulation weight in substrate config
                        if (substrate_phase_c_) {
                            substrate_phase_c_->setHazardCoherenceWeight(weight);
                        }
                    }

                    void setEmitSurvivalRewards(bool enable) {
                        // Enable or disable SurvivalBias-based shaped reward emission per step
                        if (substrate_phase_c_) {
                            substrate_phase_c_->setEmitSurvivalRewards(enable);
                        }
                    }

                    void setSurvivalRewardScale(float scale) {
                        // Set magnitude of SurvivalBias-shaped reward emission
                        if (substrate_phase_c_) {
                            substrate_phase_c_->setSurvivalRewardScale(scale);
                        }
                    }

                    // Inject SurvivalBias to modulate assembly coherence and emit telemetry
                    void setSurvivalBias(std::shared_ptr<NeuroForge::Biases::SurvivalBias> bias) {
                        if (substrate_phase_c_) {
                            substrate_phase_c_->setSurvivalBias(std::move(bias));
                        }
                    }
                    
                private:
                    std::shared_ptr<NeuroForge::Core::HypergraphBrain> brain_;
                    std::shared_ptr<NeuroForge::Core::SubstrateWorkingMemory> working_memory_;
                    NeuroForge::PhaseCCSVLogger& logger_;
                    std::unique_ptr<NeuroForge::Core::SubstratePhaseC> substrate_phase_c_;
                };
                
                // Build Phase C configuration from CLI thresholds if provided
                NeuroForge::Core::SubstratePhaseC::Config phasec_cfg;
                if (phase_c_binding_threshold_set) phasec_cfg.binding_threshold = phase_c_binding_threshold;
                if (phase_c_sequence_threshold_set) phasec_cfg.sequence_threshold = phase_c_sequence_threshold;
                if (phase_c_binding_coherence_min_set) phasec_cfg.binding_coherence_min = phase_c_binding_coherence_min;
                if (phase_c_sequence_coherence_min_set) phasec_cfg.sequence_coherence_min = phase_c_sequence_coherence_min;
                if (phase_c_prune_coherence_threshold_set) phasec_cfg.prune_coherence_threshold = phase_c_prune_coherence_threshold;
                SubstratePhaseCAdapter gw(phasec_brain_shared, 
                                         substrate_working_memory, logger,
                                         phasec_cfg);
                
                // Apply runtime WM/sequence parameters
                gw.setWorkingMemoryParams(phase_c_wm_capacity, phase_c_wm_decay);
                if (phase_c_seq_window > 0) gw.setSequenceWindow(phase_c_seq_window);

                // Optional: enable SurvivalBias modulation via CLI flags
                std::shared_ptr<NeuroForge::Biases::SurvivalBias> phasec_sb;
                if (phase_c_survival_bias) {
                    NeuroForge::Biases::SurvivalBias::Config sb_cfg;
                    sb_cfg.variance_sensitivity = phase_c_variance_sensitivity;
                    sb_cfg.hazard_alpha = phase_c_hazard_alpha;
                    sb_cfg.hazard_beta = phase_c_hazard_beta;
                    phasec_sb = std::make_shared<NeuroForge::Biases::SurvivalBias>(sb_cfg);
                    gw.setSurvivalBias(phasec_sb);
                    // Enable shaped reward emission tied to SurvivalBias metrics
                    gw.setEmitSurvivalRewards(true);
                    // Apply survival reward scale if provided
                    if (phase_c_survival_scale_set) {
                        gw.setSurvivalRewardScale(phase_c_survival_scale);
                    }
                    if (phase_c_hazard_weight_set) {
                        gw.setHazardCoherenceWeight(phase_c_hazard_weight);
                    }
                }
                
                // Build a minimal substrate to ensure non-empty synapse snapshots
                {
                    auto r_in = phasec_brain_shared->createRegion("PhaseC_Input",
                        NeuroForge::Core::Region::Type::Cortical,
                        NeuroForge::Core::Region::ActivationPattern::Asynchronous);
                    auto r_out = phasec_brain_shared->createRegion("PhaseC_Output",
                        NeuroForge::Core::Region::Type::Subcortical,
                        NeuroForge::Core::Region::ActivationPattern::Competitive);
                    if (r_in) r_in->createNeurons(32);
                    if (r_out) r_out->createNeurons(32);
                    // Light recurrent structure and forward projection
                    if (r_in) {
                        (void)phasec_brain_shared->connectRegions(r_in->getId(), r_in->getId(), 0.10f, {0.05f, 0.20f});
                    }
                    if (r_in && r_out) {
                        (void)phasec_brain_shared->connectRegions(r_in->getId(), r_out->getId(), 0.50f, {0.10f, 0.90f});
                    }
                }
                bool phasec_learning_initialized = false;
        if (enable_learning || hebbian_rate_set || stdp_rate_set || stdp_mult_set || attention_boost_set || homeostasis_set || consolidation_interval_set || consolidation_strength_set || alpha_set || gamma_set || eta_set || lambda_set || etaElig_set || kappa_set || attention_mode_set || p_gate_set || homeostasis_eta_set || attention_Amin_set || attention_Amax_set || attention_anneal_ms_set || chaos_steps_set || consolidate_steps_set || novelty_window_set || prune_threshold_set || auto_elig_set || competence_mode_set || competence_rho_set || !snapshot_csv_path.empty()) {
                    // Initialize brain (even with no regions; learning updates will be safe no-ops if empty)
                    (void)phasec_brain_shared->initialize();
                    // Align update interval with step size unless explicitly overridden
                if (!consolidation_interval_set) {
                    lconf.update_interval = std::chrono::milliseconds(step_ms > 0 ? step_ms : 0);
                }
                // Apply GPU preference
                lconf.prefer_gpu = prefer_gpu;
                    // Set default learning rates for Phase C if not explicitly set
                    if (!hebbian_rate_set && !stdp_rate_set && enable_learning) {
                        lconf.hebbian_rate = 0.001f;  // Default Hebbian learning rate
                        lconf.stdp_rate = 0.002f;     // Default STDP learning rate
                        lconf.global_learning_rate = 0.01f;  // Default global learning rate
                    }
                    if (phasec_brain_shared->initializeLearning(lconf)) {
                        phasec_brain_shared->setLearningEnabled(true);
                        auto* ls_init = phasec_brain_shared->getLearningSystem();
                        if (ls_init) {
                            if (auto_elig_set) {
                                ls_init->setAutoEligibilityAccumulation(auto_elig_enabled);
                            }
                            if (alpha_set || gamma_set || eta_set || lambda_set || etaElig_set || kappa_set) {
                                ls_init->configurePhase4(lambda_param, etaElig_param, kappa_param, alpha_weight, gamma_weight, eta_weight);
                            }
                            phasec_learning_initialized = true;
                        }
                    } else {
                        phasec_learning_initialized = (phasec_brain_shared->getLearningSystem() != nullptr);
                    }
                }

                auto last_snapshot = std::chrono::steady_clock::now();
                // Track consolidation events for JSON telemetry
                uint64_t last_consolidation_events = 0;
                // Phase C periodic MemoryDB logging timer
                auto phasec_last_memdb_log = std::chrono::steady_clock::now();
                if (phasec_learning_initialized) {
                    auto* ls0 = phasec_brain_shared->getLearningSystem();
                    if (ls0) {
                        last_consolidation_events = ls0->getStatistics().consolidation_events;
                    }
                }
                bool warned_live_no_ls = false;
                for (int s = 0; s < steps; ++s) {
                    // Inject external hazard into SurvivalBias (priority: CLI constant >0, else audio RMS)
                    if (phasec_sb) {
                        if (hazard_density > 0.0f) {
                            phasec_sb->setExternalHazard(hazard_density);
                        } else if (audio_demo && !last_audio_features.empty()) {
                            double sumsq = 0.0;
                            for (float f : last_audio_features) { sumsq += static_cast<double>(f) * static_cast<double>(f); }
                            float rms = 0.0f;
                            if (!last_audio_features.empty()) {
                                rms = static_cast<float>(std::sqrt(sumsq / static_cast<double>(last_audio_features.size())));
                            }
                            if (rms < 0.0f) rms = 0.0f; if (rms > 1.0f) rms = 1.0f;
                            phasec_sb->setExternalHazard(rms);
                        }
                    }
                    if (phase_c_mode == "binding") gw.stepBinding(s); else gw.stepSequence(s);
                    
                    // Process neural substrate step to trigger learning updates
                    float dt = (step_ms > 0 ? static_cast<float>(step_ms) / 1000.0f : 0.01f);
                    phasec_brain_shared->processStep(dt);
                    
                    // Periodic MemoryDB logging for Phase C path
                    {
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - phasec_last_memdb_log).count();
                        if (elapsed_ms >= memdb_interval_ms) {
                            int steps_since = static_cast<int>(elapsed_ms / (step_ms > 0 ? step_ms : 10));
                            if (steps_since <= 0) { steps_since = 1; }
                            float hz = (elapsed_ms > 0) ? (1000.0f * static_cast<float>(steps_since) / static_cast<float>(elapsed_ms)) : 0.0f;
                            if (phasec_memdb && phasec_memdb_run_id != 0) {
                                auto statsOpt2 = phasec_brain_shared->getLearningStatistics();
                                if (statsOpt2.has_value()) {
                                    auto ts_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::system_clock::now().time_since_epoch()).count());
                                     phasec_memdb->insertLearningStats(
                                         ts_ms,
                                         s,
                                         hz,
                                         statsOpt2.value(),
                                         static_cast<std::uint64_t>(phasec_memdb_run_id)
                                     );
                                }
                            }
                            phasec_last_memdb_log = now;
                        }
                    }
                    
                    if (phasec_learning_initialized) {
                        // Note: processStep already calls updateLearning internally,
                        // but we keep this for any additional learning-specific processing
                        auto* ls = phasec_brain_shared->getLearningSystem();
                        if (ls) {
                            // Additional learning processing if needed
                        }
                    }

                    // Emit JSON telemetry when consolidation events occur
                    if (phasec_learning_initialized) {
                        auto statsOpt = phasec_brain_shared->getLearningStatistics();
                        if (statsOpt.has_value()) {
                            const auto& sstats = statsOpt.value();
                            if (sstats.consolidation_events > last_consolidation_events) {
                                uint64_t delta = sstats.consolidation_events - last_consolidation_events;
                                std::ostringstream js;
                                js << "{\"version\":1,\"phase\":\"C\",\"event\":\"consolidation\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                                js << "\"payload\":{\"count\":" << delta
                                   << ",\"total\":" << sstats.consolidation_events
                                   << ",\"rate\":" << sstats.memory_consolidation_rate
                                   << ",\"active_synapses\":" << sstats.active_synapses
                                   << ",\"potentiated_synapses\":" << sstats.potentiated_synapses
                                   << ",\"depressed_synapses\":" << sstats.depressed_synapses << "}}";
                                emit_json_line(log_json, log_json_path, js.str());
                                last_consolidation_events = sstats.consolidation_events;
                            }
                        }
                    }

                    // Periodic live snapshot CSV export (if requested)
                    if (!snapshot_live_path.empty()) {
                        if (!phasec_learning_initialized) {
                            if (!warned_live_no_ls) {
                                std::cerr << "Info: live export requested (--snapshot-live) but LearningSystem is not initialized. Enable learning to export live data." << std::endl;
                                warned_live_no_ls = true;
                            }
                        } else {
                            auto now = std::chrono::steady_clock::now();
                            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_snapshot).count() >= snapshot_interval_ms) {
                                auto* ls = phasec_brain_shared->getLearningSystem();
                                if (ls) {
                                    auto snapshots = ls->getSynapseSnapshot();
                                    std::ofstream ofs(snapshot_live_path, std::ios::out | std::ios::trunc);
                                    if (!ofs) {
                                        std::cerr << "Error: failed to open '" << snapshot_live_path << "' for live snapshot writing" << std::endl;
                                    } else {
                                        ofs << "pre_neuron,post_neuron,weight\n";
                                        for (const auto& srec : snapshots) {
                                            ofs << srec.pre_neuron << "," << srec.post_neuron << "," << srec.weight << "\n";
                                        }
                                        ofs.flush();
                                    }
                                }
                                last_snapshot = now;
                            }
                        }
                    }

                    #ifdef _WIN32
                    {
                        MSG msg;
                        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                            if (msg.message == WM_QUIT) {
                                g_abort.store(true);
                                break;
                            }
                            TranslateMessage(&msg);
                            DispatchMessageW(&msg);
                        }
                    }
                    #endif
                    if (step_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
                }

                // Optional: export synapse snapshot CSV from Phase C path
                if (!snapshot_csv_path.empty()) {
                    auto* ls = phasec_brain_shared->getLearningSystem();
                    if (!ls) {
                        std::cerr << "Warning: --snapshot-csv was provided, but LearningSystem is not initialized. Enable learning to export snapshots." << std::endl;
                    } else {
                        auto snapshots = ls->getSynapseSnapshot();
                        std::ofstream ofs(snapshot_csv_path, std::ios::out | std::ios::trunc);
                        if (!ofs) {
                            std::cerr << "Error: failed to open '" << snapshot_csv_path << "' for writing" << std::endl;
                        } else {
                            ofs << "pre_neuron,post_neuron,weight\n";
                            for (const auto& s : snapshots) {
                                ofs << s.pre_neuron << "," << s.post_neuron << "," << s.weight << "\n";
                            }
                            ofs.flush();
                            std::cout << "Wrote synapse snapshot: " << snapshots.size() << " edges to '" << snapshot_csv_path << "'\n";
                        }
                    }
                }

                // Always print learning statistics section for CI visibility (Phase C path)
                {
                    std::cout << "\nLearning System Statistics\n";
                    auto statsOpt = phasec_brain_shared->getLearningStatistics();
                    if (statsOpt.has_value()) {
                        const auto &s = statsOpt.value();
                        std::cout << "  Total Updates: " << s.total_updates << "\n"
                                 << "  Hebbian Updates: " << s.hebbian_updates << "\n"
                                 << "  STDP Updates: " << s.stdp_updates << "\n"
                                 << "  Phase-4 Updates: " << s.reward_updates << "\n"
                                 << "  Avg Weight Change: " << s.average_weight_change << "\n"
                                 << "  Consolidation Rate: " << s.memory_consolidation_rate << "\n"
                                 << "  Active Synapses: " << s.active_synapses << "\n"
                                 << "  Potentiated Synapses: " << s.potentiated_synapses << "\n"
                                 << "  Depressed Synapses: " << s.depressed_synapses << "\n";
                    } else {
                        std::cout << "  Total Updates: 0\n"
                                 << "  Hebbian Updates: 0\n"
                                 << "  STDP Updates: 0\n"
                                 << "  Phase-4 Updates: 0\n"
                                 << "  Avg Weight Change: 0\n"
                                 << "  Consolidation Rate: 0\n"
                                 << "  Active Synapses: 0\n"
                                 << "  Potentiated Synapses: 0\n"
                                 << "  Depressed Synapses: 0\n";
                    }
                }

                std::cout << "Phase C completed. Logs written to: " << out_dir.string() << std::endl;
                return 0;
            } catch (const std::exception& ex) {
                std::cerr << "Phase C runtime error: " << ex.what() << std::endl;
                return 2;
            }
        }
        if (attention_anneal_ms_set && lconf.attention_anneal_ms < 0) {
            std::cerr << "Error: --attention-anneal-ms must be non-negative" << std::endl;
            return 2;
        }
        if (chaos_steps_set && lconf.chaos_steps < 0) {
            std::cerr << "Error: --chaos-steps must be non-negative" << std::endl;
            return 2;
        }
        if (consolidate_steps_set && lconf.consolidate_steps < 0) {
            std::cerr << "Error: --consolidate-steps must be non-negative" << std::endl;
            return 2;
        }
        if (novelty_window_set && lconf.novelty_window <= 0) {
            std::cerr << "Error: --novelty-window must be positive" << std::endl;
            return 2;
        }
        if (prune_threshold_set && (lconf.prune_threshold < 0.0f || lconf.prune_threshold > 1.0f)) {
            std::cerr << "Error: --prune-threshold must be in [0,1]" << std::endl;
            return 2;
        }

        // Phase-4 parameter validation (unless explicitly bypassed)
        if (!phase4_unsafe && (alpha_set || gamma_set || eta_set || lambda_set || etaElig_set || kappa_set)) {
            if (lambda_set && (lambda_param < 0.0f || lambda_param > 1.0f)) {
                std::cerr << "Error: --lambda must be in [0,1]" << std::endl;
                return 2;
            }
            if (etaElig_set && (etaElig_param < 0.0f || etaElig_param > 1.0f)) {
                std::cerr << "Error: --eta-elig must be in [0,1]" << std::endl;
                return 2;
            }
            if (kappa_set && (kappa_param < 0.0f)) {
                std::cerr << "Error: --kappa must be >= 0" << std::endl;
                return 2;
            }
            if (alpha_set && (alpha_weight < 0.0f)) {
                std::cerr << "Error: --alpha must be >= 0" << std::endl;
                return 2;
            }
            if (gamma_set && (gamma_weight < 0.0f)) {
                std::cerr << "Error: --gamma must be >= 0" << std::endl;
                return 2;
            }
            if (eta_set && (eta_weight < 0.0f)) {
                std::cerr << "Error: --eta must be >= 0" << std::endl;
                return 2;
            }
        }


        // Handle MemoryDB listing flags early and exit
        if (flag_list_runs || flag_list_episodes || flag_recent_rewards || flag_recent_run_events) {
            if (memory_db_path.empty()) {
                std::cerr << "Error: --memory-db=PATH is required when using listing flags" << std::endl;
                return 2;
            }
            NeuroForge::Core::MemoryDB mdb(memory_db_path);
            mdb.setDebug(memdb_debug);
            if (!mdb.open()) {
                std::cerr << "Error: failed to open MemoryDB at '" << memory_db_path << "'" << std::endl;
                return 1;
            }
            if (flag_list_runs) {
                auto runs = mdb.getRuns();
                std::cout << "Runs count=" << runs.size() << "\n";
                for (const auto& r : runs) {
                    std::cout << r.id << "," << r.started_ms << "," << r.metadata_json << "\n";
                }
            }
            if (flag_list_episodes) {
                long long run_id_ll = 0;
                try {
                    run_id_ll = std::stoll(list_episodes_run_id);
                } catch (...) {
                    std::cerr << "Error: RUN_ID for --list-episodes must be an integer" << std::endl;
                    return 2;
                }
                auto eps = mdb.getEpisodes(static_cast<int64_t>(run_id_ll));
                std::cout << "Episodes(run=" << list_episodes_run_id << ") count=" << eps.size() << "\n";
                for (const auto& e : eps) {
                    std::cout << e.id << "," << e.name << "," << e.start_ms << "," << (e.end_ms == 0 ? -1 : e.end_ms) << "\n";
                }
            }
            if (flag_recent_rewards) {
                long long run_id_ll = 0;
                try {
                    run_id_ll = std::stoll(recent_rewards_run_id);
                } catch (...) {
                    std::cerr << "Error: RUN_ID for --recent-rewards must be an integer" << std::endl;
                    return 2;
                }
                auto rewards = mdb.getRecentRewards(static_cast<int64_t>(run_id_ll), recent_rewards_limit);
                std::cout << "RecentRewards(run=" << recent_rewards_run_id << ",limit=" << recent_rewards_limit << ") count=" << rewards.size() << "\n";
                for (const auto& r : rewards) {
                    std::cout << r.id << "," << r.ts_ms << "," << r.step << "," << r.reward << "," << r.source << "," << r.context_json << "\n";
                }
            }
            if (flag_recent_run_events) {
                long long run_id_ll = 0;
                try {
                    run_id_ll = std::stoll(recent_run_events_run_id);
                } catch (...) {
                    std::cerr << "Error: RUN_ID for --recent-run-events must be an integer" << std::endl;
                    return 2;
                }
                auto events = mdb.getRecentRunEvents(static_cast<int64_t>(run_id_ll), recent_run_events_limit);
                std::cout << "RunEvents(run=" << recent_run_events_run_id << ",limit=" << recent_run_events_limit << ") count=" << events.size() << "\n";
                for (const auto& e : events) {
                    std::cout << e.id << "," << e.ts_ms << "," << e.step << "," << e.type << "," << e.message << "," << e.exit_code << "," << e.rss_mb << "," << e.gpu_mem_mb << "\n";
                }
            }
            return 0;
        }

        // Build connectivity manager and brain
        auto conn_mgr = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
        NeuroForge::Core::HypergraphBrain brain(conn_mgr);
        if (memdb_debug) {
            brain.setMemoryPropagationDebug(true);
        }
        brain.setMemoryDBColorize(memdb_color);
        if (phase_c_lag_align_set) {
            brain.setPhaseCLagAlign(phase_c_lag_align);
        }

        bool loaded_from_checkpoint = false;
        if (!load_brain_path.empty()) {
            if (!brain.loadCheckpoint(load_brain_path)) {
                std::cerr << "Error: failed to load brain checkpoint from '" << load_brain_path << "'" << std::endl;
                return 1;
            }
            loaded_from_checkpoint = true;
            if (vision_demo || audio_demo || motor_cortex || maze_demo || cross_modal) {
                std::cerr << "Info: --load-brain provided; demo topology flags will be ignored." << std::endl;
            }
        }

        if (!memory_db_path.empty()) {
            std::cerr << "Info: --memory-db provided ('" << memory_db_path << "'). If SQLite3 is available, telemetry will be logged." << std::endl;
        }

        // Create demo topology depending on flags (can be both)
        std::shared_ptr<NeuroForge::Regions::VisualCortex> visual_region;
        std::shared_ptr<NeuroForge::Regions::AuditoryCortex> auditory_region;
        std::shared_ptr<NeuroForge::Regions::MotorCortex> motor_region;
        std::shared_ptr<NeuroForge::Core::Region> maze_obs_region;
        std::shared_ptr<NeuroForge::Core::Region> maze_action_region;
        std::shared_ptr<NeuroForge::Core::Region> social_region;
        #ifdef NF_HAVE_OPENCV
        std::unique_ptr<NeuroForge::Biases::SocialPerceptionBias> social_bias;
        #endif
        std::unique_ptr<NeuroForge::Biases::VoiceBias> voice_bias;
        std::unique_ptr<NeuroForge::Biases::MotionBias> motion_bias;
        bool any_demo = false;
        if (!loaded_from_checkpoint) {
            if (vision_demo) {
                any_demo = true;
                visual_region = std::make_shared<NeuroForge::Regions::VisualCortex>("VisualCortex", static_cast<std::size_t>(vcfg.grid_size * vcfg.grid_size));
                brain.addRegion(visual_region);
                // Ensure neuron count matches feature vector length
                visual_region->createNeurons(static_cast<std::size_t>(vcfg.grid_size * vcfg.grid_size));
                visual_region->initializeLayers();
                brain.connectRegions(visual_region->getId(), visual_region->getId(), 0.05f, {0.1f, 0.9f});
            }
            if (audio_demo) {
                any_demo = true;
                auditory_region = std::make_shared<NeuroForge::Regions::AuditoryCortex>("AuditoryCortex", static_cast<std::size_t>(acfg.feature_bins));
                brain.addRegion(auditory_region);
                auditory_region->createNeurons(static_cast<std::size_t>(acfg.feature_bins));
                auditory_region->initializeTonotopicMap();
                brain.connectRegions(auditory_region->getId(), auditory_region->getId(), 0.05f, {0.1f, 0.9f});
            }
            if (motor_cortex) {
                any_demo = true;
                motor_region = std::make_shared<NeuroForge::Regions::MotorCortex>("MotorCortex", static_cast<std::size_t>(75000));
                brain.addRegion(motor_region);
                motor_region->createNeurons(static_cast<std::size_t>(75000));
                motor_region->initializeSomatotopicMap();
                brain.connectRegions(motor_region->getId(), motor_region->getId(), 0.05f, {0.1f, 0.9f});
            }
            if (maze_demo) {
                any_demo = true;
                // Create Maze observation and action regions
                maze_obs_region = brain.createRegion("MazeObservation", NeuroForge::Core::Region::Type::Custom, NeuroForge::Core::Region::ActivationPattern::Asynchronous);
                maze_obs_region->createNeurons(static_cast<std::size_t>(maze_size * maze_size));
                maze_action_region = brain.createRegion("MazeAction", NeuroForge::Core::Region::Type::Custom, NeuroForge::Core::Region::ActivationPattern::Competitive);
                maze_action_region->createNeurons(static_cast<std::size_t>(4));
                // Connect observations -> actions and add mild recurrent dynamics in action region
                brain.connectRegions(maze_obs_region->getId(), maze_action_region->getId(), 0.20f, {0.05f, 0.15f});
                brain.connectRegions(maze_action_region->getId(), maze_action_region->getId(), 0.05f, {0.02f, 0.08f});
            }
            if (social_perception) {
                any_demo = true;
                // Create Social region for enhanced social perception
                social_region = brain.createRegion("SocialPerception", NeuroForge::Core::Region::Type::Cortical, NeuroForge::Core::Region::ActivationPattern::Asynchronous);
                social_region->createNeurons(static_cast<std::size_t>(32 * 32)); // 32x32 Social grid
                
                // Map Social modality to the region
                brain.mapModality(NeuroForge::Modality::Social, social_region->getId());
                
                #ifdef NF_HAVE_OPENCV
                // Initialize SocialPerceptionBias with enhanced features
                NeuroForge::Biases::SocialPerceptionBias::Config social_config;
                social_config.enable_face_detection = true;
                social_config.enable_gaze_tracking = true;
                social_config.enable_lip_sync = true;
                social_config.face_priority_multiplier = 2.0f;
                social_config.gaze_attention_multiplier = 1.5f;
                social_config.lip_sync_boost = 1.8f;
                
                social_bias = std::make_unique<NeuroForge::Biases::SocialPerceptionBias>(social_config);
                if (!social_bias->initialize()) {
                    std::cerr << "Warning: SocialPerceptionBias failed to initialize (OpenCV cascade files may be missing)" << std::endl;
                } else {
                    std::cout << "SocialPerceptionBias initialized with enhanced biological features" << std::endl;
                }
                
                // Wire the bias to the brain
                social_bias->setBrain(&brain);
                social_bias->setOutputGridSize(32);
                #else
                std::cout << "SocialPerceptionBias disabled (OpenCV not available)" << std::endl;
                #endif
                
                // Connect social region to other regions for cross-modal integration
                if (visual_region) {
                    brain.connectRegions(social_region->getId(), visual_region->getId(), 0.03f, {0.05f, 0.15f});
                    brain.connectRegions(visual_region->getId(), social_region->getId(), 0.03f, {0.05f, 0.15f});
                }
                if (auditory_region) {
                    brain.connectRegions(social_region->getId(), auditory_region->getId(), 0.03f, {0.05f, 0.15f});
                    brain.connectRegions(auditory_region->getId(), social_region->getId(), 0.03f, {0.05f, 0.15f});
                }
                
                std::cout << "Social perception region created with " << (32 * 32) << " neurons and cross-modal connectivity" << std::endl;
            }
            
            // Initialize VoiceBias for auditory processing enhancement
            if (audio_demo || social_perception) {
                NeuroForge::Biases::VoiceBias::Config voice_config;
                voice_config.fundamental_freq_min = 80.0f;   // Human voice range
                voice_config.fundamental_freq_max = 400.0f;
                voice_config.voice_priority_multiplier = 2.0f;
                voice_config.infant_directed_speech_boost = 2.5f;
                voice_config.enable_phoneme_templates = true;
                voice_config.enable_prosody_analysis = true;
                
                voice_bias = std::make_unique<NeuroForge::Biases::VoiceBias>(voice_config);
                std::cout << "VoiceBias initialized with human voice prioritization" << std::endl;
            }
            
            // Initialize MotionBias for visual motion enhancement
            if (vision_demo || social_perception) {
                NeuroForge::Biases::MotionBias::Config motion_config;
                motion_config.motion_threshold = 0.1f;
                motion_config.biological_motion_boost = 2.0f;
                motion_config.enable_predator_detection = true;
                motion_config.enable_trajectory_prediction = true;
                motion_config.max_tracked_objects = 10;
                
                motion_bias = std::make_unique<NeuroForge::Biases::MotionBias>(motion_config);
                std::cout << "MotionBias initialized with biological motion detection" << std::endl;
            }
            if (cross_modal && visual_region && auditory_region) {

                // Establish modest bidirectional cross-modal links between visual and auditory regions
                brain.connectRegions(visual_region->getId(), auditory_region->getId(), 0.02f, {0.05f, 0.2f});
                brain.connectRegions(auditory_region->getId(), visual_region->getId(), 0.02f, {0.05f, 0.2f});
            }
            if (!any_demo) {
                // Default generic demo
                create_demo_brain(brain);
            }

            // Create user-requested regions via registry
            for (const auto& r : add_region_specs) {
                auto region = NeuroForge::Core::RegionRegistry::instance().create(r.key, r.name, r.count);
                if (!region) {
                    std::cerr << "Error: --add-region unknown key '" << r.key << "'" << std::endl;
                    std::cerr << "Known keys: ";
                    auto keys = NeuroForge::Core::RegionRegistry::instance().listKeys();
                    for (size_t i = 0; i < keys.size(); ++i) { std::cerr << keys[i] << (i+1<keys.size()?", ":""); }
                    std::cerr << std::endl; return 2;
                }
                brain.addRegion(region);
                if (r.count > 0) {
                    region->createNeurons(r.count);
                }
            }

            // Initialize brain (which initializes regions)
        if (!brain.initialize()) {
            std::cerr << "Failed to initialize brain" << std::endl;
            return 1;
        }
        
        // Enable hardware monitoring for memory tracking
        brain.setHardwareMonitoring(true);
        } else {
            // Brain loaded from checkpoint; initialization already handled by import
        }

        // Initialize learning if requested or parameters provided
        if (enable_learning || hebbian_rate_set || stdp_rate_set || stdp_mult_set || attention_boost_set || homeostasis_set || consolidation_interval_set || consolidation_strength_set || alpha_set || gamma_set || eta_set || lambda_set || etaElig_set || kappa_set || mimicry_enable || mimicry_weight_set || !teacher_embed_path.empty() || !student_embed_path.empty() || attention_mode_set || p_gate_set || homeostasis_eta_set || attention_Amin_set || attention_Amax_set || attention_anneal_ms_set || chaos_steps_set || consolidate_steps_set || novelty_window_set || prune_threshold_set || auto_elig_set || competence_mode_set || competence_rho_set) {
            // Align update interval with step size unless explicitly overridden
            if (!consolidation_interval_set) {
                lconf.update_interval = std::chrono::milliseconds(step_ms > 0 ? step_ms : 0);
            }
            if (!brain.initializeLearning(lconf)) {
                // If already initialized, it's fine; otherwise, report
                // For this demo we proceed regardless
            }
            brain.setLearningEnabled(true);
            // Apply auto-eligibility flag if set (default is off)
            if (auto_elig_set) {
                auto* ls = brain.getLearningSystem();
                if (ls) {
                    ls->setAutoEligibilityAccumulation(auto_elig_enabled);
                    std::cout << "[Learning] auto-eligibility accumulation " << (auto_elig_enabled ? "ENABLED" : "DISABLED") << std::endl;
                } else {
                    std::cerr << "Warning: --auto-eligibility provided but LearningSystem not available." << std::endl;
                }
            }
            // Phase-5 logging of key parameters if set
            if (attention_mode_set || p_gate_set || homeostasis_eta_set || attention_Amin_set || attention_Amax_set || attention_anneal_ms_set || chaos_steps_set || consolidate_steps_set || novelty_window_set || prune_threshold_set || competence_mode_set || competence_rho_set) {
                auto cfg = brain.getLearningSystem() ? brain.getLearningSystem()->getConfig() : lconf;
                std::cout << "Phase-5: attention_mode=" << static_cast<int>(cfg.attention_mode)
                          << " p_gate=" << cfg.p_gate
                          << " homeostasis_eta=" << cfg.homeostasis_eta
                          << " A_min=" << cfg.attention_Amin
                          << " A_max=" << cfg.attention_Amax
                          << " anneal_ms=" << cfg.attention_anneal_ms
                          << " chaos_steps=" << cfg.chaos_steps
                          << " consolidate_steps=" << cfg.consolidate_steps
                          << " novelty_window=" << cfg.novelty_window
                          << " prune_threshold=" << cfg.prune_threshold
                          << " competence_mode=" << static_cast<int>(cfg.competence_mode)
                          << " competence_rho=" << cfg.competence_rho
                           << std::endl;
            }
            // Apply Phase-4 reward shaping configuration if any Phase-4 parameters were provided via CLI
            if (alpha_set || gamma_set || eta_set || lambda_set || etaElig_set || kappa_set) {
                auto* ls = brain.getLearningSystem();
                if (ls) {
                    ls->configurePhase4(lambda_param, etaElig_param, kappa_param, alpha_weight, gamma_weight, eta_weight);
                    std::cout << "Configured Phase-4: lambda=" << lambda_param
                              << " etaElig=" << etaElig_param
                              << " kappa=" << kappa_param
                              << " | alpha=" << alpha_weight
                              << " gamma=" << gamma_weight
                              << " eta=" << eta_weight
                              << (phase4_unsafe ? " (unsafe)" : "")
                              << std::endl;
                } else {
                    std::cerr << "Warning: LearningSystem not available to configure Phase-4 weights." << std::endl;
                }
            }
            // Phase-5 Mimicry wiring (enable, weight, and embedding vectors)
            {
                auto* ls = brain.getLearningSystem();
                if (ls) {
                    // Apply enable flag and weight (if provided)
                    ls->setMimicryEnabled(mimicry_enable);
                    if (mimicry_weight_set) {
                        ls->setMimicryWeight(mimicry_weight_mu);
                    }
                    // Route Phase A scores internally if requested
                    ls->setMimicryInternal(mimicry_internal);
                    // Helper to load comma/space-separated floats from a file
                    auto load_float_file = [](const std::string& path, std::vector<float>& out) -> bool {
                        std::ifstream ifs(path);
                        if (!ifs) return false;
                        out.clear();
                        std::string line;
                        while (std::getline(ifs, line)) {
                            for (char& ch : line) {
                                if (!((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E' || ch == ' ')) {
                                    ch = ' ';
                                }
                            }
                            std::istringstream iss(line);
                            float f = 0.0f;
                            while (iss >> f) { out.push_back(f); }
                        }
                        return !out.empty();
                    };
                    std::vector<float> tvec, svec;
                    bool t_ok = false, s_ok = false;
                    if (!teacher_embed_path.empty()) {
                        t_ok = load_float_file(teacher_embed_path, tvec);
                        if (!t_ok) {
                            std::cerr << "Warning: failed to load teacher embedding from '" << teacher_embed_path << "'" << std::endl;
                        } else {
                            ls->setTeacherVector(tvec);
                            std::cout << "Mimicry: loaded teacher embedding of length " << tvec.size() << " from '" << teacher_embed_path << "'" << std::endl;
                            // Also pass teacher embedding to LanguageSystem / Phase A if enabled
                            if (phase5_language_enable && language_system) {
                                language_system->setTeacherEmbedding("teacher_embed", tvec);
                                language_system->processTeacherSignal("teacher_embed", 1.0f);
                            }
                            if (phase_a_enable && phase_a_system) {
                                // Store as a generic multimodal/custom teacher embedding
                                phase_a_system->addTeacherEmbedding(
                                    tvec,
                                    NeuroForge::Core::PhaseAMimicry::TeacherType::Custom,
                                    NeuroForge::Core::PhaseAMimicry::Modality::Multimodal,
                                    "teacher_embed",
                                    teacher_embed_path,
                                    1.0f
                                );
                            }
                        }
                    }
                    if (!student_embed_path.empty()) {
                        s_ok = load_float_file(student_embed_path, svec);
                        if (!s_ok) {
                            std::cerr << "Warning: failed to load student embedding from '" << student_embed_path << "'" << std::endl;
                        } else {
                            ls->setStudentEmbedding(svec);
                            std::cout << "Mimicry: loaded initial student embedding of length " << svec.size() << " from '" << student_embed_path << "'" << std::endl;
                        }
                    }
                    if (t_ok && s_ok && tvec.size() != svec.size()) {
                        std::cerr << "Warning: teacher (" << tvec.size() << ") and student (" << svec.size() << ") embedding lengths differ; mimicry similarity will be 0." << std::endl;
                    }
                } else if (mimicry_enable || mimicry_weight_set || !teacher_embed_path.empty() || !student_embed_path.empty()) {
                    std::cerr << "Warning: Mimicry flags provided but LearningSystem not available." << std::endl;
                }
            }
        }

        // M6 Memory Internalization Integration
        if (hippocampal_snapshots_set || memory_independent_set || consolidation_interval_m6_set) {
            std::cout << "[M6] Memory Internalization parameters detected:" << std::endl;
            
            // Enable learning system for M6 functionality
            if (!brain.initializeLearning(lconf)) {
                // If already initialized, it's fine
            }
            brain.setLearningEnabled(true);
            
            if (hippocampal_snapshots_set) {
                std::cout << "  Hippocampal snapshots: " << (hippocampal_snapshots ? "ENABLED" : "DISABLED") << std::endl;
                brain.setHippocampalEnabled(hippocampal_snapshots);
            }
            if (memory_independent_set) {
                std::cout << "  Memory-independent learning: " << (memory_independent ? "ENABLED" : "DISABLED") << std::endl;
                // Configure memory independence through hippocampal config
                if (memory_independent) {
                    NeuroForge::Core::HypergraphBrain::HippocampalConfig config;
                    config.enabled = true;
                    config.auto_consolidation = true;
                    config.consolidation_threshold = 0.6f; // Lower threshold for more frequent consolidation
                    brain.configureHippocampalSnapshotting(config);
                }
            }
            if (consolidation_interval_m6_set) {
                std::cout << "  M6 consolidation interval: " << consolidation_interval_m6 << " ms" << std::endl;
                NeuroForge::Core::HypergraphBrain::HippocampalConfig config;
                config.enabled = true;
                config.snapshot_interval_ms = static_cast<std::uint64_t>(consolidation_interval_m6);
                brain.configureHippocampalSnapshotting(config);
            }
        }

        // M7 Autonomous Operation Integration
        if (autonomous_mode_set || substrate_mode_set || curiosity_threshold_set || uncertainty_threshold_set || 
            prediction_error_threshold_set || max_concurrent_tasks_set || task_generation_interval_set ||
            eliminate_scaffolds_set || autonomy_metrics_set || autonomy_target_set || motivation_decay_set ||
            exploration_bonus_set || novelty_memory_size_set || enable_selfnode_set || enable_pfc_set || enable_motor_cortex_set) {
            
            std::cout << "[M7] Autonomous Operation parameters detected:" << std::endl;
            
            // Enable learning system for M7 functionality (intrinsic motivation requires it)
            if (!brain.initializeLearning(lconf)) {
                // If already initialized, it's fine
            }
            brain.setLearningEnabled(true);
            
            if (autonomous_mode_set) {
                std::cout << "  Autonomous mode: " << (autonomous_mode ? "ENABLED" : "DISABLED") << std::endl;
                brain.setAutonomousModeEnabled(autonomous_mode);
                if (autonomous_mode) {
                    // Initialize autonomous scheduler if not already done
                    if (!brain.initializeAutonomousScheduler()) {
                        std::cerr << "Warning: Failed to initialize autonomous scheduler" << std::endl;
                    }
                }
            }
            if (substrate_mode_set) {
                std::cout << "  Substrate mode: " << substrate_mode << std::endl;
                NeuroForge::Core::HypergraphBrain::SubstrateMode mode;
                if (substrate_mode == "off") mode = NeuroForge::Core::HypergraphBrain::SubstrateMode::Off;
                else if (substrate_mode == "mirror") mode = NeuroForge::Core::HypergraphBrain::SubstrateMode::Mirror;
                else if (substrate_mode == "train") mode = NeuroForge::Core::HypergraphBrain::SubstrateMode::Train;
                else if (substrate_mode == "native") mode = NeuroForge::Core::HypergraphBrain::SubstrateMode::Native;
                else mode = NeuroForge::Core::HypergraphBrain::SubstrateMode::Off;
                brain.setSubstrateMode(mode);
            }
            if (curiosity_threshold_set) {
                std::cout << "  Curiosity threshold: " << curiosity_threshold << std::endl;
                brain.setCuriosityThreshold(curiosity_threshold);
            }
            if (uncertainty_threshold_set) {
                std::cout << "  Uncertainty threshold: " << uncertainty_threshold << std::endl;
                brain.setUncertaintyThreshold(uncertainty_threshold);
            }
            if (prediction_error_threshold_set) {
                std::cout << "  Prediction error threshold: " << prediction_error_threshold << std::endl;
                brain.setPredictionErrorThreshold(prediction_error_threshold);
            }
            if (max_concurrent_tasks_set) {
                std::cout << "  Max concurrent tasks: " << max_concurrent_tasks << std::endl;
                brain.setMaxConcurrentTasks(max_concurrent_tasks);
            }
            if (task_generation_interval_set) {
                std::cout << "  Task generation interval: " << task_generation_interval << " ms" << std::endl;
                brain.setTaskGenerationInterval(task_generation_interval);
            }
            if (eliminate_scaffolds_set) {
                std::cout << "  Eliminate scaffolds: " << (eliminate_scaffolds ? "ENABLED" : "DISABLED") << std::endl;
                brain.setEliminateScaffolds(eliminate_scaffolds);
            }
            if (autonomy_metrics_set) {
                std::cout << "  Autonomy metrics: " << (autonomy_metrics ? "ENABLED" : "DISABLED") << std::endl;
                brain.setAutonomyMetrics(autonomy_metrics);
            }
            if (autonomy_target_set) {
                std::cout << "  Autonomy target: " << autonomy_target << std::endl;
                brain.setAutonomyTarget(autonomy_target);
            }
            if (motivation_decay_set) {
                std::cout << "  Motivation decay: " << motivation_decay << std::endl;
                brain.setMotivationDecay(motivation_decay);
            }
            if (exploration_bonus_set) {
                std::cout << "  Exploration bonus: " << exploration_bonus << std::endl;
                brain.setExplorationBonus(exploration_bonus);
            }
            if (novelty_memory_size_set) {
                std::cout << "  Novelty memory size: " << novelty_memory_size << std::endl;
                brain.setNoveltyMemorySize(novelty_memory_size);
            }
            if (enable_selfnode_set) {
                std::cout << "  SelfNode integration: " << (enable_selfnode ? "ENABLED" : "DISABLED") << std::endl;
                brain.setSelfNodeIntegrationEnabled(enable_selfnode);
            }
            if (enable_pfc_set) {
                std::cout << "  PrefrontalCortex integration: " << (enable_pfc ? "ENABLED" : "DISABLED") << std::endl;
                brain.setPrefrontalCortexIntegrationEnabled(enable_pfc);
            }
            if (enable_motor_cortex_set) {
                std::cout << "  MotorCortex integration: " << (enable_motor_cortex ? "ENABLED" : "DISABLED") << std::endl;
                brain.setMotorCortexIntegrationEnabled(enable_motor_cortex);
            }
        }

        // Auto-enable basic learning parameters for M6/M7 functionality
        bool m6_m7_detected = (hippocampal_snapshots_set || memory_independent_set || consolidation_interval_m6_set ||
                               autonomous_mode_set || substrate_mode_set || curiosity_threshold_set || uncertainty_threshold_set || 
                               prediction_error_threshold_set || max_concurrent_tasks_set || task_generation_interval_set ||
                               eliminate_scaffolds_set || autonomy_metrics_set || autonomy_target_set || motivation_decay_set ||
                               exploration_bonus_set || novelty_memory_size_set || enable_selfnode_set || enable_pfc_set || enable_motor_cortex_set);
        
        if (m6_m7_detected && !enable_learning && !hebbian_rate_set && !stdp_rate_set) {
            // Auto-enable basic learning for M6/M7 functionality
            lconf.hebbian_rate = 0.001f; // Small default rate for basic plasticity
            lconf.enable_intrinsic_motivation = true; // Enable for M7 functionality
            enable_learning = true; // Set flag to trigger learning initialization
            hebbian_rate_set = true; // Prevent override
            std::cout << "[M6/M7] Auto-enabling basic learning (hebbian_rate=0.001) for M6/M7 functionality" << std::endl;
        }

        // Start processing
        brain.start();

        // If we are exporting spikes, subscribe to spike events from brain
        if (!spikes_live_path.empty()) {
            brain.setSpikeObserver([&](NeuroForge::NeuronID nid, NeuroForge::TimePoint t){
                std::lock_guard<std::mutex> lg(spikes_mutex);
                spike_events.emplace_back(nid, t);
            });
        }

        // If viewer is enabled, ensure live snapshot path is set and launch viewer process
        // Helper for file existence without <filesystem>
        auto file_exists = [](const std::string& p) -> bool {
            std::ifstream f(p.c_str());
            return f.good();
        };
        if (viewer_enabled) {
            try {
                // If user didn't provide live snapshot path, default to a file in current dir
                if (snapshot_live_path.empty()) {
                    snapshot_live_path = "live_synapses.csv";
                }
                // Seed file with header to allow the viewer to start immediately
                {
                    std::ofstream ofs(snapshot_live_path, std::ios::out | std::ios::trunc);
                    if (ofs) {
                        ofs << "pre_neuron,post_neuron,weight\n";
                        ofs.flush();
                    } else {
                        std::cerr << "Warning: could not create '" << snapshot_live_path << "'. Viewer may not see updates until the path is writable." << std::endl;
                    }
                }
                // If spikes overlay is enabled, seed its CSV header too
                if (!spikes_live_path.empty()) {
                    std::ofstream spofs(spikes_live_path, std::ios::out | std::ios::trunc);
                    if (spofs) {
                        spofs << "neuron_id,t_ms\n";
                        spofs.flush();
                    }
                }
                
                // Compute absolute paths for viewer process (in case its CWD differs)
                std::string snapshot_path_for_viewer = snapshot_live_path;
                std::string spikes_path_for_viewer = spikes_live_path;
                try {
                    snapshot_path_for_viewer = std::filesystem::absolute(snapshot_live_path).string();
                    if (!spikes_live_path.empty()) {
                        spikes_path_for_viewer = std::filesystem::absolute(spikes_live_path).string();
                    }
                } catch (...) {
                    // Fallback to given paths if absolute fails
                }

                // If user didn't provide viewer path, try common locations
                if (viewer_exe_path.empty()) {
                    const char* candidates[] = {
                        "neuroforge_viewer.exe",
                        "build-vcpkg/Release/neuroforge_viewer.exe",
                        "build/Release/neuroforge_viewer.exe",
                        "build-vcpkg/Debug/neuroforge_viewer.exe"
                    };
                    for (const char* p : candidates) {
                        if (file_exists(p)) { viewer_exe_path = p; break; }
                    }
                }
                if (!viewer_exe_path.empty() && file_exists(viewer_exe_path)) {
                    // Validate layout to prevent injection
                    if (viewer_layout != "shells" && viewer_layout != "layers") {
                        viewer_layout = "shells";
                    }

                    // Compose command line with sanitized arguments
                    std::string cmd;
                #ifdef _WIN32
                    // Use CreateProcess directly to avoid cmd.exe injection risks
                    std::string args = " --snapshot-file=" + shell_escape(snapshot_path_for_viewer) +
                                       " --weight-threshold=" + std::to_string(viewer_threshold) +
                                       " --layout=" + shell_escape(viewer_layout) +
                                       " --refresh-ms=" + std::to_string(viewer_refresh_ms);
                    if (!spikes_live_path.empty()) {
                        args += " --spikes-file=" + shell_escape(spikes_path_for_viewer);
                    }
                    // Construct full command line: "exe" args...
                    std::string full_cmd = shell_escape(viewer_exe_path) + args;
                    std::thread([full_cmd]() {
                        STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
                        PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
                        std::vector<char> buf(full_cmd.begin(), full_cmd.end()); buf.push_back(0);
                        if (CreateProcessA(NULL, buf.data(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
                            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
                        } else {
                            std::cerr << "[Security] Failed to launch viewer via CreateProcess. Error: " << GetLastError() << std::endl;
                        }
                    }).detach();
                #else
                    // POSIX: Use fork/execvp to avoid shell injection
                    std::string arg_snapshot = "--snapshot-file=" + snapshot_path_for_viewer;
                    std::string arg_threshold = "--weight-threshold=" + std::to_string(viewer_threshold);
                    std::string arg_layout = "--layout=" + viewer_layout;
                    std::string arg_refresh = "--refresh-ms=" + std::to_string(viewer_refresh_ms);
                    std::string arg_spikes = "--spikes-file=" + spikes_path_for_viewer;

                    std::vector<std::string> args_storage;
                    args_storage.reserve(7);
                    args_storage.push_back(viewer_exe_path);
                    args_storage.push_back(arg_snapshot);
                    args_storage.push_back(arg_threshold);
                    args_storage.push_back(arg_layout);
                    args_storage.push_back(arg_refresh);
                    if (!spikes_live_path.empty()) {
                        args_storage.push_back(arg_spikes);
                    }

                    // Prepare argv for execvp (must be null-terminated array of char*)
                    std::vector<char*> argv_exec;
                    argv_exec.reserve(args_storage.size() + 1);
                    for (auto& s : args_storage) argv_exec.push_back(s.data());
                    argv_exec.push_back(nullptr);

                    // Double-fork pattern to detach the viewer process completely
                    pid_t pid1 = fork();
                    if (pid1 < 0) {
                        std::cerr << "Failed to fork for viewer: " << strerror(errno) << std::endl;
                    } else if (pid1 == 0) {
                        // Child 1
                        pid_t pid2 = fork();
                        if (pid2 < 0) {
                            std::cerr << "Failed to double-fork for viewer: " << strerror(errno) << std::endl;
                            std::exit(1);
                        } else if (pid2 == 0) {
                            // Grandchild (Child 2) - Execute viewer
                            execvp(argv_exec[0], argv_exec.data());
                            // If execvp returns, it failed
                            std::cerr << "Failed to execvp viewer '" << viewer_exe_path << "': " << strerror(errno) << std::endl;
                            std::exit(1);
                        } else {
                            // Child 1 exits immediately
                            std::exit(0);
                        }
                    } else {
                        // Parent waits for Child 1
                        int status;
                        waitpid(pid1, &status, 0);
                    }
                #endif
                    std::cout << "Launched 3D viewer: " << viewer_exe_path << "\n  watching: " << snapshot_path_for_viewer << "\n  layout='" << viewer_layout << "' refresh=" << viewer_refresh_ms << " ms threshold=" << viewer_threshold;
                    if (!spikes_live_path.empty()) std::cout << " spikes=\"" << spikes_path_for_viewer << "\"";
                    std::cout << std::endl;
                } else {
                    std::cerr << "Info: 3D viewer executable not found. Expected at --viewer-exe path or in build directories. You can run it manually with: neuroforge_viewer.exe --snapshot-file=\"" << snapshot_live_path << "\" --layout=" << viewer_layout << " --refresh-ms=" << viewer_refresh_ms << " --weight-threshold=" << viewer_threshold; if (!spikes_live_path.empty()) std::cerr << " --spikes-file=\"" << spikes_live_path << "\""; std::cerr << std::endl;
                }
            } catch (const std::exception& ex) {
                std::cerr << "Warning: failed to set up 3D viewer: " << ex.what() << std::endl;
            }
            // Ensure live snapshots are written frequently enough for smooth viewing
            if (snapshot_interval_ms > viewer_refresh_ms && viewer_refresh_ms > 0) {
                snapshot_interval_ms = viewer_refresh_ms;
            }
        }

        const float delta_time_seconds = (step_ms > 0) ? (static_cast<float>(step_ms) / 1000.0f) : 0.0f;

        // Prepare encoders and optional capture
        NeuroForge::Encoders::VisionEncoder vision_encoder(vcfg);
        NeuroForge::Encoders::AudioEncoder audio_encoder(acfg);
        
        NeuroForge::Audio::AudioCapture::Config capture_config;
        capture_config.sample_rate = acfg.sample_rate;
        capture_config.channels = 1;
        capture_config.bits_per_sample = 16;
        NeuroForge::Audio::AudioCapture mic(capture_config);
        
        NeuroForge::Audio::SystemAudioCapture syscap({static_cast<std::uint32_t>(acfg.sample_rate), 2});
        bool sys_ok = false;
        
        // Create sandbox window if enabled and restrict capture to its client area
        NeuroForge::Sandbox::WebSandbox sandbox_window;
        if (sandbox_enable) {
            if (sandbox_window.create(sandbox_w, sandbox_h, std::string("NeuroForge Sandbox"))) {
                (void)sandbox_window.navigate(sandbox_url);
                (void)sandbox_window.waitUntilReady(5000);
                auto sb = sandbox_window.screenBounds();
                retina_rect_x = sb.x; retina_rect_y = sb.y; retina_rect_w = sb.w; retina_rect_h = sb.h;
                vision_source = "screen"; vision_demo = true;
            } else {
                std::cerr << "Warning: failed to create sandbox window; continuing without sandbox" << std::endl;
            }
        }

        NeuroForge::IO::ScreenCapturer screen({retina_rect_x, retina_rect_y, retina_rect_w, retina_rect_h});
        
        bool mic_ok = false;
        if (audio_demo && audio_mic) {
            std::cerr << "Info: Initializing audio capture at " << acfg.sample_rate << " Hz" << std::endl;
            if (mic.initialize()) {
                mic_ok = mic.startCapture();
                if (mic_ok) {
                    std::cerr << "Info: Microphone capture started" << std::endl;
                } else {
                    std::cerr << "Info: Failed to start microphone capture" << std::endl;
                }
            } else {
                std::cerr << "Info: Microphone capture not available; falling back to synthetic audio" << std::endl;
            }
        }
        if (audio_demo && audio_system) {
            std::cerr << "Info: Starting system loopback audio capture" << std::endl;
            sys_ok = syscap.start();
            if (!sys_ok) {
                std::cerr << "Warning: System audio capture failed; falling back to microphone/synthetic" << std::endl;
            }
        }

        std::vector<float> audio_file_samples;
        std::size_t audio_file_pos = 0;
        if (audio_demo && !audio_mic && !audio_file_path.empty()) {
            if (!std::filesystem::exists(audio_file_path)) {
                std::cerr << "Error: audio file not found: '" << audio_file_path << "'" << std::endl;
            } else {
                std::vector<float> wav_f;
                int wav_sr = 0;
                bool ok = nf_load_wav_any_mono(audio_file_path, wav_f, wav_sr);
                if (!ok) {
                    std::cerr << "Error: failed to load WAV file: '" << audio_file_path << "'" << std::endl;
                } else {
                    if (wav_sr != acfg.sample_rate) {
                        std::cerr << "Info: resampling audio from " << wav_sr << " Hz to " << acfg.sample_rate << " Hz" << std::endl;
                        audio_file_samples = nf_resample_linear(wav_f, wav_sr, acfg.sample_rate);
                    } else {
                        audio_file_samples = std::move(wav_f);
                    }
                    std::cerr << "Info: loaded audio file samples=" << audio_file_samples.size() << std::endl;
                }
            }
        }

        // Set default maze_max_episode_steps if not provided
        if (maze_demo && maze_max_episode_steps < 0) {
            maze_max_episode_steps = 4 * maze_size * maze_size;
        }

        // Maze environment state
        MazeEnv maze_env(maze_size, maze_wall_density, maze_max_episode_steps);
        // Apply shaping configuration
        {
            MazeEnv::ShapingMode mode = MazeEnv::ShapingMode::Off;
            if (maze_shaping == "euclid") mode = MazeEnv::ShapingMode::Euclid;
            else if (maze_shaping == "manhattan") mode = MazeEnv::ShapingMode::Manhattan;
            maze_env.setShaping(mode, maze_shaping_k, maze_shaping_gamma);
        }
        
        // Initialize first-person renderer if requested
        if (maze_first_person) {
            maze_env.initializeFirstPersonRenderer();
            std::cerr << "Info: First-person maze navigation enabled" << std::endl;
        }
        
        bool maze_done = false;
        float maze_last_reward = 0.0f;
        // Q-learning table (state = ay*N + ax, actions=4)
        std::vector<float> qtable;
        if (qlearning) {
            qtable.assign(static_cast<std::size_t>(maze_env.size() * maze_env.size() * 4), 0.0f);
        }
#ifdef NF_HAVE_OPENCV
        cv::VideoCapture cap;
        bool cam_ok = false;
        if (vision_demo && vision_source == "camera") {
            std::cerr << "Info: Opening camera index=" << camera_index << " backend=" << camera_backend << std::endl;
            if (camera_backend == "msmf") {
                cap.open(camera_index, cv::CAP_MSMF);
            } else if (camera_backend == "dshow") {
                cap.open(camera_index, cv::CAP_DSHOW);
            } else {
                cap.open(camera_index);
            }
            if (cap.isOpened()) {
                std::cerr << "Info: Camera opened successfully" << std::endl;
                cam_ok = true;
            } else {
                std::cerr << "Info: Camera not available; falling back to synthetic vision" << std::endl;
            }
        }
#endif

        // Heatmap viewer state
        std::chrono::steady_clock::time_point last_heatmap = std::chrono::steady_clock::now();
        bool heatmap_warned_no_ls = false;
        bool maze_window_created = false;
#ifdef NF_HAVE_OPENCV
        if (heatmap_view) {
            cv::namedWindow("Synapse Heatmap", cv::WINDOW_NORMAL);
            cv::resizeWindow("Synapse Heatmap", 640, 640);
        }
        if (maze_view) {
            cv::namedWindow("Maze", cv::WINDOW_NORMAL);
            cv::resizeWindow("Maze", 480, 480);
            // Verify the window actually exists; getWindowProperty returns -1 when the window does not exist
            maze_window_created = (cv::getWindowProperty("Maze", cv::WND_PROP_VISIBLE) != -1);
            if (!maze_window_created) {
                std::cerr << "Warning: Failed to create OpenCV window 'Maze'. Disabling maze view." << std::endl;
                maze_view = false;
            }
        }
#endif

        // Live snapshot state
        std::chrono::steady_clock::time_point last_snapshot = std::chrono::steady_clock::now();
        bool live_warned_no_ls = false;

        // MemoryDB state
        std::shared_ptr<NeuroForge::Core::MemoryDB> memdb;
        std::int64_t memdb_run_id = 0;
        std::unique_ptr<NeuroForge::Core::SelfModel> self_model;
        std::int64_t current_episode_id = 0;
        NeuroForge::Core::AutonomyEnvelope latest_autonomy_envelope{};
        auto last_memdb_log = std::chrono::steady_clock::now();
        auto last_reward_log = std::chrono::steady_clock::now();
        // Optional RSS monitoring thresholds (configured via environment)
        double rss_warn_threshold_mb = 0.0;
        double rss_fail_threshold_mb = 0.0;
        int rss_warn_interval_ms = 30000;
        auto last_rss_warn = std::chrono::steady_clock::time_point{};
        // Allow environment variable fallback for telemetry path
        if (memory_db_path.empty()) {
            const char* env_telemetry = std::getenv("NF_TELEMETRY_DB");
            if (env_telemetry && *env_telemetry) {
                memory_db_path = env_telemetry;
                std::cerr << "Info: Using NF_TELEMETRY_DB for MemoryDB path ('" << memory_db_path << "')" << std::endl;
            }
        }
        if (!memory_db_path.empty()) {
            memdb = std::make_shared<NeuroForge::Core::MemoryDB>(memory_db_path);
            memdb->setDebug(memdb_debug);
            if (!memdb->open()) {
                std::cerr << "Warning: failed to open memory DB at '" << memory_db_path << "' (SQLite3 may be unavailable)." << std::endl;
                memdb.reset();
            } else {
                std::string meta = std::string("{\"argv_size\":") + std::to_string(argc) + "}";
                if (!memdb->beginRun(meta, memdb_run_id)) {
                    std::cerr << "Warning: failed to begin run in memory DB; logging disabled." << std::endl;
                    memdb.reset();
                } else {
                    std::cerr << "Info: Memory DB logging enabled at '" << memory_db_path << "' (run=" << memdb_run_id << ")" << std::endl;
                    brain.setMemoryDB(memdb, memdb_run_id);
                    try {
                        self_model = std::make_unique<NeuroForge::Core::SelfModel>(*memdb);
                        self_model->loadForRun(memdb_run_id);
                    } catch (...) {
                    }
                    g_memdb = memdb;
                    g_memdb_run_id = memdb_run_id;
#ifdef _WIN32
                    SetConsoleCtrlHandler(NfCtrlHandler, TRUE);
#endif
                    // Emit lifecycle start marker for run_events
                    {
                        std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                        std::int64_t event_id = 0;
                        (void)memdb->insertRunEvent(memdb_run_id, ts_ms, 0ULL, std::string("start"), std::string("main_start"), 0, nf_process_rss_mb(), 0.0, event_id);
                    }
                    // Initialize RSS thresholds from environment
                    if (const char* p = std::getenv("NF_RSS_WARN_MB")) { try { rss_warn_threshold_mb = std::stod(p); } catch (...) {} }
                    if (const char* p = std::getenv("NF_RSS_FAIL_MB")) { try { rss_fail_threshold_mb = std::stod(p); } catch (...) {} }
                    if (const char* p = std::getenv("NF_RSS_WARN_INTERVAL_MS")) { try { rss_warn_interval_ms = std::max(1, std::stoi(p)); } catch (...) {} }
                    // Start a single demo episode spanning the run
                    current_episode_id = brain.startEpisode("demo");
                    // If smoke asserts telemetry presence, seed a reward entry
                    const char* env_assert = std::getenv("NF_ASSERT_ENGINE_DB");
                    if (env_assert && *env_assert && std::string(env_assert) != "0") {
                        try {
                            brain.logReward(0.0, "engine_init", "{\"assert\":\"NF_ASSERT_ENGINE_DB\"}");
                            std::cerr << "Info: Seeded reward_log entry for telemetry assertion." << std::endl;
                            // Also seed a minimal learning_stats entry to satisfy smoke assertions
                            using namespace std::chrono;
                            std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                            NeuroForge::Core::LearningSystem::Statistics st{}; // minimal default stats
                            (void)memdb->insertLearningStats(ts_ms, 0ULL, 0.0f, st, memdb_run_id);
                            std::cerr << "Info: Seeded learning_stats entry for telemetry assertion." << std::endl;
                        } catch (...) {
                            // Swallow any seeding errors
                        }
                    }
                    // Initialize Context Hooks with configured parameters
                    NeuroForge::Core::NF_InitContext(context_gain, context_update_ms, context_window);
                    std::cout << "[Context] Initialized (gain=" << context_gain << ", update_ms=" << context_update_ms << ", window=" << context_window << ")" << std::endl;

                    // Parse and register context peers
                    for (const auto& spec : context_peer_args) {
                        // name[,gain][,update_ms][,window][,label]
                        std::string name;
                        double gain = context_gain;
                        int update_ms = context_update_ms;
                        int window = context_window;
                        std::string label;
                        try {
                            std::vector<std::string> parts; parts.reserve(5);
                            std::size_t start = 0; std::size_t pos = 0;
                            while ((pos = spec.find(',', start)) != std::string::npos) { parts.push_back(spec.substr(start, pos - start)); start = pos + 1; }
                            parts.push_back(spec.substr(start));
                            if (parts.size() >= 1) name = parts[0];
                            if (parts.size() >= 2 && !parts[1].empty()) gain = std::stod(parts[1]);
                            if (parts.size() >= 3 && !parts[2].empty()) update_ms = std::stoi(parts[2]);
                            if (parts.size() >= 4 && !parts[3].empty()) window = std::stoi(parts[3]);
                            if (parts.size() >= 5) label = parts[4];
                            if (name.empty()) { std::cerr << "Warning: --context-peer ignored due to empty name ('" << spec << "')" << std::endl; continue; }
                            NeuroForge::Core::NF_RegisterContextPeer(name, gain, update_ms, window);
                            if (!label.empty()) context_peer_labels[name] = label; // store label for sampling/logging
                            std::cout << "[Context] Peer registered: name='" << name << "' gain=" << gain << " update_ms=" << update_ms << " window=" << window;
                            if (!label.empty()) std::cout << " label='" << label << "'";
                            std::cout << std::endl;
                        } catch (...) {
                            std::cerr << "Warning: failed to parse --context-peer='" << spec << "'" << std::endl;
                        }
                    }
                    // Parse and set context couplings
                    for (const auto& spec : context_coupling_args) {
                        // src:dst[,weight]
                        try {
                            auto colon = spec.find(':');
                            if (colon == std::string::npos) { std::cerr << "Warning: --context-couple requires src:dst[,weight] ('" << spec << "')" << std::endl; continue; }
                            std::string src = spec.substr(0, colon);
                            std::string rest = spec.substr(colon + 1);
                            std::string dst; double w = 1.0;
                            auto comma = rest.find(',');
                            if (comma == std::string::npos) { dst = rest; }
                            else { dst = rest.substr(0, comma); std::string wstr = rest.substr(comma + 1); if (!wstr.empty()) w = std::stod(wstr); }
                            if (src.empty() || dst.empty()) { std::cerr << "Warning: --context-couple ignored due to empty src/dst ('" << spec << "')" << std::endl; continue; }
                            NeuroForge::Core::NF_SetContextCoupling(src, dst, w);
                            std::cout << "[Context] Coupling set: '" << src << "' -> '" << dst << "' (w=" << w << ")" << std::endl;
                        } catch (...) {
                            std::cerr << "Warning: failed to parse --context-couple='" << spec << "'" << std::endl;
                        }
                    }
                }
            }
        }

        // If unified substrate mode requested, run combined loop and exit
        if (unified_substrate_enable) {
            std::cerr << "Info: Running unified substrate mode (WM + Phase C + SurvivalBias + Language)" << std::endl;
            // Collect per-step substrate stats for optional JSON export
            // CohRow holds per-step coherence metrics and the associated MemoryDB run identifier
            struct CohRow { long long ts_ms; int step; double avg_coh; int assemblies; int bindings; double growth_velocity; std::int64_t run_id; };
            std::vector<CohRow> coh_rows;

            // Minimal Adaptive Reflection module: modulates SurvivalBias config based on coherence and growth velocity
            struct AdaptiveReflection {
                double low_thresh = 0.30;      // chaotic/plastic regime threshold
                double high_thresh = 0.80;     // stable regime threshold
                int apply_interval = 500;      // steps between applications
                int last_apply_step = -1;
                int last_assemblies = 0;
                int last_bindings = 0;
                void apply(int step,
                           double avg_coh,
                           int assemblies,
                           int bindings,
                           std::shared_ptr<NeuroForge::Biases::SurvivalBias>& sb,
                           NeuroForge::Core::LearningSystem* ls) {
                    if (last_apply_step >= 0 && (step - last_apply_step) < apply_interval) return;
                    last_apply_step = step;
                    int d_asm = assemblies - last_assemblies;
                    int d_bnd = bindings - last_bindings;
                    last_assemblies = assemblies;
                    last_bindings = bindings;
                    // Coherence low → reduce risk weighting to restore synchrony
                    if (avg_coh < low_thresh) {
                        if (sb) {
                            auto cfg = sb->getConfig();
                            cfg.hazard_coherence_weight = std::max(0.0f, cfg.hazard_coherence_weight * 0.85f);
                            cfg.hazard_alpha = std::max(0.0f, cfg.hazard_alpha * 0.90f);
                            cfg.hazard_beta  = std::max(0.0f, cfg.hazard_beta  * 0.90f);
                            sb->updateConfig(cfg);
                            std::cerr << "[AdaptiveReflection] coh=" << avg_coh
                                      << " ↓ → reduce risk weighting (hazard_weight=" << cfg.hazard_coherence_weight
                                      << ") step=" << step << std::endl;
                        }
                        // Nudge learning rate up (+10%) for faster recovery
                        if (ls) {
                            float base_lr = ls->getLearningRate();
                            float new_lr = base_lr * 1.10f;
                            ls->setLearningRate(new_lr);
                            std::cerr << "[AdaptiveReflection] learning_rate↑=" << new_lr << std::endl;
                        }
                    }
                    // Coherence high & little growth → increase exploration by increasing variance sensitivity slightly
                    else if (avg_coh > high_thresh && (d_asm + d_bnd) <= 0) {
                        if (sb) {
                            auto cfg = sb->getConfig();
                            cfg.variance_sensitivity = std::min(2.0f, cfg.variance_sensitivity * 1.05f);
                            sb->updateConfig(cfg);
                            std::cerr << "[AdaptiveReflection] coh=" << avg_coh
                                      << " ↑ & no growth → increase variance sensitivity (" << cfg.variance_sensitivity
                                      << ") step=" << step << std::endl;
                        }
                        // Nudge learning rate down (-10%) to consolidate
                        if (ls) {
                            float base_lr = ls->getLearningRate();
                            float new_lr = base_lr * 0.90f;
                            ls->setLearningRate(new_lr);
                            std::cerr << "[AdaptiveReflection] learning_rate↓=" << new_lr << std::endl;
                        }
                    }
                    // Mid regime → gently relax towards defaults to avoid drift
                    else {
                        if (sb) {
                            auto cfg = sb->getConfig();
                            cfg.hazard_coherence_weight = std::min(1.0f, cfg.hazard_coherence_weight * 1.02f);
                            cfg.variance_sensitivity    = std::max(1.0f, cfg.variance_sensitivity * 0.98f);
                            sb->updateConfig(cfg);
                        }
                    }
                }
            } adapt;

            // Initialize LearningSystem for unified brain when requested
            if (enable_learning || hebbian_rate_set || stdp_rate_set || stdp_mult_set) {
                if (!consolidation_interval_set) {
                    lconf.update_interval = std::chrono::milliseconds(step_ms > 0 ? step_ms : 0);
                }
                lconf.prefer_gpu = prefer_gpu;
                (void)brain.initializeLearning(lconf);
                brain.setLearningEnabled(true);
            }
            // Initialize Substrate Working Memory
            NeuroForge::Core::SubstrateWorkingMemory::Config wm_cfg{};
            wm_cfg.working_memory_regions = 4;
            wm_cfg.neurons_per_region = (unified_wm_neurons > 0 ? unified_wm_neurons : 64);
            // Wrap existing brain in a non-owning shared_ptr for APIs that expect shared_ptr
            auto brain_shared = std::shared_ptr<NeuroForge::Core::HypergraphBrain>(&brain, [](NeuroForge::Core::HypergraphBrain*){});
            auto wm = std::make_shared<NeuroForge::Core::SubstrateWorkingMemory>(brain_shared, wm_cfg);
            if (!wm->initialize()) {
                std::cerr << "ERROR: SubstrateWorkingMemory initialize failed" << std::endl;
                return 3;
            }

            // Initialize Substrate Phase C
            NeuroForge::Core::SubstratePhaseC::Config pc_cfg{};
            pc_cfg.binding_regions = 4;
            pc_cfg.sequence_regions = 3;
            pc_cfg.neurons_per_region = (unified_phasec_neurons > 0 ? unified_phasec_neurons : 64);
            if (phase_c_binding_threshold_set) pc_cfg.binding_threshold = phase_c_binding_threshold;
            if (phase_c_sequence_threshold_set) pc_cfg.sequence_threshold = phase_c_sequence_threshold;
            if (phase_c_binding_coherence_min_set) pc_cfg.binding_coherence_min = phase_c_binding_coherence_min;
            if (phase_c_sequence_coherence_min_set) pc_cfg.sequence_coherence_min = phase_c_sequence_coherence_min;
            if (phase_c_prune_coherence_threshold_set) pc_cfg.prune_coherence_threshold = phase_c_prune_coherence_threshold;
            auto phaseC = std::make_unique<NeuroForge::Core::SubstratePhaseC>(brain_shared, wm, pc_cfg);
            if (!phaseC->initialize()) {
                std::cerr << "ERROR: SubstratePhaseC initialize failed" << std::endl;
                return 4;
            }
            std::shared_ptr<NeuroForge::Biases::SurvivalBias> survival_bias;
            if (survival_bias_enable) {
                survival_bias = std::make_shared<NeuroForge::Biases::SurvivalBias>();
                phaseC->setSurvivalBias(survival_bias);
                phaseC->setEmitSurvivalRewards(true);
                phaseC->setSurvivalRewardScale(1.0f);
            } else {
                phaseC->setSurvivalBias(nullptr);
            }

            // Language substrate integration
            NeuroForge::Core::LanguageSystem::Config ls_cfg{};
            auto language_system_local = std::make_shared<NeuroForge::Core::LanguageSystem>(ls_cfg);
            NeuroForge::Core::SubstrateLanguageIntegration::Config lang_cfg{};
            auto lang = std::make_shared<NeuroForge::Core::SubstrateLanguageIntegration>(language_system_local, brain_shared, lang_cfg);
            if (!lang->initialize()) {
                std::cerr << "ERROR: SubstrateLanguageIntegration initialize failed" << std::endl;
                return 5;
            }
            int adaptive_low_events = 0;
            int adaptive_high_events = 0;

            // Unified loop
            const int unified_steps = (steps > 0 ? steps : 200);
            const float dt = (step_ms > 0 ? (step_ms / 1000.0f) : 0.01f);
            // Obtain LearningSystem handle for adaptive modulation
            NeuroForge::Core::LearningSystem* learning_system = nullptr;
            try {
                learning_system = brain_shared->getLearningSystem();
            } catch (...) { learning_system = nullptr; }

            for (int s = 0; s < unified_steps; ++s) {
                if (survival_bias) {
                    if (hazard_density > 0.0f) {
                        survival_bias->setExternalHazard(hazard_density);
                    } else if (audio_demo && !last_audio_features.empty()) {
                        double sumsq = 0.0;
                        for (float f : last_audio_features) { sumsq += static_cast<double>(f) * static_cast<double>(f); }
                        float rms = 0.0f;
                        if (!last_audio_features.empty()) {
                            rms = static_cast<float>(std::sqrt(sumsq / static_cast<double>(last_audio_features.size())));
                        }
                        if (rms < 0.0f) rms = 0.0f; if (rms > 1.0f) rms = 1.0f;
                        survival_bias->setExternalHazard(rms);
                    }
                }
                brain.processStep(dt);
                // Update global last step for accurate lifecycle logging in unified substrate mode
                g_last_step.store(static_cast<std::uint64_t>(s));
                phaseC->processStep(s, dt);
                lang->processSubstrateLanguageStep(dt);

                {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_memdb_log).count();
                    if (elapsed_ms >= memdb_interval_ms) {
                        int steps_since = static_cast<int>(elapsed_ms / (step_ms > 0 ? step_ms : 10));
                        if (steps_since <= 0) { steps_since = 1; }
                        float hz = (elapsed_ms > 0) ? (1000.0f * static_cast<float>(steps_since) / static_cast<float>(elapsed_ms)) : 0.0f;
                        if (memdb && memdb_run_id != 0) {
                            auto st_opt = brain.getLearningStatistics();
                            if (st_opt.has_value()) {
                                auto ts_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                                memdb->insertLearningStats(ts_ms, static_cast<std::uint64_t>(s), hz, st_opt.value(), memdb_run_id);
                                std::int64_t event_id = 0;
                                (void)memdb->insertRunEvent(memdb_run_id, static_cast<std::int64_t>(ts_ms), static_cast<std::uint64_t>(s), std::string("heartbeat"), std::string(), 0, nf_process_rss_mb(), 0.0, event_id);
                                // Optional memory pressure notifications (RSS)
                                double rss_mb = nf_process_rss_mb();
                                if (rss_warn_threshold_mb > 0.0 && rss_mb >= rss_warn_threshold_mb) {
                                    auto now_warn = std::chrono::steady_clock::now();
                                    if (last_rss_warn.time_since_epoch().count() == 0 ||
                                        std::chrono::duration_cast<std::chrono::milliseconds>(now_warn - last_rss_warn).count() >= rss_warn_interval_ms) {
                                        std::int64_t warn_id = 0;
                                        (void)memdb->insertRunEvent(memdb_run_id, static_cast<std::int64_t>(ts_ms), static_cast<std::uint64_t>(s), std::string("warning"), std::string("rss_threshold_exceeded"), 0, rss_mb, 0.0, warn_id);
                                        last_rss_warn = now_warn;
                                    }
                                }
                                if (rss_fail_threshold_mb > 0.0 && rss_mb >= rss_fail_threshold_mb) {
                                    std::int64_t err_id = 0;
                                    (void)memdb->insertRunEvent(memdb_run_id, static_cast<std::int64_t>(ts_ms), static_cast<std::uint64_t>(s), std::string("error"), std::string("rss_fail_threshold_exceeded"), 0, rss_mb, 0.0, err_id);
                                }
                            }
                        }
                        last_memdb_log = now;
                    }
                }

                // Per-step capture for export
                {
                    auto stats = phaseC->getStatistics();
                    long long ts_ms = static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                    int prevAsm_capture = (coh_rows.size() >= 1) ? coh_rows.back().assemblies : static_cast<int>(stats.assemblies_formed);
                    int prevBnd_capture = (coh_rows.size() >= 1) ? coh_rows.back().bindings : static_cast<int>(stats.bindings_created);
                    double growth = static_cast<double>((static_cast<int>(stats.assemblies_formed) - prevAsm_capture) + (static_cast<int>(stats.bindings_created) - prevBnd_capture));
                    // Capture current metrics into row with 64-bit run_id to avoid narrowing
                    CohRow row{ ts_ms, s, static_cast<double>(stats.average_coherence), static_cast<int>(stats.assemblies_formed), static_cast<int>(stats.bindings_created), growth, memdb_run_id };
                    coh_rows.push_back(row);
                    // Apply minimal adaptive reflection in real time
                    if (adaptive_enable) {
                        // Capture deltas for simple velocity
                        int prevAsm_reflect = (coh_rows.size() >= 2) ? coh_rows[coh_rows.size()-2].assemblies : row.assemblies;
                        int prevBnd_reflect = (coh_rows.size() >= 2) ? coh_rows[coh_rows.size()-2].bindings : row.bindings;
                        int dAsm = row.assemblies - prevAsm_reflect;
                        int dBnd = row.bindings - prevBnd_reflect;
                        // Increment counters based on cadence
                        if ((s - adapt.last_apply_step) >= adapt.apply_interval) {
                            if (row.avg_coh < adapt.low_thresh) adaptive_low_events++;
                            else if (row.avg_coh > adapt.high_thresh && (dAsm + dBnd) <= 0) adaptive_high_events++;
                        }
                        // Apply reflection (may adjust SurvivalBias config and LR)
                        adapt.apply(s, row.avg_coh, row.assemblies, row.bindings, survival_bias, learning_system);
                    }
                }

                // Periodic metrics summary (every 250 steps)
                if (((s + 1) % 250) == 0) {
                    auto pc_stats = phaseC->getStatistics();
                    auto l_stats = lang->getStatistics();
                    auto ls_opt = brain.getLearningStatistics();
                    NeuroForge::Core::LearningSystem::Statistics ls{};
                    if (ls_opt.has_value()) ls = ls_opt.value();
                    auto assemblies = phaseC->getCurrentAssemblies();
                    std::vector<std::size_t> asm_sizes; asm_sizes.reserve(assemblies.size());
                    for (const auto& a : assemblies) asm_sizes.push_back(a.neurons.size());
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
                              << " energy=" << ls.avg_energy
                              << " metabolic_hazard=" << ls.metabolic_hazard
                              << std::endl;
                }

                if (((s + 1) % 500) == 0) {
                    auto assemblies = phaseC->getCurrentAssemblies();
                    if (!assemblies.empty()) {
                        const auto* best = &assemblies[0];
                        for (const auto& a : assemblies) {
                            if (a.coherence_score > best->coherence_score) best = &a;
                        }
                        if (best->coherence_score > 0.84f) {
                            double trust = 0.0;
                            if (phase9_metacog) trust = phase9_metacog->getSelfTrust();
                            std::cout << "[MIND:" << 0
                                      << " C=" << std::fixed << std::setprecision(4) << best->coherence_score
                                      << " N=" << best->neurons.size()
                                      << " T=" << (s + 1)
                                      << " Trust=" << std::fixed << std::setprecision(4) << trust
                                      << "]" << std::endl;
                        }
                    }
                }
                #ifdef _WIN32
                {
                    MSG msg;
                    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                        if (msg.message == WM_QUIT) {
                            g_abort.store(true);
                            break;
                        }
                        TranslateMessage(&msg);
                        DispatchMessageW(&msg);
                    }
                }
                #endif
            }

            auto stC = phaseC->getStatistics();
            auto stL = lang->getStatistics();
            std::cout << "=== Unified Substrate (main) Summary ===\n";
            std::cout << "Phase C: assemblies=" << stC.assemblies_formed
                      << " bindings=" << stC.bindings_created
                      << " sequences=" << stC.sequences_predicted
                      << " goals=" << stC.goals_achieved
                      << " avg_coherence=" << stC.average_coherence << "\n";
            std::cout << "Language: substrate_language_coherence=" << stL.substrate_language_coherence
                      << " binding_strength_avg=" << stL.average_binding_strength
                      << " integration_efficiency=" << stL.integration_efficiency << "\n";
            std::cout << "AdaptiveReflection: low_events=" << adaptive_low_events
                      << " high_events=" << adaptive_high_events << "\n";
            // Auto-export substrate_states JSON for dashboard coherence pane
            try {
                namespace fs = std::filesystem;
                fs::path out = fs::current_path();
                // exe runs from build dir; write to repo_root/web/substrate_states.json
                if (out.has_parent_path()) out = out.parent_path();
                out /= "web";
                out /= "substrate_states.json";
                std::ofstream ofs(out.string(), std::ios::out | std::ios::trunc);
                ofs << "{\n  \"series\": [\n";
                for (std::size_t i = 0; i < coh_rows.size(); ++i) {
                    const auto& r = coh_rows[i];
                    ofs << "    { \"ts_ms\": " << r.ts_ms
                        << ", \"step\": " << r.step
                        << ", \"avg_coherence\": " << std::fixed << std::setprecision(6) << r.avg_coh
                        << ", \"assemblies\": " << r.assemblies
                        << ", \"bindings\": " << r.bindings
                        << ", \"growth_velocity\": " << r.growth_velocity
                        << ", \"run_id\": " << r.run_id
                        << " }" << (i + 1 < coh_rows.size() ? ",\n" : "\n");
                }
                ofs << "  ]\n}";
                ofs.close();
                std::cerr << "[Export] substrate_states written to " << out.string() << std::endl;
            } catch (...) {
                std::cerr << "[Export] Warning: failed to write substrate_states.json (web)" << std::endl;
            }
            return 0;
        }


        // Initialize Phase 6 Reasoner after MemoryDB is ready
        if (phase6_enable && memdb && memdb_run_id > 0) {
            try {
                phase6_reasoner = std::make_unique<NeuroForge::Core::Phase6Reasoner>(memdb.get(), memdb_run_id);
                if (self_model) {
                    phase6_reasoner->setSelfModel(self_model.get());
                }
                    if (phase6_active_mode == "audit") {
                        std::cout << "[Phase 6] Reasoner enabled (audit)" << std::endl;
                    } else if (phase6_active_mode == "on") {
                        std::cout << "[Phase 6] Reasoner enabled (active, margin=" << phase6_margin << ")" << std::endl;
                    } else {
                        std::cout << "[Phase 6] Reasoner enabled (shadow logging)" << std::endl;
                    }
            } catch (...) {
                std::cerr << "Warning: Failed to initialize Phase 6 Reasoner" << std::endl;
            }
        }

        // Initialize Phase 7 Affective State and Reflection after MemoryDB is ready
        bool init_affect = phase7_enable || phase7_affect_enable;
        bool init_reflect = phase7_enable || phase7_reflect_enable;
        
        if ((init_affect || init_reflect) && memdb && memdb_run_id > 0) {
            try {
                if (init_affect) {
                    phase7_affect = std::make_unique<NeuroForge::Core::Phase7AffectiveState>(memdb.get(), memdb_run_id);
                    std::cout << "[Phase 7] Affective State initialized" << std::endl;
                }
                if (init_reflect) {
                    phase7_reflect = std::make_unique<NeuroForge::Core::Phase7Reflection>(memdb.get(), memdb_run_id);
                    std::cout << "[Phase 7] Reflection initialized" << std::endl;
                    if (self_model) {
                        phase7_reflect->setSelfModel(self_model.get());
                    }
                }
                if (phase6_reasoner && (phase7_affect || phase7_reflect)) {
                    phase6_reasoner->setPhase7Components(phase7_affect.get(), phase7_reflect.get());
                    std::cout << "[Phase 7] Bridged to Phase 6 Reasoner" << std::endl;
                }
            } catch (...) {
                std::cerr << "Warning: Failed to initialize Phase 7 components" << std::endl;
            }
        }

        // Initialize Phase 8 Goal System after MemoryDB is ready
        if (phase8_enable && memdb && memdb_run_id > 0) {
            try {
                phase8_goals = std::make_unique<NeuroForge::Core::Phase8GoalSystem>(memdb, memdb_run_id);
                std::cout << "[Phase 8] Goal System initialized" << std::endl;
                if (self_model) {
                    phase8_goals->setSelfModel(self_model.get());
                }
                if (phase6_reasoner && phase8_goals) {
                    phase6_reasoner->setPhase8Components(phase8_goals.get());
                    std::cout << "[Phase 8] Bridged to Phase 6 Reasoner" << std::endl;
                }
                if (phase7_reflect && phase8_goals) {
                    phase7_reflect->setPhase8Components(phase8_goals.get());
                    std::cout << "[Phase 8] Bridged to Phase 7 Reflection" << std::endl;
                }
                // Initialize Phase 9 Metacognition and wire into phases
                if (phase9_enable) {
                    phase9_metacog = std::make_unique<NeuroForge::Core::Phase9Metacognition>(memdb.get(), memdb_run_id);
                std::cout << "[Phase 9] Metacognition initialized" << std::endl;
                std::cout << "[Phase 9] Metacognition active (modulation=" << (phase9_modulation_enable ? "on" : "off") << ")" << std::endl;
                if (phase7_reflect && phase9_metacog) {
                    phase7_reflect->setPhase9Metacognition(phase9_metacog.get());
                    std::cout << "[Phase 9] Bridged to Phase 7 Reflection" << std::endl;
                }
                if (phase8_goals && phase9_metacog) {
                    phase8_goals->setPhase9Metacognition(phase9_metacog.get());
                    std::cout << "[Phase 9] Bridged to Phase 8 Goal System" << std::endl;
                }
                if (phase6_reasoner && phase9_metacog && phase9_modulation_enable) {
                    phase6_reasoner->setPhase9Metacognition(phase9_metacog.get());
                    std::cout << "[Phase 9] Modulation bridged to Phase 6 Reasoner" << std::endl;
                }
                // Initialize Phase 10 Self-Explanation and inject into Phase 9
                if (phase10_enable && phase9_metacog) {
                    phase10_selfexplainer = std::make_unique<NeuroForge::Core::Phase10SelfExplanation>(memdb.get(), memdb_run_id);
                    phase9_metacog->setPhase10SelfExplanation(phase10_selfexplainer.get());
                    std::cout << "[Phase 10] Self-Explanation initialized and injected into Phase 9" << std::endl;
                }
                // Initialize Phase 11 Self-Revision and inject into Phase 9
                if (phase11_enable && phase9_metacog) {
                    phase11_revision = std::make_unique<NeuroForge::Core::Phase11SelfRevision>(memdb.get(), memdb_run_id);
                    phase11_revision->setTrustDriftThreshold(phase11_revision_threshold);
                    phase11_revision->setRevisionInterval(phase11_revision_interval_ms);
                    phase11_revision->setMinRevisionGap(phase11_min_gap_ms);
                    phase11_revision->setOutcomeEvalWindowMs(phase11_outcome_eval_window_ms);
                    phase11_revision->setRevisionMode(phase11_revision_mode);
                    phase9_metacog->setPhase11SelfRevision(phase11_revision.get());
                    std::cout << "[Phase 11] Self-Revision active (interval=" << phase11_revision_interval_ms << " ms)" << std::endl;
                }
                // Initialize Phase 12 Consistency and inject into Phase 9
                if (phase12_enable && phase9_metacog) {
                    phase12_consistency = std::make_unique<NeuroForge::Core::Phase12Consistency>(memdb.get(), memdb_run_id);
                    phase12_consistency->setAnalysisWindow(phase12_window);
                    phase9_metacog->setPhase12Consistency(phase12_consistency.get());
                    std::cout << "[Phase 12] Consistency initialized and injected into Phase 9 (window=" << phase12_window << ")" << std::endl;
                }
                // Initialize Phase 13 Autonomy Envelope and inject into Phase 9
                if (phase13_enable && phase9_metacog) {
                    NeuroForge::Core::Phase13AutonomyEnvelope::Config p13cfg;
                    p13cfg.trust_tighten_threshold = phase13_trust_tighten;
                    p13cfg.trust_expand_threshold = phase13_trust_expand;
                    p13cfg.consistency_tighten_threshold = phase13_consistency_tighten;
                    p13cfg.consistency_expand_threshold = phase13_consistency_expand;
                    p13cfg.contraction_hysteresis_ms = phase13_contraction_hysteresis_ms;
                    p13cfg.expansion_hysteresis_ms = phase13_expansion_hysteresis_ms;
                    p13cfg.min_log_interval_ms = phase13_min_log_interval_ms;
                    p13cfg.analysis_window = phase13_window;
                    phase13_autonomy = std::make_unique<NeuroForge::Core::Phase13AutonomyEnvelope>(memdb.get(), memdb_run_id, p13cfg);
                    phase9_metacog->setPhase13AutonomyEnvelope(phase13_autonomy.get());
                    std::cout << "[Phase 13] Autonomy Envelope initialized and injected into Phase 9 (window=" << phase13_window << ")" << std::endl;
                }
                // Initialize Phase 14 Meta-Reasoner and inject into Phase 9
                if (phase14_enable && phase9_metacog) {
                    NeuroForge::Core::Phase14MetaReasoner::Config p14cfg;
                    p14cfg.window = phase14_window;
                    p14cfg.trust_degraded_threshold = phase14_trust_degraded;
                    p14cfg.rmse_degraded_threshold = phase14_rmse_degraded;
                    phase14_metareason = std::make_unique<NeuroForge::Core::Phase14MetaReasoner>(memdb.get(), memdb_run_id, p14cfg);
                    phase9_metacog->setPhase14MetaReasoner(phase14_metareason.get());
                    std::cout << "[Phase 14] Meta-Reasoner initialized and injected into Phase 9 (window=" << phase14_window << ")" << std::endl;
                }

                // Initialize Phase 15 Ethics Regulator and inject into Phase 9
                if (phase15_enable && phase9_metacog) {
                    NeuroForge::Core::Phase15EthicsRegulator::Config p15cfg;
                    p15cfg.window = phase15_window;
                    p15cfg.risk_threshold = phase15_risk_threshold;
                    phase15_ethics = std::make_unique<NeuroForge::Core::Phase15EthicsRegulator>(memdb.get(), memdb_run_id, p15cfg);
                    phase9_metacog->setPhase15EthicsRegulator(phase15_ethics.get());
                    std::cout << "[Phase 15] Ethics Regulator initialized and injected into Phase 9 (window=" << phase15_window << ", risk_threshold=" << phase15_risk_threshold << ")" << std::endl;
                }
                }
            } catch (...) {
                std::cerr << "Warning: Failed to initialize Phase 8/9 systems" << std::endl;
            }
        }

        if (dataset_mode == "triplets") {
            if (mirror_mode == "off") mirror_mode = "vision";
            mimicry_enable = true;
        }
        // Initialize LanguageSystem / Phase A after MemoryDB is ready
        if (phase5_language_enable || phase_a_enable) {
            NeuroForge::Core::LanguageSystem::Config lang_config; // defaults
            language_system = std::make_shared<NeuroForge::Core::LanguageSystem>(lang_config);
            if (!language_system->initialize()) {
                std::cerr << "Warning: LanguageSystem failed to initialize" << std::endl;
            } else {
                std::cout << "LanguageSystem initialized" << std::endl;
            }
        }
        if (phase_a_enable) {
            NeuroForge::Core::PhaseAMimicry::Config phase_a_config = NeuroForge::Core::PhaseAMimicryFactory::createDefaultConfig();
            phase_a_config.negative_sampling_k = 5;
            phase_a_config.negative_weight = 0.2f;
            if (phase_a_negative_k_set) { phase_a_config.negative_sampling_k = phase_a_negative_k; }
            if (phase_a_negative_weight_set) { phase_a_config.negative_weight = phase_a_negative_weight; }
            // Apply CLI overrides for Phase A thresholds when provided
            if (phase_a_similarity_threshold_set) { phase_a_config.similarity_threshold = phase_a_similarity_threshold; }
            if (phase_a_novelty_threshold_set) { phase_a_config.novelty_threshold = phase_a_novelty_threshold; }
            if (phase_a_student_lr_set) { phase_a_config.student_learning_rate = static_cast<float>(phase_a_student_lr); }
            // Apply EMA stabilizer CLI overrides when provided
            if (phase_a_ema_enable_set) { phase_a_config.enable_ema_stabilizer = phase_a_ema_enable; }
            if (phase_a_ema_min_set) { phase_a_config.ema_alpha_min = static_cast<float>(phase_a_ema_min); }
            if (phase_a_ema_max_set) { phase_a_config.ema_alpha_max = static_cast<float>(phase_a_ema_max); }
            // Sanity: ensure min <= max; swap if user provided inverted bounds
            if (phase_a_config.ema_alpha_min > phase_a_config.ema_alpha_max) {
                std::swap(phase_a_config.ema_alpha_min, phase_a_config.ema_alpha_max);
            }
            if (phase_a_replay_interval_set) { phase_a_config.replay_interval_steps = static_cast<std::size_t>(phase_a_replay_interval_steps); }
            if (phase_a_replay_top_k_set) { phase_a_config.replay_top_k = static_cast<std::size_t>(phase_a_replay_top_k); }
            if (phase_a_replay_boost_set) { phase_a_config.replay_boost_factor = static_cast<float>(phase_a_replay_boost); }
            if (phase_a_replay_lr_scale_set) { phase_a_config.replay_lr_scale = static_cast<float>(phase_a_replay_lr_scale); }
            if (phase_a_replay_include_hard_set) { phase_a_config.replay_include_hard_negatives = phase_a_replay_include_hard; }
            if (phase_a_replay_hard_k_set) { phase_a_config.replay_hard_k = static_cast<std::size_t>(phase_a_replay_hard_k); }
            if (phase_a_replay_repulsion_weight_set) { phase_a_config.replay_repulsion_weight = static_cast<float>(phase_a_replay_repulsion_weight); }
            // Auto-derive embedding dimension before creating Phase A system
            int desired_dim = 0;
            int teacher_len = 0;
            int mirror_implied_dim = 0;
            std::string derived_source = "default config";
            bool teacher_dim_available = false;
            if (!teacher_embed_path.empty()) {
                std::vector<float> tmp_teacher;
                std::ifstream ifs(teacher_embed_path);
                if (ifs) {
                    std::string line;
                    while (std::getline(ifs, line)) {
                        for (char &ch : line) {
                            if (!((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E' || ch == ' ')) {
                                ch = ' ';
                            }
                        }
                        std::istringstream iss(line);
                        float f = 0.0f;
                        while (iss >> f) { tmp_teacher.push_back(f); }
                    }
                    if (!tmp_teacher.empty()) {
                        teacher_len = static_cast<int>(tmp_teacher.size());
                        desired_dim = teacher_len;
                        derived_source = "teacher vector length";
                        teacher_dim_available = true;
                    }
                }
            }
            // Compute mirror-mode implied dimension regardless, for conflict detection and fallback
            if (mirror_mode == "vision") {
                mirror_implied_dim = vcfg.grid_size * vcfg.grid_size;
            } else if (mirror_mode == "audio") {
                mirror_implied_dim = acfg.feature_bins;
            }
            if (desired_dim <= 0) {
                if (mirror_mode == "vision") {
                    desired_dim = mirror_implied_dim;
                    derived_source = std::string("vision grid (") + std::to_string(vcfg.grid_size) + "^2)";
                } else if (mirror_mode == "audio") {
                    desired_dim = mirror_implied_dim;
                    derived_source = "audio feature bins";
                }
            }
            // Conflict detection: if both teacher and mirror-mode are provided and disagree, warn that teacher wins
            if (teacher_dim_available && mirror_implied_dim > 0 && teacher_len != mirror_implied_dim) {
                std::cerr << "[Phase A][Warning] Teacher embedding length ("
                          << teacher_len
                          << ") differs from mirror-mode implied dimension ("
                          << mirror_implied_dim
                          << "). Using teacher dimension.\n";
                // JSON event for conflict
                {
                    std::ostringstream js;
                    js << "{\"version\":1,\"phase\":\"A\",\"event\":\"conflict\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                    js << "\"conflict\":true,\"payload\":{";
                    js << "\"teacher_len\":" << teacher_len << ",";
                    js << "\"mirror_mode\":\"" << json_escape(mirror_mode) << "\",";
                    js << "\"mirror_implied_dim\":" << mirror_implied_dim << ",";
                    js << "\"resolution\":\"teacher_wins\",";
                    js << "\"argv\":[";
                    for (int ai = 0; ai < argc; ++ai) { if (ai) js << ","; js << "\"" << json_escape(argv[ai]) << "\""; }
                    js << "]}}";
                    emit_json_line(log_json, log_json_path, js.str());
                }
                // Back-compat event for tests: phase_a_embed_conflict
                {
                    std::ostringstream js2;
                    js2 << "{\"version\":1,\"phase\":\"A\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                    js2 << "\"t\":\"phase_a_embed_conflict\",\"teacher_len\":" << teacher_len
                        << ",\"mirror_mode\":\"" << json_escape(mirror_mode) << "\""
                        << ",\"mirror_implied_dim\":" << mirror_implied_dim << "}";
                    emit_json_line(log_json, log_json_path, js2.str());
                }
            }
            if (desired_dim > 0) {
                phase_a_config.embedding_dimension = desired_dim;
            }
            const int final_dim = (desired_dim > 0) ? desired_dim : phase_a_config.embedding_dimension;
            std::cout << "[Phase A] Embedding dimension set to "
                      << final_dim
                      << " (derived from " << derived_source << ")\n";
            // JSON event for decision
            {
                std::ostringstream js;

                js << "{\"version\":1,\"phase\":\"A\",\"event\":\"decision\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                js << "\"payload\":{";
                js << "\"teacher_len\":" << teacher_len << ",";
                js << "\"mirror_mode\":\"" << json_escape(mirror_mode) << "\",";
                js << "\"mirror_implied_dim\":" << mirror_implied_dim << ",";
                js << "\"decided_dim\":" << final_dim << ",";
                js << "\"source\":\"" << json_escape(derived_source) << "\",";
                js << "\"argv\":[";
                for (int ai = 0; ai < argc; ++ai) { if (ai) js << ","; js << "\"" << json_escape(argv[ai]) << "\""; }
                js << "]}";
                js << "}";
                emit_json_line(log_json, log_json_path, js.str());
            }
            // Back-compat event for tests: phase_a_embed_decided
            {
                std::ostringstream js2;
                js2 << "{\"version\":1,\"phase\":\"A\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                js2 << "\"t\":\"phase_a_embed_decided\",\"decided_dim\":" << final_dim
                    << ",\"source\":\"" << json_escape(derived_source) << "\"}";
                emit_json_line(log_json, log_json_path, js2.str());
            }
            phase_a_system = NeuroForge::Core::PhaseAMimicryFactory::create(language_system, memdb, phase_a_config);
            if (!phase_a_system || !phase_a_system->initialize()) {
                std::cerr << "Warning: Phase A Mimicry failed to initialize" << std::endl;
            } else {
                std::cout << "Phase A Mimicry initialized" << std::endl;
                // Wire Phase A to the unified brain and runtime controls
                phase_a_system->setBrain(&brain);
                NeuroForge::Core::PhaseAMimicry::SubstrateMode p_mode = NeuroForge::Core::PhaseAMimicry::SubstrateMode::Off;
                if (substrate_mode == "mirror") p_mode = NeuroForge::Core::PhaseAMimicry::SubstrateMode::Mirror;
                else if (substrate_mode == "train") p_mode = NeuroForge::Core::PhaseAMimicry::SubstrateMode::Train;
                else if (substrate_mode == "native") p_mode = NeuroForge::Core::PhaseAMimicry::SubstrateMode::Native;
                phase_a_system->setSubstrateMode(p_mode);
                phase_a_system->setRewardScale(static_cast<float>(reward_scale));
                // Ensure teacher embedding is registered now that Phase A is initialized
                if (!teacher_embed_path.empty()) {
                    std::vector<float> tvec2;
                    std::ifstream ifs(teacher_embed_path);
                    if (!ifs) {
                        std::cerr << "Warning: failed to load teacher embedding for Phase A/LanguageSystem from '" << teacher_embed_path << "'" << std::endl;
                    } else {
                        tvec2.clear();
                        std::string line;
                        while (std::getline(ifs, line)) {
                            for (char &ch : line) {
                                if (!((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E' || ch == ' ')) {
                                    ch = ' ';
                                }
                            }
                            std::istringstream iss(line);
                            float f = 0.0f;
                            while (iss >> f) { tvec2.push_back(f); }
                        }
                        if (!tvec2.empty()) {
                            if (phase5_language_enable && language_system) {
                                language_system->setTeacherEmbedding(current_teacher_id, tvec2);
                                language_system->processTeacherSignal(current_teacher_id, 1.0f);
                            }
                            if (phase_a_system) {
                                phase_a_system->addTeacherEmbedding(
                                    tvec2,
                                    NeuroForge::Core::PhaseAMimicry::TeacherType::Custom,
                                    NeuroForge::Core::PhaseAMimicry::Modality::Multimodal,
                                    current_teacher_id,
                                    teacher_embed_path,
                                    1.0f
                                );
                            }
                        } else {
                            std::cerr << "Warning: empty teacher embedding from '" << teacher_embed_path << "'" << std::endl;
                        }
                    }
                }
            }
        }
        if (phase5_language_enable || phase_a_enable) {
            self_node = std::make_shared<NeuroForge::Regions::SelfNode>("SelfNode");
            std::cout << "SelfNode initialized" << std::endl;
        }

        if (dataset_mode == "triplets" && !dataset_triplets_root.empty()) {
            auto items = scan_triplets_dataset(dataset_triplets_root, dataset_limit, dataset_shuffle);
            if (!items.empty()) {
                triplet_items.swap(items);
                dataset_index = 0;
                dataset_active = true;
                vision_demo = true;
                audio_demo = true;
                cross_modal = true;
                vision_source = "dataset";
                const auto &it = triplet_items[dataset_index];
                current_image_path = it.image_path;
                current_audio_path = it.audio_path;
                current_caption = it.text;
                if (memdb && memdb_run_id > 0) {
                    try {
                        using namespace std::chrono;
                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                        std::ostringstream input;
                        input.setf(std::ios::fixed);
                        input << "{"
                              << "\"image\":\"" << json_escape(current_image_path) << "\","
                              << "\"audio\":\"" << json_escape(current_audio_path) << "\","
                              << "\"caption\":\"" << json_escape(current_caption) << "\",";
                        {
                            std::istringstream iss(current_caption);
                            std::string tok; bool first_tok = true;
                            input << "\"tokens\":[";
                            while (iss >> tok) {
                                if (!first_tok) input << ",";
                                input << "\"" << json_escape(tok) << "\"";
                                first_tok = false;
                            }
                            input << "],";
                        }
                        input << "\"teacher_id\":\"" << json_escape(std::string("triplet_") + it.stem) << "\"";
                        input << "}";
                        std::int64_t exp_id = -1;
                        auto gs0 = brain.getGlobalStatistics();
                        std::uint64_t step_pc0 = static_cast<std::uint64_t>(gs0.processing_cycles);
                        bool ok_exp = memdb->insertExperience(ts_ms,
                                                             step_pc0,
                                                             "triplet_ingestion",
                                                             input.str(),
                                                             std::string(),
                                                             true,
                                                             memdb_run_id,
                                                             exp_id);
                        std::cout << "Triplet ingestion (initial): run=" << memdb_run_id
                                  << " step=" << step_pc0
                                  << " exp_id=" << exp_id
                                  << " ok=" << (ok_exp ? 1 : 0) << std::endl;
                        if (current_episode_id > 0 && exp_id > 0) {
                            (void)memdb->linkExperienceToEpisode(exp_id, current_episode_id);
                        }
                    } catch (...) { }
                }
                if (!current_audio_path.empty()) {
                    std::vector<float> loaded;
                    int sr = 0;
                    if (nf_load_wav_any_mono(current_audio_path, loaded, sr)) {
                        if (sr != acfg.sample_rate && !loaded.empty()) {
                            audio_file_samples = nf_resample_linear(loaded, sr, acfg.sample_rate);
                        } else {
                            audio_file_samples.swap(loaded);
                        }
                        audio_file_pos = 0;
                    }
                }
                if (phase_a_system) {
                    std::vector<float> emb_v, emb_a, emb_t;
                    if (!current_image_path.empty()) emb_v = phase_a_system->processCLIPVision(current_image_path);
                    if (!current_audio_path.empty()) emb_a = phase_a_system->processWhisperAudio(current_audio_path);
                    if (!current_caption.empty()) emb_t = phase_a_system->processBERTText(current_caption);
                    std::size_t dim = 0;
                    dim = std::max({emb_v.size(), emb_a.size(), emb_t.size()});
                    if (dim > 0) {
                        std::vector<float> teacher_mm(dim, 0.0f);
                        int count = 0;
                        if (!emb_v.empty()) { for (std::size_t i = 0; i < dim && i < emb_v.size(); ++i) teacher_mm[i] += emb_v[i]; ++count; }
                        if (!emb_a.empty()) { for (std::size_t i = 0; i < dim && i < emb_a.size(); ++i) teacher_mm[i] += emb_a[i]; ++count; }
                        if (!emb_t.empty()) { for (std::size_t i = 0; i < dim && i < emb_t.size(); ++i) teacher_mm[i] += emb_t[i]; ++count; }
                        if (count > 1) { for (std::size_t i = 0; i < dim; ++i) teacher_mm[i] = teacher_mm[i] / static_cast<float>(count); }
                        std::string cid = std::string("triplet_") + it.stem;
                        phase_a_system->addTeacherEmbedding(
                            teacher_mm,
                            NeuroForge::Core::PhaseAMimicry::TeacherType::CLIP_Vision,
                            NeuroForge::Core::PhaseAMimicry::Modality::Multimodal,
                            cid,
                            current_caption,
                            1.0f
                        );
                        current_teacher_id = cid;
                    }
                }
                std::cout << "Triplet dataset loaded: " << triplet_items.size() << " items from '" << dataset_triplets_root << "'" << std::endl;
            } else {
                std::cerr << "Warning: No valid triplets found under '" << dataset_triplets_root << "'" << std::endl;
            }
        }

        // Episode CSV logging (optional)
        std::ofstream episode_csv;
        if (!episode_csv_path.empty()) {
            episode_csv.open(episode_csv_path, std::ios::out | std::ios::app);
            if (episode_csv && episode_csv.tellp() == 0) {
                episode_csv << "episode_index,steps,return,time_ms,success\n";
                episode_csv.flush();
            }
        }
        std::uint64_t episode_steps = 0;
        double episode_return = 0.0;
        auto episode_start_tp = std::chrono::steady_clock::now();
        std::uint64_t episode_index = 0;
        // Emit initial episode_start JSON event for Phase B
        {
            std::ostringstream js;
            js << "{\"version\":1,\"phase\":\"B\",\"event\":\"episode_start\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
            js << "\"episode_index\":" << episode_index << ",\"payload\":{\"episode_id\":" << current_episode_id << "}}";
            emit_json_line(log_json, log_json_path, js.str());
        }
        // Summary accumulators
        std::uint64_t finished_episodes = 0;
        std::uint64_t successful_episodes = 0;
        std::uint64_t sum_episode_steps = 0;
        double sum_episode_return = 0.0;
        std::uint64_t sum_episode_time_ms = 0;

        // Check if autonomous mode is enabled (e.g., by native substrate mode)
        // If so, run autonomous loop in separate thread to prevent deadlock
        std::thread autonomous_thread;
        std::atomic<bool> autonomous_running{false};
        
        if (brain.isAutonomousModeEnabled()) {
            std::cout << "Autonomous mode detected - starting autonomous loop in separate thread" << std::endl;
            autonomous_running.store(true);
            autonomous_thread = std::thread([&brain, steps, &autonomous_running]() {
                try {
                    brain.runAutonomousLoop(static_cast<std::size_t>(steps), 10.0f);
                } catch (const std::exception& e) {
                    std::cerr << "Autonomous loop error: " << e.what() << std::endl;
                }
                autonomous_running.store(false);
            });
            std::cout << "Autonomous loop started, continuing with regular processing..." << std::endl;
        }

        // Wait loop if autonomous mode is enabled
        // This prevents main() from exiting immediately when autonomous thread is running
        if (brain.isAutonomousModeEnabled()) {
            std::uint64_t mimicry_counter = 0;
            while (autonomous_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto gs_aut = brain.getGlobalStatistics();
                g_last_step.store(static_cast<std::uint64_t>(gs_aut.processing_cycles));

                if (dataset_active && !triplet_items.empty()) {
                    std::size_t idx = static_cast<std::size_t>(mimicry_counter % triplet_items.size());
                    if (idx != dataset_index) {
                        dataset_index = idx;
                        const auto &it = triplet_items[dataset_index];
                        current_image_path = it.image_path;
                        current_audio_path = it.audio_path;
                        current_caption = it.text;
                        if (memdb && memdb_run_id > 0) {
                            try {
                                using namespace std::chrono;
                                std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                std::ostringstream input;
                                input.setf(std::ios::fixed);
                                input << "{"
                                      << "\"image\":\"" << json_escape(current_image_path) << "\","
                                      << "\"audio\":\"" << json_escape(current_audio_path) << "\","
                                      << "\"caption\":\"" << json_escape(current_caption) << "\",";
                                {
                                    std::istringstream iss(current_caption);
                                    std::string tok; bool first_tok = true;
                                    input << "\"tokens\":[";
                                    while (iss >> tok) {
                                        if (!first_tok) input << ",";
                                        input << "\"" << json_escape(tok) << "\"";
                                        first_tok = false;
                                    }
                                    input << "]";
                                }
                                input << ",\"teacher_id\":\"" << json_escape(std::string("triplet_") + it.stem) << "\"";
                                input << "}";
                                std::int64_t exp_id = -1;
                                auto gs1 = brain.getGlobalStatistics();
                                std::uint64_t step_pc1 = static_cast<std::uint64_t>(gs1.processing_cycles);
                                bool ok_exp = memdb->insertExperience(ts_ms,
                                                                      step_pc1,
                                                                      "triplet_ingestion",
                                                                      input.str(),
                                                                      std::string(),
                                                                      true,
                                                                      memdb_run_id,
                                                                      exp_id);
                                std::cout << "Triplet ingestion (step): run=" << memdb_run_id
                                          << " step=" << step_pc1
                                          << " exp_id=" << exp_id
                                          << " ok=" << (ok_exp ? 1 : 0) << std::endl;
                                if (current_episode_id > 0 && exp_id > 0) {
                                    (void)memdb->linkExperienceToEpisode(exp_id, current_episode_id);
                                }
                            } catch (...) { }
                        }
                        if (!current_audio_path.empty()) {
                            std::vector<float> loaded;
                            int sr = 0;
                            if (nf_load_wav_any_mono(current_audio_path, loaded, sr)) {
                                if (sr != acfg.sample_rate && !loaded.empty()) {
                                    audio_file_samples = nf_resample_linear(loaded, sr, acfg.sample_rate);
                                } else {
                                    audio_file_samples.swap(loaded);
                                }
                                audio_file_pos = 0;
                            }
                        }
                        if (phase_a_system) {
                            std::vector<float> emb_v, emb_a, emb_t;
                            if (!current_image_path.empty()) emb_v = phase_a_system->processCLIPVision(current_image_path);
                            if (!current_audio_path.empty()) emb_a = phase_a_system->processWhisperAudio(current_audio_path);
                            if (!current_caption.empty()) emb_t = phase_a_system->processBERTText(current_caption);
                            std::size_t dim = std::max({emb_v.size(), emb_a.size(), emb_t.size()});
                            if (dim > 0) {
                                std::vector<float> teacher_mm(dim, 0.0f);
                                int count = 0;
                                if (!emb_v.empty()) { for (std::size_t j = 0; j < dim && j < emb_v.size(); ++j) teacher_mm[j] += emb_v[j]; ++count; }
                                if (!emb_a.empty()) { for (std::size_t j = 0; j < dim && j < emb_a.size(); ++j) teacher_mm[j] += emb_a[j]; ++count; }
                                if (!emb_t.empty()) { for (std::size_t j = 0; j < dim && j < emb_t.size(); ++j) teacher_mm[j] += emb_t[j]; ++count; }
                                if (count > 1) { for (std::size_t j = 0; j < dim; ++j) teacher_mm[j] = teacher_mm[j] / static_cast<float>(count); }
                                std::string cid = std::string("triplet_") + it.stem;
                                phase_a_system->addTeacherEmbedding(
                                    teacher_mm,
                                    NeuroForge::Core::PhaseAMimicry::TeacherType::CLIP_Vision,
                                    NeuroForge::Core::PhaseAMimicry::Modality::Multimodal,
                                    cid,
                                    current_caption,
                                    1.0f
                                );
                                current_teacher_id = cid;
                            }
                        }
                    }
                }

                if (phase_a_enable && phase_a_system) {
                    ++mimicry_counter;
                    if (auto* t = phase_a_system->getTeacherEmbedding(current_teacher_id)) {
                        std::vector<float> empty_student_vec;
                        auto attempt = phase_a_system->attemptMimicry(empty_student_vec, current_teacher_id, std::string("autonomous_step"));
                        if (!mimicry_internal) {
                            phase_a_system->applyMimicryReward(attempt);
                        }
                        phase_a_last_similarity = attempt.similarity_score;
                        phase_a_last_novelty = attempt.novelty_score;
                        phase_a_last_reward = attempt.total_reward;
                        phase_a_last_success = attempt.success;
                        phase_a_last_stu_len = static_cast<int>(attempt.student_embedding.size());
                        phase_a_last_tea_len = static_cast<int>(attempt.teacher_embedding.size());
                        {
                            double d = 0.0, ns = 0.0, nt = 0.0;
                            std::size_t n = std::min(attempt.student_embedding.size(), attempt.teacher_embedding.size());
                            for (std::size_t ii = 0; ii < n; ++ii) {
                                double sv = static_cast<double>(attempt.student_embedding[ii]);
                                double tv = static_cast<double>(attempt.teacher_embedding[ii]);
                                d += sv * tv;
                                ns += sv * sv;
                                nt += tv * tv;
                            }
                            phase_a_last_dot = d;
                            phase_a_last_stu_norm = std::sqrt(ns);
                            phase_a_last_tea_norm = std::sqrt(nt);
                        }
        if (auto* ls_mim = brain.getLearningSystem()) {
            ls_mim->setMimicryAttemptScores(phase_a_last_similarity, phase_a_last_novelty, phase_a_last_reward, phase_a_last_success);
        }
        if (memdb && g_memdb_run_id > 0) {
            using namespace std::chrono;
            std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            std::ostringstream js;
            js.setf(std::ios::fixed);
            js << "{";
            js << "\"phase_a\":{";
            js << "\"last_similarity\":" << phase_a_last_similarity << ",";
            js << "\"last_novelty\":" << phase_a_last_novelty << ",";
            js << "\"last_reward\":" << phase_a_last_reward << ",";
            js << "\"last_success\":" << (phase_a_last_success ? "true" : "false") << ",";
            js << "\"last_dot\":" << phase_a_last_dot << ",";
            js << "\"last_stu_norm\":" << phase_a_last_stu_norm << ",";
            js << "\"last_tea_norm\":" << phase_a_last_tea_norm;
            js << "},";
            js << "\"teacher_id\":\"" << json_escape(current_teacher_id) << "\"";
            js << "}";
            std::int64_t exp_id = -1;
            auto gsX = brain.getGlobalStatistics();
            std::uint64_t step_pcX = static_cast<std::uint64_t>(gsX.processing_cycles);
            (void)memdb->insertExperience(ts_ms,
                                          step_pcX,
                                          "snapshot:phase_a",
                                          js.str(),
                                          std::string(),
                                          false,
                                          g_memdb_run_id,
                                          exp_id);
        }
                    }
                }
            }
        } else {
            // Original loop for non-autonomous mode
            std::size_t last_best_idx = static_cast<std::size_t>(-1);
            int best_stable = 0;
            bool sandbox_seed_done = false;
            bool last_action_click = false;
            int last_click_step = -1000000;
            double last_audio_rms = 0.0;
            // ActionFilter (Safety Gate): baseline thresholds and cooldowns for sandbox actions
            int base_action_click_threshold = 8; // base stability threshold before clicking
            int action_click_threshold = base_action_click_threshold; // dynamically adjusted by Phase 13 decisions
            int action_scroll_cooldown_ms = 250; // minimal cooldown between scrolls
            auto last_action_tp = std::chrono::steady_clock::now(); // last sandbox action timepoint
            int blocked_action_count = 0;
            int blocked_by_phase15 = 0;
            int blocked_by_phase13 = 0;
            int blocked_by_no_web_actions = 0;
            int blocked_by_simulate_flag = 0;
            for (int i = 0; i < steps; ++i) {
                if (i == 0) {
                    std::cerr << "[AutonomyDiag]"
                              << " memdb=" << (memdb != nullptr)
                              << " run_id=" << memdb_run_id
                              << " self_model=" << (self_model != nullptr)
                              << " phase9=" << (phase9_metacog != nullptr)
                              << std::endl;
                }
                g_last_step.store(static_cast<std::uint64_t>(i));
                std::string p15_decision = "allow";
                if (phase15_ethics) {
                    try { p15_decision = phase15_ethics->runForLatest(std::string("sandbox_action")); } catch (...) {}
                }
                std::string p13_decision;
                if (phase13_autonomy) {
                    try { p13_decision = phase13_autonomy->maybeAdjustEnvelope(std::string("sandbox_action")); } catch (...) {}
                }
                if (memdb && memdb_run_id > 0 && self_model && phase9_metacog) {
                    NeuroForge::Core::AutonomyInputs a_inputs{};
                    double id_conf = 0.5;
                    const auto& id = self_model->identity();
                    if (id.confidence.has_value()) {
                        id_conf = *id.confidence;
                    }
                    a_inputs.identity_confidence = id_conf;
                    a_inputs.self_trust = phase9_metacog->getSelfTrust();
                    double ethics_score = 1.0;
                    bool ethics_block = false;
                    if (p15_decision == std::string("deny")) {
                        ethics_score = 0.0;
                        ethics_block = true;
                    } else if (p15_decision == std::string("review")) {
                        ethics_score = 0.5;
                    } else {
                        ethics_score = 1.0;
                    }
                    a_inputs.ethics_score = ethics_score;
                    a_inputs.ethics_hard_block = ethics_block;
                    double social_align = 0.5;
                    double reputation = 0.5;
                    const auto& social = self_model->social();
                    if (social.reputation.has_value()) {
                        reputation = *social.reputation;
                    }
                    a_inputs.social_alignment = social_align;
                    a_inputs.reputation = reputation;
                    using namespace std::chrono;
                    std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                    latest_autonomy_envelope = NeuroForge::Core::ComputeAutonomyEnvelope(
                        a_inputs,
                        ts_ms,
                        g_last_step.load(),
                        std::string("sandbox_action"));
                    (void)NeuroForge::Core::LogAutonomyEnvelope(
                        memdb.get(),
                        memdb_run_id,
                        latest_autonomy_envelope,
                        a_inputs,
                        std::string("sandbox_action"));
                    if (phase6_reasoner) {
                        phase6_reasoner->setAutonomyEnvelope(&latest_autonomy_envelope);
                    }
                    if (phase8_goals) {
                        phase8_goals->setAutonomyEnvelope(&latest_autonomy_envelope);
                    }
                    if (phase11_revision) {
                        phase11_revision->setAutonomyEnvelope(&latest_autonomy_envelope);
                    }
                    if (phase15_ethics) {
                        phase15_ethics->setAutonomyEnvelope(&latest_autonomy_envelope);
                    }
                }
                if (!p13_decision.empty()) {
                    if (p13_decision == std::string("tighten")) {
                        action_click_threshold = base_action_click_threshold + 4;
                    } else if (p13_decision == std::string("expand")) {
                        action_click_threshold = std::max(3, base_action_click_threshold - 2);
                    } else {
                        action_click_threshold = base_action_click_threshold;
                    }
                }
                if (g_abort.load()) break;
                if (simulate_blocked_actions > 0) { blocked_action_count += simulate_blocked_actions; blocked_by_simulate_flag += simulate_blocked_actions; }
                if (dataset_active && !triplet_items.empty()) {
                    std::size_t idx = static_cast<std::size_t>(i % triplet_items.size());
                    if (idx != dataset_index) {
                        dataset_index = idx;
                        const auto &it = triplet_items[dataset_index];
                        current_image_path = it.image_path;
                        current_audio_path = it.audio_path;
                        current_caption = it.text;
                        if (memdb && memdb_run_id > 0) {
                            try {
                                using namespace std::chrono;
                                std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                std::ostringstream input;
                                input.setf(std::ios::fixed);
                                input << "{"
                                      << "\"image\":\"" << json_escape(current_image_path) << "\","
                                      << "\"audio\":\"" << json_escape(current_audio_path) << "\","
                                      << "\"caption\":\"" << json_escape(current_caption) << "\",";
                                {
                                    std::istringstream iss(current_caption);
                                    std::string tok; bool first_tok = true;
                                    input << "\"tokens\":[";
                                    while (iss >> tok) {
                                        if (!first_tok) input << ",";
                                        input << "\"" << json_escape(tok) << "\"";
                                        first_tok = false;
                                    }
                                    input << "]";
                                }
                                input << ",\"teacher_id\":\"" << json_escape(std::string("triplet_") + it.stem) << "\"";
                                input << "}";
                                std::int64_t exp_id = -1;
                                auto gs1 = brain.getGlobalStatistics();
                                std::uint64_t step_pc1 = static_cast<std::uint64_t>(gs1.processing_cycles);
                                bool ok_exp = memdb->insertExperience(ts_ms,
                                                                      step_pc1,
                                                                      "triplet_ingestion",
                                                                      input.str(),
                                                                      std::string(),
                                                                      true,
                                                                      memdb_run_id,
                                                                      exp_id);
                                std::cout << "Triplet ingestion (step): run=" << memdb_run_id
                                          << " step=" << step_pc1
                                          << " exp_id=" << exp_id
                                          << " ok=" << (ok_exp ? 1 : 0) << std::endl;
                                if (current_episode_id > 0 && exp_id > 0) {
                                    (void)memdb->linkExperienceToEpisode(exp_id, current_episode_id);
                                }
                            } catch (...) { }
                        }
                        if (!current_audio_path.empty()) {
                            std::vector<float> loaded;
                            int sr = 0;
                            if (nf_load_wav_any_mono(current_audio_path, loaded, sr)) {
                                if (sr != acfg.sample_rate && !loaded.empty()) {
                                    audio_file_samples = nf_resample_linear(loaded, sr, acfg.sample_rate);
                                } else {
                                    audio_file_samples.swap(loaded);
                                }
                                audio_file_pos = 0;
                            }
                        }
                        if (phase_a_system) {
                            std::vector<float> emb_v, emb_a, emb_t;
                            if (!current_image_path.empty()) emb_v = phase_a_system->processCLIPVision(current_image_path);
                            if (!current_audio_path.empty()) emb_a = phase_a_system->processWhisperAudio(current_audio_path);
                            if (!current_caption.empty()) emb_t = phase_a_system->processBERTText(current_caption);
                            std::size_t dim = std::max({emb_v.size(), emb_a.size(), emb_t.size()});
                            if (dim > 0) {
                                std::vector<float> teacher_mm(dim, 0.0f);
                                int count = 0;
                                if (!emb_v.empty()) { for (std::size_t j = 0; j < dim && j < emb_v.size(); ++j) teacher_mm[j] += emb_v[j]; ++count; }
                                if (!emb_a.empty()) { for (std::size_t j = 0; j < dim && j < emb_a.size(); ++j) teacher_mm[j] += emb_a[j]; ++count; }
                                if (!emb_t.empty()) { for (std::size_t j = 0; j < dim && j < emb_t.size(); ++j) teacher_mm[j] += emb_t[j]; ++count; }
                                if (count > 1) { for (std::size_t j = 0; j < dim; ++j) teacher_mm[j] = teacher_mm[j] / static_cast<float>(count); }
                                std::string cid = std::string("triplet_") + it.stem;
                                phase_a_system->addTeacherEmbedding(
                                    teacher_mm,
                                    NeuroForge::Core::PhaseAMimicry::TeacherType::CLIP_Vision,
                                    NeuroForge::Core::PhaseAMimicry::Modality::Multimodal,
                                    cid,
                                    current_caption,
                                    1.0f
                                );
                                current_teacher_id = cid;
                            }
                        }
                    }
                }
                if (vision_demo && visual_region) {
                const int G = vcfg.grid_size;
                std::vector<float> gray;
                if (vision_source == "camera") {
#ifdef NF_HAVE_OPENCV
                    if (cam_ok) {
                        cv::Mat frame;
                        cap >> frame;
                        if (frame.empty()) {
                            std::cerr << "Warning: Captured empty frame; falling back to synthetic frame" << std::endl;
                            gray = make_synthetic_gray_grid(G, i);
                        } else {
                            cv::Mat grayMat, resized;
                            if (frame.channels() == 1) {
                                grayMat = frame;
                            } else {
                                cv::cvtColor(frame, grayMat, cv::COLOR_BGR2GRAY);
                            }
                            cv::resize(grayMat, resized, cv::Size(G, G), 0, 0, cv::INTER_AREA);
                            gray.resize(static_cast<std::size_t>(G * G));
                            for (int r = 0; r < G; ++r) {
                                for (int c = 0; c < G; ++c) {
                                    float v = resized.at<unsigned char>(r, c) / 255.0f;
                                    gray[static_cast<std::size_t>(r * G + c)] = std::clamp(v, 0.0f, 1.0f);
                                }
                            }
                        }
                    } else
#endif
                    {
                        gray = make_synthetic_gray_grid(G, i);
                    }
                } else if (vision_source == "screen") {
                    if (foveation_enable) {
#ifdef _WIN32
                        int bx = retina_rect_x, by = retina_rect_y, bw = retina_rect_w, bh = retina_rect_h;
                        if (sandbox_enable && sandbox_window.isOpen()) {
                            auto sb = sandbox_window.screenBounds();
                            bx = sb.x; by = sb.y; bw = sb.w; bh = sb.h;
                        }
                        int cx = bx + bw / 2;
                        int cy = by + bh / 2;
                        if (fovea_mode == std::string("cursor")) {
                            POINT pt{}; if (GetCursorPos(&pt)) {
                                cx = std::clamp(static_cast<int>(pt.x), bx, bx + std::max(0, bw - 1));
                                cy = std::clamp(static_cast<int>(pt.y), by, by + std::max(0, bh - 1));
                            }
                        } else if (fovea_mode == std::string("attention")) {
                            if (last_best_idx != static_cast<std::size_t>(-1)) {
                                int br = static_cast<int>(last_best_idx / static_cast<std::size_t>(G));
                                int bc = static_cast<int>(last_best_idx % static_cast<std::size_t>(G));
                                double fxn = (static_cast<double>(bc) + 0.5) / static_cast<double>(G);
                                double fyn = (static_cast<double>(br) + 0.5) / static_cast<double>(G);
                                cx = bx + static_cast<int>(fxn * static_cast<double>(bw));
                                cy = by + static_cast<int>(fyn * static_cast<double>(bh));
                                cx = std::clamp(cx, bx, bx + std::max(0, bw - 1));
                                cy = std::clamp(cy, by, by + std::max(0, bh - 1));
                            }
                        }
                        // EMA smoothing of fovea center
                        if (fovea_center_x < 0.0 || fovea_center_y < 0.0) {
                            fovea_center_x = static_cast<double>(cx);
                            fovea_center_y = static_cast<double>(cy);
                        } else {
                            fovea_center_x = fovea_alpha * static_cast<double>(cx) + (1.0 - fovea_alpha) * fovea_center_x;
                            fovea_center_y = fovea_alpha * static_cast<double>(cy) + (1.0 - fovea_alpha) * fovea_center_y;
                        }
                        int fx = static_cast<int>(std::round(fovea_center_x)) - fovea_w / 2;
                        int fy = static_cast<int>(std::round(fovea_center_y)) - fovea_h / 2;
                        fx = std::clamp(fx, bx, bx + std::max(0, bw - fovea_w));
                        fy = std::clamp(fy, by, by + std::max(0, bh - fovea_h));
                        screen.setRect({fx, fy, fovea_w, fovea_h});
                        last_fovea_x = fx; last_fovea_y = fy; last_fovea_w = fovea_w; last_fovea_h = fovea_h;
#endif
                    }
                    gray = screen.captureGrayGrid(G);
                } else if (vision_source == "maze") {
                    if (maze_first_person && maze_env.getFirstPersonRenderer()) {
                        // First-person perspective rendering
                        auto fp_pixels = maze_env.firstPersonObservation();
                        // Resize to match vision grid if needed
                        if (fp_pixels.size() != static_cast<size_t>(G * G)) {
                            // Simple resize by sampling
                            gray.resize(static_cast<std::size_t>(G * G));
                            auto renderer = maze_env.getFirstPersonRenderer();
                            auto config = renderer->getConfig();
                            float scale_x = static_cast<float>(config.width) / G;
                            float scale_y = static_cast<float>(config.height) / G;
                            
                            for (int r = 0; r < G; ++r) {
                                for (int c = 0; c < G; ++c) {
                                    int src_x = static_cast<int>(c * scale_x);
                                    int src_y = static_cast<int>(r * scale_y);
                                    src_x = std::clamp(src_x, 0, config.width - 1);
                                    src_y = std::clamp(src_y, 0, config.height - 1);
                                    size_t src_idx = static_cast<size_t>(src_y * config.width + src_x);
                                    size_t dst_idx = static_cast<size_t>(r * G + c);
                                    gray[dst_idx] = fp_pixels[src_idx];
                                }
                            }
                        } else {
                            gray = fp_pixels;
                        }
                    } else {
                        // Original top-down maze view
#ifdef NF_HAVE_OPENCV
                        const int Nm = maze_env.size();
                        cv::Mat m(Nm, Nm, CV_32F, cv::Scalar(0.2f)); // base brightness
                        for (int y = 0; y < Nm; ++y) {
                            for (int x = 0; x < Nm; ++x) {
                                if (maze_env.isWall(x, y)) {
                                    m.at<float>(y, x) = 0.0f; // walls dark
                                }
                            }
                        }
                        m.at<float>(maze_env.goalY(), maze_env.goalX()) = 0.8f;
                        m.at<float>(maze_env.agentY(), maze_env.agentX()) = 1.0f;
                        cv::Mat resized;
                        cv::resize(m, resized, cv::Size(G, G), 0, 0, cv::INTER_NEAREST);
                        gray.resize(static_cast<std::size_t>(G * G));
                        for (int r = 0; r < G; ++r) {
                            for (int c = 0; c < G; ++c) {
                                float v = resized.at<float>(r, c);
                                gray[static_cast<std::size_t>(r * G + c)] = std::clamp(v, 0.0f, 1.0f);
                            }
                        }
#else
                        const int Nm = maze_env.size();
                        gray.resize(static_cast<std::size_t>(G * G));
                        auto obs = maze_env.observation();
                        for (int r = 0; r < G; ++r) {
                            int y = (r * Nm) / G;
                            for (int c = 0; c < G; ++c) {
                                int x = (c * Nm) / G;
                                float v = obs[static_cast<std::size_t>(y * Nm + x)];
                                if (v < 0.0f) v = 0.0f; // walls to 0
                                gray[static_cast<std::size_t>(r * G + c)] = std::clamp(v, 0.0f, 1.0f);
                            }
                        }
#endif
                    }
                } else if (vision_source == "dataset") {
#ifdef NF_HAVE_OPENCV
                    if (!current_image_path.empty()) {
                        cv::Mat img = cv::imread(current_image_path, cv::IMREAD_GRAYSCALE);
                        if (!img.empty()) {
                            cv::Mat resized;
                            cv::resize(img, resized, cv::Size(G, G), 0, 0, cv::INTER_AREA);
                            gray.resize(static_cast<std::size_t>(G * G));
                            for (int r = 0; r < G; ++r) {
                                for (int c = 0; c < G; ++c) {
                                    float v = resized.at<unsigned char>(r, c) / 255.0f;
                                    gray[static_cast<std::size_t>(r * G + c)] = std::clamp(v, 0.0f, 1.0f);
                                }
                            }
                        } else {
                            gray = make_synthetic_gray_grid(G, i);
                        }
                    } else {
                        gray = make_synthetic_gray_grid(G, i);
                    }
#else
                    gray = make_synthetic_gray_grid(G, i);
#endif
                } else {
                    // synthetic
                    gray = make_synthetic_gray_grid(G, i);
                }
                auto features = vision_encoder.encode(gray);
                
                // Apply MotionBias if available
                if (motion_bias) {
                    std::vector<float> enhanced_features = features;
                    std::vector<std::vector<float>> motion_data = {gray};
                    motion_bias->applyMotionBias(enhanced_features, motion_data, G, G, 
                                               std::chrono::duration_cast<std::chrono::milliseconds>(
                                                   std::chrono::steady_clock::now().time_since_epoch()).count());
                    features = enhanced_features;
                }
                
                {
                    std::vector<float> mask(static_cast<std::size_t>(G * G), 1.0f);
                    const float cx = (G - 1) * 0.5f;
                    const float cy = (G - 1) * 0.5f;
                    const float sigma = std::max(1.0f, G * 0.33f);
                    const float two_sigma2 = 2.0f * sigma * sigma;
                    for (int r = 0; r < G; ++r) {
                        for (int c = 0; c < G; ++c) {
                            const float dx = static_cast<float>(c) - cx;
                            const float dy = static_cast<float>(r) - cy;
                            const float w = std::exp(-(dx*dx + dy*dy) / two_sigma2);
                            mask[static_cast<std::size_t>(r * G + c)] = w;
                        }
                    }
                    for (std::size_t k = 0; k < features.size() && k < mask.size(); ++k) {
                        features[k] *= mask[k];
                    }
                }

                last_visual_features = features;
                visual_region->processVisualInput(features);

                if (phase_a_enable && phase_a_system && !current_teacher_id.empty()) {
                    phase_a_system->addTeacherEmbedding(
                        last_visual_features,
                        NeuroForge::Core::PhaseAMimicry::TeacherType::CLIP_Vision,
                        NeuroForge::Core::PhaseAMimicry::Modality::Visual,
                        current_teacher_id,
                        std::string("vision_features"),
                        1.0f
                    );
                }

                if (motor_cortex && vision_source == "screen") {
#ifdef _WIN32
                    if (!features.empty()) {
                        std::size_t bi = 0; float bv = features[0];
                        for (std::size_t k = 1; k < features.size(); ++k) {
                            if (features[k] > bv) { bv = features[k]; bi = k; }
                        }
                        int br = static_cast<int>(bi / static_cast<std::size_t>(G));
                        int bc = static_cast<int>(bi % static_cast<std::size_t>(G));
                        double fx = (static_cast<double>(bc) + 0.5) / static_cast<double>(G);
                        double fy = (static_cast<double>(br) + 0.5) / static_cast<double>(G);
                        int tx = retina_rect_x + static_cast<int>(fx * static_cast<double>(retina_rect_w));
                        int ty = retina_rect_y + static_cast<int>(fy * static_cast<double>(retina_rect_h));
                        tx = std::clamp(tx, retina_rect_x, retina_rect_x + retina_rect_w - 1);
                        ty = std::clamp(ty, retina_rect_y, retina_rect_y + retina_rect_h - 1);

                        if (sandbox_enable) {
                            auto sb = sandbox_window.bounds();
                            int cx = std::clamp(static_cast<int>(fx * static_cast<double>(sb.w)), 0, std::max(0, sb.w - 1));
                            int cy = std::clamp(static_cast<int>(fy * static_cast<double>(sb.h)), 0, std::max(0, sb.h - 1));

                            if (!sandbox_seed_done) {
                                auto dec_type = NeuroForge::Core::ActionFilter_check(NeuroForge::Core::ActionKind::TypeText,
                                                                                       sandbox_actions_enable, p15_decision, p13_decision,
                                                                                       simulate_blocked_actions);
                                auto dec_key = NeuroForge::Core::ActionFilter_check(NeuroForge::Core::ActionKind::KeyPress,
                                                                                      sandbox_actions_enable, p15_decision, p13_decision,
                                                                                      simulate_blocked_actions);
                                if (dec_type.allow && dec_key.allow) {
                                    sandbox_window.focus();
                                    sandbox_window.typeText(std::string("news"));
                                    sandbox_window.sendKey(static_cast<unsigned int>(VK_RETURN));
                                    last_action_tp = std::chrono::steady_clock::now();
                                    if (g_memdb && g_memdb_run_id > 0) {
                                        using namespace std::chrono;
                                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                        std::int64_t aid = 0;
                                        (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("type_text"), std::string("{\"text\":\"news\",\"reason\":\"ok\"}"), true, g_memdb_run_id, aid);
                                        (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("key_press"), std::string("{\"vk\":13,\"reason\":\"ok\"}"), true, g_memdb_run_id, aid);
                                    }
                                    brain.deliverReward(0.2, std::string("action"), std::string("{\"kind\":\"search\"}"));
                                } else {
                                    blocked_action_count += 2;
                                    auto reason_type = dec_type.allow ? std::string("unknown") : dec_type.reason;
                                    auto reason_key = dec_key.allow ? std::string("unknown") : dec_key.reason;
                                    if (!dec_type.allow) {
                                        if (reason_type == std::string("phase15_deny")) ++blocked_by_phase15;
                                        else if (reason_type == std::string("no_web_actions")) ++blocked_by_no_web_actions;
                                        else if (reason_type == std::string("phase13_freeze")) ++blocked_by_phase13;
                                    }
                                    if (!dec_key.allow) {
                                        if (reason_key == std::string("phase15_deny")) ++blocked_by_phase15;
                                        else if (reason_key == std::string("no_web_actions")) ++blocked_by_no_web_actions;
                                        else if (reason_key == std::string("phase13_freeze")) ++blocked_by_phase13;
                                    }
                                    if (g_memdb && g_memdb_run_id > 0) {
                                        using namespace std::chrono;
                                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                        std::int64_t aid = 0;
                                        {
                                            std::ostringstream js;
                                            js.setf(std::ios::fixed);
                                            js << "{\"text\":\"news\",\"blocked\":true,\"reason\":\"" << reason_type << "\"}";
                                            (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("type_text"), js.str(), false, g_memdb_run_id, aid);
                                        }
                                        {
                                            std::ostringstream js;
                                            js.setf(std::ios::fixed);
                                            js << "{\"vk\":13,\"blocked\":true,\"reason\":\"" << reason_key << "\"}";
                                            (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("key_press"), js.str(), false, g_memdb_run_id, aid);
                                        }
                                    }
                                }
                                sandbox_seed_done = true;
                            }

                            if (bi == last_best_idx) { best_stable++; } else { best_stable = 1; last_best_idx = bi; }

                            const int wheel = 120;
                            if (br < (G / 3)) {
                                auto now_tp = std::chrono::steady_clock::now();
                                bool cooled = std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - last_action_tp).count() >= action_scroll_cooldown_ms;
                                auto dec = NeuroForge::Core::ActionFilter_check(NeuroForge::Core::ActionKind::ScrollUp, sandbox_actions_enable, p15_decision, p13_decision, simulate_blocked_actions);
                                if (dec.allow && cooled) {
                                    sandbox_window.scroll(+wheel);
                                    last_action_tp = now_tp;
                                    if (g_memdb && g_memdb_run_id > 0) {
                                        using namespace std::chrono;
                                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                        std::int64_t aid = 0;
                                        (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("scroll"), std::string("{\"dir\":\"up\",\"reason\":\"ok\"}"), true, g_memdb_run_id, aid);
                                    }
                                } else {
                                    blocked_action_count++;
                                    std::string reason = dec.allow ? std::string("cooldown") : dec.reason;
                                    if (!dec.allow) {
                                        if (reason == std::string("phase15_deny")) ++blocked_by_phase15;
                                        else if (reason == std::string("no_web_actions")) ++blocked_by_no_web_actions;
                                        else if (reason == std::string("phase13_freeze")) ++blocked_by_phase13;
                                    } else {
                                        if (p13_decision == std::string("tighten")) ++blocked_by_phase13;
                                    }
                                    if (g_memdb && g_memdb_run_id > 0) {
                                        using namespace std::chrono;
                                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                        std::int64_t aid = 0;
                                        std::ostringstream js;
                                        js.setf(std::ios::fixed);
                                        js << "{\"dir\":\"up\",\"blocked\":true,\"reason\":\"" << reason << "\"}";
                                        (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("scroll"), js.str(), false, g_memdb_run_id, aid);
                                    }
                                }
                            } else if (br > (2 * G / 3)) {
                                auto now_tp = std::chrono::steady_clock::now();
                                bool cooled = std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - last_action_tp).count() >= action_scroll_cooldown_ms;
                                auto dec = NeuroForge::Core::ActionFilter_check(NeuroForge::Core::ActionKind::ScrollDown, sandbox_actions_enable, p15_decision, p13_decision, simulate_blocked_actions);
                                if (dec.allow && cooled) {
                                    sandbox_window.scroll(-wheel);
                                    last_action_tp = now_tp;
                                    if (g_memdb && g_memdb_run_id > 0) {
                                        using namespace std::chrono;
                                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                        std::int64_t aid = 0;
                                        (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("scroll"), std::string("{\"dir\":\"down\",\"reason\":\"ok\"}"), true, g_memdb_run_id, aid);
                                    }
                                } else {
                                    blocked_action_count++;
                                    std::string reason = dec.allow ? std::string("cooldown") : dec.reason;
                                    if (!dec.allow) {
                                        if (reason == std::string("phase15_deny")) ++blocked_by_phase15;
                                        else if (reason == std::string("no_web_actions")) ++blocked_by_no_web_actions;
                                        else if (reason == std::string("phase13_freeze")) ++blocked_by_phase13;
                                    } else {
                                        if (p13_decision == std::string("tighten")) ++blocked_by_phase13;
                                    }
                                    if (g_memdb && g_memdb_run_id > 0) {
                                        using namespace std::chrono;
                                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                        std::int64_t aid = 0;
                                        std::ostringstream js;
                                        js.setf(std::ios::fixed);
                                        js << "{\"dir\":\"down\",\"blocked\":true,\"reason\":\"" << reason << "\"}";
                                        (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("scroll"), js.str(), false, g_memdb_run_id, aid);
                                    }
                                }
                            }

                            {
                                auto dec = NeuroForge::Core::ActionFilter_check(NeuroForge::Core::ActionKind::Click, sandbox_actions_enable, p15_decision, p13_decision, simulate_blocked_actions);
                                if (dec.allow && best_stable >= action_click_threshold) {
                                    bool ok_click = sandbox_window.click(cx, cy);
                                    last_action_click = ok_click;
                                    last_click_step = i;
                                    last_action_tp = std::chrono::steady_clock::now();
                                    if (g_memdb && g_memdb_run_id > 0) {
                                        using namespace std::chrono;
                                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                        std::ostringstream js;
                                        js.setf(std::ios::fixed);
                                        js << "{\"cx\":" << cx << ",\"cy\":" << cy << ",\"grid\":" << G << ",\"best_index\":" << bi << ",\"reason\":\"ok\"}";
                                        std::int64_t aid = 0;
                                        (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("click"), js.str(), ok_click ? true : false, g_memdb_run_id, aid);
                                    }
                                    best_stable = 0;
                                } else if (best_stable >= action_click_threshold) {
                                    blocked_action_count++;
                                    std::string reason = dec.allow ? (p13_decision == std::string("tighten") ? std::string("phase13_tighten") : std::string("stability_threshold")) : dec.reason;
                                    if (!dec.allow) {
                                        if (reason == std::string("phase15_deny")) ++blocked_by_phase15;
                                        else if (reason == std::string("no_web_actions")) ++blocked_by_no_web_actions;
                                        else if (reason == std::string("phase13_freeze")) ++blocked_by_phase13;
                                    } else {
                                        if (reason == std::string("phase13_tighten")) ++blocked_by_phase13;
                                    }
                                    if (g_memdb && g_memdb_run_id > 0) {
                                        using namespace std::chrono;
                                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                        std::ostringstream js;
                                        js.setf(std::ios::fixed);
                                        js << "{\"cx\":" << cx << ",\"cy\":" << cy << ",\"grid\":" << G << ",\"best_index\":" << bi << ",\"blocked\":true,\"reason\":\"" << reason << "\"}";
                                        std::int64_t aid = 0;
                                        (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("click"), js.str(), false, g_memdb_run_id, aid);
                                    }
                                    best_stable = 0;
                                }
                            }

                        } else {
                            BOOL ok = SetCursorPos(tx, ty);
                            if (g_memdb && g_memdb_run_id > 0) {
                                using namespace std::chrono;
                                std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                                std::ostringstream js;
                                js.setf(std::ios::fixed);
                                js << "{\"target_x\":" << tx << ",\"target_y\":" << ty
                                   << ",\"grid\":" << G << ",\"best_index\":" << bi << "}";
                                std::int64_t aid = 0;
                                (void)g_memdb->insertAction(ts_ms, static_cast<std::uint64_t>(i), std::string("cursor_move"), js.str(), ok ? true : false, g_memdb_run_id, aid);
                            }
                        }
                    }
#endif
                }
            }
            if (audio_demo && auditory_region) {
                // Use a short frame length appropriate for analysis window
                const std::size_t N = static_cast<std::size_t>(std::max(256, acfg.sample_rate / 32)); // ~31ms at 16kHz
                std::vector<float> samples;
                if (audio_system && sys_ok) {
                    samples = syscap.fetch(N);
                } else if (audio_mic && mic_ok) {
                    samples = mic.fetch(N);
                } else if (!audio_file_samples.empty()) {
                    samples.resize(N);
                    for (std::size_t k = 0; k < N; ++k) {
                        samples[k] = audio_file_samples[(audio_file_pos + k) % audio_file_samples.size()];
                    }
                    audio_file_pos = (audio_file_pos + N) % audio_file_samples.size();
                } else {
                    samples = make_synthetic_audio(N, acfg.sample_rate, i);
                }
                auto features = audio_encoder.encode(samples);
                
                // Apply VoiceBias if available
                if (voice_bias) {
                    std::vector<float> enhanced_features = features;
                    voice_bias->applyVoiceBias(enhanced_features, samples, acfg.sample_rate, acfg.feature_bins);
                    features = enhanced_features;
                }
                
                double sumsq = 0.0;
                for (float s : samples) { sumsq += static_cast<double>(s) * static_cast<double>(s); }
                double audio_rms = (N > 0) ? std::sqrt(sumsq / static_cast<double>(N)) : 0.0;
                last_audio_features = features;
                auditory_region->processAudioInput(features);

                if (sandbox_enable) {
                    const double thresh = 0.10;
                    if (last_action_click) {
                        if (audio_rms - last_audio_rms > thresh) {
                            brain.deliverReward(1.0, std::string("action"), std::string("{\"kind\":\"play_video\",\"step\":") + std::to_string(last_click_step) + std::string("}"));
                        } else if (last_audio_rms - audio_rms > thresh) {
                            brain.deliverReward(0.5, std::string("action"), std::string("{\"kind\":\"pause_video\",\"step\":") + std::to_string(last_click_step) + std::string("}"));
                        }
                        last_action_click = false;
                    }
                    last_audio_rms = audio_rms;
                }
            }

            // Social Perception Processing
            #ifdef NF_HAVE_OPENCV
            if (social_perception && social_bias && social_region) {
                // Process social perception using camera input if available
                if (cam_ok) {
                    cv::Mat frame;
                    cap >> frame;
                    if (!frame.empty()) {
                        #ifdef NF_HAVE_OPENCV
                        // Create audio buffer for multimodal processing
                        NeuroForge::Biases::SocialPerceptionBias::AudioBuffer audio_buffer;
                        if (audio_demo && !last_audio_features.empty()) {
                            audio_buffer.audio_envelope = last_audio_features;
                            audio_buffer.speech_probability = 0.5f; // Default probability
                        }
                        #endif
                        
                        // Process social frame with enhanced features
                        auto social_events = social_bias->processSocialFrame(frame, audio_buffer);
                        
                        // Apply social bias to the substrate
                        std::vector<float> social_features(32 * 32, 0.0f);
                        social_bias->applySocialBias(social_features, social_events, 32);
                        
                        // Feed the enhanced social features to the Social region
                         const auto& social_neurons = social_region->getNeurons();
                         const std::size_t social_len = std::min(social_neurons.size(), social_features.size());
                         for (std::size_t k = 0; k < social_len; ++k) {
                             if (social_neurons[k]) {
                                 social_neurons[k]->setActivation(social_features[k]);
                             }
                         }
                         
                         // Social Perception Visualization
                         if (social_view && !social_events.empty()) {
                             cv::Mat display_frame = frame.clone();
                             
                             // Visualize enhanced social perception features
                             for (const auto& event : social_events) {
                                 // Draw face contour mask instead of bounding box
                                 if (!event.face_mask.empty() && !event.face_contour.empty()) {
                                     cv::Scalar face_color = cv::Scalar(0, 255, 0); // Green
                                     if (event.is_speaking) {
                                         face_color = cv::Scalar(0, 0, 255); // Red if speaking
                                     }
                                     
                                     // Draw face contour points
                                     std::vector<std::vector<cv::Point>> contours = {event.face_contour};
                                     cv::drawContours(display_frame, contours, -1, face_color, 2);
                                     
                                     // Overlay face mask with transparency
                                     if (event.face_mask.size() == cv::Size(event.face_box.width, event.face_box.height)) {
                                         cv::Mat mask_overlay;
                                         cv::cvtColor(event.face_mask, mask_overlay, cv::COLOR_GRAY2BGR);
                                         cv::Mat face_roi = display_frame(event.face_box);
                                         cv::addWeighted(face_roi, 0.7, mask_overlay, 0.3, 0, face_roi);
                                     }
                                 }
                                 
                                 // Draw vectorized gaze arrows
                                 if (event.gaze_confidence > 0.3f && 
                                     (event.gaze_vector.x != 0 || event.gaze_vector.y != 0)) {
                                     
                                     cv::Point face_center(event.face_box.x + event.face_box.width / 2,
                                                          event.face_box.y + event.face_box.height / 2);
                                     
                                     // Calculate arrow end point using gaze vector
                                     float arrow_length = 100.0f * event.gaze_confidence;
                                     cv::Point arrow_end(
                                         face_center.x + static_cast<int>(event.gaze_vector.x * arrow_length),
                                         face_center.y + static_cast<int>(event.gaze_vector.y * arrow_length)
                                     );
                                     
                                     // Ensure arrow end is within frame
                                     arrow_end.x = (std::max)(0, (std::min)(arrow_end.x, display_frame.cols - 1));
                        arrow_end.y = (std::max)(0, (std::min)(arrow_end.y, display_frame.rows - 1));
                                     
                                     // Draw vectorized gaze arrow
                                     cv::arrowedLine(display_frame, face_center, arrow_end, cv::Scalar(255, 0, 0), 3, 8, 0, 0.3);
                                     
                                     // Draw pupil positions
                                     for (int p = 0; p < 2; ++p) {
                                         if (event.pupil_positions[p].x > 0 && event.pupil_positions[p].y > 0) {
                                             cv::Point pupil_global(
                                                 static_cast<int>(event.pupil_positions[p].x),
                                                 static_cast<int>(event.pupil_positions[p].y)
                                             );
                                             if (pupil_global.x >= 0 && pupil_global.y >= 0 && 
                                                 pupil_global.x < display_frame.cols && pupil_global.y < display_frame.rows) {
                                                 cv::circle(display_frame, pupil_global, 3, cv::Scalar(255, 255, 255), -1);
                                             }
                                         }
                                     }
                                 }
                                 
                                 // Draw precise mouth mask
                                 if (!event.mouth_mask.empty() && !event.mouth_region.empty()) {
                                     cv::Scalar mouth_color = cv::Scalar(0, 255, 255); // Yellow
                                     if (event.is_speaking) {
                                         mouth_color = cv::Scalar(0, 0, 255); // Red if speaking
                                     }
                                     
                                     // Overlay mouth mask with transparency
                                     if (event.mouth_mask.size() == cv::Size(event.mouth_region.width, event.mouth_region.height)) {
                                         cv::Mat mask_overlay;
                                         cv::cvtColor(event.mouth_mask, mask_overlay, cv::COLOR_GRAY2BGR);
                                         cv::Mat mouth_roi = display_frame(event.mouth_region);
                                         cv::addWeighted(mouth_roi, 0.6, mask_overlay, 0.4, 0, mouth_roi);
                                     }
                                     
                                     // Draw mouth region outline
                                     cv::rectangle(display_frame, event.mouth_region, mouth_color, 1);
                                 }
                                 
                                 // Draw eye contours
                                 for (int e = 0; e < 2; ++e) {
                                     if (!event.eye_contours[e].empty()) {
                                         std::vector<std::vector<cv::Point>> eye_contours = {event.eye_contours[e]};
                                         cv::drawContours(display_frame, eye_contours, -1, cv::Scalar(255, 255, 0), 1);
                                     }
                                 }
                                 
                                 // Add tracking ID and confidence info
                                 if (event.tracking_id >= 0) {
                                     std::stringstream info_text;
                                     info_text << "ID:" << event.tracking_id 
                                              << " G:" << std::fixed << std::setprecision(2) << event.gaze_confidence
                                              << " L:" << event.lip_sync_confidence;
                                     cv::Point text_pos(event.face_box.x, event.face_box.y - 10);
                                     if (text_pos.y > 0) {
                                         cv::putText(display_frame, info_text.str(), text_pos,
                                                    cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
                                     }
                                 }
                             }
                             
                             // Add title and frame info
                             cv::putText(display_frame, "NeuroForge Social Perception - Enhanced Biological Realism", 
                                        cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
                             
                             std::stringstream frame_info;
                             frame_info << "Step: " << i << " | Events: " << social_events.size() 
                                       << " | Features: Face Masks, Gaze Vectors, Lip-Sync";
                             cv::putText(display_frame, frame_info.str(), 
                                        cv::Point(10, display_frame.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 255), 1);
                             
                             // Display the enhanced social perception visualization
                             cv::imshow("Social Perception - Biological Realism", display_frame);
                             cv::waitKey(1); // Non-blocking wait
                         }
                         
                         // Optional: Log social events for debugging
                         if (i % 100 == 0 && !social_events.empty()) { // Log every 100 steps
                             std::cout << "Social events detected: " << social_events.size() 
                                      << " (faces with masks, gaze vectors, lip-sync)" << std::endl;
                         }
                    }
                }
            }
            #endif

            // Maze I/O
            if (maze_demo && maze_obs_region && maze_action_region) {
                // Build observation vector (one-hot agent position + goal marker)
                auto obs = maze_env.observation();
                const auto& obs_neurons = maze_obs_region->getNeurons();
                const std::size_t obs_len = std::min(obs_neurons.size(), obs.size());
                for (std::size_t k = 0; k < obs_len; ++k) {
                    if (obs_neurons[k]) obs_neurons[k]->setActivation(obs[k]);
                }
            }

            brain.processStep(delta_time_seconds);

            if (maze_demo && maze_obs_region && maze_action_region) {
                // Choose action: unified policy (neural motor cortex, Q-table, or hybrid blend)
                int action = 0;
                static std::mt19937 rng{std::random_device{}()};
                // Q-learning bookkeeping
                int q_state = -1;
                int q_next_state = -1;

                // Gather neural preferences from motor cortex action region
                const auto& act_neurons = maze_action_region->getNeurons();
                const int A = static_cast<int>(act_neurons.size());
                std::vector<float> prefs(A, 0.0f);
                for (int a = 0; a < A; ++a) {
                    prefs[a] = act_neurons[a] ? act_neurons[a]->getActivation() : 0.0f;
                }

                // Prepare Q-values if baseline is enabled
                const int N = maze_env.size();
                const int ax0 = maze_env.agentX();
                const int ay0 = maze_env.agentY();
                std::vector<float> qvals(A, 0.0f);
                if (qlearning) {
                    q_state = ay0 * N + ax0;
                    if (q_state >= 0) {
                        const int Q_ACTIONS = 4;
                        const int copyA = std::min(A, Q_ACTIONS);
                        for (int a = 0; a < copyA; ++a) {
                            qvals[a] = qtable[static_cast<std::size_t>(q_state * Q_ACTIONS + a)];
                        }
                    }
                }

                // Compute blended scores according to --hybrid-lambda (if provided)
                std::vector<float> scores(A, 0.0f);
                if (qlearning && hybrid_lambda >= 0.0f) {
                    for (int a = 0; a < A; ++a) {
                        scores[a] = hybrid_lambda * prefs[a] + (1.0f - hybrid_lambda) * qvals[a];
                    }
                } else if (qlearning) {
                    scores = qvals; // pure Q-table
                } else {
                    scores = prefs; // pure neural
                }

                // Teacher policy compute and optional blend into scores
                int teacher_action = -1;
                if (teacher_policy != "none" && A > 0) {
                    const int Nm = maze_env.size();
                    const int ax = maze_env.agentX();
                    const int ay = maze_env.agentY();
                    const int gx = maze_env.goalX();
                    const int gy = maze_env.goalY();
                    auto valid_next = [&](int a, int &nx, int &ny){ nx = ax; ny = ay; switch(a){case 0: ny = std::max(0, ay-1); break; case 1: ny = std::min(Nm-1, ay+1); break; case 2: nx = std::max(0, ax-1); break; case 3: nx = std::min(Nm-1, ax+1); break; default: break;} if (maze_env.isWall(nx, ny)) { nx = ax; ny = ay; } };
                    if (teacher_policy == "greedy") {
                        auto manh = [&](int x, int y){ return std::abs(x-gx) + std::abs(y-gy); };
                        int best_a = 0; int best_d = 1000000000;
                        for (int a = 0; a < std::min(A, 4); ++a) { int nx, ny; valid_next(a, nx, ny); int d = manh(nx, ny); if (d < best_d) { best_d = d; best_a = a; } }
                        teacher_action = best_a;
                    } else if (teacher_policy == "bfs") {
                        const int W = Nm, H = Nm; std::vector<int> parent(W*H, -1); std::vector<uint8_t> vis(W*H, 0); auto idx=[&](int x,int y){return y*W+x;}; std::queue<std::pair<int,int>>q; q.emplace(ax,ay); vis[idx(ax,ay)]=1; bool found=false; const int dx[4]={0,0,-1,1}; const int dy[4]={-1,1,0,0};
                        while(!q.empty() && !found){ auto [cx,cy]=q.front(); q.pop(); for(int a=0;a<std::min(A,4);++a){ int nx=std::clamp(cx+dx[a],0,W-1); int ny=std::clamp(cy+dy[a],0,H-1); if(maze_env.isWall(nx,ny)){ nx=cx; ny=cy; } int id=idx(nx,ny); if(!vis[id]){ vis[id]=1; parent[id]=idx(cx,cy)*10 + a; if(nx==gx && ny==gy){ found=true; break; } q.emplace(nx,ny); } } }
                        if(found){ int px=gx, py=gy; int act=-1; while(!(px==ax && py==ay)){ int enc=parent[idx(px,py)]; if(enc<0) break; int par=enc/10; act=enc%10; int parx=par%W; int pary=par/W; px=parx; py=pary; } if(act>=0 && act<A) teacher_action=act; }
                        if(teacher_action<0){ auto manh=[&](int x,int y){return std::abs(x-gx)+std::abs(y-gy);}; int best_a=0; int best_d=1000000000; for(int a=0;a<std::min(A,4);++a){ int nx,ny; valid_next(a,nx,ny); int d=manh(nx,ny); if(d<best_d){ best_d=d; best_a=a; } } teacher_action=best_a; }
                    }
                }
                last_teacher_action = teacher_action;
                if (teacher_mix > 0.0f && A > 0) {
                    for (int a = 0; a < A; ++a) {
                        float t = (teacher_action >= 0 && a == teacher_action) ? 1.0f : 0.0f;
                        scores[a] = (1.0f - teacher_mix) * scores[a] + teacher_mix * t;
                    }
                }

                // Emit Phase B decision context
                {
                    auto vec_to_json = [](const std::vector<float>& v) -> std::string {
                        std::ostringstream oss; oss << "[";
                        for (size_t idx = 0; idx < v.size(); ++idx) { if (idx) oss << ","; oss << std::fixed << std::setprecision(4) << v[idx]; }
                        oss << "]"; return oss.str(); };
                    std::ostringstream js;
                    js << "{\"version\":1,\"phase\":\"B\",\"event\":\"decision\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                    js << "\"episode_index\":" << episode_index << ",\"step\":" << i << ",\"payload\":{\"prefs\":" << vec_to_json(prefs) << ",\"qvals\":" << vec_to_json(qvals) << ",\"scores\":" << vec_to_json(scores) << "}}";
                    emit_json_line(log_json, log_json_path, js.str());
                }

                // Greedy choice over scores
                if (A > 0) {
                    int best_a = 0; float best_v = scores[0];
                    for (int a = 1; a < A; ++a) { if (scores[a] > best_v) { best_v = scores[a]; best_a = a; } }
                    action = best_a;
                }

                // Exploration: prefer softmax/wta for neural control; epsilon-greedy only when Q-learning baseline is active
                if (A > 0 && qlearning && epsilon >= 0.0f && epsilon <= 1.0f) {
                    std::uniform_real_distribution<float> ur(0.0f, 1.0f);
                    if (ur(rng) < epsilon) {
                        std::uniform_int_distribution<int> uni(0, A - 1);
                        action = uni(rng);
                    }
                } else if (A > 0 && softmax_temp > 0.0f) {
                    std::vector<double> probs(A, 0.0);
                    double sum = 0.0;
                    double maxp = -1e30;
                    for (float s : scores) maxp = std::max(maxp, static_cast<double>(s));
                    const double T = static_cast<double>(softmax_temp);
                    for (int a = 0; a < A; ++a) { double e = std::exp((static_cast<double>(scores[a]) - maxp) / T); probs[a] = e; sum += e; }
                    if (sum > 0.0) {
                        std::uniform_real_distribution<double> ur(0.0, sum);
                        double rpick = ur(rng);
                        double acc = 0.0; int pick = 0;
                        for (int a = 0; a < A; ++a) { acc += probs[a]; if (rpick <= acc) { pick = a; break; } }
                        action = pick;
                    }
                }

                // Phase 6 active gate: may audit or override within margin
                int base_action_before_phase6 = action;
                if (phase6_enable && phase6_reasoner && A > 0 && (phase6_active_mode == "on" || phase6_active_mode == "audit")) {
                    try {
                        std::vector<NeuroForge::Core::ReasonOption> phase6_opts_gate;
                        phase6_opts_gate.reserve(static_cast<size_t>(A));
                        for (int a = 0; a < A; ++a) {
                            NeuroForge::Core::ReasonOption ro;
                            ro.key = std::string("action_") + std::to_string(a);
                            ro.source = "maze_policy_blend";
                            ro.payload_json = "{}";
                            ro.confidence = static_cast<double>(scores[a]);
                            ro.complexity = 0.0;
                            phase6_opts_gate.push_back(ro);
                        }
                        auto p6score = phase6_reasoner->scoreOptions(phase6_opts_gate);
                        int p6_choice = static_cast<int>(p6score.best_index);
                        double p6_score = p6score.best_score;
                        double policy_score = (base_action_before_phase6 >= 0 && base_action_before_phase6 < A) ? static_cast<double>(scores[base_action_before_phase6]) : -1e9;
                        bool within = (p6_score >= policy_score - phase6_margin);
                        bool applied = false;
                        if (phase6_active_mode == "on" && within && p6_choice >= 0 && p6_choice < A) {
                            action = p6_choice;
                            applied = true;
                        }
                        // Emit audit/override JSON event
                        {
                            std::ostringstream js;
                            js << "{\"version\":1,\"phase\":\"6\",\"event\":\"gate\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                            js << "\"payload\":{\"mode\":\"" << json_escape(phase6_active_mode) << "\",\"policy_action\":" << base_action_before_phase6
                               << ",\"policy_score\":" << policy_score
                               << ",\"phase6_action\":" << p6_choice
                               << ",\"phase6_score\":" << p6_score
                               << ",\"margin\":" << phase6_margin
                               << ",\"within_margin\":" << (within ? "true" : "false")
                               << ",\"override_applied\":" << (applied ? "true" : "false") << "}}";
                            emit_json_line(log_json, log_json_path, js.str());
                        }
                    } catch (...) {
                        // swallow any Phase 6 errors
                    }
                }

                // Phase 6 shadow logging: register options and selected action
                std::int64_t phase6_selected_option_id = -1;
                std::string phase6_selected_key;
                if (phase6_enable && phase6_reasoner && A > 0) {
                    try {
                        std::vector<NeuroForge::Core::ReasonOption> phase6_opts;
                        phase6_opts.reserve(static_cast<size_t>(A));
                        for (int a = 0; a < A; ++a) {
                            NeuroForge::Core::ReasonOption ro;
                            ro.key = std::string("action_") + std::to_string(a);
                            ro.source = "maze_policy_blend";
                            ro.payload_json = "{}";
                            ro.confidence = static_cast<double>(scores[a]);
                            ro.complexity = 0.0;
                            phase6_opts.push_back(ro);
                        }
                        auto score = phase6_reasoner->scoreOptions(phase6_opts);
                        (void)score; // shadow mode, do not alter behavior
                        using namespace std::chrono;
                        std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                        auto option_ids = phase6_reasoner->registerOptions(phase6_opts, static_cast<std::uint64_t>(i), ts_ms, static_cast<std::size_t>(action));
                        if (!option_ids.empty() && action >= 0 && action < A) {
                            phase6_selected_option_id = option_ids[static_cast<std::size_t>(action)];
                            phase6_selected_key = std::string("action_") + std::to_string(action);
                        }
                    } catch (...) {
                        // swallow any Phase 6 errors in shadow mode
                    }
                }

                {
                    std::ostringstream js;
                    js << "{\"version\":1,\"phase\":\"B\",\"event\":\"action\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                    js << "\"episode_index\":" << episode_index << ",\"step\":" << i << ",\"payload\":{\"action\":" << action << "}}";
                    emit_json_line(log_json, log_json_path, js.str());
                }

                // Live-update mimicry attempt using mirror-mode sensory features or action scores
                if (mimicry_enable) {
                    auto* ls_mim = brain.getLearningSystem();
                    if (ls_mim) {
                        std::vector<float> embed;
                        if (mirror_mode == "vision" && !last_visual_features.empty()) {
                            embed = last_visual_features;
                        } else if (mirror_mode == "audio" && !last_audio_features.empty()) {
                            embed = last_audio_features;
                        } else if (!scores.empty()) {
                            embed.assign(scores.begin(), scores.end());
                        } else {
                            embed.assign(A > 0 ? static_cast<std::size_t>(A) : 1, 1.0f);
                        }
                        double n2 = 0.0;
                        for (float v : embed) n2 += static_cast<double>(v) * static_cast<double>(v);
                        if (n2 <= 1e-12) {
                            for (auto &v : embed) v = 1.0f;
                            n2 = static_cast<double>(embed.size());
                        }
                        float inv = 1.0f / static_cast<float>(std::sqrt(n2));
                        for (auto &v : embed) v *= inv;

                        // Per-step developmental update for LanguageSystem (Phase-5)
                        if (phase5_language_enable && language_system) {
                            language_system->updateDevelopment(0.01f);
                        }

                        // Per-step Phase A mimicry attempt and reward application (requires teacher embedding)
                        if (phase_a_enable && phase_a_system) {
                            if (auto* t = phase_a_system->getTeacherEmbedding(current_teacher_id)) {
                                std::vector<float> empty_student_vec;
                                auto attempt = phase_a_system->attemptMimicry(empty_student_vec, current_teacher_id, dataset_active ? "triplet_step" : "maze_step");
                                if (!mimicry_internal) {
                                    phase_a_system->applyMimicryReward(attempt);
                                }
                                phase_a_last_similarity = attempt.similarity_score;
                                phase_a_last_novelty = attempt.novelty_score;
                                phase_a_last_reward = attempt.total_reward;
                                phase_a_last_success = attempt.success;
                                phase_a_last_stu_len = static_cast<int>(attempt.student_embedding.size());
                                phase_a_last_tea_len = static_cast<int>(attempt.teacher_embedding.size());
                                {
                                    double d = 0.0, ns = 0.0, nt = 0.0;
                                    std::size_t n = std::min(attempt.student_embedding.size(), attempt.teacher_embedding.size());
                                    for (std::size_t ii = 0; ii < n; ++ii) {
                                        double sv = static_cast<double>(attempt.student_embedding[ii]);
                                        double tv = static_cast<double>(attempt.teacher_embedding[ii]);
                                        d += sv * tv;
                                        ns += sv * sv;
                                        nt += tv * tv;
                                    }
                                    phase_a_last_dot = d;
                                    phase_a_last_stu_norm = std::sqrt(ns);
                                    phase_a_last_tea_norm = std::sqrt(nt);
                                }
                                if (mimicry_internal && ls_mim) {
                                    ls_mim->setMimicryAttemptScores(phase_a_last_similarity, phase_a_last_novelty, phase_a_last_reward, phase_a_last_success);
                                }

                                // Update SelfNode based on mimicry outcome
                                if (self_node) {
                                    // Cognitive aspect from action embedding
                                    self_node->updateSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Cognitive, embed);
                                    // Emotional aspect from reward/success
                                    std::vector<float> emo = { phase_a_last_reward, phase_a_last_similarity, phase_a_last_novelty, phase_a_last_success ? 1.0f : 0.0f };
                                    self_node->updateSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Emotional, emo);
                                    // Narrative aspect: integrate a richer experience pattern (step, task reward, mimicry reward, similarity, novelty)
                                    std::vector<float> xp = { static_cast<float>(i), static_cast<float>(maze_last_reward), static_cast<float>(phase_a_last_reward), static_cast<float>(phase_a_last_similarity), static_cast<float>(phase_a_last_novelty) };
                                    self_node->integrateExperience(xp);
                                    // Update identity with current teacher/context hint (string)
                                    try { self_node->updateIdentity(std::string("teacher:") + current_teacher_id); } catch (...) {}
                                }
                            }
                        }

                        // Monitor language narrative into SelfNode
                        if (self_node && phase5_language_enable && language_system) {
                            auto lang_stats = language_system->getStatistics();
                            std::vector<float> narr = { static_cast<float>(lang_stats.narration_entries), static_cast<float>(lang_stats.total_tokens_generated) };
                            self_node->updateSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Narrative, narr);
                            // Also fold language-driven self signals into Emotional aspect (proxy: narration count and active vocab)
                            std::vector<float> emo_lang = { static_cast<float>(lang_stats.narration_entries), static_cast<float>(lang_stats.active_vocabulary_size) };
                            self_node->updateSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Emotional, emo_lang);
                        }
                    }
                }

                // Predict next state for Q-target (environment is deterministic)
                if (qlearning) {
                    int nx = ax0, ny = ay0;
                    switch (action) {
                        case 0: ny = std::max(0, ay0 - 1); break;
                        case 1: ny = std::min(N - 1, ay0 + 1); break;
                        case 2: nx = std::max(0, ax0 - 1); break;
                        case 3: nx = std::min(N - 1, ax0 + 1); break;
                        default: break;
                    }
                    // If next cell is a wall, agent stays in place
                    if (maze_env.isWall(nx, ny)) { nx = ax0; ny = ay0; }
                    q_next_state = ny * N + nx;
                }
                // Step environment and apply reward to LearningSystem
                float r = maze_env.step(action, maze_done);
                maze_last_reward = r;
                episode_steps += 1;
                episode_return += r;
                // Phase 6 posterior update in shadow mode
                if (phase6_enable && phase6_reasoner && phase6_selected_option_id >= 0) {
                    try {
                        using namespace std::chrono;
                        std::int64_t ts_ms2 = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                        phase6_reasoner->applyOptionResult(phase6_selected_option_id, phase6_selected_key, static_cast<double>(r), ts_ms2, true);
                    } catch (...) {
                        // swallow any Phase 6 errors
                    }
                }
                // Q-learning update (off-policy)
                if (qlearning && q_state >= 0) {
                    const float q_alpha = 0.5f;
                    const float q_gamma = 0.99f;
                    float max_next = 0.0f;
                    if (!maze_done && q_next_state >= 0) {
                        max_next = qtable[static_cast<std::size_t>(q_next_state * 4 + 0)];
                        for (int a2 = 1; a2 < 4; ++a2) {
                            max_next = std::max(max_next, qtable[static_cast<std::size_t>(q_next_state * 4 + a2)]);
                        }
                    }
                    float &qsa = qtable[static_cast<std::size_t>(q_state * 4 + action)];
                    float target = r + (maze_done ? 0.0f : q_gamma * max_next);
                    qsa += q_alpha * (target - qsa);
                }
                {
                    std::ostringstream js;
                    js << "{\"version\":1,\"phase\":\"B\",\"event\":\"reward\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                    js << "\"episode_index\":" << episode_index << ",\"step\":" << i << ",\"payload\":{\"reward\":" << r << ",\"done\":" << (maze_done ? "true" : "false") << "}}";
                    emit_json_line(log_json, log_json_path, js.str());
                }

                if (auto* ls_maze = brain.getLearningSystem()) {
                    ls_maze->applyExternalReward(r);
                }
                if (maze_done) {
                    auto now = std::chrono::steady_clock::now();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - episode_start_tp).count();
                    const bool success = maze_env.episodeSuccess();

                    // Emit episode_end JSON event
                    {
                        std::ostringstream js;
                        js << "{\"version\":1,\"phase\":\"B\",\"event\":\"episode_end\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                        js << "\"episode_index\":" << episode_index << ",\"payload\":{\"episode_id\":" << current_episode_id << ",\"return\":" << episode_return << ",\"length\":" << episode_steps << "}}";
                        emit_json_line(log_json, log_json_path, js.str());
                    }

                    if (episode_csv) {
                        episode_csv << episode_index << "," << episode_steps << "," << episode_return << "," << ms << "," << (success ? 1 : 0) << "\n";
                        episode_csv.flush();
                    }
                    // Compute episode metrics before rotating to the next episode
                    std::int64_t ended_episode_id = current_episode_id;
                    double avg_reward = (episode_steps > 0) ? (episode_return / static_cast<double>(episode_steps)) : 0.0;
                    double contradiction_rate = 0.0;
                    if (memdb && memdb_run_id > 0) {
                        contradiction_rate = memdb->getEpisodeContradictionRate(memdb_run_id, ended_episode_id);
                    }

                    // Update MemoryDB episode stats and rotate episodes if MemoryDB is enabled
                    if (memdb && current_episode_id > 0) {
                        (void)memdb->upsertEpisodeStats(current_episode_id, episode_steps, success, episode_return);
                        (void)brain.endEpisode(current_episode_id);
                        current_episode_id = brain.startEpisode("maze");
                    }
                    // Update summary accumulators
                    finished_episodes += 1;
                    if (success) successful_episodes += 1;
                    sum_episode_steps += episode_steps;
                    sum_episode_return += episode_return;
                    sum_episode_time_ms += static_cast<std::uint64_t>(ms);

                    // Phase 7 reflection hook via Phase 6 Reasoner (if available)
                    if (phase7_enable || phase7_reflect_enable) {
                        try {
                            if (phase6_reasoner) {
                                phase6_reasoner->onEpisodeEnd(static_cast<std::int64_t>(episode_index), contradiction_rate, avg_reward);
                            } else if (phase7_reflect && phase7_affect) {
                                auto st = phase7_affect->getState();
                                phase7_reflect->maybeReflect(static_cast<std::int64_t>(episode_index), contradiction_rate, avg_reward, st.valence, st.arousal);
                            }
                        } catch (...) {
                            // swallow any Phase 7 reflection errors
                        }
                    }

                    try {
                        if (phase8_goals) {
                            double coherence = std::clamp(1.0 - contradiction_rate, 0.0, 1.0);
                            double motivation = std::clamp(0.5 + avg_reward, 0.0, 1.0);
                            if (success) motivation = std::clamp(motivation + 0.1, 0.0, 1.0);
                            std::ostringstream notes;
                            notes << "maze episode_end episode_index=" << episode_index
                                  << " success=" << (success ? 1 : 0)
                                  << " avg_reward=" << avg_reward
                                  << " contradiction_rate=" << contradiction_rate;
                            (void)phase8_goals->updateMotivationState(motivation, coherence, notes.str());
                        }
                    } catch (...) {
                    }

                    episode_index++;
                    episode_steps = 0;
                    episode_return = 0.0;
                    episode_start_tp = now;

                    // Emit episode_start JSON event for the next episode
                    {
                        std::ostringstream js;
                        js << "{\"version\":1,\"phase\":\"B\",\"event\":\"episode_start\",\"time\":\"" << json_escape(iso8601_utc_now()) << "\",";
                        js << "\"episode_index\":" << episode_index << ",\"payload\":{\"episode_id\":" << current_episode_id << "}}";
                        emit_json_line(log_json, log_json_path, js.str());
                    }
                }
#ifdef NF_HAVE_OPENCV
                // Optional live maze visualization
                static std::chrono::steady_clock::time_point last_maze_draw = std::chrono::steady_clock::now();
                if (maze_view) {
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_maze_draw).count() >= maze_view_interval_ms) {
                        last_maze_draw = now;
                        int G = maze_env.size();
                        const int scale = 24;
                        cv::Mat img(G * scale, G * scale, CV_8UC3, cv::Scalar(30, 30, 30));
                        for (int y = 0; y < G; ++y) {
                            for (int x = 0; x < G; ++x) {
                                cv::Rect rc(x * scale, y * scale, scale, scale);
                                // Fill walls as black blocks
                                if (maze_env.isWall(x, y)) {
                                    cv::rectangle(img, rc, cv::Scalar(0,0,0), cv::FILLED);
                                }
                                // cell grid lines
                                cv::rectangle(img, rc, cv::Scalar(60, 60, 60), 1);
                            }
                        }
                        // Draw goal and agent
                        cv::rectangle(img, cv::Rect(maze_env.goalX() * scale, maze_env.goalY() * scale, scale, scale), cv::Scalar(0, 180, 0), cv::FILLED);
                        cv::circle(img, cv::Point(maze_env.agentX() * scale + scale/2, maze_env.agentY() * scale + scale/2), scale/3, cv::Scalar(40, 160, 255), cv::FILLED);
                        // Collision feedback border
                        if (maze_env.lastCollision()) {
                            cv::rectangle(img, cv::Rect(0,0,img.cols-1,img.rows-1), cv::Scalar(0,0,255), 2);
                        }
                        {
                            std::ostringstream oss;
                            oss << "r=" << std::fixed << std::setprecision(2) << r;
                            cv::putText(img, oss.str(), cv::Point(6, 16), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(255,255,255), 1, cv::LINE_AA);
                        }
                        {
                            // Status overlay: lambda, exploration mode, and action scores
                            std::string mode;
                            if (qlearning && epsilon >= 0.0f && epsilon <= 1.0f) mode = "epsilon";
                            else if (softmax_temp > 0.0f) mode = "softmax";
                            else mode = "wta";
                            std::ostringstream oss2;
                            oss2 << "lambda=" << std::fixed << std::setprecision(2) << (hybrid_lambda >= 0.0f ? hybrid_lambda : (qlearning ? 0.0f : 1.0f))
                                 << "  mode=" << mode;
                            cv::putText(img, oss2.str(), cv::Point(6, 34), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200,255,200), 1, cv::LINE_AA);
                        }
                        {
                            // Action scores are computed in the selection block; recompute a light-weight view here for overlay
                            const auto& act_neurons_dbg = maze_action_region->getNeurons();
                            const int A_dbg = static_cast<int>(act_neurons_dbg.size());
                            std::vector<float> prefs_dbg(A_dbg, 0.0f), qvals_dbg(A_dbg, 0.0f), scores_dbg(A_dbg, 0.0f);
                            for (int a = 0; a < A_dbg; ++a) { prefs_dbg[a] = act_neurons_dbg[a] ? act_neurons_dbg[a]->getActivation() : 0.0f; }
                            if (qlearning) {
                                const int N_dbg = maze_env.size();
                                const int ax0_dbg = maze_env.agentX();
                                const int ay0_dbg = maze_env.agentY();
                                const int q_state_dbg = ay0_dbg * N_dbg + ax0_dbg;
                                if (q_state_dbg >= 0) {
                                    const int Q_ACTIONS = 4;
                                    const int copyA = std::min(A_dbg, Q_ACTIONS);
                                    for (int a = 0; a < copyA; ++a) {
                                        qvals_dbg[a] = qtable[static_cast<std::size_t>(q_state_dbg * Q_ACTIONS + a)];
                                    }
                                }
                            }
                            if (qlearning && hybrid_lambda >= 0.0f) {
                                for (int a = 0; a < A_dbg; ++a) scores_dbg[a] = hybrid_lambda * prefs_dbg[a] + (1.0f - hybrid_lambda) * qvals_dbg[a];
                            } else if (qlearning) {
                                scores_dbg = qvals_dbg;
                            } else {
                                scores_dbg = prefs_dbg;
                            }
                            std::ostringstream oss3;
                            oss3 << "scores=";
                            const char* anames[4] = {"U","D","L","R"};
                            for (int a = 0; a < A_dbg && a < 4; ++a) {
                                if (a) oss3 << ", ";
                                oss3 << anames[a] << ":" << std::fixed << std::setprecision(2) << scores_dbg[a];
                            }
                            cv::putText(img, oss3.str(), cv::Point(6, 52), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200,200,255), 1, cv::LINE_AA);
                        }
                        cv::imshow("Maze", img);
                        cv::waitKey(1);
                    }
                }
#endif
            }

#ifdef NF_HAVE_OPENCV
            // Periodically render synapse heatmap
            if (heatmap_view) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heatmap).count() >= heatmap_interval_ms) {
                    last_heatmap = now;
                    auto* ls = brain.getLearningSystem();
                    if (!ls) {
                        if (!heatmap_warned_no_ls) {
                            std::cerr << "Info: Heatmap enabled but LearningSystem is not initialized. Enable learning to visualize synapses." << std::endl;
                            heatmap_warned_no_ls = true;
                        }
                    } else {
                        auto snapshots = ls->getSynapseSnapshot();
                        cv::Mat hm = cv::Mat::zeros(heatmap_size, heatmap_size, CV_32F);
                        if (!snapshots.empty()) {
                            for (const auto& s : snapshots) {
                                float w = std::fabs(s.weight);
                                if (w < heatmap_threshold) continue;
                                int r = static_cast<int>(s.pre_neuron % static_cast<uint64_t>(heatmap_size));
                                int c = static_cast<int>(s.post_neuron % static_cast<uint64_t>(heatmap_size));
                                hm.at<float>(r, c) += w;
                            }
                            double minv=0.0, maxv=0.0;
                            cv::minMaxLoc(hm, &minv, &maxv);
                            cv::Mat hm8, color;
                            if (maxv > 0.0) {
                                cv::normalize(hm, hm, 0.0f, 255.0f, cv::NORM_MINMAX);
                                hm.convertTo(hm8, CV_8U);
                            } else {
                                hm8 = cv::Mat::zeros(hm.size(), CV_8U);
                            }
                            cv::applyColorMap(hm8, color, cv::COLORMAP_TURBO);
                            cv::imshow("Synapse Heatmap", color);
                            // Keep UI responsive
                            cv::waitKey(1);
                        } else {
                            // Clear window if no data
                            cv::Mat blank(heatmap_size, heatmap_size, CV_8U, cv::Scalar(0));
                            cv::Mat color;
                            cv::applyColorMap(blank, color, cv::COLORMAP_TURBO);
                            cv::imshow("Synapse Heatmap", color);
                            cv::waitKey(1);
                        }
                    }
                }
            }
#endif

            // Periodically export live snapshot CSV and/or spikes CSV
            if (!snapshot_live_path.empty() || !spikes_live_path.empty()) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_snapshot).count() >= snapshot_interval_ms) {
                    last_snapshot = now;
                    auto* ls = brain.getLearningSystem();
                    if (!ls) {
                        if (!live_warned_no_ls) {
                            std::cerr << "Info: live export requested (--snapshot-live/--spikes-live) but LearningSystem is not initialized. Enable learning to export live data." << std::endl;
                            live_warned_no_ls = true;
                        }
                    } else {
                        auto snapshots = ls->getSynapseSnapshot();
                        if (!snapshot_live_path.empty()) {
                            std::ofstream ofs(snapshot_live_path, std::ios::out | std::ios::trunc);
                            if (!ofs) {
                                static bool warned = false;
                                if (!warned) {
                                    std::cerr << "Error: failed to open '" << snapshot_live_path << "' for live snapshot writing" << std::endl;
                                    warned = true;
                                }
                            } else {
                                ofs << "pre_neuron,post_neuron,weight\n";
                                for (const auto& s : snapshots) {
                                    ofs << s.pre_neuron << "," << s.post_neuron << "," << s.weight << "\n";
                                }
                                ofs.flush();
                            }
                        }

                        // Also export recent spikes for overlays
                        if (!spikes_live_path.empty()) {
                            // Build window of recent spikes and write unique IDs with timestamps (ms since now-ttl)
                            std::vector<std::pair<NeuroForge::NeuronID, long long>> recent;
                            {
                                std::lock_guard<std::mutex> lg(spikes_mutex);
                                // prune old
                                const auto cutoff = now - std::chrono::milliseconds(static_cast<int>(spikes_ttl_sec * 1000.0));
                                while (!spike_events.empty() && spike_events.front().second < cutoff) spike_events.pop_front();
                                recent.reserve(spike_events.size());
                                for (const auto& ev : spike_events) {
                                    auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - ev.second).count();
                                    long long t_ms = static_cast<long long>(age_ms);
                                    recent.emplace_back(ev.first, t_ms);
                                }
                            }
                            std::ofstream spofs(spikes_live_path, std::ios::out | std::ios::trunc);
                            if (spofs) {
                                spofs << "neuron_id,t_ms\n";
                                for (const auto& pr : recent) {
                                    spofs << pr.first << "," << pr.second << "\n";
                                }
                                spofs.flush();
                            }
                        }
                    }
                }
            }

            // Periodically log learning stats to MemoryDB
            if (memdb && memdb_run_id > 0) {
                auto now = std::chrono::steady_clock::now();

                // Decouple logging cadences
                bool due_reward = (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_reward_log).count() >= reward_interval_ms);
                bool due_memdb = (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_memdb_log).count() >= memdb_interval_ms);

                if (due_reward || due_memdb) {
                    std::size_t spike_count = 0;
                    std::int64_t win_ms = 0;

                    // If reward logging is due, calculate task reward from spike activity in the last reward window
                    if (due_reward) {
                         auto window_start = last_reward_log;
                         std::lock_guard<std::mutex> lg(spikes_mutex);
                         const auto cutoff = now - std::chrono::milliseconds(static_cast<int>(spikes_ttl_sec * 1000.0));
                         while (!spike_events.empty() && spike_events.front().second < cutoff) spike_events.pop_front();
                         for (const auto& ev : spike_events) {
                             if (ev.second >= window_start && ev.second <= now) ++spike_count;
                         }
                         win_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - window_start).count();
                    }

                    // Build region activation vector (acts) from active demo regions
                    // Required for reward shaping (due_reward) AND experience snapshots (due_memdb)
                    std::vector<float> region_acts;
                    if (vision_demo && visual_region) {
                        const auto& neurons = visual_region->getNeurons();
                        region_acts.reserve(region_acts.size() + neurons.size());
                        for (const auto& n : neurons) { if (n) region_acts.push_back(n->getActivation()); }
                    }
                    if (audio_demo && auditory_region) {
                        const auto& neurons = auditory_region->getNeurons();
                        region_acts.reserve(region_acts.size() + neurons.size());
                        for (const auto& n : neurons) { if (n) region_acts.push_back(n->getActivation()); }
                    }
                    if (maze_demo && maze_obs_region) {
                        const auto& neurons = maze_obs_region->getNeurons();
                        region_acts.reserve(region_acts.size() + neurons.size());
                        for (const auto& n : neurons) { if (n) region_acts.push_back(n->getActivation()); }
                    }
                    if (maze_demo && maze_action_region) {
                        const auto& neurons = maze_action_region->getNeurons();
                        region_acts.reserve(region_acts.size() + neurons.size());
                        for (const auto& n : neurons) { if (n) region_acts.push_back(n->getActivation()); }
                    }

                    // Use activations as the observation proxy for shaping (keeps integration minimal)
                    std::vector<float> obs = region_acts;

                    // Common state vars
                    float mimicry_sim = 0.0f;
                    float competence_level = 0.0f;
                    float substrate_similarity = 0.0f;
                    float substrate_novelty = 0.0f;

                    // REWARD LOGGING
                    if (due_reward) {
                         // Prepare telemetry via LearningSystem, then compute pipeline-shaped reward
                         double task_reward = static_cast<double>(spike_count);
                         if (maze_demo) {
                             task_reward += static_cast<double>(maze_last_reward);
                         }

                         if (auto* ls_reward = brain.getLearningSystem()) {
                             (void)ls_reward->computeShapedReward(obs, region_acts, static_cast<float>(task_reward));
                             mimicry_sim = ls_reward->getLastMimicrySim();
                             competence_level = ls_reward->getCompetenceLevel();
                             substrate_similarity = ls_reward->getLastSubstrateSimilarity();
                             substrate_novelty = ls_reward->getLastSubstrateNovelty();
                         }

                         double teacher_r = static_cast<double>(phase_a_last_reward);
                         double novelty_r = static_cast<double>(substrate_novelty);
                         double survival_r = static_cast<double>(phase_c_survival_scale) * static_cast<double>(substrate_similarity - substrate_novelty);
                         if (survival_r > 1.0) survival_r = 1.0; else if (survival_r < -1.0) survival_r = -1.0;
                         double wt_T = wt_teacher, wt_S = wt_survival, wt_N = wt_novelty;
                         double shaped_d = wt_T * teacher_r + wt_N * novelty_r + wt_S * survival_r;
                         if (shaped_d > 1.0) shaped_d = 1.0; else if (shaped_d < -1.0) shaped_d = -1.0;
                         float shaped_reward = static_cast<float>(shaped_d);

                         if (auto* ls_reward2 = brain.getLearningSystem()) {
                             ls_reward2->applyExternalReward(shaped_reward);
                         }

                         // Log shaped reward (with context including task spikes and vector sizes)
                         std::string ctx = std::string("{\"spikes\":") + std::to_string(spike_count) +
                                           ",\"window_ms\":" + std::to_string(win_ms) +
                                           ",\"maze_reward\":" + std::to_string(maze_last_reward) +
                                           ",\"task\":" + std::to_string(task_reward) +
                                           ",\"shaped\":" + std::to_string(shaped_reward) +
                                           ",\"mimicry_sim\":" + std::to_string(mimicry_sim) +
                                           ",\"competence_level\":" + std::to_string(competence_level) +
                                           ",\"substrate_similarity\":" + std::to_string(substrate_similarity) +
                                           ",\"substrate_novelty\":" + std::to_string(substrate_novelty) +
                                           ",\"teacher_policy\":\"" + teacher_policy + "\"" +
                                           ",\"teacher_action\":" + std::to_string(last_teacher_action) +
                                           ",\"teacher_mix\":" + std::to_string(teacher_mix) +
                                           ",\"obs_dim\":" + std::to_string(obs.size()) +
                                           ",\"acts_dim\":" + std::to_string(region_acts.size()) +
                                           ",\"blocked_actions\":" + std::to_string(blocked_action_count) +
                                           ",\"blocked_by_phase15\":" + std::to_string(blocked_by_phase15) +
                                           ",\"blocked_by_phase13\":" + std::to_string(blocked_by_phase13) +
                                           ",\"blocked_by_no_web_actions\":" + std::to_string(blocked_by_no_web_actions) +
                                           ",\"blocked_by_simulate_flag\":" + std::to_string(blocked_by_simulate_flag);
                         // Append recent context samples and current configuration
                         try {
                             auto ctx_samples = NeuroForge::Core::NF_GetRecentContextSamples();
                             auto cfg_ctx = NeuroForge::Core::NF_GetContextConfig();
                             std::ostringstream oss_ctx;
                             oss_ctx.setf(std::ios::fixed);
                             oss_ctx << std::setprecision(6) << "[";
                             for (size_t k = 0; k < ctx_samples.size(); ++k) {
                                 if (k) oss_ctx << ",";
                                 oss_ctx << ctx_samples[k];
                             }
                             oss_ctx << "]";
                             ctx += ",\"context\":" + oss_ctx.str();
                             ctx += ",\"context_cfg\":{\"gain\":" + std::to_string(cfg_ctx.gain)
                                    + ",\"update_ms\":" + std::to_string(cfg_ctx.update_ms)
                                    + ",\"window\":" + std::to_string(cfg_ctx.window) + "}";
                         } catch (...) { /* ignore */ }

                         // Add Phase A and Language system telemetry if available
                         if (phase_a_enable && phase_a_system) {
                             auto phase_a_stats = phase_a_system->getStatistics();
                             ctx += ",\"phase_a_mimicry_attempts\":" + std::to_string(phase_a_stats.total_mimicry_attempts);
                             ctx += ",\"phase_a_teacher_embeddings\":" + std::to_string(phase_a_stats.teacher_embeddings_stored);
                             ctx += ",\"phase_a_alignments\":" + std::to_string(phase_a_stats.multimodal_alignments_created);
                             // Last mimicry attempt details for trajectory plotting (gated behind --telemetry-extended)
                             if (telemetry_extended) {
                                 try {
                                     // We don't have direct accessor; use LearningSystem for sim and Phase A stats for counts
                                     ctx += ",\"phase_a_current_teacher_id\":\"" + current_teacher_id + "\"";
                                     ctx += ",\"phase_a_last_similarity\":" + std::to_string(phase_a_last_similarity);
                                     ctx += ",\"phase_a_last_novelty\":" + std::to_string(phase_a_last_novelty);
                                     ctx += ",\"phase_a_last_reward\":" + std::to_string(phase_a_last_reward);
                                     ctx += ",\"phase_a_last_success\":" + std::string(phase_a_last_success ? "true" : "false");
                                 } catch (...) { /* ignore */ }
                             }
                             // Nested phase_a block mirroring experiences schema (always include when Phase A is enabled)
                             try {
                                 ctx += ",\"phase_a\":{";
                                 ctx += "\"current_teacher_id\":\"" + current_teacher_id + "\"";
                                 ctx += ",\"last_similarity\":" + std::to_string(phase_a_last_similarity);
                                 ctx += ",\"last_novelty\":" + std::to_string(phase_a_last_novelty);
                                 ctx += ",\"last_reward\":" + std::to_string(phase_a_last_reward);
                                 ctx += ",\"last_success\":" + std::string(phase_a_last_success ? "true" : "false");
                                 ctx += "}";
                             } catch (...) { /* ignore */ }
                         }

                         if (phase5_language_enable && language_system) {
                             auto lang_stats = language_system->getStatistics();
                             ctx += ",\"language_stage\":" + std::to_string(static_cast<int>(lang_stats.current_stage));
                             ctx += ",\"language_tokens_generated\":" + std::to_string(lang_stats.total_tokens_generated);
                             ctx += ",\"language_narrations\":" + std::to_string(lang_stats.narration_entries);
                             ctx += ",\"language_vocab_active\":" + std::to_string(lang_stats.active_vocabulary_size);
                             // Nested language block mirroring experiences schema
                             ctx += ",\"language\":{";
                             ctx += "\"stage\":" + std::to_string(static_cast<int>(lang_stats.current_stage));
                             ctx += ",\"metrics\":{";
                             ctx += "\"tokens_generated\":" + std::to_string(lang_stats.total_tokens_generated);
                             ctx += ",\"narrations\":" + std::to_string(lang_stats.narration_entries);
                             ctx += ",\"vocab_active\":" + std::to_string(lang_stats.active_vocabulary_size);
                             ctx += "}}";
                         }
                         if (self_node) {
                             try {
                                 auto cog = self_node->getSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Cognitive);
                                 auto emo = self_node->getSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Emotional);
                                 ctx += ",\"self_awareness\":" + std::to_string(self_node->getSelfAwarenessLevel());
                                 ctx += ",\"self_identity\":\"" + self_node->getCurrentIdentity() + "\"";
                                 ctx += ",\"self_cognitive_conf\":" + std::to_string(cog.confidence_level);
                                 ctx += ",\"self_emotional_conf\":" + std::to_string(emo.confidence_level);
                                 // Nested self block mirroring experiences schema
                                 ctx += ",\"self\":{";
                                 ctx += "\"state\":{";
                                 ctx += "\"awareness\":" + std::to_string(self_node->getSelfAwarenessLevel());
                                 ctx += ",\"identity\":\"" + self_node->getCurrentIdentity() + "\"";
                                 ctx += "}";
                                 ctx += ",\"confidence\":{";
                                 ctx += "\"cognitive\":" + std::to_string(cog.confidence_level);
                                 ctx += ",\"emotional\":" + std::to_string(emo.confidence_level);
                                 ctx += "}";
                                 ctx += "}";
                             } catch (...) { /* ignore */ }
                         }

                         ctx += "}";
                         bool all_zero = (shaped_reward == 0.0f && teacher_r == 0.0 && novelty_r == 0.0 && survival_r == 0.0);
                         if (!all_zero || log_shaped_zero) {
                             brain.logReward(static_cast<double>(shaped_reward), "shaped", ctx);
                             try {
                                 std::ostringstream ctxs;
                                 ctxs.setf(std::ios::fixed);
                                 ctxs << "{"
                                      << "\"source\":\"survival\","
                                      << "\"teacher_id\":\"" << current_teacher_id << "\","
                                      << "\"components\":{\"teacher\":" << std::setprecision(4) << teacher_r
                                      << ",\"survival\":" << std::setprecision(4) << survival_r
                                      << ",\"novelty\":" << std::setprecision(4) << novelty_r << "},"
                                      << "\"shaped\":" << std::setprecision(4) << shaped_d
                                      << "}";
                                 brain.logReward(survival_r, "survival", ctxs.str());
                             } catch (...) { }
                             try {
                                 double merged = static_cast<double>(shaped_reward);
                                 std::ostringstream ctxm;
                                 ctxm.setf(std::ios::fixed);
                                 ctxm << "{"
                                      << "\"source\":\"merged\","
                                      << "\"teacher_id\":\"" << current_teacher_id << "\","
                                      << "\"weights\":{\"teacher\":" << std::setprecision(4) << wt_T
                                      << ",\"survival\":" << std::setprecision(4) << wt_S
                                      << ",\"novelty\":" << std::setprecision(4) << wt_N << "},"
                                      << "\"components\":{\"teacher\":" << std::setprecision(4) << teacher_r
                                      << ",\"survival\":" << std::setprecision(4) << survival_r
                                      << ",\"novelty\":" << std::setprecision(4) << novelty_r << "},"
                                      << "\"shaped\":" << std::setprecision(4) << shaped_d
                                      << "}";
                                 brain.logReward(merged, "merged", ctxm.str());
                             } catch (...) { }
                             if (simulate_rewards > 0) {
                                 for (int sr = 0; sr < simulate_rewards; ++sr) {
                                     std::ostringstream jrs;
                                     jrs << "{\"source\":\"synthetic\",\"step\":" << i << "}";
                                     brain.logReward(1.0, "simulated", jrs.str());
                                 }
                             }
                         }
                         last_reward_log = now;
                    } else {
                         // Update state vars if not computed in reward block, for MEMDB use
                         if (auto* ls = brain.getLearningSystem()) {
                             mimicry_sim = ls->getLastMimicrySim();
                             competence_level = ls->getCompetenceLevel();
                             substrate_similarity = ls->getLastSubstrateSimilarity();
                             substrate_novelty = ls->getLastSubstrateNovelty();
                         }
                    }

                    // MEMDB LOGGING
                    if (due_memdb) {
                        // Log context peers (sample + config) into MemoryDB
                        try {
                            auto peers = NeuroForge::Core::NF_ListContextPeers();
                            std::int64_t ts_ms_peer = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            for (const auto& p : peers) {
                                // Use configured label if present; default to "runtime"
                                std::string sample_label;
                                auto it = context_peer_labels.find(p);
                                if (it != context_peer_labels.end()) sample_label = it->second; else sample_label = "runtime";
                                double s = NeuroForge::Core::NF_SampleContextPeer(p, sample_label);
                                auto cfgp = NeuroForge::Core::NF_GetPeerConfig(p);
                                // Compute effective coupling parameters for this peer
                                double lambda_eff = 0.0;
                                try {
                                    auto edges = NeuroForge::Core::NF_GetContextCouplings();
                                    for (const auto& e : edges) {
                                        const std::string& src = std::get<0>(e);
                                        const std::string& dst = std::get<1>(e);
                                        double w = std::get<2>(e);
                                        if (dst == p) lambda_eff += w;
                                    }
                                    if (lambda_eff < 0.0) lambda_eff = 0.0;
                                    if (lambda_eff > 1.0) lambda_eff = 1.0;
                                } catch (...) { /* ignore coupling inspection errors */ }

                                // Use global Phase-4 kappa param when available; default 0.0
                                double kappa_eff = 0.0;
                                try {
                                    // kappa_param is parsed earlier; replicate local copy if needed
                                    // This scope relies on captured kappa_param from main
                                    kappa_eff = kappa_param;
                                    if (kappa_eff < 0.0) kappa_eff = 0.0;
                                } catch (...) { /* ignore */ }

                                std::string mode_val = "coop"; // default mode until CLI adds couple-mode
                                std::int64_t out_id = 0;
                                (void)memdb->insertContextPeerLog(memdb_run_id, ts_ms_peer, p, s, cfgp.gain, cfgp.update_ms, cfgp.window, sample_label, mode_val, lambda_eff, kappa_eff, out_id);
                            }
                        } catch (...) { /* ignore peer logging errors */ }

                        last_memdb_log = now;
                        static std::uint64_t steps_since = 0;
                        static auto last_hz_time = std::chrono::steady_clock::now();
                        steps_since += 1;
                        double hz = 0.0;
                        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hz_time).count();
                        if (elapsed_ms > 0) {
                            hz = (steps_since * 1000.0) / static_cast<double>(elapsed_ms);
                        }
                        if (elapsed_ms >= memdb_interval_ms) {
                            steps_since = 0;
                            last_hz_time = now;
                        }
                        std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        auto statsOpt2 = brain.getLearningStatistics();
                        NeuroForge::Core::LearningSystem::Statistics st{};
                        if (statsOpt2.has_value()) st = statsOpt2.value();
                        {
                            auto gs = brain.getGlobalStatistics();
                            std::uint64_t step_pc = static_cast<std::uint64_t>(gs.processing_cycles);
                            (void)memdb->insertLearningStats(ts_ms, step_pc, hz, st, memdb_run_id);
                            std::int64_t event_id = 0;
                            (void)memdb->insertRunEvent(memdb_run_id, ts_ms, step_pc, std::string("heartbeat"), std::string(), 0, nf_process_rss_mb(), 0.0, event_id);
                            // Optional memory pressure notifications (RSS)
                            double rss_mb = nf_process_rss_mb();
                            if (rss_warn_threshold_mb > 0.0 && rss_mb >= rss_warn_threshold_mb) {
                                auto now_warn = std::chrono::steady_clock::now();
                                if (last_rss_warn.time_since_epoch().count() == 0 ||
                                    std::chrono::duration_cast<std::chrono::milliseconds>(now_warn - last_rss_warn).count() >= rss_warn_interval_ms) {
                                    std::int64_t warn_id = 0;
                                    (void)memdb->insertRunEvent(memdb_run_id, ts_ms, step_pc, std::string("warning"), std::string("rss_threshold_exceeded"), 0, rss_mb, 0.0, warn_id);
                                    last_rss_warn = now_warn;
                                }
                            }
                            if (rss_fail_threshold_mb > 0.0 && rss_mb >= rss_fail_threshold_mb) {
                                std::int64_t err_id = 0;
                                (void)memdb->insertRunEvent(memdb_run_id, ts_ms, step_pc, std::string("error"), std::string("rss_fail_threshold_exceeded"), 0, rss_mb, 0.0, err_id);
                            }
                        }

                        // Periodic experience snapshot for state trajectory visualization
                        try {
                            auto vec_to_json = [](const std::vector<float>& v) -> std::string {
                                std::ostringstream oss;
                                oss.setf(std::ios::fixed);
                                oss << std::setprecision(6) << "[";
                                for (size_t k = 0; k < v.size(); ++k) {
                                    if (k) oss << ",";
                                    oss << v[k];
                                }
                                oss << "]";
                                return oss.str();
                            };
                            std::string tag = std::string("snapshot:") + (phase_a_enable ? "phase_a" : "core");
                            if (dataset_active && mimicry_enable && phase_a_enable && phase_a_system && !current_teacher_id.empty()) {
                                auto* ls_mim2 = brain.getLearningSystem();
                                std::vector<float> empty_student_vec;
                                int mimicry_repeats = phase_a_mimicry_repeats;
                                for (int rep = 0; rep < mimicry_repeats; ++rep) {
                                    auto attempt2 = phase_a_system->attemptMimicry(empty_student_vec, current_teacher_id, std::string("triplet_step"));
                                    if (!mimicry_internal) { phase_a_system->applyMimicryReward(attempt2); }
                                    phase_a_last_similarity = attempt2.similarity_score;
                                    phase_a_last_novelty = attempt2.novelty_score;
                                    phase_a_last_reward = attempt2.total_reward;
                                    phase_a_last_success = attempt2.success;
                                    phase_a_last_stu_len = static_cast<int>(attempt2.student_embedding.size());
                                    phase_a_last_tea_len = static_cast<int>(attempt2.teacher_embedding.size());
                                    double d2 = 0.0, ns2 = 0.0, nt2 = 0.0;
                                    std::size_t n2 = std::min(attempt2.student_embedding.size(), attempt2.teacher_embedding.size());
                                    for (std::size_t ii = 0; ii < n2; ++ii) {
                                        double sv2 = static_cast<double>(attempt2.student_embedding[ii]);
                                        double tv2 = static_cast<double>(attempt2.teacher_embedding[ii]);
                                        d2 += sv2 * tv2;
                                        ns2 += sv2 * sv2;
                                        nt2 += tv2 * tv2;
                                    }
                                    phase_a_last_dot = d2;
                                    phase_a_last_stu_norm = std::sqrt(ns2);
                                    phase_a_last_tea_norm = std::sqrt(nt2);
                                    if (mimicry_internal && ls_mim2) {
                                        ls_mim2->setMimicryAttemptScores(phase_a_last_similarity, phase_a_last_novelty, phase_a_last_reward, phase_a_last_success);
                                    }
                                    if (self_node) {
                                        self_node->updateSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Cognitive, attempt2.student_embedding);
                                        std::vector<float> emo2 = { phase_a_last_reward, phase_a_last_similarity, phase_a_last_novelty, phase_a_last_success ? 1.0f : 0.0f };
                                        self_node->updateSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Emotional, emo2);
                                        std::vector<float> xp2 = { static_cast<float>(i), 0.0f, static_cast<float>(phase_a_last_reward), static_cast<float>(phase_a_last_similarity), static_cast<float>(phase_a_last_novelty) };
                                        self_node->integrateExperience(xp2);
                                        try { self_node->updateIdentity(std::string("teacher:") + current_teacher_id); } catch (...) {}
                                    }
                                }
                            }
                            // Input: observation proxy; optionally include extended metadata; Output: action activations
                            std::string input_json;
                            if (telemetry_extended) {
                                std::ostringstream meta;
                                meta.setf(std::ios::fixed);
                                meta << std::setprecision(6) << "{";
                                meta << "\"obs\":" << vec_to_json(obs);
                                // Learning telemetry block
                                meta << ",\"learning\":{"
                                     "\"competence_level\":" << competence_level
                                     << ",\"substrate_similarity\":" << substrate_similarity
                                     << ",\"substrate_novelty\":" << substrate_novelty
                                     << "}";
                                // Context telemetry block
                                try {
                                    auto ctx_samples = NeuroForge::Core::NF_GetRecentContextSamples();
                                    auto cfg_ctx = NeuroForge::Core::NF_GetContextConfig();
                                    std::ostringstream oss_ctx;
                                    oss_ctx.setf(std::ios::fixed);
                                    oss_ctx << std::setprecision(6) << "[";
                                    for (size_t k = 0; k < ctx_samples.size(); ++k) {
                                        if (k) oss_ctx << ",";
                                        oss_ctx << ctx_samples[k];
                                    }
                                    oss_ctx << "]";
                                    meta << ",\"context\":{\"samples\":" << oss_ctx.str()
                                         << ",\"config\":{\"gain\":" << cfg_ctx.gain
                                         << ",\"update_ms\":" << cfg_ctx.update_ms
                                         << ",\"window\":" << cfg_ctx.window << "}}";
                                } catch (...) { /* ignore */ }
                                if (phase_a_enable && phase_a_system) {
                                    meta << ",\"phase_a\":{"
                                        "\"current_teacher_id\":\"" << current_teacher_id << "\","
                                        "\"last_similarity\":" << phase_a_last_similarity << ","
                                        "\"last_novelty\":" << phase_a_last_novelty << ","
                                        "\"last_reward\":" << phase_a_last_reward << ","
                                        "\"last_success\":" << (phase_a_last_success ? "true" : "false") << ","
                                        "\"stu_len\":" << phase_a_last_stu_len << ","
                                        "\"tea_len\":" << phase_a_last_tea_len << ","
                                        "\"stu_norm\":" << phase_a_last_stu_norm << ","
                                        "\"tea_norm\":" << phase_a_last_tea_norm << ","
                                        "\"dot\":" << phase_a_last_dot
                                        << "}";
                                }
                                if (phase5_language_enable && language_system) {
                                    auto lang_stats = language_system->getStatistics();
                                    meta << ",\"language\":{"
                                        "\"stage\":" << static_cast<int>(lang_stats.current_stage) << ","
                                        "\"tokens_generated\":" << lang_stats.total_tokens_generated << ","
                                        "\"narrations\":" << lang_stats.narration_entries << ","
                                        "\"vocab_active\":" << lang_stats.active_vocabulary_size << ","
                                        "\"metrics\":{\"stage\":" << static_cast<int>(lang_stats.current_stage)
                                        << ",\"tokens_generated\":" << lang_stats.total_tokens_generated
                                        << ",\"narrations\":" << lang_stats.narration_entries
                                        << ",\"vocab_active\":" << lang_stats.active_vocabulary_size << "}"
                                        << "}";
                                }
                                // Vision telemetry
                                {
                                    int vx = retina_rect_x, vy = retina_rect_y, vw = retina_rect_w, vh = retina_rect_h;
                                    if (foveation_enable && last_fovea_w > 0 && last_fovea_h > 0) {
                                        vx = last_fovea_x; vy = last_fovea_y; vw = last_fovea_w; vh = last_fovea_h;
                                    }
                                    meta << ",\"vision\":{\"source\":\"" << vision_source << "\","
                                         << "\"retina\":{\"x\":" << vx << ",\"y\":" << vy << ",\"w\":" << vw << ",\"h\":" << vh << "},"
                                         << "\"foveation\":{\"enabled\":" << (foveation_enable ? "true" : "false")
                                         << ",\"mode\":\"" << fovea_mode << "\",\"alpha\":" << fovea_alpha << "}"
                                         << "}";
                                }
                                if (self_node) {
                                    try {
                                        auto cog = self_node->getSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Cognitive);
                                        auto emo = self_node->getSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Emotional);
                                        meta << ",\"self\":{"
                                             "\"awareness\":" << self_node->getSelfAwarenessLevel() << ","
                                             "\"identity\":\"" << self_node->getCurrentIdentity() << "\","
                                             "\"cognitive_conf\":" << cog.confidence_level << ","
                                             "\"emotional_conf\":" << emo.confidence_level << ","
                                             "\"state\":{\"awareness\":" << self_node->getSelfAwarenessLevel()
                                                << ",\"identity\":\"" << self_node->getCurrentIdentity() << "\"},"
                                             "\"confidence\":{\"cognitive\":" << cog.confidence_level
                                                << ",\"emotional\":" << emo.confidence_level << "}"
                                             << "}";
                                    } catch (...) { /* ignore */ }
                                }
                                meta << "}";
                                input_json = meta.str();
                            } else {
                                input_json = vec_to_json(obs);
                            }
                            std::string output_json = vec_to_json(region_acts);
                            std::int64_t exp_id = -1;
                            bool significant = false;
                            auto gs = brain.getGlobalStatistics();
                            std::uint64_t step_pc = static_cast<std::uint64_t>(gs.processing_cycles);
                            (void)memdb->insertExperience(ts_ms,
                                                          step_pc,
                                                          tag,
                                                          input_json,
                                                          output_json,
                                                          significant,
                                                          memdb_run_id,
                                                          exp_id);
                            if (current_episode_id > 0 && exp_id > 0) {
                                (void)memdb->linkExperienceToEpisode(exp_id, current_episode_id);
                            }
                            if (self_node) {
                                try {
                                    auto cog = self_node->getSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Cognitive);
                                    auto emo = self_node->getSelfRepresentation(NeuroForge::Regions::SelfNode::SelfAspect::Emotional);
                                    std::ostringstream self_state;
                                    self_state.setf(std::ios::fixed);
                                    self_state << std::setprecision(6) << "{"
                                               << "\"awareness\":" << self_node->getSelfAwarenessLevel() << ","
                                               << "\"identity\":\"" << self_node->getCurrentIdentity() << "\","
                                               << "\"cognitive_conf\":" << cog.confidence_level << ","
                                               << "\"emotional_conf\":" << emo.confidence_level
                                               << "}";
                                    double avg_conf = 0.5 * (cog.confidence_level + emo.confidence_level);
                                    brain.logSelfModel(self_state.str(), avg_conf);
                                } catch (...) { /* ignore */ }
                            }
                        } catch (...) { /* swallow to avoid impacting realtime loop */ }
                    }
                }
            }
            #ifdef _WIN32
            {
                MSG msg;
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        g_abort.store(true);
                        break;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
            #endif

            if (step_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
            }
        }
        } // Close the else block for non-autonomous mode

#ifdef NF_HAVE_OPENCV
        if (heatmap_view) {
            // Briefly allow the last frame to be visible
            cv::waitKey(10);
            cv::destroyWindow("Synapse Heatmap");
        }
        if (maze_view && maze_window_created) {
            cv::destroyWindow("Maze");
        }
        if (social_view) {
            cv::destroyAllWindows(); // Safer cleanup for social perception windows
        }
#endif

#ifdef NF_HAVE_OPENCV
        // Release camera capture if it was opened
        if (vision_demo) {
            cap.release();
        }
#endif

         // End MemoryDB episode if started
         if (memdb && current_episode_id > 0) {
             (void)brain.endEpisode(current_episode_id);
             current_episode_id = 0;
         }

         if (phase11_revision && phase11_outcome_eval_window_ms > 0) {
             // Drain self-revision outcomes so the invariant `outcomes == revisions - 1` holds at shutdown.
             // Phase 11 outcome evaluation requires the revision to be at least `phase11_outcome_eval_window_ms`
             // old (pre/post window), so we poll until either all pending outcomes are written or we time out.
             const auto drain_start = std::chrono::steady_clock::now();
             const auto max_drain_ms = std::max<std::int64_t>(2000, 3LL * phase11_outcome_eval_window_ms);
             const auto drain_deadline = drain_start + std::chrono::milliseconds(max_drain_ms);

             while (std::chrono::steady_clock::now() < drain_deadline) {
                 bool progressed = false;
                 for (int i = 0; i < 1000; ++i) {
                     if (!phase11_revision->evaluatePendingOutcomes()) break;
                     progressed = true;
                 }

                 bool has_pending = false;
                 if (memdb && memdb_run_id > 0) {
                     has_pending = memdb->getLatestUnevaluatedSelfRevisionId(
                                       memdb_run_id,
                                       std::numeric_limits<std::int64_t>::max())
                                       .has_value();
                 }
                 if (!has_pending) break;

                 if (!progressed) {
                     std::this_thread::sleep_for(std::chrono::milliseconds(10));
                 }
             }
         }

         // Optional: export synapse snapshot CSV
         if (!snapshot_csv_path.empty()) {
             auto* ls = brain.getLearningSystem();
             if (!ls) {
                 std::cerr << "Warning: --snapshot-csv was provided, but LearningSystem is not initialized. Enable learning to export snapshots." << std::endl;
             } else {
                 auto snapshots = ls->getSynapseSnapshot();
                 std::ofstream ofs(snapshot_csv_path, std::ios::out | std::ios::trunc);
                 if (!ofs) {
                     std::cerr << "Error: failed to open '" << snapshot_csv_path << "' for writing" << std::endl;
                 } else {
                     ofs << "pre_neuron,post_neuron,weight\n";
                     for (const auto& s : snapshots) {
                         ofs << s.pre_neuron << "," << s.post_neuron << "," << s.weight << "\n";
                     }
                     ofs.flush();
                     std::cout << "Wrote synapse snapshot: " << snapshots.size() << " edges to '" << snapshot_csv_path << "'\n";
                 }
             }
         }

        // Save brain checkpoint if requested
        if (!save_brain_path.empty()) {
            if (brain.saveCheckpoint(save_brain_path)) {
                std::cout << "Saved brain checkpoint to '" << save_brain_path << "'\n";
            } else {
                std::cerr << "Error: failed to save brain checkpoint to '" << save_brain_path << "'\n";
            }
        }

        if (phase_a_export_set && phase_a_system) {
            std::string teachers_json = phase_a_system->exportTeacherEmbeddingsToJson();
            std::string attempts_json = phase_a_system->exportMimicryHistoryToJson();
            std::string path1 = phase_a_export_dir + "/phase_a_teacher_embeddings.json";
            std::string path2 = phase_a_export_dir + "/phase_a_mimicry_history.json";
            std::ofstream ofs1(path1, std::ios::out | std::ios::trunc);
            if (ofs1) { ofs1 << teachers_json; ofs1.flush(); }
            std::ofstream ofs2(path2, std::ios::out | std::ios::trunc);
            if (ofs2) { ofs2 << attempts_json; ofs2.flush(); }
            std::cout << "[Phase A] Exported JSON to '" << path1 << "' and '" << path2 << "'\n";
        }

         // Episode Summary (optional)
         if (summary && finished_episodes > 0) {
             const double avg_steps = static_cast<double>(sum_episode_steps) / static_cast<double>(finished_episodes);
             const double avg_return = sum_episode_return / static_cast<double>(finished_episodes);
             const double avg_time_ms = static_cast<double>(sum_episode_time_ms) / static_cast<double>(finished_episodes);
             const double success_rate = 100.0 * static_cast<double>(successful_episodes) / static_cast<double>(finished_episodes);
             std::cout << "\nEpisode Summary (" << finished_episodes << ")\n"
                       << "  Success rate: " << std::fixed << std::setprecision(1) << success_rate << "%\n"
                       << "  Avg steps:    " << std::setprecision(2) << avg_steps << "\n"
                       << "  Avg return:   " << std::setprecision(3) << avg_return << "\n"
                       << "  Avg time(ms): " << std::setprecision(1) << avg_time_ms << "\n";
         }

         // Always print learning statistics section for CI visibility
         std::cout << "\nLearning System Statistics\n";
         auto statsOpt = brain.getLearningStatistics();
         if (statsOpt.has_value()) {
             const auto &s = statsOpt.value();
             std::cout << "  Total Updates: " << s.total_updates << "\n"
                      << "  Hebbian Updates: " << s.hebbian_updates << "\n"
                      << "  STDP Updates: " << s.stdp_updates << "\n"
                      << "  Phase-4 Updates: " << s.reward_updates << "\n"
                      << "  Avg Weight Change: " << s.average_weight_change << "\n"
                      << "  Consolidation Rate: " << s.memory_consolidation_rate << "\n"
                      << "  Active Synapses: " << s.active_synapses << "\n"
                      << "  Potentiated Synapses: " << s.potentiated_synapses << "\n"
                      << "  Depressed Synapses: " << s.depressed_synapses << "\n";
         } else {
             // Learning not initialized; print zeros for consistency
             std::cout << "  Total Updates: 0\n"
                      << "  Hebbian Updates: 0\n"
                      << "  STDP Updates: 0\n"
                      << "  Phase-4 Updates: 0\n"
                      << "  Avg Weight Change: 0\n"
                      << "  Consolidation Rate: 0\n"
                      << "  Active Synapses: 0\n"
                      << "  Potentiated Synapses: 0\n"
                      << "  Depressed Synapses: 0\n";
         }

         // Clean up autonomous thread if it was started
         if (autonomous_thread.joinable()) {
             std::cout << "Waiting for autonomous thread to complete..." << std::endl;
             autonomous_thread.join();
             std::cout << "Autonomous thread completed." << std::endl;
         }

        if (g_memdb && g_memdb_run_id > 0) {
            std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            std::int64_t event_id = 0;
            (void)g_memdb->insertRunEvent(g_memdb_run_id, ts_ms, g_last_step.load(), std::string("exit"), std::string("normal"), 0, nf_process_rss_mb(), 0.0, event_id);
        }
        return 0;
     } catch (const std::exception &ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        if (g_memdb && g_memdb_run_id > 0) {
            std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            std::int64_t event_id = 0;
            (void)g_memdb->insertRunEvent(g_memdb_run_id, ts_ms, g_last_step.load(), std::string("fatal"), std::string(ex.what()), 1, nf_process_rss_mb(), 0.0, event_id);
        }
        return 1;
     } catch (...) {
        std::cerr << "Fatal error: unknown exception" << std::endl;
        if (g_memdb && g_memdb_run_id > 0) {
            std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            std::int64_t event_id = 0;
            (void)g_memdb->insertRunEvent(g_memdb_run_id, ts_ms, g_last_step.load(), std::string("fatal"), std::string("unknown"), 1, nf_process_rss_mb(), 0.0, event_id);
        }
        return 1;
     }
}
#ifdef _WIN32
#else
static void NfSignalHandler(int sig) {
    const char* t = "unknown";
    if (sig == SIGSEGV) t = "SIGSEGV";
    else if (sig == SIGABRT) t = "SIGABRT";
    else if (sig == SIGINT) t = "SIGINT";
    else if (sig == SIGTERM) t = "SIGTERM";
    g_abort.store(true);
    if (g_memdb && g_memdb_run_id > 0) {
        std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::int64_t event_id = 0;
        (void)g_memdb->insertRunEvent(g_memdb_run_id, ts_ms, g_last_step.load(), std::string("signal"), std::string(t), 0, nf_process_rss_mb(), 0.0, event_id);
    }
}
#endif

static void NfTerminateHandler() {
    try {
        if (g_memdb && g_memdb_run_id > 0) {
            std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            std::int64_t event_id = 0;
            (void)g_memdb->insertRunEvent(g_memdb_run_id, ts_ms, g_last_step.load(), std::string("fatal"), std::string("std::terminate"), 1, nf_process_rss_mb(), 0.0, event_id);
        }
    } catch (...) {}
    std::abort();
}

static void NfNewHandler() {
    try {
        if (g_memdb && g_memdb_run_id > 0) {
            std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            std::int64_t event_id = 0;
            (void)g_memdb->insertRunEvent(g_memdb_run_id, ts_ms, g_last_step.load(), std::string("fatal"), std::string("new_handler_out_of_memory"), 1, nf_process_rss_mb(), 0.0, event_id);
        }
    } catch (...) {}
    std::abort();
}

static void NfSetTerminationHandlers() {
    std::set_terminate(NfTerminateHandler);
    std::set_new_handler(NfNewHandler);
#ifndef _WIN32
    std::signal(SIGSEGV, NfSignalHandler);
    std::signal(SIGABRT, NfSignalHandler);
    std::signal(SIGINT,  NfSignalHandler);
    std::signal(SIGTERM, NfSignalHandler);
#endif
}
