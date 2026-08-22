#pragma once

/**
 * @file ExpressionIntent.h
 * @brief Phase E1: Language Expression Intent
 *
 * Defines what the system wants to express and how.
 *
 * @invariant Language describes cognition — it never causes cognition.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Expression {

/**
 * @brief Types of expression
 */
enum class ExpressionType {
  DESCRIBE,  ///< Describe a concept or observation
  ANSWER,    ///< Answer a question
  EXPLAIN,   ///< Explain reasoning or evidence chain
  SUMMARIZE, ///< Summarize knowledge area
  CLARIFY,   ///< Clarify ambiguity
  REFLECT    ///< Reflect on own state/behavior
};

/**
 * @brief Target audience for expression
 */
enum class Audience {
  HUMAN,   ///< General human user
  AUDITOR, ///< External auditor/regulator
  PEER,    ///< Another AI system
  DEBUG    ///< Internal debugging
};

/**
 * @brief Expression intent - what to express and how
 */
struct ExpressionIntent {
  ExpressionType type = ExpressionType::DESCRIBE;
  std::vector<std::uint64_t> concept_ids; ///< ConceptNode IDs to express
  Audience audience = Audience::HUMAN;
  float verbosity = 0.5f; ///< 0.0 = terse, 1.0 = verbose
  bool include_evidence = true;
  bool include_confidence = true;
  std::string context; ///< Additional context (e.g., question asked)

  /**
   * @brief Create a simple description intent
   */
  static ExpressionIntent describe(std::uint64_t concept_id) {
    ExpressionIntent intent;
    intent.type = ExpressionType::DESCRIBE;
    intent.concept_ids.push_back(concept_id);
    return intent;
  }

  /**
   * @brief Create an explanation intent
   */
  static ExpressionIntent explain(const std::vector<std::uint64_t> &concepts,
                                  const std::string &context = "") {
    ExpressionIntent intent;
    intent.type = ExpressionType::EXPLAIN;
    intent.concept_ids = concepts;
    intent.context = context;
    intent.include_evidence = true;
    return intent;
  }

  /**
   * @brief Create an answer intent
   */
  static ExpressionIntent
  answer(const std::string &question,
         const std::vector<std::uint64_t> &relevant_concepts) {
    ExpressionIntent intent;
    intent.type = ExpressionType::ANSWER;
    intent.concept_ids = relevant_concepts;
    intent.context = question;
    return intent;
  }

  /**
   * @brief Convert type to string
   */
  static std::string typeToString(ExpressionType t) {
    switch (t) {
    case ExpressionType::DESCRIBE:
      return "DESCRIBE";
    case ExpressionType::ANSWER:
      return "ANSWER";
    case ExpressionType::EXPLAIN:
      return "EXPLAIN";
    case ExpressionType::SUMMARIZE:
      return "SUMMARIZE";
    case ExpressionType::CLARIFY:
      return "CLARIFY";
    case ExpressionType::REFLECT:
      return "REFLECT";
    default:
      return "UNKNOWN";
    }
  }
};

} // namespace Expression
} // namespace NeuroForge
