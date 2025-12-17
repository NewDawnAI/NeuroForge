# Biases Calibration and Ethics

Version: v0.15.0-rc1-12-gdf2999b5
Timestamp: 2025-12-08

## Bias Catalog
- Vision: ContrastEdge, FaceDetection, Motion
- Audio: Voice, Temporal
- Intrinsic: Novelty, Survival, SocialPerception

## Parameters
- Defaults and ranges per bias; attention boosts; reward shaping coefficients

## Calibration
- Dataset selection; cross-modal alignment; measure salience and coherence; tune for stability

## Ethics Integration
- ActionFilter + Phase15EthicsRegulator: deny/freeze signals; logging reasons; sandbox boundaries

## Validation
- Metrics: reward stability, assembly coherence, denial rates, false positives/negatives

