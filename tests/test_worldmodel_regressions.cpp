#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

#include "perception/world/WorldEncoder.h"
#include "perception/world/WorldPredictor.h"
#include "perception/world/WorldState.h"

static float max_abs_diff(const std::vector<float> &a,
                          const std::vector<float> &b) {
  float m = 0.0f;
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    m = std::max(m, std::fabs(a[i] - b[i]));
  }
  if (a.size() != b.size()) {
    m = std::max(m, 1.0f);
  }
  return m;
}

int main() {
  using namespace NeuroForge::Perception;

  int failed = 0;

  {
    WorldEncoderConfig cfg;
    cfg.random_seed = 1234;
    WorldEncoder e1(cfg);
    WorldEncoder e2(cfg);

    std::vector<float> visual(cfg.visual_input_dim, 0.1f);
    std::vector<float> semantic(cfg.semantic_input_dim, 0.2f);
    WorldState s1 = e1.encode(visual, {}, {}, {}, {}, {}, {}, semantic);
    WorldState s2 = e2.encode(visual, {}, {}, {}, {}, {}, {}, semantic);

    float md = max_abs_diff(s1.latent, s2.latent);
    if (md > 1e-6f) {
      std::cerr << "determinism_same_seed_failed max_diff=" << md << "\n";
      failed++;
    }
  }

  {
    WorldEncoderConfig cfg1;
    cfg1.random_seed = 1234;
    WorldEncoderConfig cfg2;
    cfg2.random_seed = 1235;
    WorldEncoder e1(cfg1);
    WorldEncoder e2(cfg2);

    std::vector<float> visual(cfg1.visual_input_dim, 0.1f);
    std::vector<float> semantic(cfg1.semantic_input_dim, 0.2f);
    WorldState s1 = e1.encode(visual, {}, {}, {}, {}, {}, {}, semantic);
    WorldState s2 = e2.encode(visual, {}, {}, {}, {}, {}, {}, semantic);

    float md = max_abs_diff(s1.latent, s2.latent);
    if (md < 1e-4f) {
      std::cerr << "determinism_diff_seed_failed max_diff=" << md << "\n";
      failed++;
    }
  }

  {
    WorldPredictorConfig pcfg;
    pcfg.enable_learning = false;
    pcfg.action_dim = 2;
    WorldPredictor p(pcfg, 8);

    WorldState cur(8);
    for (std::size_t i = 0; i < cur.latent.size(); ++i) {
      cur.latent[i] = 0.01f * static_cast<float>(i + 1);
    }
    cur.normalize();

    std::vector<float> a1 = {1.0f, 0.0f};
    std::vector<float> a2 = {0.0f, 1.0f};

    WorldState pred1 = p.predict(cur, a1);
    WorldState pred2 = p.predict(cur, a2);
    if (pred1.distanceTo(pred2) > 1e-6f) {
      std::cerr << "action_predictor_initially_diff_failed\n";
      failed++;
    }

    WorldState observed(8);
    observed.latent = pred1.latent;
    if (!observed.latent.empty()) {
      observed.latent[0] += 0.2f;
    }
    observed.normalize();

    float dn = p.updateModel(pred1, observed, a1, 0.2f);
    if (dn <= 0.0f) {
      std::cerr << "action_predictor_update_failed delta_norm=" << dn << "\n";
      failed++;
    }

    WorldState post1 = p.predict(cur, a1);
    WorldState post2 = p.predict(cur, a2);
    if (post1.distanceTo(post2) < 1e-4f) {
      std::cerr << "action_predictor_no_effect_failed dist="
                << post1.distanceTo(post2) << "\n";
      failed++;
    }
  }

  if (failed == 0) {
    std::cout << "OK\n";
  }
  return failed == 0 ? 0 : 1;
}

