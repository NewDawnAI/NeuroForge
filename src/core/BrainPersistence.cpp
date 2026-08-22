#include "core/BrainPersistence.h"
#include "core/LanguageSystem.h"
#include "memory/EpisodicMemoryManager.h"
#include "memory/ProceduralMemory.h"
#include "memory/SemanticMemory.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace NeuroForge {
namespace Core {

// ────────────────────────────────────────────────────────────────
//  JSON Helpers (minimal, no external dependency)
// ────────────────────────────────────────────────────────────────

std::string BrainPersistence::vectorToJson(const std::vector<float> &vec) {
  std::ostringstream ss;
  ss << "[";
  for (std::size_t i = 0; i < vec.size(); ++i) {
    if (i > 0)
      ss << ",";
    ss << vec[i];
  }
  ss << "]";
  return ss.str();
}

std::vector<float> BrainPersistence::jsonToVector(const std::string &json) {
  std::vector<float> result;
  std::string num;
  for (char c : json) {
    if (c == '[' || c == ' ')
      continue;
    if (c == ',' || c == ']') {
      if (!num.empty()) {
        try {
          result.push_back(std::stof(num));
        } catch (...) {
        }
        num.clear();
      }
    } else {
      num += c;
    }
  }
  return result;
}

// Escape a string for JSON (handles quotes and backslashes)
static std::string jsonEscape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '"')
      out += "\\\"";
    else if (c == '\\')
      out += "\\\\";
    else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else
      out += c;
  }
  return out;
}

// Extract a JSON string value by key (simple parser for our controlled output)
static std::string jsonGetString(const std::string &json,
                                 const std::string &key) {
  std::string search = "\"" + key + "\":\"";
  auto pos = json.find(search);
  if (pos == std::string::npos)
    return "";
  pos += search.size();
  std::string result;
  bool escaped = false;
  for (std::size_t i = pos; i < json.size(); ++i) {
    if (escaped) {
      if (json[i] == 'n')
        result += '\n';
      else if (json[i] == 'r')
        result += '\r';
      else if (json[i] == 't')
        result += '\t';
      else
        result += json[i];
      escaped = false;
    } else if (json[i] == '\\') {
      escaped = true;
    } else if (json[i] == '"') {
      break;
    } else {
      result += json[i];
    }
  }
  return result;
}

// Extract a JSON number value by key
static double jsonGetNumber(const std::string &json, const std::string &key) {
  std::string search = "\"" + key + "\":";
  auto pos = json.find(search);
  if (pos == std::string::npos)
    return 0.0;
  pos += search.size();
  // Skip whitespace
  while (pos < json.size() && json[pos] == ' ')
    pos++;
  std::string num;
  for (std::size_t i = pos; i < json.size(); ++i) {
    if (json[i] == ',' || json[i] == '}' || json[i] == ']')
      break;
    num += json[i];
  }
  try {
    return std::stod(num);
  } catch (...) {
    return 0.0;
  }
}

// Extract a JSON array string (the raw [ ... ] substring)
static std::string jsonGetArray(const std::string &json,
                                const std::string &key) {
  std::string search = "\"" + key + "\":[";
  auto pos = json.find(search);
  if (pos == std::string::npos)
    return "[]";
  pos += search.size() - 1; // point at '['
  int depth = 0;
  std::string result;
  for (std::size_t i = pos; i < json.size(); ++i) {
    result += json[i];
    if (json[i] == '[')
      depth++;
    if (json[i] == ']')
      depth--;
    if (depth == 0)
      break;
  }
  return result;
}

// Split a JSON array of objects into individual object strings
static std::vector<std::string> jsonSplitObjects(const std::string &arrayJson) {
  std::vector<std::string> objects;
  int depth = 0;
  std::string current;
  for (char c : arrayJson) {
    if (c == '{') {
      if (depth == 0)
        current.clear();
      depth++;
      current += c;
    } else if (c == '}') {
      current += c;
      depth--;
      if (depth == 0)
        objects.push_back(current);
    } else if (depth > 0) {
      current += c;
    }
  }
  return objects;
}

// ────────────────────────────────────────────────────────────────
//  LANGUAGE SYSTEM save/load
// ────────────────────────────────────────────────────────────────

