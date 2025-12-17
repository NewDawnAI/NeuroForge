#include "core/Phase9Metacognition.h"
#include "core/MemoryDB.h"
#include "core/Phase10SelfExplanation.h"
#include "core/Phase11SelfRevision.h"
#include "core/Phase12Consistency.h"
#include "core/Phase13AutonomyEnvelope.h"
#include "core/Phase14MetaReasoner.h"
#include "core/Phase15EthicsRegulator.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>

namespace NeuroForge {
namespace Core {

static inline std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

static double squash(double x) {
    if (x >= 10.0) return 1.0;
    if (x <= -10.0) return 0.0;
    return 1.0 / (1.0 + std::exp(-x));
}

static double latest_reward_norm(MemoryDB* db, std::int64_t run_id) {
    if (!db || run_id <= 0) {
        return 0.0;
    }
    auto rewards = db->getRecentRewards(run_id, 1);
    if (rewards.empty()) {
        return 0.0;
    }
    double r = rewards.front().reward;
    if (r > 5.0) r = 5.0;
    if (r < -5.0) r = -5.0;
    return r;
}

static double latest_consistency_score(MemoryDB* db, std::int64_t run_id) {
    if (!db || run_id <= 0) {
        return 0.5;
    }
    auto entries = db->getRecentConsistency(run_id, 1);
    if (entries.empty()) {
        return 0.5;
    }
    double s = entries.front().consistency_score;
    if (s < 0.0) s = 0.0;
    if (s > 1.0) s = 1.0;
    return s;
}

static bool latest_ethics_hard_block(MemoryDB* db, std::int64_t run_id) {
    if (!db || run_id <= 0) {
        return false;
    }
    auto entries = db->getRecentEthicsRegulator(run_id, 1);
    if (entries.empty()) {
        return false;
    }
    const auto& e = entries.front();
    return e.decision == "deny";
}

static double compute_stage7_trust(MemoryDB* db,
                                   std::int64_t run_id,
                                   double current_trust,
                                   double composite_error,
                                   double confidence) {
    const double lambda = 0.001;
    const double alpha0 = 0.01;
    const double beta0 = 0.05;

    double R_t = latest_reward_norm(db, run_id);
    double C_t = latest_consistency_score(db, run_id);

    double effective_error = composite_error * (1.0 + 0.5 * std::max(0.0, confidence));
    double delta = clamp01(effective_error);

    double alpha = alpha0 * (1.0 - clamp01(current_trust));
    double beta = beta0 * (1.0 + clamp01(current_trust));

    double G = squash(R_t) * C_t;
    double L = delta * delta;

    double retention = (1.0 - lambda) * current_trust;
    double t_next = retention + alpha * G - beta * L;
    t_next = clamp01(t_next);

    if (latest_ethics_hard_block(db, run_id)) {
        t_next = 0.0;
    }

    return t_next;
}

void Phase9Metacognition::registerNarrativePrediction(std::int64_t reflection_id,
                                                      double predicted_coherence_delta,
                                                      double confidence,
                                                      std::int64_t horizon_ms,
                                                      const std::string& targets_json) {
    const std::int64_t ts = now_ms();
    PendingPred p{ts, /*prediction_id*/0, reflection_id, predicted_coherence_delta, confidence, horizon_ms, targets_json};
    persistNarrativePrediction(p);
    pending_.push_back(p);
    if (pending_.size() > pending_limit_) pending_.pop_front();
}

void Phase9Metacognition::resolveActuals(double actual_coherence,
                                         double actual_goal_shift,
                                         const std::string& notes) {
    if (pending_.empty()) {
        double coherence_err = 0.0;
        double goal_mae = std::fabs(actual_goal_shift);
        double composite = 0.7 * coherence_err + 0.3 * goal_mae;
        self_trust_ = compute_stage7_trust(db_, run_id_, self_trust_, composite, 0.0);
        std::optional<double> trust_delta = prev_self_trust_.has_value() ? std::optional<double>(self_trust_ - *prev_self_trust_) : std::optional<double>();
        std::optional<double> coherence_delta = prev_coherence_err_.has_value() ? std::optional<double>(*prev_coherence_err_ - coherence_err) : std::optional<double>();
        std::optional<double> goal_accuracy_delta = prev_goal_mae_.has_value() ? std::optional<double>(*prev_goal_mae_ - goal_mae) : std::optional<double>();
        persistMetacognitionRow(now_ms(), self_trust_, /*narrative_rmse=*/coherence_err, /*goal_mae=*/goal_mae, /*ece=*/0.0, notes, trust_delta, coherence_delta, goal_accuracy_delta);
        prev_self_trust_ = self_trust_;
        prev_coherence_err_ = coherence_err;
        prev_goal_mae_ = goal_mae;
        if (phase10_selfexplainer_) {
            phase10_selfexplainer_->runForLatest("post-resolution attribution");
        }
        if (phase11_revision_) {
            phase11_revision_->maybeRevise("metacog heartbeat");
        }
    if (phase12_consistency_) {
        phase12_consistency_->runForLatest("metacog heartbeat");
    }
    if (phase13_autonomy_) {
        phase13_autonomy_->maybeAdjustEnvelope("metacog heartbeat");
    }
    if (phase14_metareason_) {
        phase14_metareason_->runForLatest("metacog heartbeat");
    }
    if (phase15_ethics_) {
        phase15_ethics_->runForLatest("metacog heartbeat");
    }
    return;
    }

    PendingPred p = pending_.front();
    pending_.pop_front();

    double coherence_err = std::fabs(actual_coherence - p.predicted_coherence_delta);
    double goal_mae = std::fabs(actual_goal_shift);
    double composite = 0.7 * coherence_err + 0.3 * goal_mae;
    self_trust_ = compute_stage7_trust(db_, run_id_, self_trust_, composite, p.confidence);

    if (db_ && p.prediction_id > 0) {
        std::int64_t ts_res = now_ms();
        std::ostringstream oss;
        oss << "{\"actual_coherence\":" << actual_coherence
            << ",\"actual_goal_shift\":" << actual_goal_shift
            << ",\"confidence\":" << p.confidence
            << ",\"notes\":\"" << notes << "\""
            << ",\"predicted_coherence_delta\":" << p.predicted_coherence_delta
            << ",\"error_abs\":" << coherence_err
            << "}";
        db_->insertPredictionResolution(run_id_, p.prediction_id, ts_res, /*observed_delta=*/actual_coherence, oss.str());
    }

    // Persist evaluation row
    std::optional<double> trust_delta = prev_self_trust_.has_value() ? std::optional<double>(self_trust_ - *prev_self_trust_) : std::optional<double>();
    std::optional<double> coherence_delta = prev_coherence_err_.has_value() ? std::optional<double>(*prev_coherence_err_ - coherence_err) : std::optional<double>();
    std::optional<double> goal_accuracy_delta = prev_goal_mae_.has_value() ? std::optional<double>(*prev_goal_mae_ - goal_mae) : std::optional<double>();
    persistMetacognitionRow(now_ms(), self_trust_, /*narrative_rmse=*/coherence_err, /*goal_mae=*/goal_mae, /*ece=*/0.0, notes, trust_delta, coherence_delta, goal_accuracy_delta);
    prev_self_trust_ = self_trust_;
    prev_coherence_err_ = coherence_err;
    prev_goal_mae_ = goal_mae;
    if (phase10_selfexplainer_) {
        phase10_selfexplainer_->runForLatest("post-resolution attribution");
    }
    if (phase11_revision_) {
        phase11_revision_->maybeRevise("post-resolution");
    }
    if (phase12_consistency_) {
        phase12_consistency_->runForLatest("post-resolution");
    }
    if (phase13_autonomy_) {
        phase13_autonomy_->maybeAdjustEnvelope("post-resolution");
    }
    if (phase14_metareason_) {
        phase14_metareason_->runForLatest("post-resolution");
    }
    if (phase15_ethics_) {
        phase15_ethics_->runForLatest("post-resolution");
    }
}

void Phase9Metacognition::persistMetacognitionRow(std::int64_t ts_ms,
                                                  double self_trust,
                                                  double narrative_rmse,
                                                  double goal_mae,
                                                  double ece,
                                                  const std::string& notes,
                                                  std::optional<double> trust_delta,
                                                  std::optional<double> coherence_delta,
                                                  std::optional<double> goal_accuracy_delta) {
    if (!db_) return;
    // Insert into metacognition table with delta fields
    db_->insertMetacognition(ts_ms,
                             self_trust,
                             narrative_rmse,
                             goal_mae,
                             ece,
                             notes,
                             trust_delta,
                             coherence_delta,
                             goal_accuracy_delta,
                             run_id_);
}

void Phase9Metacognition::persistNarrativePrediction(PendingPred& p) {
    if (!db_) return;
    std::int64_t out_id = 0;
    db_->insertNarrativePrediction(p.ts_ms,
                                   p.reflection_id,
                                   p.horizon_ms,
                                   p.predicted_coherence_delta,
                                   p.confidence,
                                   p.targets_json,
                                   run_id_,
                                   out_id);
    p.prediction_id = out_id;
}

} // namespace Core
} // namespace NeuroForge
