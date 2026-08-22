/**
 * @file neuroforge_learn.cpp
 * @brief Production Autonomous Learning Runner - OPEN INTERNET LEARNING
 *
 * Main entry point for NeuroForge's autonomous internet learning.
 * Learns from Wikipedia, arXiv, educational sites, and more.
 * Extracts actual DOM text content from web pages.
 *
 * Usage: neuroforge_learn [db_path]
 */

#include "connectivity/ConnectivityManager.h"
#include "core/Agent2EpistemicReviewer.h"
#include "core/Curiosity/ExplorationRequest.h"
#include "core/Curiosity/ResolutionTracker.h"
#include "core/EntityResolver.h"
#include "core/EpistemicGates.h"
#include "core/HypergraphBrain.h"
#include "core/LanguageSystem.h"
#include "core/MemoryDB.h"
#include "core/ReasoningStep.h"
#include "core/ReasoningTrace.h"
#include "core/RelationGate.h"
#include "core/VerificationStack.h"
#include "core/VerificationTraceRecorder.h"
#include "language/LanguageAcquisitionLoop.h"
#include "navigation/CuriosityNavigator.h"
#include "navigation/SearchLinkFilter.h"
#include "navigation/VerificationSystem.h"
#include "perception/LivePerceptionLoop.h"
#include "sandbox/WebSandbox.h"
#include "verification/GroundingVerifier.h"
#include "visualization/VerificationGraphviz.h"
#include "visualization/VerificationJSON.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace NeuroForge;

// Global flag for graceful shutdown
static std::atomic<bool> g_running{true};

void signalHandler(int signal) {
  std::cout << "\n[Signal] Received signal " << signal
            << ", shutting down gracefully...\n";
  g_running = false;
}

std::int64_t getCurrentTimestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

class AutonomousLearningRunner {
public:
  AutonomousLearningRunner(const std::string &db_path, int max_pages,
                           int max_seconds, std::string single_url,
                           std::string ask, bool ask_every_page,
                           int verify_min_sources, std::string ask_seq,
                           bool agent2_enabled, bool simulate_resolution,
                           bool perception_debug)
      : db_path_(db_path), max_pages_(max_pages), max_seconds_(max_seconds),
        single_url_(std::move(single_url)), ask_(std::move(ask)),
        ask_every_page_(ask_every_page),
        verify_min_sources_(verify_min_sources),
        agent2_enabled_(agent2_enabled),
        simulate_resolution_(simulate_resolution),
        perception_debug_(perception_debug) {
    setAskSequence(std::move(ask_seq));
    initializeUrlSources();
  }

  void setAskSequence(std::string ask_seq) {
    ask_sequence_.clear();
    if (ask_seq.empty())
      return;

    std::istringstream iss(ask_seq);
    std::string item;
    while (std::getline(iss, item, '|')) {
      while (!item.empty() && item.front() == ' ')
        item.erase(item.begin());
      while (!item.empty() && item.back() == ' ')
        item.pop_back();
      if (!item.empty())
        ask_sequence_.push_back(item);
    }
  }