bool BrainPersistence::saveLanguage(const std::string &path,
                                    LanguageSystem &lang, std::size_t &count) {
  std::ofstream out(path);
  if (!out.is_open())
    return false;

  auto tokens = lang.getActiveTokens(0.0f); // Get all tokens
  count = tokens.size();

  out << "{\"tokens\":[\n";
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    const auto &tok = tokens[i];

    out << "{\"s\":\"" << jsonEscape(tok.symbol) << "\""
        << ",\"t\":" << static_cast<int>(tok.type)
        << ",\"e\":" << vectorToJson(tok.embedding)
        << ",\"a\":" << tok.activation_strength << ",\"u\":" << tok.usage_count
        << "}";
    if (i + 1 < tokens.size())
      out << ",";
    out << "\n";
  }
  out << "]}\n";
  return true;
}

bool BrainPersistence::loadLanguage(const std::string &path,
                                    LanguageSystem &lang, std::size_t &count) {
  std::ifstream in(path);
  if (!in.is_open())
    return false;

  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  std::string tokensArray = jsonGetArray(content, "tokens");
  auto objects = jsonSplitObjects(tokensArray);
  count = 0;

  for (const auto &obj : objects) {
    std::string symbol = jsonGetString(obj, "s");
    if (symbol.empty())
      continue;

    std::string embStr = jsonGetArray(obj, "e");
    auto embedding = jsonToVector(embStr);
    int typeInt = static_cast<int>(jsonGetNumber(obj, "t"));
    auto type = static_cast<LanguageSystem::TokenType>(typeInt);

    // Create or update the token
    auto *existing = lang.getToken(symbol);
    if (!existing) {
      lang.createToken(symbol, type, embedding);
      existing = lang.getToken(symbol);
    }
    if (existing && !embedding.empty()) {
      existing->embedding = embedding;
      existing->activation_strength =
          static_cast<float>(jsonGetNumber(obj, "a"));
      existing->usage_count =
          static_cast<std::uint64_t>(jsonGetNumber(obj, "u"));
    }
    count++;
  }
  return true;
}

// ────────────────────────────────────────────────────────────────
//  EPISODIC MEMORY save/load
// ────────────────────────────────────────────────────────────────

bool BrainPersistence::saveEpisodic(const std::string &path,
                                    Memory::EpisodicMemoryManager &mem,
                                    std::size_t &count) {
  std::ofstream out(path);
  if (!out.is_open())
    return false;

  auto patterns = mem.getPatterns();
  count = patterns.size();

  out << "{\"patterns\":[\n";
  for (std::size_t i = 0; i < patterns.size(); ++i) {
    const auto &p = patterns[i];
    out << "{\"sensory\":" << vectorToJson(p.sensory_state)
        << ",\"context\":" << vectorToJson(p.context_state)
        << ",\"emotional\":" << vectorToJson(p.emotional_state)
        << ",\"salience\":" << p.salience
        << ",\"strength\":" << p.consolidation_strength
        << ",\"ts\":" << p.timestamp_ms
        << ",\"consolidated\":" << (p.consolidated ? "true" : "false") << "}";
    if (i + 1 < patterns.size())
      out << ",";
    out << "\n";
  }
  out << "]}\n";
  return true;
}

bool BrainPersistence::loadEpisodic(const std::string &path,
                                    Memory::EpisodicMemoryManager &mem,
                                    std::size_t &count) {
  std::ifstream in(path);
  if (!in.is_open())
    return false;

  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  std::string patternsArray = jsonGetArray(content, "patterns");
  auto objects = jsonSplitObjects(patternsArray);
  count = 0;

  for (const auto &obj : objects) {
    auto sensory = jsonToVector(jsonGetArray(obj, "sensory"));
    auto context = jsonToVector(jsonGetArray(obj, "context"));
    auto emotional = jsonToVector(jsonGetArray(obj, "emotional"));
    float salience = static_cast<float>(jsonGetNumber(obj, "salience"));

    if (!sensory.empty() || !context.empty()) {
      mem.encodePattern(sensory, context, emotional, salience);
      count++;
    }
  }
  return true;
}

// ────────────────────────────────────────────────────────────────
//  SEMANTIC MEMORY save/load
// ────────────────────────────────────────────────────────────────

