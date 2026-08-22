# Neurons, Synapses, and Regions API

Version: v0.16.0-auditory-enhanced
Timestamp: 2026-01-11

## Neuron
- Fields: id, activation, refractory, mito, inputs, outputs
- Methods: activate(), addInput(), addOutput(), onSpike(cb)

## Synapse
- Fields: id, weight, delay, plasticity, eligibility, stats
- Methods: transmit(), updateHebbian(), updateSTDP()

## Region
- Fields: id, neurons[], connections, stats
- Methods: createNeurons(n), connectNeurons(a,b), connectToRegion(r), step()

## AuditoryCortex (Specialized Region)
- Inherits from: Region
- Purpose: Advanced speech processing and spectral analysis
- Methods:
  - `processAudioInput(std::vector<float>& audio_data)`: Processes raw audio samples using FFT and tonotopic mapping.
  - `detectFeatures()`: Analyzes spectral peaks to detect Pitch and Phonemes.
  - `getDetectedFeatures()`: Returns a list of detected `SoundFeature` enums.
- Features:
  - **FFT-Based Analysis**: Real-time spectral decomposition with Hanning windowing.
  - **Tonotopic Mapping**: Frequency-specific neuron activation (20Hz - 20kHz).
  - **Pitch Detection**: Fundamental frequency identification (< 1000Hz).
  - **Phoneme Recognition**: Multi-formant detection for speech processing (200Hz-4000Hz).
  - **Integration**: Direct feed from `SubstratePhaseCAdapter`.

## Update Rules
- Hebbian: Δw = η·pre·post
- STDP: pairwise timing windows; eligibility traces

## Serialization
- Snapshot: neuron activations, synapse weights, layout metadata