  void initializeUrlSources() {
    if (!single_url_.empty()) {
      seed_urls_.push_back(single_url_);
      if (max_pages_ > 1) {
        if (auto next = pickFollowupUrl(single_url_); next.has_value()) {
          seed_urls_.push_back(next.value());
        }
      }
      return;
    }
    // Wikipedia - Science & Technology
    seed_urls_.push_back(
        "https://en.wikipedia.org/wiki/Artificial_intelligence");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Machine_learning");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Neural_network");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Deep_learning");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Computer_science");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Algorithm");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Data_structure");

    // Wikipedia - Natural Sciences
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Physics");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Chemistry");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Biology");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Mathematics");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Astronomy");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Quantum_mechanics");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Relativity");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Evolution");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/DNA");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Cell_(biology)");

    // Wikipedia - Humanities & Social Sciences
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Philosophy");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Psychology");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/History");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Sociology");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Economics");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Political_science");

    // Wikipedia - Technology & Engineering
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Robotics");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Nanotechnology");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Biotechnology");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Renewable_energy");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Electric_vehicle");
    seed_urls_.push_back("https://en.wikipedia.org/wiki/Space_exploration");

    // Simple Wikipedia - Basic Concepts
    seed_urls_.push_back("https://simple.wikipedia.org/wiki/Science");
    seed_urls_.push_back("https://simple.wikipedia.org/wiki/Nature");
    seed_urls_.push_back("https://simple.wikipedia.org/wiki/Animal");
    seed_urls_.push_back("https://simple.wikipedia.org/wiki/Plant");
    seed_urls_.push_back("https://simple.wikipedia.org/wiki/Human");
    seed_urls_.push_back("https://simple.wikipedia.org/wiki/Earth");
    seed_urls_.push_back("https://simple.wikipedia.org/wiki/Universe");

    // arXiv - Academic Papers (abstracts are accessible)
    seed_urls_.push_back(
        "https://arxiv.org/abs/1706.03762"); // Attention Is All You Need
    seed_urls_.push_back("https://arxiv.org/abs/2005.14165"); // GPT-3
    seed_urls_.push_back("https://arxiv.org/abs/1810.04805"); // BERT
    seed_urls_.push_back("https://arxiv.org/abs/1512.03385"); // ResNet
    seed_urls_.push_back("https://arxiv.org/abs/1406.2661");  // GANs

    // Stanford Encyclopedia of Philosophy
    seed_urls_.push_back("https://plato.stanford.edu/entries/consciousness/");
    seed_urls_.push_back(
        "https://plato.stanford.edu/entries/artificial-intelligence/");
    seed_urls_.push_back("https://plato.stanford.edu/entries/ethics-ai/");

    // Britannica
    seed_urls_.push_back("https://www.britannica.com/science/physics-science");
    seed_urls_.push_back("https://www.britannica.com/science/chemistry");
    seed_urls_.push_back("https://www.britannica.com/science/biology");

    // Educational sites
    seed_urls_.push_back(
        "https://www.nature.com/subjects/science-technology-and-society");
    seed_urls_.push_back("https://www.scientificamerican.com/");

    // Shuffle for variety
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(seed_urls_.begin(), seed_urls_.end(), g);
  }

  bool initialize() {
    std::cout << "\n";
    std::cout
        << "================================================================\n";
    std::cout
        << "       NEUROFORGE AUTONOMOUS LEARNING SYSTEM                   \n";
    std::cout
        << "       Open Internet Learning - Wikipedia, arXiv & More        \n";
    std::cout << "============================================================="
                 "===\n\n";

    // Initialize MemoryDB
    std::cout << "[Init] Opening database: " << db_path_ << "\n";
    db_ = std::make_unique<Core::MemoryDB>(db_path_);
    if (!db_->open()) {
      std::cerr << "[Error] Failed to open database\n";
      return false;
    }

    // Create run
    if (!db_->beginRun(
            "{\"mode\": \"open_internet_learning\", \"version\": \"2.0\"}",
            run_id_)) {
      std::cerr << "[Error] Failed to create run\n";
      return false;
    }
    std::cout << "[Init] Created run ID: " << run_id_ << "\n";
    std::cout << "[Init] Queued " << seed_urls_.size() << " URLs to explore\n";

    // Initialize LanguageSystem
    std::cout << "[Init] Initializing Language System...\n";
    Core::LanguageSystem::Config lang_config;
    language_system_ = std::make_unique<Core::LanguageSystem>(lang_config);
    if (!language_system_->initialize()) {
      std::cerr << "[Error] Failed to initialize LanguageSystem\n";
      return false;
    }

    std::cout << "[Init] Initializing Hypergraph Brain...\n";
    connectivity_manager_ =
        std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
    brain_ = std::make_shared<NeuroForge::Core::HypergraphBrain>(
        connectivity_manager_);
    if (!brain_->initialize()) {
      std::cerr << "[Error] Failed to initialize HypergraphBrain\n";
      return false;
    }

    relation_gates_ = std::make_unique<Core::RelationGateManager>(
        brain_, language_system_.get());

    // Initialize CuriosityNavigator
    std::cout << "[Init] Initializing Curiosity Navigator...\n";
    Navigation::NavigatorConfig nav_config;
    nav_config.rate_limit_ms = 3000;
    navigator_ = std::make_unique<Navigation::CuriosityNavigator>(
        language_system_.get(), nullptr, nav_config);

    // Initialize Perception
    std::cout << "[Init] Initializing Perception Pipeline...\n";
    Perception::PerceptionConfig perc_config;
    perc_config.max_text_length = 20000;
    perc_config.max_entities_per_page = 200;
    perc_config.max_relations_per_page = 50;
    perc_config.debug_logging = perception_debug_;
    perception_ = std::make_unique<Perception::LivePerceptionLoop>(
        language_system_.get(), relation_gates_.get(), perc_config);
    if (!perception_->initialize()) {
      std::cerr << "[Error] Failed to initialize Perception\n";
      return false;
    }

    restoreEntityAliases();

    Language::AcquisitionConfig acq_config;
    acq_config.min_occurrences_to_learn = 1;
    acq_config.min_token_confidence = 0.3f;
    acq_config.enable_direct_tokenization = false;

    // Initialize GroundingVerifier
    std::cout << "[Init] Initializing Grounding Verifier...\n";
    Verification::VerifierConfig ver_config;
    ver_config.min_sources_for_verification =
        verify_min_sources_ > 0 ? verify_min_sources_ : 1;
    verifier_ = std::make_unique<Verification::GroundingVerifier>(
        language_system_.get(), relation_gates_.get(), ver_config);
    if (!verifier_->initialize()) {
      std::cerr << "[Error] Failed to initialize GroundingVerifier\n";
      return false;
    }

    // Initialize LanguageAcquisitionLoop
    std::cout << "[Init] Initializing Language Acquisition...\n";
    acquisition_ = std::make_unique<Language::LanguageAcquisitionLoop>(
        language_system_.get(), perception_.get(), navigator_.get(),
        verifier_.get(), acq_config);
    if (!acquisition_->initialize()) {
      std::cerr << "[Error] Failed to initialize LanguageAcquisitionLoop\n";
      return false;
    }

    perception_->setPageCallback(
        [this](const Perception::PageSnapshot &snapshot) {
          auto reduceSnapshot = [](Perception::PageSnapshot s) {
            if (s.entities.size() > 80)
              s.entities.resize(80);

            std::unordered_set<std::string> allowed;
            allowed.reserve(s.entities.size() * 2);
            for (const auto &e : s.entities)
              allowed.insert(e.text);

            std::vector<Perception::CandidateRelation> rels;
            rels.reserve(s.candidate_relations.size());
            for (const auto &r : s.candidate_relations) {
              if (allowed.count(r.subject.text) == 0)
                continue;
              if (allowed.count(r.object.text) == 0)
                continue;
              rels.push_back(r);
              if (rels.size() >= 30)
                break;
            }
            s.candidate_relations = std::move(rels);

            if (s.segments.size() > 12)
              s.segments.resize(12);
            for (auto &seg : s.segments) {
              if (seg.text.size() > 450)
                seg.text.resize(450);
            }

            return s;
          };

          auto reduced = reduceSnapshot(snapshot);
          {
            std::lock_guard<std::mutex> lock(snapshot_mutex_);
            last_snapshot_ = reduced;
            has_snapshot_ = true;
            recent_snapshots_.push_back(reduced);
            while (recent_snapshots_.size() > recent_snapshot_window_) {
              recent_snapshots_.pop_front();
            }
          }
          acquisition_->learnFromEntities(reduced.entities, reduced.url);
          acquisition_->learnFromRelations(reduced.candidate_relations,
                                           reduced.url);
        });

    // Initialize WebSandbox
    std::cout << "[Init] Initializing Web Sandbox...\n";
    web_sandbox_ = std::make_unique<Sandbox::WebSandbox>();
    if (!web_sandbox_->create(1200, 800,
                              "NeuroForge - Learning from the Internet")) {
      std::cerr << "[Error] WebSandbox creation failed\n";
      return false;
    }

    // Wait for WebView2 to be ready
    std::cout << "[Init] Waiting for browser to initialize...\n";
    for (int i = 0; i < 30 && g_running; ++i) {
      web_sandbox_->poll();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n[Init] All systems initialized!\n";
    std::cout << "[Init] NeuroForge will now learn from the open internet.\n";
    return true;
  }

  void restoreEntityAliases() {
    if (!db_ || !perception_)
      return;

    auto runs = db_->getRuns();
    if (runs.empty())
      return;

    std::optional<Core::MemoryDB::RunEntry> prev;
    for (const auto &r : runs) {
      if (r.id == run_id_)
        continue;
      if (!prev.has_value() || r.started_ms > prev->started_ms) {
        prev = r;
      }
    }
    if (!prev.has_value())
      return;

    std::vector<Core::EntityResolver::AliasRecord> records;
    auto events = db_->getRecentRunEvents(prev->id, 2000);

    std::optional<std::string> dump_message;
    for (const auto &e : events) {
      if (e.type == "entity_alias_dump") {
        dump_message = e.message;
        break;
      }
    }

    if (dump_message.has_value()) {
      std::istringstream iss(dump_message.value());
      std::string line;
      while (std::getline(iss, line)) {
        if (line.empty())
          continue;
        auto t1 = line.find('\t');
        if (t1 == std::string::npos)
          continue;
        auto t2 = line.find('\t', t1 + 1);
        if (t2 == std::string::npos)
          continue;

        Core::EntityResolver::AliasRecord r;
        r.alias = line.substr(0, t1);
        r.canonical = line.substr(t1 + 1, t2 - (t1 + 1));
        try {
          r.confidence = std::stof(line.substr(t2 + 1));
        } catch (...) {
          r.confidence = 0.0f;
        }
        if (!r.alias.empty() && !r.canonical.empty()) {
          records.push_back(r);
        }
      }
    } else {
      records.reserve(events.size());
      for (const auto &e : events) {
        if (e.type != "entity_alias")
          continue;
        std::string msg = e.message;

        auto t1 = msg.find('\t');
        if (t1 == std::string::npos)
          continue;
        auto t2 = msg.find('\t', t1 + 1);
        if (t2 == std::string::npos)
          continue;

        Core::EntityResolver::AliasRecord r;
        r.alias = msg.substr(0, t1);
        r.canonical = msg.substr(t1 + 1, t2 - (t1 + 1));
        try {
          r.confidence = std::stof(msg.substr(t2 + 1));
        } catch (...) {
          r.confidence = 0.0f;
        }

        if (!r.alias.empty() && !r.canonical.empty()) {
          records.push_back(r);
        }
      }
    }

    if (!records.empty()) {
      perception_->loadEntityAliases(records);
      qa_resolver_.loadAliases(records);
    }
  }

  void syncQaResolverAliases() {
    if (!perception_)
      return;
    qa_resolver_.loadAliases(perception_->dumpEntityAliases());
  }

  std::string answerQuestion(const std::string &question) {
    syncQaResolverAliases();
    last_reasoning_steps_.clear();
    last_reasoning_trace_ = std::nullopt;

    std::vector<Perception::PageSnapshot> snaps;
    {
      std::lock_guard<std::mutex> lock(snapshot_mutex_);
      if (!has_snapshot_) {
        return "I do not have a recent page snapshot yet.";
      }
      snaps.reserve(recent_snapshots_.size());
      for (auto it = recent_snapshots_.rbegin(); it != recent_snapshots_.rend();
           ++it) {
        snaps.push_back(*it);
      }
      if (snaps.empty())
        snaps.push_back(last_snapshot_);
    }

    auto normalize = [](std::string s) -> std::string {
      auto lower = [](unsigned char c) -> char {
        return static_cast<char>(
            static_cast<unsigned char>(std::tolower(static_cast<int>(c))));
      };
      for (auto &ch : s)
        ch = lower(static_cast<unsigned char>(ch));
      while (!s.empty() &&
             (s.back() == '?' || s.back() == '.' || s.back() == ' '))
        s.pop_back();
      while (!s.empty() && s.front() == ' ')
        s.erase(s.begin());
      return s;
    };

    std::string q = normalize(question);
    auto wantsUsedFor = q.find("used for") != std::string::npos;
    auto wantsIsUseful =
        (q.rfind("is ", 0) == 0 && q.find(" useful") != std::string::npos) ||
        (q.find(" is ") != std::string::npos &&
         q.find(" useful") != std::string::npos);

    auto formatSource = [](const std::string &url) -> std::string {
      auto host = [&](const std::string &u) -> std::string {
        auto p = u.find("://");
        std::size_t start = (p == std::string::npos) ? 0 : (p + 3);
        auto end = u.find('/', start);
        if (end == std::string::npos)
          end = u.size();
        return u.substr(start, end - start);
      };

      auto wikiTitle = [&](const std::string &u) -> std::string {
        auto p = u.find("/wiki/");
        if (p == std::string::npos)
          return {};
        std::string t = u.substr(p + 6);
        auto hash = t.find('#');
        if (hash != std::string::npos)
          t.resize(hash);
        auto qpos = t.find('?');
        if (qpos != std::string::npos)
          t.resize(qpos);
        return t;
      };

      std::string h = host(url);
      std::string t = wikiTitle(url);
      if (!t.empty())
        return t + " (" + h + ")";
      if (!h.empty())
        return h;
      return url;
    };

    auto stripPrefix = [&](const std::string &prefix) -> std::string {
      if (q.rfind(prefix, 0) != 0)
        return {};
      std::string rest = q.substr(prefix.size());
      while (!rest.empty() && rest.front() == ' ')
        rest.erase(rest.begin());
      return rest;
    };

    std::string term;
    if (auto t = stripPrefix("what is "); !t.empty())
      term = t;
    if (term.empty()) {
      if (auto t = stripPrefix("what does "); !t.empty()) {
        term = t;
        auto pos = term.find(" mean");
        if (pos != std::string::npos)
          term = term.substr(0, pos);
      }
    }
    if (term.empty() && wantsIsUseful) {
      auto pos_is = q.rfind("is ", 0) == 0 ? 0 : q.find(" is ");
      if (pos_is != std::string::npos) {
        std::size_t start = (pos_is == 0) ? 3 : (pos_is + 4);
        auto pos_useful = q.find(" useful", start);
        if (pos_useful != std::string::npos && pos_useful > start) {
          term = q.substr(start, pos_useful - start);
        }
      }
    }

    auto resolveFocus = [&](const std::string &raw_term) -> std::string {
      std::string t = raw_term;
      std::string norm = qa_resolver_.normalizeSurface(t, false);
      if (qa_resolver_.isPronoun(norm) || norm.rfind("it ", 0) == 0 ||
          norm.rfind("this ", 0) == 0 || norm.rfind("that ", 0) == 0 ||
          norm.rfind("these ", 0) == 0 || norm.rfind("those ", 0) == 0) {
        if (!discourse_focus_.empty())
          return discourse_focus_;
        return raw_term;
      }

      if (norm.rfind("this ", 0) == 0 || norm.rfind("that ", 0) == 0 ||
          norm.rfind("the ", 0) == 0) {
        std::string stripped = qa_resolver_.normalizeSurface(t, true);
        if (!stripped.empty() && !qa_resolver_.isPronoun(stripped)) {
          std::size_t tid = 0;
          if (language_system_ && language_system_->getTokenId(stripped, tid)) {
            return stripped;
          }
        }
        if (!discourse_focus_.empty())
          return discourse_focus_;
      }

      auto canon = qa_resolver_.canonicalize(t);
      if (!canon.empty())
        return canon;
      return raw_term;
    };

    auto equalsNorm = [&](const std::string &a, const std::string &b) -> bool {
      return normalize(a) == normalize(b);
    };

    auto containsNorm = [&](const std::string &haystack,
                            const std::string &needle) -> bool {
      std::string h = normalize(haystack);
      std::string n = normalize(needle);
      if (h.empty() || n.empty())
        return false;
      return h.find(n) != std::string::npos;
    };

    auto countSeenInWindow = [&](const std::string &canon) -> int {
      if (canon.empty())
        return 0;
      int seen = 0;
      for (const auto &s : snaps) {
        bool has = false;
        for (const auto &e : s.entities) {
          if (equalsNorm(e.text, canon)) {
            has = true;
            break;
          }
        }
        if (has)
          seen++;
      }
      return seen;
    };

    auto withProvenance = [&](std::string answer, const std::string &url,
                              std::size_t window_index,
                              const std::string &focus_canon,
                              const std::string &evidence) -> std::string {
      if (answer.empty())
        return answer;
      int seen = countSeenInWindow(focus_canon);
      std::string suffix = " (source: " + formatSource(url) +
                           " | window=" + std::to_string(window_index);
      if (!evidence.empty())
        suffix += " | evidence=" + evidence;
      suffix += ")";
      if (seen > 1)
        suffix += " (seen_in_pages=" + std::to_string(seen) + ")";
      return answer + suffix;
    };

    auto answerUsedFor = [&](const std::string &focus) -> std::string {
      if (focus.empty())
        return {};
      std::string canon = qa_resolver_.canonicalize(focus);
      if (canon.empty())
        canon = focus;

      for (std::size_t si = 0; si < snaps.size(); ++si) {
        const auto &s = snaps[si];
        for (const auto &rel : s.candidate_relations) {
          if (rel.predicate.text != "used_for")
            continue;
          if (!equalsNorm(rel.subject.text, canon))
            continue;
          if (!rel.source_sentence.empty())
            return withProvenance(rel.source_sentence, s.url, si, canon,
                                  rel.predicate.text);
          return withProvenance(rel.subject.text + " " + rel.predicate.text +
                                    " " + rel.object.text + ".",
                                s.url, si, canon, rel.predicate.text);
        }
      }

      if (relation_gates_ && language_system_) {
        std::size_t focus_id = 0;
        std::size_t rel_id = 0;
        if (language_system_->getTokenId(canon, focus_id) &&
            language_system_->getTokenId("used_for", rel_id)) {
          auto triples =
              relation_gates_->findRelationsFor(focus_id, true, false, false);
          float best = -1.0f;
          Core::RelationTriple best_t{};
          bool has = false;
          for (const auto &t : triples) {
            if (t.is_provisional)
              continue;
            if (t.relation_token_id != rel_id)
              continue;
            if (t.confidence > best) {
              best = t.confidence;
              best_t = t;
              has = true;
            }
          }
          if (has) {
            auto s_tok = language_system_->getToken(best_t.subject_token_id);
            auto r_tok = language_system_->getToken(best_t.relation_token_id);
            auto o_tok = language_system_->getToken(best_t.object_token_id);
            if (s_tok && r_tok && o_tok) {
              return s_tok->symbol + " " + r_tok->symbol + " " + o_tok->symbol +
                     ". (source: memory_graph | evidence=gate)";
            }
          }
        }
      }

      return {};
    };

    auto answerUsedForInferred = [&](const std::string &focus) -> std::string {
      std::string canon = qa_resolver_.canonicalize(focus);
      if (canon.empty())
        canon = focus;
      if (canon.empty())
        return {};

      std::vector<Core::ObservedRelation> observed;
      for (std::size_t si = 0; si < snaps.size(); ++si) {
        const auto &s = snaps[si];
        for (const auto &rel : s.candidate_relations) {
          Core::ObservedRelation o;
          o.subject = rel.subject.text;
          o.predicate = rel.predicate.text;
          o.object = rel.object.text;
          o.confidence = rel.confidence;
          o.source = s.url;
          o.window_index = static_cast<int>(si);
          observed.push_back(std::move(o));
        }
      }

      auto step = Core::ReasoningEngine::inferUsedForViaIsA(canon, observed);
      if (!step.has_value())
        return {};
      last_reasoning_steps_.push_back(step.value());

      const auto &c = step->conclusion;
      std::string sentence =
          c.subject + " " + c.predicate + " " + c.object + ".";
      return withProvenance(
          sentence, c.source,
          static_cast<std::size_t>(std::max(0, c.window_index)), canon,
          "inferred_used_for_via_is_a");
    };

    if (!term.empty()) {
      term = resolveFocus(term);
      {
        auto c = qa_resolver_.canonicalize(term);
        if (!c.empty())
          discourse_focus_ = c;
      }
      std::string focus_canon = qa_resolver_.canonicalize(term);
      if (focus_canon.empty())
        focus_canon = term;

      if (wantsUsedFor) {
        if (auto a = answerUsedFor(term); !a.empty())
          return a;
        if (auto a = answerUsedForInferred(term); !a.empty())
          return a;
      }
      if (wantsIsUseful) {
        std::vector<Core::FactNode> facts;
        for (std::size_t si = 0; si < snaps.size(); ++si) {
          const auto &s = snaps[si];
          for (const auto &rel : s.candidate_relations) {
            Core::FactNode f;
            f.subject = rel.subject.text;
            f.predicate = rel.predicate.text;
            f.object = rel.object.text;
            f.polarity =
                Core::ReasoningFirstEngine::predicatePolarity(f.predicate);
            f.evidence = rel.predicate.text;
            f.source = s.url;
            f.window_index = static_cast<int>(si);
            f.trust = Core::ReasoningFirstEngine::sourceTrustFromUrl(f.source);
            f.confidence = rel.confidence;
            f.transe_score = rel.transe_score;
            f.certainty = rel.transe_score >= 0.2f
                              ? Core::FactNode::Certainty::Confirmed
                              : Core::FactNode::Certainty::Provisional;
            facts.push_back(std::move(f));
          }
        }

        auto trace =
            Core::ReasoningFirstEngine::reasonIsUseful(focus_canon, facts);
        trace.trace_id = ++next_trace_id_;
        last_reasoning_trace_ = trace;
        if (trace.conclusion.has_value()) {
          auto c = trace.conclusion.value();
          std::string sentence =
              c.subject + " " + c.predicate + " " + c.object + ".";
          return withProvenance(sentence, c.source, 0, focus_canon,
                                "inferred_is_useful");
        }
      }

      for (std::size_t si = 0; si < snaps.size(); ++si) {
        const auto &s = snaps[si];
        for (const auto &rel : s.candidate_relations) {
          if (equalsNorm(rel.subject.text, term)) {
            if (!rel.source_sentence.empty())
              return withProvenance(rel.source_sentence, s.url, si, focus_canon,
                                    rel.predicate.text);
            return withProvenance(rel.subject.text + " " + rel.predicate.text +
                                      " " + rel.object.text + ".",
                                  s.url, si, focus_canon, rel.predicate.text);
          }
        }
      }

      float best_conf = -1.0f;
      std::string best_sentence;
      std::size_t best_si = 0;
      for (std::size_t si = 0; si < snaps.size(); ++si) {
        const auto &s = snaps[si];
        float recency_bonus = 0.01f * static_cast<float>(snaps.size() - 1 - si);
        for (const auto &rel : s.candidate_relations) {
          bool fuzzy = containsNorm(rel.subject.text, term) ||
                       containsNorm(term, rel.subject.text);
          if (!fuzzy)
            continue;
          float score = rel.confidence + recency_bonus;
          if (score > best_conf) {
            best_conf = score;
            best_sentence = !rel.source_sentence.empty()
                                ? rel.source_sentence
                                : (rel.subject.text + " " + rel.predicate.text +
                                   " " + rel.object.text + ".");
            best_si = si;
          }
        }
      }
      if (!best_sentence.empty()) {
        return withProvenance(best_sentence, snaps[best_si].url, best_si,
                              focus_canon, "relation");
      }

      for (std::size_t si = 0; si < snaps.size(); ++si) {
        const auto &s = snaps[si];
        for (const auto &rel : s.candidate_relations) {
          if (equalsNorm(rel.object.text, term)) {
            if (!rel.source_sentence.empty())
              return withProvenance(rel.source_sentence, s.url, si, focus_canon,
                                    rel.predicate.text);
          }
        }
      }

      for (std::size_t si = 0; si < snaps.size(); ++si) {
        const auto &s = snaps[si];
        if (containsNorm(s.title, term) && !s.segments.empty()) {
          std::string seg = s.segments.front().text;
          if (seg.size() > 400)
            seg.resize(400);
          return withProvenance(seg, s.url, si, focus_canon, "segment");
        }
      }

      if (relation_gates_ && language_system_) {
        std::size_t focus_id = 0;
        if (language_system_->getTokenId(term, focus_id)) {
          auto triples =
              relation_gates_->findRelationsFor(focus_id, true, true, false);
          float best = -1.0f;
          Core::RelationTriple best_t{};
          bool has = false;
          for (const auto &t : triples) {
            if (t.is_provisional)
              continue;
            if (t.confidence > best) {
              best = t.confidence;
              best_t = t;
              has = true;
            }
          }
          if (has) {
            auto s_tok = language_system_->getToken(best_t.subject_token_id);
            auto r_tok = language_system_->getToken(best_t.relation_token_id);
            auto o_tok = language_system_->getToken(best_t.object_token_id);
            if (s_tok && r_tok && o_tok) {
              return s_tok->symbol + " " + r_tok->symbol + " " + o_tok->symbol +
                     ". (source: memory_graph | evidence=gate)";
            }
          }
        }
      }

      return "I did not find a clear relation for \"" + term +
             "\" in the recent pages.";
    }

    auto pos = q.find(" related to ");
    if (pos != std::string::npos) {
      std::string a = q.substr(0, pos);
      std::string b = q.substr(pos + std::string(" related to ").size());
      for (std::size_t si = 0; si < snaps.size(); ++si) {
        const auto &s = snaps[si];
        for (const auto &rel : s.candidate_relations) {
          bool direct =
              equalsNorm(rel.subject.text, a) && equalsNorm(rel.object.text, b);
          bool reverse =
              equalsNorm(rel.subject.text, b) && equalsNorm(rel.object.text, a);
          if (direct || reverse) {
            std::string focus = qa_resolver_.canonicalize(a);
            if (focus.empty())
              focus = a;
            if (!rel.source_sentence.empty())
              return withProvenance(rel.source_sentence, s.url, si, focus,
                                    rel.predicate.text);
            return withProvenance(rel.subject.text + " " + rel.predicate.text +
                                      " " + rel.object.text + ".",
                                  s.url, si, focus, rel.predicate.text);
          }
        }
      }
      return "I did not find a direct relation between those in the recent "
             "pages.";
    }

    if (!discourse_focus_.empty() && wantsUsedFor) {
      if (auto a = answerUsedFor(discourse_focus_); !a.empty())
        return a;
    }

    for (const auto &s : snaps) {
      if (!s.title.empty()) {
        return "I can answer about recent pages (latest: " + s.title +
               "). Try asking: \"What is <term>?\"";
      }
    }
    return "Ask a question like: \"What is <term>?\" or \"<A> related to "
           "<B>?\"";
  }

  void run() {
    std::cout << "\n[Learning] Starting autonomous learning loop...\n";
    std::cout << "[Learning] Press Ctrl+C to stop and save all knowledge\n\n";

    auto start_time = std::chrono::steady_clock::now();
    auto last_persist_time = start_time;
    auto last_status_time = start_time;

    // Build URL queue from seeds
    for (const auto &url : seed_urls_) {
      url_queue_.push(url);
    }

    acquisition_->start();

    std::size_t total_chars_learned = 0;

    while (g_running) {
      // Poll WebSandbox for window messages
      if (web_sandbox_) {
        web_sandbox_->poll();
      }

      auto now = std::chrono::steady_clock::now();
      if (max_seconds_ > 0) {
        auto elapsed_s =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
                .count();
        if (elapsed_s >= max_seconds_) {
          g_running = false;
          break;
        }
      }

      if (max_pages_ > 0) {
        auto st = acquisition_->getStatistics();
        if (st.pages_processed >= max_pages_) {
          g_running = false;
          break;
        }
      }

      // Get next URL to explore
      std::string current_url;
      if (!url_queue_.empty()) {
        current_url = url_queue_.front();
        url_queue_.pop();
      }

      if (!current_url.empty() && web_sandbox_ && web_sandbox_->isOpen()) {
        std::cout << "\n[Explore] " << current_url << "\n";

        // Navigate to URL
        if (web_sandbox_->navigate(current_url)) {
          // Wait for page to load with polling
          for (int i = 0; i < 80 && g_running; ++i) {
            web_sandbox_->poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
          }

          if (web_sandbox_->waitUntilReady(8000)) {
            // Extract real page content using DOM
            if (web_sandbox_->extractPageContent()) {
              // Wait for async extraction
              for (int i = 0;
                   i < 50 && g_running && !web_sandbox_->isExtractionComplete();
                   ++i) {
                web_sandbox_->poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
              }

              std::string content = web_sandbox_->getPageText();
              std::string title = web_sandbox_->getPageTitle();

              if (!content.empty()) {
                total_chars_learned += content.size();
                std::cout << "  [Title] " << title << "\n";
                std::cout << "  [Extracted] " << content.size()
                          << " characters\n";

                acquisition_->processWebContent(content, current_url);
                navigator_->recordPageVisit(current_url, {});

                for (int i = 0;
                     i < 200 && g_running && perception_->getPendingCount() > 0;
                     ++i) {
                  perception_->update(0.05f);
                  acquisition_->verifyPendingKnowledge();
                  std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }

                auto stats = acquisition_->getStatistics();
                std::cout << "  [Tokens=" << stats.tokens_learned
                          << " | Vocab=" << stats.vocabulary_size
                          << " | Relations=" << stats.relations_formed
                          << " | Pages=" << stats.pages_processed << "]\n";
                if (perception_debug_) {
                  auto p = perception_->getStatistics();
                  auto perc_config = perception_->getConfig();
                  std::cout
                      << "  [Perception] raw_sentences=" << p.raw_sentences
                      << " normalized=" << p.normalized_sentences
                      << " parsed=" << p.parsed_sentences
                      << " candidates=" << p.relation_candidates_generated
                      << " accepted=" << p.relations_created << "\n";
                  std::cout
                      << "  [Perception] rejected_empty_sentence="
                      << p.rejected_empty_sentence
                      << " rejected_unlinked_entity="
                      << p.rejected_unlinked_entity
                      << " rejected_low_confidence="
                      << p.rejected_low_confidence
                      << " rejected_low_transe=" << p.rejected_low_transe_score
                      << "\n";
                  // Phase 14: TransE histogram instrumentation
                  if (p.transe_scored > 0) {
                    float transe_mean =
                        p.transe_sum / static_cast<float>(p.transe_scored);
                    std::cout
                        << "  [Perception] transe[min=" << std::fixed
                        << std::setprecision(2) << p.transe_min
                        << " max=" << p.transe_max << " mean=" << transe_mean
                        << " threshold_prov="
                        << perc_config.min_transe_score_provisional
                        << " threshold_conf=" << perc_config.min_transe_score
                        << "]\n";
                  }
                  // Phase 14: Provisional vs Confirmed acceptance
                  std::cout << "  [Perception] accepted_provisional="
                            << p.accepted_provisional
                            << " accepted_confirmed=" << p.accepted_confirmed
                            << "\n";

                  // Phase 17: Epistemic Readiness Gates
                  Core::EpistemicGates gates;
                  Core::ReadinessReport readiness = gates.checkReadiness(
                      static_cast<int>(
                          p.accepted_provisional), // provisional count
                      static_cast<int>(p.accepted_provisional +
                                       p.accepted_confirmed), // accepted
                      static_cast<int>(p.rejected_low_transe_score +
                                       p.rejected_low_confidence), // rejected
                      false // TODO: check for unresolved contradictions
                  );

                  if (readiness.is_ready) {
                    std::cout
                        << "  [Epistemic] System is READY for verification "
                        << "(prov=" << readiness.provisional_count
                        << " s/n=" << std::fixed << std::setprecision(2)
                        << readiness.signal_to_noise << ")\n";

                    // --- PHASE 20: MULTI-STEP VERIFIED EXPLORATION ---
                    // Stack-based bounded recursive verification
                    static Core::VerificationStack vstack;
                    static Core::VerificationTraceRecorder trace_recorder;
                    static int verified_this_session = 0;
                    constexpr int MAX_VERIFICATIONS_PER_SESSION = 3;

                    // Only push initial goal if stack is empty and we haven't
                    // hit limit
                    if (vstack.empty() &&
                        verified_this_session < MAX_VERIFICATIONS_PER_SESSION) {
                      auto all_relations = relation_gates_->getAllRelations();
                      auto maybe_goal = Navigation::VerificationSelector::
                          selectForVerification(all_relations);

                      if (maybe_goal.has_value()) {
                        auto &goal = maybe_goal.value();
                        auto root = vstack.createRootFrame(
                            goal.subject, goal.predicate, goal.object);
                        vstack.push(root);

                        // Phase 20a: Record trace node
                        Core::VerificationTraceNode trace_node;
                        trace_node.frame_id = root.frame_id;
                        trace_node.parent_frame_id = -1;
                        trace_node.subject = goal.subject;
                        trace_node.predicate = goal.predicate;
                        trace_node.object = goal.object;
                        trace_node.depth = 0;
                        trace_node.status = Core::TraceStatus::PENDING;
                        trace_node.roi_score = goal.score;
                        trace_recorder.recordNode(trace_node);

                        std::cout << "  [Phase20b] Queued: " << goal.subject
                                  << " " << goal.predicate << " " << goal.object
                                  << " score=" << std::fixed
                                  << std::setprecision(2) << goal.score << "\n";
                      }
                    }

                    // Process verification stack (bounded loop)
                    if (!vstack.empty()) {
                      auto frame = vstack.pop();

                      // Safety: check depth limit
                      if (frame.depth >= vstack.getMaxDepth()) {
                        std::cout << "  [Phase20] Depth limit reached ("
                                  << frame.depth << "), abandoning\n";
                        frame.markAbandoned();
                      } else {
                        std::cout << "  [Phase20] Processing frame "
                                  << frame.frame_id << " depth=" << frame.depth
                                  << "\n";

                        // Verification sources (real, stable)
                        std::vector<std::string> verification_sources = {
                            "https://www.ibm.com/topics/machine-learning",
                            "https://www.britannica.com/technology/"
                            "machine-learning",
                            "https://www.nature.com/articles/"
                            "s42256-019-0088-2"};

                        // Get existing domain classes from the fact
                        std::set<Core::DomainClass> existing_classes;
                        auto all_rels = relation_gates_->getAllRelations();
                        for (const auto &rel : all_rels) {
                          if (rel.is_provisional) {
                            existing_classes.insert(rel.domain_classes.begin(),
                                                    rel.domain_classes.end());
                            break;
                          }
                        }

                        std::string target_url = Navigation::SearchLinkFilter::
                            selectHighTrustCandidate(verification_sources,
                                                     existing_classes);

                        if (!target_url.empty()) {
                          std::cout << "  [Phase20] INDEPENDENT SOURCE: "
                                    << target_url << "\n";
                          frame.recordAttempt(target_url);
                          url_queue_.push(target_url);
                          verified_this_session++;
                          std::cout << "  [Phase20] Queued for fetch ("
                                    << verified_this_session << "/"
                                    << MAX_VERIFICATIONS_PER_SESSION << ")\n";
                        } else {
                          std::cout << "  [Phase20] No independent source, "
                                       "abandoning\n";
                          frame.markAbandoned();
                        }
                      }
                    } else if (verified_this_session > 0) {
                      std::cout
                          << "  [Phase20] Stack empty, verification complete\n";
                    }
                  } else {
                    std::cout << "  [Epistemic] Verification BLOCKED: "
                              << readiness.denial_reason << "\n";
                  }
                }

                if (ask_every_page_) {
                  if (!ask_sequence_.empty()) {
                    for (const auto &to_ask : ask_sequence_) {
                      if (to_ask.empty())
                        continue;
                      std::cout << "  [Q] " << to_ask << "\n";
                      auto ans = answerQuestion(to_ask);
                      std::cout << "  [A] " << ans << "\n";
                      if (!last_reasoning_steps_.empty()) {
                        for (const auto &step : last_reasoning_steps_) {
                          std::cout << "  [S] rule=" << step.rule_id
                                    << " conf=" << step.derived_confidence
                                    << " concl=" << step.conclusion.subject
                                    << " " << step.conclusion.predicate << " "
                                    << step.conclusion.object << "\n";
                          for (std::size_t pi = 0; pi < step.premises.size();
                               ++pi) {
                            const auto &p = step.premises[pi];
                            std::cout << "    premise[" << pi << "] "
                                      << p.subject << " " << p.predicate << " "
                                      << p.object << " (source=" << p.source
                                      << " | window=" << p.window_index
                                      << " | conf=" << p.confidence << ")\n";
                          }
                        }
                      }
                      if (last_reasoning_trace_.has_value()) {
                        const auto &t = last_reasoning_trace_.value();
                        if (t.conclusion.has_value()) {
                          const auto &c = t.conclusion.value();
                          std::cout << "  [T] conf=" << t.aggregate_confidence
                                    << " concl=" << c.subject << " "
                                    << c.predicate << " " << c.object << "\n";
                        } else {
                          std::cout << "  [T] conf=" << t.aggregate_confidence
                                    << " no_conclusion\n";
                        }
                        for (std::size_t pi = 0; pi < t.premises.size(); ++pi) {
                          const auto &p = t.premises[pi];
                          std::cout << "    fact[" << pi << "] " << p.subject
                                    << " " << p.predicate << " " << p.object
                                    << " (source=" << p.source
                                    << " | window=" << p.window_index
                                    << " | evidence=" << p.evidence
                                    << " | conf=" << p.confidence
                                    << " | trust=" << p.trust << ")\n";
                        }
                        for (std::size_t ci = 0; ci < t.counter_evidence.size();
                             ++ci) {
                          const auto &p = t.counter_evidence[ci];
                          std::cout << "    counter[" << ci << "] " << p.subject
                                    << " " << p.predicate << " " << p.object
                                    << " (source=" << p.source
                                    << " | window=" << p.window_index
                                    << " | evidence=" << p.evidence
                                    << " | conf=" << p.confidence
                                    << " | trust=" << p.trust << ")\n";
                        }
                        for (std::size_t mi = 0; mi < t.missing_premises.size();
                             ++mi) {
                          const auto &m = t.missing_premises[mi];
                          std::cout << "    missing[" << mi << "] " << m.subject
                                    << " " << m.predicate << " "
                                    << m.object_hint << "\n";
                        }
                        submitExplorationRequestsFromTrace(t, discourse_focus_);
                      }
                      if (agent2_enabled_) {
                        auto review =
                            last_reasoning_trace_.has_value()
                                ? agent2_.review({to_ask, ans},
                                                 last_reasoning_trace_.value())
                                : (!last_reasoning_steps_.empty()
                                       ? agent2_.review({to_ask, ans},
                                                        last_reasoning_steps_)
                                       : agent2_.review({to_ask, ans}));
                        if (!review.findings.empty()) {
                          std::cout << "  [R] ";
                          for (std::size_t i = 0; i < review.findings.size();
                               ++i) {
                            if (i)
                              std::cout << "; ";
                            std::cout << review.findings[i].code;
                            if (!review.findings[i].suggestion.empty()) {
                              std::cout << " -> "
                                        << review.findings[i].suggestion;
                            }
                          }
                          std::cout << "\n";
                        }
                      }
                      last_reasoning_steps_.clear();
                      last_reasoning_trace_ = std::nullopt;
                    }
                  } else if (!ask_.empty()) {
                    std::cout << "  [Q] " << ask_ << "\n";
                    auto ans = answerQuestion(ask_);
                    std::cout << "  [A] " << ans << "\n";
                    if (!last_reasoning_steps_.empty()) {
                      for (const auto &step : last_reasoning_steps_) {
                        std::cout << "  [S] rule=" << step.rule_id
                                  << " conf=" << step.derived_confidence
                                  << " concl=" << step.conclusion.subject << " "
                                  << step.conclusion.predicate << " "
                                  << step.conclusion.object << "\n";
                        for (std::size_t pi = 0; pi < step.premises.size();
                             ++pi) {
                          const auto &p = step.premises[pi];
                          std::cout << "    premise[" << pi << "] " << p.subject
                                    << " " << p.predicate << " " << p.object
                                    << " (source=" << p.source
                                    << " | window=" << p.window_index
                                    << " | conf=" << p.confidence << ")\n";
                        }
                      }
                    }
                    if (last_reasoning_trace_.has_value()) {
                      const auto &t = last_reasoning_trace_.value();
                      if (t.conclusion.has_value()) {
                        const auto &c = t.conclusion.value();
                        std::cout << "  [T] conf=" << t.aggregate_confidence
                                  << " concl=" << c.subject << " "
                                  << c.predicate << " " << c.object << "\n";
                      } else {
                        std::cout << "  [T] conf=" << t.aggregate_confidence
                                  << " no_conclusion\n";
                      }
                      for (std::size_t pi = 0; pi < t.premises.size(); ++pi) {
                        const auto &p = t.premises[pi];
                        std::cout << "    fact[" << pi << "] " << p.subject
                                  << " " << p.predicate << " " << p.object
                                  << " (source=" << p.source
                                  << " | window=" << p.window_index
                                  << " | evidence=" << p.evidence
                                  << " | conf=" << p.confidence
                                  << " | trust=" << p.trust << ")\n";
                      }
                      for (std::size_t ci = 0; ci < t.counter_evidence.size();
                           ++ci) {
                        const auto &p = t.counter_evidence[ci];
                        std::cout << "    counter[" << ci << "] " << p.subject
                                  << " " << p.predicate << " " << p.object
                                  << " (source=" << p.source
                                  << " | window=" << p.window_index
                                  << " | evidence=" << p.evidence
                                  << " | conf=" << p.confidence
                                  << " | trust=" << p.trust << ")\n";
                      }
                      for (std::size_t mi = 0; mi < t.missing_premises.size();
                           ++mi) {
                        const auto &m = t.missing_premises[mi];
                        std::cout << "    missing[" << mi << "] " << m.subject
                                  << " " << m.predicate << " " << m.object_hint
                                  << "\n";
                      }
                      submitExplorationRequestsFromTrace(t, discourse_focus_);
                    }
                    if (agent2_enabled_) {
                      auto review =
                          last_reasoning_trace_.has_value()
                              ? agent2_.review({ask_, ans},
                                               last_reasoning_trace_.value())
                              : (!last_reasoning_steps_.empty()
                                     ? agent2_.review({ask_, ans},
                                                      last_reasoning_steps_)
                                     : agent2_.review({ask_, ans}));
                      if (!review.findings.empty()) {
                        std::cout << "  [R] ";
                        for (std::size_t i = 0; i < review.findings.size();
                             ++i) {
                          if (i)
                            std::cout << "; ";
                          std::cout << review.findings[i].code;
                          if (!review.findings[i].suggestion.empty()) {
                            std::cout << " -> "
                                      << review.findings[i].suggestion;
                          }
                        }
                        std::cout << "\n";
                      }
                    }
                    last_reasoning_steps_.clear();
                    last_reasoning_trace_ = std::nullopt;
                  }
                }
              }
            }
          }
        }

        // Rate limiting - be respectful to servers
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));

      } else if (url_queue_.empty()) {
        // No more URLs - generate more based on learned concepts
        generateMoreUrls();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      // Periodic status update
      auto since_status = std::chrono::duration_cast<std::chrono::seconds>(
                              now - last_status_time)
                              .count();
      if (since_status >= 30) {
        auto stats = acquisition_->getStatistics();
        auto elapsed_min =
            std::chrono::duration_cast<std::chrono::minutes>(now - start_time)
                .count();
        std::cout << "\n=== Status: " << elapsed_min
                  << "m | Tokens=" << stats.tokens_learned
                  << " | Vocab=" << stats.vocabulary_size
                  << " | Pages=" << stats.pages_processed
                  << " | Queue=" << url_queue_.size() << " ===\n";
        last_status_time = now;
      }

      // Periodic persistence (every 60 seconds)
      auto since_persist = std::chrono::duration_cast<std::chrono::seconds>(
                               now - last_persist_time)
                               .count();
      if (since_persist >= 60) {
        persistKnowledge();
        last_persist_time = now;
      }
    }

    acquisition_->stop();

    // Final persistence
    std::cout << "\n[Shutdown] Saving final state...\n";
    persistKnowledge();

    // Print final stats
    printFinalStats(start_time, total_chars_learned);
  }

  void shutdown() {
    std::cout << "[Shutdown] Closing database...\n";
    if (db_) {
      db_->close();
    }
    std::cout << "[Shutdown] Complete.\n";
  }

