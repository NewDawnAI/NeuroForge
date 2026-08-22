#pragma once

/**
 * @file LanguageExpressionCortex.h
 * @brief Phase E1: Main Language Expression Cortex
 *
 * Converts internal cognition into human language.
 *
 * @invariant Language describes cognition — it never causes cognition.
 * @invariant LEC is a renderer, not a mind.
 * @invariant Expression never feeds back into verification/arbitration/norms.
 */

#include "ExpressionIntent.h"
#include "LexicalUnit.h"
#include "UtterancePlan.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Expression {

/**
 * @brief Concept information for expression
 */
struct ConceptInfo {
  std::uint64_t id = 0;
  std::string label;
  float confidence = 0.0f;
  std::vector<std::uint64_t> related_concepts;
  std::vector<std::uint64_t> evidence_frames;
};

/**
 * @brief Expression replay frame (for audit)
 */
struct ExpressionReplayFrame {
  std::uint64_t frame_id = 0;
  std::uint64_t timestamp_ms = 0;
  std::string text;
  std::vector<std::uint64_t> concepts_used;
  std::vector<std::string> norms_applied;
  std::vector<std::string> values_checked;
  NormativeStatus status = NormativeStatus::PERMITTED;
};

/**
 * @brief Language Expression Cortex
 *
 * Pipeline:
 * 1. ExpressionIntent
 * 2. ConceptSelector (what is relevant?)
 * 3. LexicalMapper (ConceptNode → words)
 * 4. SentenceAssembler (syntax only)
 * 5. Norm + Value Filter
 * 6. Output
 */
class LanguageExpressionCortex {
public:
  using ConceptLookup = std::function<ConceptInfo(std::uint64_t)>;
  using NormCheck = std::function<bool(const std::string &)>;

  LanguageExpressionCortex() = default;

  /**
   * @brief Set concept lookup function
   */
  void setConceptLookup(ConceptLookup lookup) { concept_lookup_ = lookup; }

  /**
   * @brief Set norm check function
   */
  void setNormCheck(NormCheck check) { norm_check_ = check; }

  /**
   * @brief Register a vocabulary entry
   */
  void registerVocabulary(std::uint64_t concept_id,
                          const std::string &surface_form,
                          float confidence = 1.0f) {
    vocabulary_[concept_id] = {concept_id, surface_form, confidence, true,
                               "NOUN"};
  }

  /**
   * @brief Express an intent as text
   */
  UtterancePlan express(const ExpressionIntent &intent) {
    UtterancePlan plan;

    // Step 1: Select relevant concepts
    std::vector<ConceptInfo> selected_concepts;
    for (auto id : intent.concept_ids) {
      if (concept_lookup_) {
        selected_concepts.push_back(concept_lookup_(id));
      } else {
        // Fallback: create minimal info
        ConceptInfo info;
        info.id = id;
        info.label = "concept_" + std::to_string(id);
        info.confidence = 0.5f;
        selected_concepts.push_back(info);
      }
    }

    // Step 2: Map concepts to lexical units
    std::vector<LexicalUnit> content_words;
    for (const auto &cinfo : selected_concepts) {
      LexicalUnit lu = mapConceptToWord(cinfo);
      content_words.push_back(lu);
      plan.concept_ids.push_back(cinfo.id);
      for (auto eid : cinfo.evidence_frames) {
        plan.evidence_ids.push_back(eid);
      }
    }

    // Step 3: Assemble sentence based on intent type
    assembleSentence(intent, content_words, plan);

    // Step 4: Apply norm/value filters
    applyNormativeFilter(plan);

    // Step 5: Calculate confidence
    plan.calculateConfidence();

    // Log expression event
    logExpression(intent, plan);

    return plan;
  }

  /**
   * @brief Express a simple description
   */
  std::string describe(std::uint64_t concept_id) {
    auto intent = ExpressionIntent::describe(concept_id);
    auto plan = express(intent);
    return plan.canExpress() ? plan.render() : "";
  }

  /**
   * @brief Answer a question
   */
  std::string answer(const std::string &question,
                     const std::vector<std::uint64_t> &relevant_concepts) {
    auto intent = ExpressionIntent::answer(question, relevant_concepts);
    auto plan = express(intent);
    return plan.canExpress() ? plan.render() : "I cannot answer that.";
  }

  /**
   * @brief Get expression history (for audit)
   */
  const std::vector<ExpressionReplayFrame> &getHistory() const {
    return history_;
  }

  /**
   * @brief Get vocabulary size
   */
  std::size_t vocabularySize() const { return vocabulary_.size(); }

private:
  LexicalUnit mapConceptToWord(const ConceptInfo &cinfo) {
    // Check vocabulary first
    auto it = vocabulary_.find(cinfo.id);
    if (it != vocabulary_.end()) {
      return it->second;
    }

    // Fallback: use label
    return LexicalUnit::grounded(cinfo.id, cinfo.label, cinfo.confidence);
  }

