#include "core/ReasoningTrace.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>

namespace NeuroForge {
namespace Core {

static std::string lowerCopy(std::string s) {
  for (auto &c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

void HypothesisBuffer::clear() {
  premises_.clear();
  counter_.clear();
}

void HypothesisBuffer::addPremise(FactNode f) {
  premises_.push_back(std::move(f));
}

void HypothesisBuffer::addCounter(FactNode f) {
  counter_.push_back(std::move(f));
}

bool HypothesisBuffer::hasContradiction() const {
  std::unordered_map<std::string, int> polarity_by_key;
  polarity_by_key.reserve(premises_.size() + counter_.size());

  auto scan = [&](const std::vector<FactNode> &facts) {
    for (const auto &f : facts) {
      std::string key =
          lowerCopy(f.subject) + "|" + ReasoningFirstEngine::predicateBase(f.predicate) +
          "|" + lowerCopy(f.object);
      int pol = f.polarity;
      auto it = polarity_by_key.find(key);
      if (it == polarity_by_key.end()) {
        polarity_by_key.emplace(std::move(key), pol);
      } else {
        if (it->second != pol) return true;
      }
    }
    return false;
  };

  if (scan(premises_)) return true;
  if (scan(counter_)) return true;

  std::unordered_map<std::string, int> polarity_any;
  for (const auto &kv : polarity_by_key) {
    polarity_any.emplace(kv.first, kv.second);
  }
  for (const auto &kv : polarity_any) {
    auto it = polarity_by_key.find(kv.first);
    if (it != polarity_by_key.end() && it->second != kv.second) return true;
  }

  return false;
}

float HypothesisBuffer::aggregateSupportConfidence(float epsilon) const {
  float support = 0.0f;
  float conflict = 0.0f;

  for (const auto &f : premises_) {
    support += std::max(0.0f, f.confidence) * std::max(0.0f, f.trust);
  }
  for (const auto &f : counter_) {
    conflict += std::max(0.0f, f.confidence) * std::max(0.0f, f.trust);
  }

  float denom = support + conflict + epsilon;
  if (denom <= 0.0f) return 0.0f;
  return std::clamp(support / denom, 0.0f, 1.0f);
}

float ReasoningFirstEngine::sourceTrustFromUrl(const std::string &url) {
  static const std::vector<std::pair<std::string, float>> TRUST = {
      {"britannica.com", 0.95f},
      {"wikipedia.org", 0.90f},
      {"simple.wikipedia.org", 0.85f},
  };
  for (const auto &p : TRUST) {
    if (url.find(p.first) != std::string::npos) return p.second;
  }
  return 0.50f;
}

int ReasoningFirstEngine::predicatePolarity(const std::string &predicate) {
  std::string p = lowerCopy(predicate);
  if (p.rfind("not_", 0) == 0) return -1;
  if (p.rfind("does_not_", 0) == 0) return -1;
  if (p.rfind("do_not_", 0) == 0) return -1;
  if (p.rfind("is_not_", 0) == 0) return -1;
  return 1;
}

std::string ReasoningFirstEngine::predicateBase(const std::string &predicate) {
  std::string p = lowerCopy(predicate);
  auto strip = [&](const std::string &prefix) {
    if (p.rfind(prefix, 0) == 0) p = p.substr(prefix.size());
  };
  strip("does_not_");
  strip("do_not_");
  strip("is_not_");
  strip("not_");
  if (p.size() > 3 && p.back() == 's' && p != "is" && p != "was" && p != "has") {
    p.pop_back();
  }
  return p;
}

static bool equalsFold(const std::string &a, const std::string &b) {
  return lowerCopy(a) == lowerCopy(b);
}

static bool isAntonym(const std::string &a, const std::string &b) {
  static const std::unordered_map<std::string, std::vector<std::string>> ANTONYM = {
      {"useful", {"useless", "harmful"}},
      {"useless", {"useful"}},
      {"harmful", {"useful"}},
      {"safe", {"dangerous"}},
      {"dangerous", {"safe"}},
      {"true", {"false"}},
      {"false", {"true"}},
  };
  auto la = lowerCopy(a);
  auto lb = lowerCopy(b);
  auto it = ANTONYM.find(la);
  if (it != ANTONYM.end()) {
    for (const auto &x : it->second) {
      if (x == lb) return true;
    }
  }
  return false;
}

static bool startsWith(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

static std::pair<std::string, std::string>
normalizePredicateObject(const std::string &predicate,
                         const std::string &object) {
  std::string pred = ReasoningFirstEngine::predicateBase(predicate);
  std::string obj = lowerCopy(object);

  if (startsWith(pred, "is_")) {
    std::string adjective = pred.substr(3);
    if (!adjective.empty()) {
      pred = "is";
      obj = adjective;
    }
  }

  if (pred != "is" && obj.empty()) {
    std::string base = ReasoningFirstEngine::predicateBase(predicate);
    if (!base.empty() && base != "is" && base != "is_a" && base != "used_for") {
      pred = "is";
      obj = lowerCopy(base);
    }
  }

  return {pred, obj};
}

void ReasoningFirstEngine::scanForCounterEvidence(
    const std::string &subject, const std::string &predicate,
    const std::string &object, const std::vector<FactNode> &facts,
    ReasoningTrace &trace) {
  auto [target_pred, target_obj] = normalizePredicateObject(predicate, object);
  if (subject.empty() || target_pred.empty()) return;

  for (const auto &f : facts) {
    if (!equalsFold(f.subject, subject)) continue;

    auto [cand_pred, cand_obj] = normalizePredicateObject(f.predicate, f.object);
    if (cand_pred.empty()) continue;

    bool direct_negation =
        (cand_pred == target_pred && cand_obj == target_obj && f.polarity < 0);
    bool antonym_object =
        (cand_pred == target_pred && isAntonym(target_obj, cand_obj));
    bool antonym_predicate =
        (isAntonym(target_pred, cand_pred) &&
         equalsFold(f.object, object));

    bool negated_adjective =
        (target_pred == "is" && cand_obj == target_obj && f.polarity < 0);

    if (direct_negation || antonym_object || antonym_predicate ||
        negated_adjective) {
      trace.counter_evidence.push_back(f);
    }
  }
}

ReasoningTrace ReasoningFirstEngine::reasonIsUseful(
    const std::string &focus, const std::vector<FactNode> &facts) {
  ReasoningTrace out;
  if (focus.empty()) {
    out.failure_code = ReasoningFailure::PremiseGap;
    out.missing_premises.push_back({focus, "used_for", "<something>"});
    out.missing_premises.push_back({focus, "is", "useful"});
    return out;
  }
  if (facts.empty()) {
    out.failure_code = ReasoningFailure::EpistemicVoid;
    return out;
  }

  HypothesisBuffer buf;

  const FactNode *best_used_for = nullptr;
  for (const auto &f : facts) {
    if (!equalsFold(f.subject, focus)) continue;
    if (predicateBase(f.predicate) != "used_for") continue;
    if (!best_used_for || f.confidence > best_used_for->confidence) {
      best_used_for = &f;
    }
  }
  if (!best_used_for) {
    scanForCounterEvidence(focus, "is", "useful", facts, out);
    out.failure_code = out.counter_evidence.empty()
                           ? ReasoningFailure::PremiseGap
                           : ReasoningFailure::LogicalContradiction;
    if (out.failure_code == ReasoningFailure::PremiseGap) {
      out.missing_premises.push_back({focus, "used_for", "<something>"});
      out.missing_premises.push_back({focus, "is", "useful"});
    }
    return out;
  }

  const FactNode *best_useful = nullptr;
  for (const auto &f : facts) {
    if (!equalsFold(f.subject, best_used_for->object)) continue;
    std::string base = predicateBase(f.predicate);
    bool useful =
        (base == "is" && equalsFold(f.object, "useful")) ||
        (base == "is_a" && equalsFold(f.object, "useful"));
    if (!useful) continue;
    if (!best_useful || f.confidence > best_useful->confidence) {
      best_useful = &f;
    }
  }
  if (!best_useful) {
    scanForCounterEvidence(focus, "is", "useful", facts, out);
    out.failure_code = out.counter_evidence.empty()
                           ? ReasoningFailure::PremiseGap
                           : ReasoningFailure::LogicalContradiction;
    buf.addPremise(*best_used_for);
    out.premises = buf.premises();
    out.aggregate_confidence = buf.aggregateSupportConfidence();
    if (out.failure_code == ReasoningFailure::PremiseGap) {
      out.missing_premises.push_back({best_used_for->object, "is", "useful"});
      out.missing_premises.push_back({focus, "is", "useful"});
    }
    return out;
  }

  buf.addPremise(*best_used_for);
  buf.addPremise(*best_useful);

  {
    ReasoningTrace tmp;
    scanForCounterEvidence(focus, "is", "useful", facts, tmp);
    for (auto &c : tmp.counter_evidence) {
      buf.addCounter(c);
      out.counter_evidence.push_back(std::move(c));
    }
  }

  if (buf.hasContradiction()) {
    out.failure_code = ReasoningFailure::LogicalContradiction;
    out.premises = buf.premises();
    if (out.counter_evidence.empty()) out.counter_evidence = buf.counterEvidence();
    out.aggregate_confidence = 0.0f;
    return out;
  }

  FactNode c;
  c.subject = focus;
  c.predicate = "is";
  c.object = "useful";
  c.evidence = "inferred";
  c.source = "inference";
  c.window_index = std::max(best_used_for->window_index, best_useful->window_index);
  float base = std::min(best_used_for->confidence * best_used_for->trust,
                        best_useful->confidence * best_useful->trust);
  c.confidence = std::clamp(base * 0.65f, 0.0f, 1.0f);
  c.trust = 1.0f;

  out.conclusion = c;
  out.premises = buf.premises();
  if (!out.counter_evidence.empty()) {
    out.failure_code = ReasoningFailure::LogicalContradiction;
    out.conclusion.reset();
    out.aggregate_confidence = buf.aggregateSupportConfidence();
  } else {
    out.counter_evidence = buf.counterEvidence();
    out.aggregate_confidence = std::clamp(c.confidence, 0.0f, 1.0f);
    out.failure_code = ReasoningFailure::None;
  }
  return out;
}

} // namespace Core
} // namespace NeuroForge