private:
  void markOutcomeStub(const Core::Curiosity::CuriosityGoal &goal) {
    resolution_tracker_.markOutcome(goal, false);
  }

  void submitExplorationRequestsFromTrace(const Core::ReasoningTrace &trace,
                                          const std::string &focus_subject) {
    if (!navigator_)
      return;

    using Auth = Core::Curiosity::ExplorationAuthorization;
    Auth auth = Auth::None;
    float priority = 0.0f;

    auto authToString = [&](Auth a) {
      switch (a) {
      case Auth::None:
        return std::string("NONE");
      case Auth::Diagnostic:
        return std::string("DIAGNOSTIC");
      case Auth::ContradictionResolution:
        return std::string("RESOLUTION");
      }
      return std::string("NONE");
    };

    auto simulateOnce = [&]() {
      if (!navigator_->hasPendingRequests())
        return;
      auto req = navigator_->getNextRequest();
      if (!req.isValid())
        return;
      resolution_tracker_.recordAttempt(req.goal);
      std::string query = navigator_->generateQueryFromGoal(req.goal);
      std::cout << "[Active] Authorized execution (SIMULATED): "
                << "\"" << query << "\""
                << " | Reason: " << authToString(req.authorization)
                << " | TraceID: " << req.goal.originating_trace_id << "\n";
      if (simulate_resolution_) {
        Core::FactNode fake;
        fake.subject = req.goal.target_subject;
        fake.predicate = req.goal.target_predicate;
        fake.certainty = Core::FactNode::Certainty::Confirmed;
        if (req.goal.target_predicate == "used_for") {
          fake.object = "automation";
        } else if (req.goal.target_predicate == "is" &&
                   req.goal.object_hint == "useful") {
          fake.object = "useful";
        } else if (!req.goal.object_hint.empty() &&
                   req.goal.object_hint != "<something>") {
          fake.object = req.goal.object_hint;
        }

        if (!fake.object.empty() &&
            resolution_tracker_.attemptResolution(fake)) {
          std::cout << "[Curiosity] RESOLVED: Goal satisfied by fact ("
                    << fake.subject << " " << fake.predicate << " "
                    << fake.object << ")\n";
        }
      }
    };

    if (trace.failure_code == Core::ReasoningFailure::PremiseGap) {
      auth = Auth::Diagnostic;
      priority = 1.0f;
      for (const auto &m : trace.missing_premises) {
        Core::Curiosity::ExplorationRequest req;
        req.authorization = auth;
        req.goal.target_subject = m.subject;
        req.goal.target_predicate = m.predicate;
        req.goal.object_hint = m.object_hint;
        req.goal.priority = priority;
        req.goal.originating_trace_id = trace.trace_id;
        if (resolution_tracker_.isExplorationAllowed(req.goal)) {
          navigator_->submitRequest(req);
          simulateOnce();
        }
      }
      return;
    }

    if (trace.failure_code == Core::ReasoningFailure::LogicalContradiction) {
      auth = Auth::ContradictionResolution;
      priority = 1.0f;
      Core::Curiosity::ExplorationRequest req;
      req.authorization = auth;
      req.goal.target_subject =
          focus_subject.empty() ? "unknown" : focus_subject;
      req.goal.target_predicate = "is";
      req.goal.object_hint = "useful";
      req.goal.priority = priority;
      req.goal.originating_trace_id = trace.trace_id;
      if (resolution_tracker_.isExplorationAllowed(req.goal)) {
        navigator_->submitRequest(req);
        simulateOnce();
      }
    }
  }
  static std::optional<std::string> pickFollowupUrl(const std::string &url) {
    auto startsWith = [](const std::string &s, const std::string &p) {
      return s.rfind(p, 0) == 0;
    };

    if (startsWith(url, "https://en.wikipedia.org/wiki/Machine_learning")) {
      return "https://en.wikipedia.org/wiki/Supervised_learning";
    }
    if (startsWith(url,
                   "https://en.wikipedia.org/wiki/Artificial_intelligence")) {
      return "https://en.wikipedia.org/wiki/Machine_learning";
    }
    return std::nullopt;
  }

  void generateMoreUrls() {
    // Generate Wikipedia URLs from learned vocabulary
    auto vocab = acquisition_->getLearnedVocabulary();

    // Pick high-confidence tokens as potential Wikipedia topics
    for (const auto &token : vocab) {
      float conf = acquisition_->getTokenConfidence(token);
      if (conf > 0.5f && token.length() > 4) {
        // Capitalize first letter for Wikipedia
        std::string topic = token;
        if (!topic.empty()) {
          topic[0] = static_cast<char>(
              std::toupper(static_cast<unsigned char>(topic[0])));
        }

        // Add if not already queued
        std::string url = "https://en.wikipedia.org/wiki/" + topic;
        if (explored_urls_.find(url) == explored_urls_.end()) {
          url_queue_.push(url);
          explored_urls_.insert(url);
          std::cout << "[Navigator] Discovered: " << topic << "\n";
          if (url_queue_.size() >= 5)
            break; // Don't overwhelm
        }
      }
    }

    // Add more diverse URLs if queue still empty
    if (url_queue_.empty()) {
      static std::vector<std::string> backup_urls = {
          "https://en.wikipedia.org/wiki/Cognition",
          "https://en.wikipedia.org/wiki/Neuroscience",
          "https://en.wikipedia.org/wiki/Learning",
          "https://en.wikipedia.org/wiki/Memory",
          "https://en.wikipedia.org/wiki/Language",
          "https://en.wikipedia.org/wiki/Intelligence",
          "https://en.wikipedia.org/wiki/Consciousness",
          "https://en.wikipedia.org/wiki/Knowledge",
          "https://en.wikipedia.org/wiki/Information",
          "https://en.wikipedia.org/wiki/Complex_system",
          "https://en.wikipedia.org/wiki/Emergence",
          "https://en.wikipedia.org/wiki/Self-organization"};
      static size_t backup_idx = 0;

      if (backup_idx < backup_urls.size()) {
        url_queue_.push(backup_urls[backup_idx++]);
      }
    }
  }

  void persistKnowledge() {
    auto escapeJson = [](const std::string &s) -> std::string {
      std::string out;
      out.reserve(s.size());
      for (char c : s) {
        switch (c) {
        case '\\':
          out += "\\\\";
          break;
        case '"':
          out += "\\\"";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          out.push_back(c);
          break;
        }
      }
      return out;
    };

    auto vocab = acquisition_->getLearnedVocabulary();
    auto ts = getCurrentTimestamp();
    auto stats = acquisition_->getStatistics();

    std::cout << "[Persist] Saving " << vocab.size() << " tokens...\n";

    std::int64_t out_id;
    int saved = 0;

    for (const auto &token : vocab) {
      float confidence = acquisition_->getTokenConfidence(token);
      std::string state_json =
          "{\"confidence\": " + std::to_string(confidence) + "}";

      if (db_->insertLanguageGroundingMap(run_id_, ts, 0, 5, token, state_json,
                                          confidence, std::nullopt,
                                          "open_internet", out_id)) {
        saved++;
      }
    }

    // Log event
    std::string event_msg =
        "Tokens: " + std::to_string(stats.tokens_learned) +
        ", Pages: " + std::to_string(stats.pages_processed) +
        ", Vocab: " + std::to_string(vocab.size());
    db_->insertRunEvent(run_id_, ts, 0, "checkpoint", event_msg, 0, 0.0, 0.0,
                        out_id);

    if (verifier_) {
      auto verified = verifier_->getHypothesesByState(
          Verification::RelationHypothesis::State::Verified);
      int emitted = 0;
      for (const auto &hyp : verified) {
        if (emitted >= 200)
          break;
        std::string json =
            "{\"subject\":\"" + escapeJson(hyp.subject_text) +
            "\",\"predicate\":\"" + escapeJson(hyp.relation_text) +
            "\",\"object\":\"" + escapeJson(hyp.object_text) +
            "\",\"score\":" + std::to_string(hyp.verification_score) + "}";
        db_->insertRunEvent(run_id_, ts, 0, "verified_relation", json, 0, 0.0,
                            0.0, out_id);
        emitted++;
      }
    }

    if (perception_) {
      auto aliases = perception_->dumpEntityAliases();
      if (!aliases.empty()) {
        std::sort(aliases.begin(), aliases.end(),
                  [](const auto &a, const auto &b) {
                    return a.confidence > b.confidence;
                  });

        std::string dump;
        dump.reserve(aliases.size() * 24);
        int emitted = 0;
        for (const auto &r : aliases) {
          if (emitted >= 5000)
            break;
          if (r.alias.empty() || r.canonical.empty())
            continue;
          dump += r.alias;
          dump += "\t";
          dump += r.canonical;
          dump += "\t";
          dump += std::to_string(static_cast<double>(r.confidence));
          dump += "\n";
          emitted++;
        }
        db_->insertRunEvent(run_id_, ts, 0, "entity_alias_dump", dump, 0, 0.0,
                            0.0, out_id);
      }
    }

    std::cout << "[Persist] Saved " << saved << " tokens.\n";
  }

  void printFinalStats(std::chrono::steady_clock::time_point start,
                       std::size_t total_chars) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto minutes =
        std::chrono::duration_cast<std::chrono::minutes>(elapsed).count();
    auto stats = acquisition_->getStatistics();
    auto vocab = acquisition_->getLearnedVocabulary();

    std::cout << "\n";
    std::cout
        << "================================================================\n";
    std::cout
        << "            OPEN INTERNET LEARNING SESSION COMPLETE            \n";
    std::cout
        << "================================================================\n";
    std::cout << "  Duration:             " << minutes << " minutes\n";
    std::cout << "  Pages Processed:      " << stats.pages_processed << "\n";
    std::cout << "  Characters Read:      " << total_chars << "\n";
    std::cout << "  Tokens Learned:       " << stats.tokens_learned << "\n";
    std::cout << "  Vocabulary Size:      " << vocab.size() << "\n";
    std::cout << "  Relations Formed:     " << stats.relations_formed << "\n";
    std::cout << "  Avg Confidence:       " << std::fixed
              << std::setprecision(2) << stats.avg_token_confidence << "\n";
    std::cout << "  Database:             " << db_path_ << "\n";
    std::cout
        << "================================================================\n";
    std::cout << "\n  NeuroForge has learned from the open internet!\n";
    std::cout << "  Knowledge persisted to: " << db_path_ << "\n\n";
  }

  std::string db_path_;
  std::vector<std::string> seed_urls_;
  std::queue<std::string> url_queue_;
  std::set<std::string> explored_urls_;
  std::unique_ptr<Core::MemoryDB> db_;
  std::unique_ptr<Core::LanguageSystem> language_system_;
  std::shared_ptr<Connectivity::ConnectivityManager> connectivity_manager_;
  std::shared_ptr<Core::HypergraphBrain> brain_;
  std::unique_ptr<Core::RelationGateManager> relation_gates_;
  std::unique_ptr<Navigation::CuriosityNavigator> navigator_;
  std::unique_ptr<Perception::LivePerceptionLoop> perception_;
  std::unique_ptr<Language::LanguageAcquisitionLoop> acquisition_;
  std::unique_ptr<Verification::GroundingVerifier> verifier_;
  std::unique_ptr<Sandbox::WebSandbox> web_sandbox_;
  std::mutex snapshot_mutex_;
  Perception::PageSnapshot last_snapshot_;
  bool has_snapshot_ = false;
  std::deque<Perception::PageSnapshot> recent_snapshots_;
  std::size_t recent_snapshot_window_ = 6;
  std::int64_t run_id_ = 0;
  int max_pages_ = 0;
  int max_seconds_ = 0;
  std::string single_url_;
  std::string ask_;
  std::vector<std::string> ask_sequence_;
  bool ask_every_page_ = false;
  int verify_min_sources_ = 1;
  bool agent2_enabled_ = true;
  bool simulate_resolution_ = false;
  bool perception_debug_ = false;
  Core::Agent2EpistemicReviewer agent2_;
  std::vector<Core::ReasoningStep> last_reasoning_steps_;
  std::optional<Core::ReasoningTrace> last_reasoning_trace_;
  int next_trace_id_ = 0;
  Core::Curiosity::ResolutionTracker resolution_tracker_;
  Core::EntityResolver qa_resolver_;
  std::string discourse_focus_;
  std::set<std::string> persisted_alias_keys_;
};

