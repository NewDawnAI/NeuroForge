#pragma once

/**
 * @file LexicalUnit.h
 * @brief Phase E1: Lexical units for expression
 *
 * Maps concepts to surface forms (words).
 */

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Expression {

/**
 * @brief A lexical unit - ties a concept to a word/phrase
 */
struct LexicalUnit {
  std::uint64_t concept_id = 0; ///< Source ConceptNode ID
  std::string surface_form;     ///< Surface text (e.g., "quantum mechanics")
  float confidence = 0.0f;      ///< Confidence in this grounding
  bool is_grounded = false;     ///< True if backed by ConceptNode
  std::string pos_tag;          ///< Part of speech (noun, verb, etc.)

  /**
   * @brief Create a grounded lexical unit
   */
  static LexicalUnit grounded(std::uint64_t id, const std::string &text,
                              float conf = 1.0f) {
    LexicalUnit lu;
    lu.concept_id = id;
    lu.surface_form = text;
    lu.confidence = conf;
    lu.is_grounded = true;
    return lu;
  }

  /**
   * @brief Create a functional word (not grounded to concept)
   */
  static LexicalUnit functional(const std::string &text,
                                const std::string &pos = "FUNC") {
    LexicalUnit lu;
    lu.concept_id = 0;
    lu.surface_form = text;
    lu.confidence = 1.0f;
    lu.is_grounded = false;
    lu.pos_tag = pos;
    return lu;
  }
};

} // namespace Expression
} // namespace NeuroForge