bool BrainPersistence::saveSemantic(const std::string &path,
                                    Memory::SemanticMemory &mem,
                                    std::size_t &count) {
  std::ofstream out(path);
  if (!out.is_open())
    return false;

  const auto &attractors = mem.getAttractors();
  const auto &weights = mem.getConceptWeights();
  count = attractors.size();

  out << "{\"attractors\":[\n";
  for (std::size_t i = 0; i < attractors.size(); ++i) {
    const auto &a = attractors[i];
    out << "{\"p\":" << vectorToJson(a.pattern) << ",\"s\":" << a.strength
        << ",\"ac\":" << a.activation_count
        << ",\"ct\":" << a.creation_timestamp_ms
        << ",\"la\":" << a.last_activation_ms << "}";
    if (i + 1 < attractors.size())
      out << ",";
    out << "\n";
  }
  out << "],\"weights\":" << vectorToJson(weights) << "}\n";
  return true;
}

bool BrainPersistence::loadSemantic(const std::string &path,
                                    Memory::SemanticMemory &mem,
                                    std::size_t &count) {
  std::ifstream in(path);
  if (!in.is_open())
    return false;

  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  // Load attractors
  std::string attractorsArray = jsonGetArray(content, "attractors");
  auto objects = jsonSplitObjects(attractorsArray);

  std::vector<Memory::ConceptAttractor> attractors;
  for (const auto &obj : objects) {
    Memory::ConceptAttractor a;
    a.pattern = jsonToVector(jsonGetArray(obj, "p"));
    a.strength = static_cast<float>(jsonGetNumber(obj, "s"));
    a.activation_count = static_cast<uint32_t>(jsonGetNumber(obj, "ac"));
    a.creation_timestamp_ms = static_cast<uint64_t>(jsonGetNumber(obj, "ct"));
    a.last_activation_ms = static_cast<uint64_t>(jsonGetNumber(obj, "la"));
    attractors.push_back(std::move(a));
  }
  count = attractors.size();
  mem.loadAttractors(attractors);

  // Load concept weights
  auto weights = jsonToVector(jsonGetArray(content, "weights"));
  if (!weights.empty()) {
    // Infer dimension from sqrt of weight count
    std::size_t dim = 0;
    for (std::size_t d = 1; d * d <= weights.size(); ++d) {
      if (d * d == weights.size())
        dim = d;
    }
    if (dim > 0) {
      mem.loadConceptWeights(weights, dim);
    }
  }

  return true;
}

// ────────────────────────────────────────────────────────────────
//  PROCEDURAL MEMORY save/load
// ────────────────────────────────────────────────────────────────

bool BrainPersistence::saveProcedural(const std::string &path,
                                      Memory::ProceduralMemory &mem,
                                      std::size_t &count) {
  std::ofstream out(path);
  if (!out.is_open())
    return false;

  const auto &traces = mem.getTraces();
  const auto &weights = mem.getWeightMatrix();
  count = traces.size();

  out << "{\"context_dim\":" << mem.getContextDim()
      << ",\"action_dim\":" << mem.getActionDim() << ",\"traces\":[\n";
  for (std::size_t i = 0; i < traces.size(); ++i) {
    const auto &t = traces[i];
    out << "{\"ctx\":" << vectorToJson(t.context_pattern)
        << ",\"act\":" << vectorToJson(t.action_pattern)
        << ",\"str\":" << t.strength << ",\"pc\":" << t.practice_count
        << ",\"auto\":" << (t.automated ? "true" : "false") << "}";
    if (i + 1 < traces.size())
      out << ",";
    out << "\n";
  }
  out << "],\"weights\":" << vectorToJson(weights) << "}\n";
  return true;
}

bool BrainPersistence::loadProcedural(const std::string &path,
                                      Memory::ProceduralMemory &mem,
                                      std::size_t &count) {
  std::ifstream in(path);
  if (!in.is_open())
    return false;

  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  std::size_t ctx_dim =
      static_cast<std::size_t>(jsonGetNumber(content, "context_dim"));
  std::size_t act_dim =
      static_cast<std::size_t>(jsonGetNumber(content, "action_dim"));

  // Load traces
  std::string tracesArray = jsonGetArray(content, "traces");
  auto objects = jsonSplitObjects(tracesArray);

  std::vector<Memory::ProceduralTrace> traces;
  for (const auto &obj : objects) {
    Memory::ProceduralTrace t;
    t.context_pattern = jsonToVector(jsonGetArray(obj, "ctx"));
    t.action_pattern = jsonToVector(jsonGetArray(obj, "act"));
    t.strength = static_cast<float>(jsonGetNumber(obj, "str"));
    t.practice_count = static_cast<uint32_t>(jsonGetNumber(obj, "pc"));
    t.last_practiced = std::chrono::steady_clock::now();
    traces.push_back(std::move(t));
  }
  count = traces.size();
  mem.loadTraces(traces);

  // Load weight matrix
  auto weights = jsonToVector(jsonGetArray(content, "weights"));
  if (!weights.empty() && ctx_dim > 0 && act_dim > 0) {
    mem.loadWeights(weights, ctx_dim, act_dim);
  }

  return true;
}

