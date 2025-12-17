@0xbcd2c890f31dc5c7;

# NeuroForge Cap'n Proto schema for brain persistence

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("NeuroForge::Schema");

# Enums mirroring C++ definitions

enum SynapseType {
  excitatory @0;  # NeuroForge::SynapseType::Excitatory = 0
  inhibitory @1;  # NeuroForge::SynapseType::Inhibitory = 1
  modulatory @2;  # NeuroForge::SynapseType::Modulatory = 2
}

enum PlasticityRule {
  none @0;        # Synapse::PlasticityRule::None
  hebbian @1;     # Synapse::PlasticityRule::Hebbian
  stdp @2;        # Synapse::PlasticityRule::STDP
  bcm @3;         # Synapse::PlasticityRule::BCM
  oja @4;         # Synapse::PlasticityRule::Oja
}

enum NeuronState {
  inactive @0;
  active @1;
  inhibited @2;
  refractory @3;
}

enum RegionType {
  cortical @0;
  subcortical @1;
  brainstem @2;
  special @3;
  custom @4;
}

enum ActivationPattern {
  synchronous @0;
  asynchronous @1;
  layered @2;
  competitive @3;
  oscillatory @4;
}

enum ProcessingMode {
  sequential @0;
  parallel @1;
  hierarchical @2;
  custom @3;
}

enum BrainState {
  uninitialized @0;
  initializing @1;
  running @2;
  paused @3;
  resetting @4;
  shutdown @5;
}

struct Neuron {
  id @0 :UInt64;
  activation @1 :Float32;
  state @2 :NeuronState;
  threshold @3 :Float32;
  decayRate @4 :Float32;
  refractoryTimer @5 :Float32;
  refractoryPeriod @6 :Float32;
  fireCount @7 :UInt64;
  processCount @8 :UInt64;
}

struct Synapse {
  id @0 :UInt64;
  sourceRegionId @1 :UInt32;
  targetRegionId @2 :UInt32;
  sourceNeuronId @3 :UInt64;
  targetNeuronId @4 :UInt64;
  weight @5 :Float32;
  initialWeight @6 :Float32;
  type @7 :SynapseType;
  plasticityRule @8 :PlasticityRule;
  learningRate @9 :Float32;
  delayMs @10 :Float32;
  minWeight @11 :Float32;
  maxWeight @12 :Float32;
  # Optional basic stats
  signalCount @13 :UInt64 = 0;
  updateCount @14 :UInt64 = 0;
}

struct RegionNameEntry { id @0 :UInt32; name @1 :Text; }

struct Region {
  id @0 :UInt32;
  name @1 :Text;
  type @2 :RegionType;
  activationPattern @3 :ActivationPattern;
  neurons @4 :List(Neuron);
  internalSynapses @5 :List(Synapse);
  isActive @6 :Bool;
  globalActivation @7 :Float32;
  processingCycles @8 :UInt64;
}

struct Brain {
  formatVersion @0 :Int32;
  processingMode @1 :ProcessingMode;
  brainState @2 :BrainState;
  targetFrequency @3 :Float32;
  processingCycles @4 :UInt64;
  regions @5 :List(Region);
  globalSynapses @6 :List(Synapse);
  regionNames @7 :List(RegionNameEntry);
  customUpdateOrder @8 :List(UInt32);
}