#pragma once

/**
 * @file BrainPersistence.h
 * @brief Brain state persistence — save/load across sessions
 *
 * Coordinates serialization of all cognitive subsystems to a directory
 * of JSON files, enabling persistent identity across runs.
 *
 * State directory layout:
 *   data/brain_state/
 *   ├── manifest.json          (version, timestamp, step count)
 *   ├── language.json          (vocabulary tokens + embeddings)
 *   ├── episodic_memory.json   (patterns, salience, consolidation)
 *   ├── semantic_memory.json   (attractors, concept weights)
 *   └── procedural_memory.json (traces, weight matrix)
 */

#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {

// Forward declarations — we don't want to include everything here
namespace Core {
class LanguageSystem;
}
namespace Memory {
class EpisodicMemoryManager;
class SemanticMemory;
class ProceduralMemory;
} // namespace Memory

namespace Core {

/**
 * @brief Brain state persistence coordinator
 *
 * Manages save/load of all subsystem state to a directory.
 * Uses JSON format for portability — no external dependencies.
 */
class BrainPersistence {
public:
  struct SaveResult {
    bool success = false;
    std::string error;
    std::size_t tokens_saved = 0;
    std::size_t patterns_saved = 0;
    std::size_t attractors_saved = 0;
    std::size_t traces_saved = 0;
  };

  struct LoadResult {
    bool success = false;
    bool found = false; // true if state directory exists
    std::string error;
    std::size_t tokens_loaded = 0;
    std::size_t patterns_loaded = 0;
    std::size_t attractors_loaded = 0;
    std::size_t traces_loaded = 0;
    std::uint64_t saved_step_count = 0;
  };

  /**
   * @brief Save all subsystem state to the given directory.
   * Creates the directory and all parent directories if needed.
   */
  static SaveResult saveState(const std::string &dir, LanguageSystem &language,
                              Memory::EpisodicMemoryManager &episodic,
                              Memory::SemanticMemory &semantic,
                              Memory::ProceduralMemory &procedural,
                              std::uint64_t step_count,
                              std::uint64_t episode_count);

  /**
   * @brief Load all subsystem state from the given directory.
   * Returns LoadResult with found=false if directory doesn't exist.
   */
  static LoadResult loadState(const std::string &dir, LanguageSystem &language,
                              Memory::EpisodicMemoryManager &episodic,
                              Memory::SemanticMemory &semantic,
                              Memory::ProceduralMemory &procedural);

private:
  // Per-subsystem save/load helpers
  static bool saveLanguage(const std::string &path, LanguageSystem &lang,
                           std::size_t &count);
  static bool loadLanguage(const std::string &path, LanguageSystem &lang,
                           std::size_t &count);

  static bool saveEpisodic(const std::string &path,
                           Memory::EpisodicMemoryManager &mem,
                           std::size_t &count);
  static bool loadEpisodic(const std::string &path,
                           Memory::EpisodicMemoryManager &mem,
                           std::size_t &count);

  static bool saveSemantic(const std::string &path, Memory::SemanticMemory &mem,
                           std::size_t &count);
  static bool loadSemantic(const std::string &path, Memory::SemanticMemory &mem,
                           std::size_t &count);

  static bool saveProcedural(const std::string &path,
                             Memory::ProceduralMemory &mem, std::size_t &count);
  static bool loadProcedural(const std::string &path,
                             Memory::ProceduralMemory &mem, std::size_t &count);

  // JSON helpers (minimal, no external deps)
  static std::string vectorToJson(const std::vector<float> &vec);
  static std::vector<float> jsonToVector(const std::string &json);
};

} // namespace Core
} // namespace NeuroForge
