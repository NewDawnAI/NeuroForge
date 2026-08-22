#include "core/Agent2EpistemicReviewer.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace NeuroForge {
namespace Core {

static std::string trimCopy(std::string s) {
  auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
  while (!s.empty() && isSpace(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && isSpace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  return s;
}

static std::string toLowerCopy(std::string s) {
  for (auto &c : s) {
    c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

static bool startsWith(const std::string &s, const std::string &prefix) {
  return s.rfind(prefix, 0) == 0;
}

std::optional<Agent2Provenance>
Agent2EpistemicReviewer::parseProvenanceFromAnswer(const std::string &answer) {
  auto pos = answer.find("(source:");
  if (pos == std::string::npos) return std::nullopt;

  std::size_t end = std::string::npos;
  int depth = 0;
  for (std::size_t i = pos; i < answer.size(); ++i) {
    char c = answer[i];
    if (c == '(') {
      depth++;
    } else if (c == ')') {
      depth--;
      if (depth == 0) {
        end = i;
        break;
      }
    }
  }
  if (end == std::string::npos || end <= pos) return std::nullopt;

  std::string inside = answer.substr(pos + std::string("(source:").size(),
                                     end - (pos + std::string("(source:").size()));
  inside = trimCopy(inside);

  Agent2Provenance p;

  std::vector<std::string> parts;
  {
    std::istringstream iss(inside);
    std::string part;
    while (std::getline(iss, part, '|')) {
      parts.push_back(trimCopy(part));
    }
  }

  if (!parts.empty()) {
    p.source = parts[0];
  }

  for (std::size_t i = 1; i < parts.size(); ++i) {
    const auto &kv = parts[i];
    if (startsWith(kv, "window=")) {
      try {
        p.window_index = std::stoi(kv.substr(std::string("window=").size()));
      } catch (...) {
        p.window_index = -1;
      }
    } else if (startsWith(kv, "evidence=")) {
      p.evidence = trimCopy(kv.substr(std::string("evidence=").size()));
    }
  }

  auto seen_pos = answer.find("(seen_in_pages=", end);
  if (seen_pos != std::string::npos) {
    auto seen_end = answer.find(')', seen_pos);
    if (seen_end != std::string::npos) {
      std::string num = answer.substr(
          seen_pos + std::string("(seen_in_pages=").size(),
          seen_end - (seen_pos + std::string("(seen_in_pages=").size()));
      num = trimCopy(num);
      try {
        p.seen_in_pages = std::stoi(num);
      } catch (...) {
        p.seen_in_pages = 0;
      }
    }
  }

  if (p.source.empty()) return std::nullopt;
  return p;
}

Agent2ReviewResult Agent2EpistemicReviewer::review(const Agent2ReviewInput &in) const {
  Agent2ReviewResult out;
  out.provenance = parseProvenanceFromAnswer(in.answer);

  auto addFinding = [&](Agent2ReviewSeverity severity, std::string code,
                        std::string message, std::string suggestion) {
    Agent2ReviewFinding f;
    f.severity = severity;
    f.code = std::move(code);
    f.message = std::move(message);
    f.suggestion = std::move(suggestion);
    out.findings.push_back(std::move(f));
  };

  std::string q = toLowerCopy(in.question);
  bool wants_used_for = q.find("used for") != std::string::npos;
  bool wants_what_is = q.rfind("what is ", 0) == 0 || q.find(" what is ") != std::string::npos;

  if (!out.provenance.has_value()) {
    addFinding(Agent2ReviewSeverity::Warning, "no_provenance",
               "Answer has no provenance suffix.", "");
  } else {
    const auto &p = out.provenance.value();
    if (p.window_index < 0) {
      addFinding(Agent2ReviewSeverity::Warning, "bad_window_index",
                 "Provenance window index missing or invalid.", "");
    }
    if (p.evidence.empty()) {
      addFinding(Agent2ReviewSeverity::Notice, "missing_evidence_type",
                 "Provenance has no evidence type.", "");
    }

    if (wants_used_for && p.evidence != "used_for") {
      addFinding(Agent2ReviewSeverity::Notice, "intent_mismatch",
                 "Question intent is used_for but winning evidence is not used_for.",
                 "Answer may not address usage; consider qualifying uncertainty.");
    }
    if (wants_what_is && p.evidence == "segment") {
      addFinding(Agent2ReviewSeverity::Notice, "weak_support",
                 "Definition answered from a segment, not a relation or gate.", "");
    }
  }

  out.overall = Agent2ReviewSeverity::Ok;
  for (const auto &f : out.findings) {
    if (f.severity == Agent2ReviewSeverity::Warning) {
      out.overall = Agent2ReviewSeverity::Warning;
      break;
    }
    if (f.severity == Agent2ReviewSeverity::Notice) {
      out.overall = Agent2ReviewSeverity::Notice;
    }
  }

  return out;
}

Agent2ReviewResult Agent2EpistemicReviewer::review(
    const Agent2ReviewInput &in,
    const std::vector<ReasoningStep> &steps) const {
  auto out = review(in);

  auto addFinding = [&](Agent2ReviewSeverity severity, std::string code,
                        std::string message, std::string suggestion) {
    Agent2ReviewFinding f;
    f.severity = severity;
    f.code = std::move(code);
    f.message = std::move(message);
    f.suggestion = std::move(suggestion);
    out.findings.push_back(std::move(f));
  };

  for (const auto &s : steps) {
    if (s.derived_confidence < 0.35f) {
      addFinding(Agent2ReviewSeverity::Notice, "weak_inference",
                 "Derived confidence is low for a reasoning step.",
                 "Inference confidence is low; qualify uncertainty.");
      break;
    }
  }

  out.overall = Agent2ReviewSeverity::Ok;
  for (const auto &f : out.findings) {
    if (f.severity == Agent2ReviewSeverity::Warning) {
      out.overall = Agent2ReviewSeverity::Warning;
      break;
    }
    if (f.severity == Agent2ReviewSeverity::Notice) {
      out.overall = Agent2ReviewSeverity::Notice;
    }
  }

  return out;
}

Agent2ReviewResult Agent2EpistemicReviewer::review(const Agent2ReviewInput &in,
                                                  const ReasoningTrace &trace) const {
  auto out = review(in);

  auto addFinding = [&](Agent2ReviewSeverity severity, std::string code,
                        std::string message, std::string suggestion) {
    Agent2ReviewFinding f;
    f.severity = severity;
    f.code = std::move(code);
    f.message = std::move(message);
    f.suggestion = std::move(suggestion);
    out.findings.push_back(std::move(f));
  };

  if (trace.failure_code == ReasoningFailure::PremiseGap) {
    addFinding(Agent2ReviewSeverity::Notice, "premise_gap",
               "Reasoning could not find required premises.",
               "Insufficient premises; consider qualifying uncertainty.");
    if (!trace.missing_premises.empty()) {
      addFinding(Agent2ReviewSeverity::Notice, "missing_premise",
                 "Key evidence required for the conclusion is absent.",
                 "Key evidence absent; consider qualifying uncertainty.");
    }
  } else if (trace.failure_code == ReasoningFailure::LogicalContradiction) {
    addFinding(Agent2ReviewSeverity::Warning, "logical_contradiction",
               "Reasoning encountered contradictory premises.",
               "Contradiction detected; avoid drawing a strong conclusion.");
  } else if (trace.failure_code == ReasoningFailure::EpistemicVoid) {
    addFinding(Agent2ReviewSeverity::Warning, "epistemic_void",
               "No factual basis available for reasoning.",
               "No evidence available; avoid asserting a conclusion.");
  }

  if (trace.failure_code == ReasoningFailure::None &&
      trace.aggregate_confidence < 0.45f) {
    addFinding(Agent2ReviewSeverity::Notice, "weak_inference",
               "Aggregate confidence is low for a derived conclusion.",
               "Inference confidence is low; qualify uncertainty.");
  }

  out.overall = Agent2ReviewSeverity::Ok;
  for (const auto &f : out.findings) {
    if (f.severity == Agent2ReviewSeverity::Warning) {
      out.overall = Agent2ReviewSeverity::Warning;
      break;
    }
    if (f.severity == Agent2ReviewSeverity::Notice) {
      out.overall = Agent2ReviewSeverity::Notice;
    }
  }

  return out;
}

} // namespace Core
} // namespace NeuroForge