// ────────────────────────────────────────────────────────────────
//  TOP-LEVEL save/load
// ────────────────────────────────────────────────────────────────

BrainPersistence::SaveResult BrainPersistence::saveState(
    const std::string &dir, LanguageSystem &language,
    Memory::EpisodicMemoryManager &episodic, Memory::SemanticMemory &semantic,
    Memory::ProceduralMemory &procedural, std::uint64_t step_count,
    std::uint64_t episode_count) {
  SaveResult result;

  try {
    // Create directory
    fs::create_directories(dir);

    // Save manifest
    {
      std::ofstream manifest(dir + "/manifest.json");
      if (manifest.is_open()) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        manifest << "{\"version\":1"
                 << ",\"timestamp_ms\":" << now
                 << ",\"step_count\":" << step_count
                 << ",\"episode_count\":" << episode_count << "}\n";
      }
    }

    // Save each subsystem
    if (!saveLanguage(dir + "/language.json", language, result.tokens_saved)) {
      result.error = "Failed to save language state";
      return result;
    }
    if (!saveEpisodic(dir + "/episodic_memory.json", episodic,
                      result.patterns_saved)) {
      result.error = "Failed to save episodic memory";
      return result;
    }
    if (!saveSemantic(dir + "/semantic_memory.json", semantic,
                      result.attractors_saved)) {
      result.error = "Failed to save semantic memory";
      return result;
    }
    if (!saveProcedural(dir + "/procedural_memory.json", procedural,
                        result.traces_saved)) {
      result.error = "Failed to save procedural memory";
      return result;
    }

    result.success = true;
    std::cout << "[BrainPersistence] Saved: " << result.tokens_saved
              << " tokens, " << result.patterns_saved << " patterns, "
              << result.attractors_saved << " attractors, "
              << result.traces_saved << " traces → " << dir << "\n";

  } catch (const std::exception &e) {
    result.error = std::string("Exception: ") + e.what();
  }
  return result;
}

BrainPersistence::LoadResult
BrainPersistence::loadState(const std::string &dir, LanguageSystem &language,
                            Memory::EpisodicMemoryManager &episodic,
                            Memory::SemanticMemory &semantic,
                            Memory::ProceduralMemory &procedural) {
  LoadResult result;

  if (!fs::exists(dir) || !fs::exists(dir + "/manifest.json")) {
    result.found = false;
    result.success = true; // Not finding state is not an error
    return result;
  }
  result.found = true;

  try {
    // Read manifest
    {
      std::ifstream manifest(dir + "/manifest.json");
      if (manifest.is_open()) {
        std::string content((std::istreambuf_iterator<char>(manifest)),
                            std::istreambuf_iterator<char>());
        result.saved_step_count =
            static_cast<std::uint64_t>(jsonGetNumber(content, "step_count"));
      }
    }

    // Load each subsystem (failures are non-fatal — partial load is OK)
    if (fs::exists(dir + "/language.json")) {
      loadLanguage(dir + "/language.json", language, result.tokens_loaded);
    }
    if (fs::exists(dir + "/episodic_memory.json")) {
      loadEpisodic(dir + "/episodic_memory.json", episodic,
                   result.patterns_loaded);
    }
    if (fs::exists(dir + "/semantic_memory.json")) {
      loadSemantic(dir + "/semantic_memory.json", semantic,
                   result.attractors_loaded);
    }
    if (fs::exists(dir + "/procedural_memory.json")) {
      loadProcedural(dir + "/procedural_memory.json", procedural,
                     result.traces_loaded);
    }

    result.success = true;
    std::cout << "[BrainPersistence] Loaded: " << result.tokens_loaded
              << " tokens, " << result.patterns_loaded << " patterns, "
              << result.attractors_loaded << " attractors, "
              << result.traces_loaded << " traces"
              << " (was at step " << result.saved_step_count << ")\n";

  } catch (const std::exception &e) {
    result.error = std::string("Exception: ") + e.what();
  }
  return result;
}

} // namespace Core
} // namespace NeuroForge
