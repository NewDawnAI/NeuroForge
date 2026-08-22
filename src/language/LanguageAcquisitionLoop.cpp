/**
 * @file LanguageAcquisitionLoop.cpp
 * @brief Implementation of autonomous language learning from web content
 */

#include "language/LanguageAcquisitionLoop.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace NeuroForge {
namespace Language {

LanguageAcquisitionLoop::LanguageAcquisitionLoop(
    Core::LanguageSystem *language_system,
    Perception::LivePerceptionLoop *perception,
    Navigation::CuriosityNavigator *navigator,
    Verification::GroundingVerifier *verifier, const AcquisitionConfig &config)
    : language_system_(language_system), perception_(perception),
      navigator_(navigator), verifier_(verifier), config_(config) {
  stats_.start_time = std::chrono::steady_clock::now();
}

LanguageAcquisitionLoop::~LanguageAcquisitionLoop() { stop(); }

bool LanguageAcquisitionLoop::initialize() {
  if (is_initialized_)
    return true;

  if (!language_system_) {
    std::cerr << "[LanguageAcquisitionLoop] Error: LanguageSystem is null\n";
    return false;
  }

  // if (!perception_) {
  //   std::cerr
  //       << "[LanguageAcquisitionLoop] Error: LivePerceptionLoop is null\n";
  //   return false;
  // }

  // Initialize seed topics for exploration
  if (navigator_) {
    for (const auto &topic : config_.seed_topics) {
      navigator_->addGoal(topic, 1.0f, {});
    }
  }

  is_initialized_ = true;
  std::cout << "[LanguageAcquisitionLoop] Initialized with "
            << config_.seed_topics.size() << " seed topics\n";
  return true;
}

void LanguageAcquisitionLoop::start() {
  if (!is_initialized_) {
    if (!initialize())
      return;
  }

  is_running_ = true;

  // Start perception if not running
  if (perception_ && !perception_->isRunning()) {
    perception_->start();
  }

  std::cout
      << "[LanguageAcquisitionLoop] Started autonomous language learning\n";
}

void LanguageAcquisitionLoop::stop() {
  is_running_ = false;
  std::cout << "[LanguageAcquisitionLoop] Stopped\n";
}

void LanguageAcquisitionLoop::processWebContent(const std::string &text,
                                                const std::string &source_url) {
  if (text.empty())
    return;

  // Submit to perception for entity/relation extraction
  if (perception_) {
    perception_->submitText(text, source_url);
  }

  if (config_.enable_direct_tokenization) {
    if (config_.max_tokenization_chars_per_page > 0 &&
        text.size() >
            static_cast<std::size_t>(config_.max_tokenization_chars_per_page)) {
      extractAndLearnTokens(
          text.substr(0, static_cast<std::size_t>(
                             config_.max_tokenization_chars_per_page)),
          source_url);
    } else {
      extractAndLearnTokens(text, source_url);
    }
  }

  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_.pages_processed++;
  stats_.last_learn_time = std::chrono::steady_clock::now();
}

void LanguageAcquisitionLoop::learnFromEntities(
    const std::vector<Perception::ExtractedEntity> &entities,
    const std::string &source_url) {

  std::lock_guard<std::mutex> lock(learn_mutex_);

  for (const auto &entity : entities) {
    // Track occurrence
    token_occurrences_[entity.text]++;

    // Check if we should learn this token
    if (shouldLearnToken(entity.text)) {
      if (createToken(entity, source_url)) {
        stats_.tokens_learned++;

        if (learn_callback_) {
          learn_callback_(entity.text, token_confidence_[entity.text]);
        }
      }
    }
  }

  // Update vocabulary size
  stats_.vocabulary_size = token_confidence_.size();
}

void LanguageAcquisitionLoop::learnFromRelations(
    const std::vector<Perception::CandidateRelation> &relations,
    const std::string &source_url) {

  for (const auto &relation : relations) {
    // Skip low-confidence relations
    if (relation.confidence < config_.min_relation_confidence) {
      continue;
    }

    // Optionally verify before storing
    if (config_.verify_before_storing && verifier_) {
      {
        std::lock_guard<std::mutex> lock(learn_mutex_);
        pending_verifications_.push_back(relation);
      }
      std::lock_guard<std::mutex> lock(stats_mutex_);
      stats_.relations_formed++;
    } else {
      // Store directly
      if (createRelation(relation, source_url)) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.relations_formed++;
      }
    }
  }
}

