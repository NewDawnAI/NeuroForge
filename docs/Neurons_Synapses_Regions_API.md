# Neurons, Synapses, and Regions API

Version: v0.15.0-rc1-12-gdf2999b5
Timestamp: 2025-12-08

## Neuron
- Fields: id, activation, refractory, mito, inputs, outputs
- Methods: activate(), addInput(), addOutput(), onSpike(cb)

## Synapse
- Fields: id, weight, delay, plasticity, eligibility, stats
- Methods: transmit(), updateHebbian(), updateSTDP()

## Region
- Fields: id, neurons[], connections, stats
- Methods: createNeurons(n), connectNeurons(a,b), connectToRegion(r), step()

## Update Rules
- Hebbian: Δw = η·pre·post
- STDP: pairwise timing windows; eligibility traces

## Serialization
- Snapshot: neuron activations, synapse weights, layout metadata