int main(int argc, char *argv[]) {
  // Set up signal handler for graceful shutdown
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  std::string db_path = "neuroforge_knowledge.db";
  int max_pages = 0;
  int max_seconds = 0;
  std::string single_url;
  std::string ask;
  std::string ask_seq;
  bool ask_every_page = true;
  int verify_min_sources = 1;
  bool agent2_enabled = true;
  bool simulate_resolution = false;
  bool perception_debug = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i] ? std::string(argv[i]) : std::string();
    if (arg.rfind("--db=", 0) == 0) {
      db_path = arg.substr(std::string("--db=").size());
    } else if (arg.rfind("--max-pages=", 0) == 0) {
      max_pages = std::stoi(arg.substr(std::string("--max-pages=").size()));
    } else if (arg.rfind("--max-seconds=", 0) == 0) {
      max_seconds = std::stoi(arg.substr(std::string("--max-seconds=").size()));
    } else if (arg.rfind("--single-url=", 0) == 0) {
      single_url = arg.substr(std::string("--single-url=").size());
    } else if (arg.rfind("--ask=", 0) == 0) {
      ask = arg.substr(std::string("--ask=").size());
    } else if (arg.rfind("--ask-seq=", 0) == 0) {
      ask_seq = arg.substr(std::string("--ask-seq=").size());
    } else if (arg.rfind("--ask-every-page=", 0) == 0) {
      ask_every_page =
          std::stoi(arg.substr(std::string("--ask-every-page=").size())) != 0;
    } else if (arg.rfind("--verify-min-sources=", 0) == 0) {
      verify_min_sources =
          std::stoi(arg.substr(std::string("--verify-min-sources=").size()));
    } else if (arg.rfind("--agent2=", 0) == 0) {
      agent2_enabled =
          std::stoi(arg.substr(std::string("--agent2=").size())) != 0;
    } else if (arg.rfind("--simulate-resolution=", 0) == 0) {
      simulate_resolution =
          std::stoi(arg.substr(std::string("--simulate-resolution=").size())) !=
          0;
    } else if (arg.rfind("--perception-debug=", 0) == 0) {
      perception_debug =
          std::stoi(arg.substr(std::string("--perception-debug=").size())) != 0;
    } else if (!arg.empty() && arg[0] != '-') {
      db_path = arg;
    }
  }

  AutonomousLearningRunner runner(db_path, max_pages, max_seconds, single_url,
                                  ask, ask_every_page, verify_min_sources,
                                  ask_seq, agent2_enabled, simulate_resolution,
                                  perception_debug);

  if (!runner.initialize()) {
    std::cerr << "[Error] Initialization failed\n";
    return 1;
  }

  runner.run();
  runner.shutdown();

  return 0;
}