void LanguageAcquisitionLoop::verifyPendingKnowledge() {
  if (!verifier_ || pending_verifications_.empty())
    return;

  std::vector<Perception::CandidateRelation> to_verify;
  {
    std::lock_guard<std::mutex> lock(learn_mutex_);
    to_verify = std::move(pending_verifications_);
    pending_verifications_.clear();
  }

  for (const auto &rel : to_verify) {
    Verification::RelationEvidence evidence;
    evidence.source_url = rel.source_url;
    evidence.source_text = rel.source_sentence;
    evidence.confidence = rel.confidence;
    evidence.transe_score = rel.transe_score;
    evidence.supports = true;
    evidence.found_at = std::chrono::system_clock::now();

    auto hyp_id = verifier_->createHypothesis(
        rel.subject.text, rel.predicate.text, rel.object.text, &evidence);
    if (hyp_id == 0)
      continue;

    auto state = verifier_->verifyHypothesis(hyp_id);
    bool verified = state == Verification::RelationHypothesis::State::Verified;

    {
      std::lock_guard<std::mutex> lock(stats_mutex_);
      if (verified) {
        stats_.relations_verified++;
      } else if (state ==
                 Verification::RelationHypothesis::State::Contradicted) {
        stats_.contradictions_found++;
      }
    }

    if (verify_callback_) {
      std::string rel_key =
          rel.subject.text + ":" + rel.predicate.text + ":" + rel.object.text;
      verify_callback_(rel_key, verified);
    }
  }
}

void LanguageAcquisitionLoop::addLearningTopic(const std::string &topic,
                                               float priority) {
  if (navigator_) {
    // Generate Wikipedia URL for the topic
    std::string seed_url = "https://en.wikipedia.org/wiki/" + topic;
    navigator_->addGoal(topic, priority, {seed_url});
  }

  std::cout << "[LanguageAcquisitionLoop] Added learning topic: " << topic
            << " (priority=" << priority << ")\n";
}

std::vector<std::string>
LanguageAcquisitionLoop::getLowConfidenceTopics(int top_k) const {
  std::lock_guard<std::mutex> lock(learn_mutex_);

  // Collect tokens with low confidence
  std::vector<std::pair<std::string, float>> sorted_tokens;
  for (const auto &[token, confidence] : token_confidence_) {
    if (confidence < 0.7f) {
      sorted_tokens.emplace_back(token, confidence);
    }
  }

  // Sort by confidence (ascending)
  std::sort(sorted_tokens.begin(), sorted_tokens.end(),
            [](const auto &a, const auto &b) { return a.second < b.second; });

  // Return top-k
  std::vector<std::string> result;
  for (int i = 0; i < std::min(top_k, static_cast<int>(sorted_tokens.size()));
       ++i) {
    result.push_back(sorted_tokens[i].first);
  }

  return result;
}

AcquisitionStats LanguageAcquisitionLoop::getStatistics() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  // Calculate average confidences
  AcquisitionStats result = stats_;

  if (!token_confidence_.empty()) {
    float total = 0.0f;
    for (const auto &[_, conf] : token_confidence_) {
      total += conf;
    }
    result.avg_token_confidence = total / token_confidence_.size();
  }

  return result;
}

void LanguageAcquisitionLoop::resetStatistics() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_ = AcquisitionStats{};
  stats_.start_time = std::chrono::steady_clock::now();
}

std::vector<std::string> LanguageAcquisitionLoop::getLearnedVocabulary() const {
  std::lock_guard<std::mutex> lock(learn_mutex_);

  std::vector<std::string> vocab;
  for (const auto &[token, _] : token_confidence_) {
    vocab.push_back(token);
  }
  return vocab;
}

float LanguageAcquisitionLoop::getTokenConfidence(
    const std::string &token) const {
  std::lock_guard<std::mutex> lock(learn_mutex_);

  auto it = token_confidence_.find(token);
  return (it != token_confidence_.end()) ? it->second : 0.0f;
}

// ========== Internal Methods ==========

