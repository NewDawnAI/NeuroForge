@0xf2e4d8c1a9b7e356;

# NeuroForge Brain State Persistence Schema
# This schema provides a specialized interface for saving/loading complete brain states
# It imports the comprehensive definitions from neuroforge.capnp

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("NeuroForge::BrainState");

using import "neuroforge.capnp".Brain;
using import "neuroforge.capnp".BrainState;
using import "neuroforge.capnp".ProcessingMode;

# Top-level structure for brain state persistence files
struct BrainStateFile {
  # File format metadata
  fileFormatVersion @0 :Int32 = 1;
  createdTimestamp @1 :UInt64;    # Unix timestamp when file was created
  createdBy @2 :Text = "NeuroForge";
  description @3 :Text;           # Optional description of the brain state
  
  # The actual brain data (imported from neuroforge.capnp)
  brain @4 :Brain;
  
  # Additional metadata for persistence
  metadata @5 :PersistenceMetadata;
}

struct PersistenceMetadata {
  # Runtime statistics at time of save
  totalNeurons @0 :UInt64;
  totalSynapses @1 :UInt64;
  totalRegions @2 :UInt32;
  avgNeuronActivation @3 :Float32;
  avgSynapseWeight @4 :Float32;
  
  # Performance metrics
  processingFrequencyHz @5 :Float32;
  lastUpdateDurationMs @6 :Float32;
  memoryUsageMB @7 :Float32;
  
  # Plasticity and learning state
  totalPlasticUpdates @8 :UInt64;
  learningEnabled @9 :Bool;
  
  # Compatibility info
  neuroforgeVersion @10 :Text;
  schemaVersion @11 :Text;
  
  # Optional checksum for integrity verification
  dataChecksum @12 :Text;
}

# For future extensibility - additional state that might be needed
struct ExtendedState {
  # Reserved for future use
  customData @0 :Data;
  extensionFields @1 :List(KeyValuePair);
}

struct KeyValuePair {
  key @0 :Text;
  value @1 :Text;
}