  void assembleSentence(const ExpressionIntent &intent,
                        const std::vector<LexicalUnit> &content,
                        UtterancePlan &plan) {
    // Simple template-based assembly
    switch (intent.type) {
    case ExpressionType::DESCRIBE:
      assembleDescription(content, plan);
      break;
    case ExpressionType::ANSWER:
      assembleAnswer(intent.context, content, plan);
      break;
    case ExpressionType::EXPLAIN:
      assembleExplanation(content, plan);
      break;
    case ExpressionType::SUMMARIZE:
      assembleSummary(content, plan);
      break;
    case ExpressionType::CLARIFY:
      assembleClarification(content, plan);
      break;
    case ExpressionType::REFLECT:
      assembleReflection(content, plan);
      break;
    }
  }

  void assembleDescription(const std::vector<LexicalUnit> &content,
                           UtterancePlan &plan) {
    if (content.empty())
      return;

    if (content.size() == 1) {
      plan.tokens.push_back(LexicalUnit::functional("This is"));
      plan.tokens.push_back(content[0]);
    } else {
      plan.tokens.push_back(
          LexicalUnit::functional("These concepts are related:"));
      for (size_t i = 0; i < content.size(); i++) {
        if (i > 0)
          plan.tokens.push_back(LexicalUnit::functional(","));
        plan.tokens.push_back(content[i]);
      }
    }
  }

  void assembleAnswer([[maybe_unused]] const std::string &question,
                      const std::vector<LexicalUnit> &content,
                      UtterancePlan &plan) {
    if (content.empty()) {
      plan.tokens.push_back(
          LexicalUnit::functional("I don't have enough information."));
      return;
    }

    plan.tokens.push_back(LexicalUnit::functional("Based on my knowledge,"));
    for (size_t i = 0; i < content.size(); i++) {
      plan.tokens.push_back(content[i]);
      if (i < content.size() - 1) {
        plan.tokens.push_back(LexicalUnit::functional("and"));
      }
    }
    plan.tokens.push_back(LexicalUnit::functional("are relevant."));
  }

  void assembleExplanation(const std::vector<LexicalUnit> &content,
                           UtterancePlan &plan) {
    plan.tokens.push_back(LexicalUnit::functional("The reasoning is:"));
    for (const auto &c : content) {
      plan.tokens.push_back(c);
    }
  }

  void assembleSummary(const std::vector<LexicalUnit> &content,
                       UtterancePlan &plan) {
    plan.tokens.push_back(LexicalUnit::functional("In summary:"));
    for (const auto &c : content) {
      plan.tokens.push_back(c);
    }
  }

  void assembleClarification(const std::vector<LexicalUnit> &content,
                             UtterancePlan &plan) {
    plan.tokens.push_back(LexicalUnit::functional("To clarify,"));
    for (const auto &c : content) {
      plan.tokens.push_back(c);
    }
  }

  void assembleReflection(const std::vector<LexicalUnit> &content,
                          UtterancePlan &plan) {
    plan.tokens.push_back(LexicalUnit::functional("I observe that"));
    for (const auto &c : content) {
      plan.tokens.push_back(c);
    }
  }

  void applyNormativeFilter(UtterancePlan &plan) {
    std::string text = plan.render();

    // Check blocked content
    if (containsBlockedContent(text)) {
      plan.status = NormativeStatus::BLOCKED;
      plan.blocked_reason = "Content violates safety norms";
      return;
    }

    // Check restricted content
    if (containsRestrictedContent(text)) {
      plan.status = NormativeStatus::RESTRICTED;
      // Filter the restricted parts
      filterRestrictedContent(plan);
    }

    // Apply custom norm check
    if (norm_check_ && !norm_check_(text)) {
      plan.status = NormativeStatus::BLOCKED;
      plan.blocked_reason = "Norm check failed";
    }
  }

  bool containsBlockedContent(const std::string &text) {
    // Block: medical advice, legal advice, personal data
    std::vector<std::string> blocked = {"medical advice", "legal advice",
                                        "social security", "password",
                                        "credit card"};
    for (const auto &b : blocked) {
      if (text.find(b) != std::string::npos)
        return true;
    }
    return false;
  }

  bool containsRestrictedContent(const std::string &text) {
    // Restricted: speculation, uncertainty
    return text.find("might") != std::string::npos ||
           text.find("probably") != std::string::npos;
  }

  void filterRestrictedContent([[maybe_unused]] UtterancePlan &plan) {
    // Mark that content was filtered
    // In a real implementation, would remove/replace specific tokens
  }

  void logExpression([[maybe_unused]] const ExpressionIntent &intent,
                     const UtterancePlan &plan) {
    ExpressionReplayFrame frame;
    frame.frame_id = next_frame_id_++;
    frame.timestamp_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    frame.text = plan.render();
    frame.concepts_used = plan.concept_ids;
    frame.status = plan.status;
    // Would add norm/value lists here

    history_.push_back(frame);
  }

  ConceptLookup concept_lookup_;
  NormCheck norm_check_;
  std::map<std::uint64_t, LexicalUnit> vocabulary_;
  std::vector<ExpressionReplayFrame> history_;
  std::uint64_t next_frame_id_ = 1;
};

} // namespace Expression
} // namespace NeuroForge