void LanguageAcquisitionLoop::extractAndLearnTokens(
    const std::string &text, const std::string &source_url) {

  // Simple tokenization (split on whitespace and punctuation)
  std::vector<std::string> tokens;
  std::string current_token;
  bool token_too_long = false;

  for (char c : text) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) || c == '-' || c == '\'') {
      if (!token_too_long) {
        if (current_token.size() >= 64) {
          token_too_long = true;
        } else {
          current_token +=
              static_cast<char>(std::tolower(static_cast<int>(uc)));
        }
      }
    } else {
      if (!current_token.empty() && !token_too_long &&
          current_token.length() >= 3) {
        tokens.push_back(current_token);
      }
      current_token.clear();
      token_too_long = false;
    }
  }
  if (!current_token.empty() && !token_too_long &&
      current_token.length() >= 3) {
    tokens.push_back(current_token);
  }

  // Track occurrences and learn
  std::lock_guard<std::mutex> lock(learn_mutex_);
  int created_this_page = 0;

  for (const auto &token : tokens) {
    token_occurrences_[token]++;
    updateTokenConfidence(token);

    // Feed to LanguageSystem if meets threshold
    if (shouldLearnToken(token) && language_system_) {
      // Check if token exists, if not create it
      std::size_t token_id = 0;
      bool exists = language_system_->getTokenId(token, token_id);
      if (!exists) {
        if (created_this_page >= config_.max_new_tokens_per_page) {
          break;
        }
        // Create new token in LanguageSystem
        language_system_->createToken(
            token, Core::LanguageSystem::TokenType::Word, {});
        stats_.tokens_learned++;
        created_this_page++;

        if (stats_.tokens_learned % 200 == 0) {
          std::cout << "[LanguageAcquisitionLoop] Tokens learned: "
                    << stats_.tokens_learned << "\n";
        }

        if (learn_callback_) {
          learn_callback_(token, token_confidence_[token]);
        }
      }
    }
  }
}

bool LanguageAcquisitionLoop::createToken(
    const Perception::ExtractedEntity &entity,
    const std::string & /*source_url*/) {

  if (!language_system_)
    return false;

  // Map entity type to token type
  Core::LanguageSystem::TokenType token_type =
      Core::LanguageSystem::TokenType::Word;

  // Check if token already exists
  std::size_t existing_id = 0;
  bool exists = language_system_->getTokenId(entity.text, existing_id);
  if (exists) {
    // Token exists, update confidence
    updateTokenConfidence(entity.text);
    return false;
  }

  // Create new token
  language_system_->createToken(entity.text, token_type, {});
  token_confidence_[entity.text] = config_.min_token_confidence;

  return true;
}

bool LanguageAcquisitionLoop::createRelation(
    const Perception::CandidateRelation &relation,
    const std::string &source_url) {

  if (!language_system_)
    return false;

  // Ensure subject, predicate, object tokens exist
  std::size_t subj_id = 0;
  if (!language_system_->getTokenId(relation.subject.text, subj_id)) {
    language_system_->createToken(relation.subject.text,
                                  Core::LanguageSystem::TokenType::Word, {});
    language_system_->getTokenId(relation.subject.text, subj_id);
  }

  std::size_t pred_id = 0;
  if (!language_system_->getTokenId(relation.predicate.text, pred_id)) {
    language_system_->createToken(relation.predicate.text,
                                  Core::LanguageSystem::TokenType::Word, {});
    language_system_->getTokenId(relation.predicate.text, pred_id);
  }

  std::size_t obj_id = 0;
  if (!language_system_->getTokenId(relation.object.text, obj_id)) {
    language_system_->createToken(relation.object.text,
                                  Core::LanguageSystem::TokenType::Word, {});
    language_system_->getTokenId(relation.object.text, obj_id);
  }

  // Update confidence for all related tokens
  token_occurrences_[relation.subject.text]++;
  token_occurrences_[relation.predicate.text]++;
  token_occurrences_[relation.object.text]++;

  updateTokenConfidence(relation.subject.text);
  updateTokenConfidence(relation.predicate.text);
  updateTokenConfidence(relation.object.text);

  std::cout << "[LanguageAcquisitionLoop] Learned relation: "
            << relation.subject.text << " " << relation.predicate.text << " "
            << relation.object.text << " (source: " << source_url << ")\n";

  return true;
}

void LanguageAcquisitionLoop::updateTokenConfidence(const std::string &token) {
  int occurrences = token_occurrences_[token];

  // Logarithmic confidence growth based on occurrences
  // Starts at min_token_confidence, approaches 1.0 asymptotically
  float base_conf = config_.min_token_confidence;
  float growth = std::log(1.0f + occurrences) / std::log(10.0f); // log10 growth
  float confidence = std::min(1.0f, base_conf + growth * 0.2f);

  token_confidence_[token] = confidence;
}

bool LanguageAcquisitionLoop::shouldLearnToken(const std::string &token) const {
  auto it = token_occurrences_.find(token);
  if (it == token_occurrences_.end())
    return false;

  return it->second >= config_.min_occurrences_to_learn;
}

} // namespace Language
} // namespace NeuroForge
