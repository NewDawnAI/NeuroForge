#pragma once

#include "actuation/ReplayFrame.h"
#include "memory/ProceduralMemory.h"

#include <cmath>
#include <random>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Memory {

/**
 * @brief Phase 21: Extract skills from successful action sequences
 *
 * When verification succeeds, the action sequence becomes a reusable skill.
 */
class SkillExtractor {
public:
  explicit SkillExtractor(ProceduralMemory &memory) : memory_(memory) {}

  /**
   * @brief Try to extract a skill from a successful replay
   *
   * @return Skill ID if extracted, 0 otherwise
   */
  std::uint64_t tryExtractSkill(const Actuation::ReplayFrame &frame) {
    if (!frame.success || !frame.fact_confirmed) {
      return 0; // Only successful, fact-confirming actions become skills
    }

    std::string skill_name = generateSkillName(frame);
    std::vector<std::string> action_sequence = extractActionSequence(frame);

    // Check if skill already exists
    auto existing = memory_.findSkill(skill_name);
    if (existing) {
      // Practice existing skill
      memory_.practiceSkill(existing->id, 1.0f);
      return existing->id;
    }

    // Create new skill
    return memory_.addSkill(skill_name, action_sequence);
  }

  /**
   * @brief Process entire session and extract skills
   */
  std::size_t
  processSession(const std::vector<Actuation::ReplayFrame> &frames) {
    std::size_t skills_created = 0;
    for (const auto &frame : frames) {
      if (tryExtractSkill(frame) != 0) {
        ++skills_created;
      }
    }
    return skills_created;
  }

  // ════════════════════════════════════════════════
  //  WEIGHT-SPACE SKILL EXTRACTION
  // ════════════════════════════════════════════════

  /// Encode frame context into a pattern vector
  std::vector<float> extractContextPattern(const Actuation::ReplayFrame &frame,
                                           std::size_t dim = 64) {
    std::vector<float> pattern(dim, 0.0f);
    // Hash the fact signature + action kind into a deterministic vector
    std::string seed_str =
        frame.fact_signature +
        Actuation::ActionCommand::kindToString(frame.action.kind);
    std::hash<std::string> hasher;
    std::mt19937 rng(static_cast<unsigned>(hasher(seed_str)));
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto &v : pattern)
      v = dist(rng);
    // Normalize
    float norm = 0;
    for (auto v : pattern)
      norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-8f)
      for (auto &v : pattern)
        v /= norm;
    return pattern;
  }

  /// Extract pattern and reinforce directly via weight matrix
  bool extractSkillPattern(const Actuation::ReplayFrame &frame,
                           float reward = 1.0f) {
    if (!frame.success || !frame.fact_confirmed)
      return false;

    auto context = extractContextPattern(frame);
    auto action = extractContextPattern(frame, 16); // action space is 16-dim

    // Reinforce through the weight matrix (bypasses string API)
    memory_.reinforce(context, action, reward);
    return true;
  }

private:
  std::string generateSkillName(const Actuation::ReplayFrame &frame) {
    std::string name = "verify";

    if (!frame.fact_signature.empty()) {
      // Create skill name from fact signature
      name += "_" + sanitize(frame.fact_signature);
    } else {
      name += "_" + Actuation::ActionCommand::kindToString(frame.action.kind);
    }

    return name;
  }

  std::vector<std::string>
  extractActionSequence(const Actuation::ReplayFrame &frame) {
    std::vector<std::string> sequence;

    // Single action frame
    sequence.push_back(
        Actuation::ActionCommand::kindToString(frame.action.kind));

    if (!frame.action.parameters.empty()) {
      sequence.push_back(frame.action.parameters);
    }

    return sequence;
  }

  std::string sanitize(const std::string &s) {
    std::string result;
    for (char c : s) {
      if (std::isalnum(c) || c == '_') {
        result += std::tolower(c);
      } else if (c == ' ') {
        result += '_';
      }
    }
    if (result.length() > 50) {
      result = result.substr(0, 50);
    }
    return result;
  }

  ProceduralMemory &memory_;
};

} // namespace Memory
} // namespace NeuroForge
