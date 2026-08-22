@0xb8e0f8e1c5d3a7f9;  # Unique file ID

# NeuroForge Boundary Schemas
# Cap'n Proto = nervous system + memory spine, NOT brain tissue
#
# ALLOWED: ReplayFrame, Observation, ActionCommand, AccountabilityEvent
# FORBIDDEN: ConceptNode internals, Preference vectors, Arbitration heuristics

struct ReplayFrame {
  frameId @0 :UInt64;
  timestampMs @1 :UInt64;
  frameType @2 :Text;  # "perception", "action", "decision", "expression"
  
  # Observation data
  observationType @3 :Text;
  observationData @4 :Data;
  
  # Action data
  actionKind @5 :Text;
  actionParameters @6 :Text;
  actionOutcome @7 :Text;
  
  # Verification
  wasVerified @8 :Bool;
  verificationConfidence @9 :Float32;
  verificationSource @10 :Text;
  
  # Accountability
  justificationId @11 :Text;
  activeRole @12 :Text;
  activeContract @13 :Text;
}

struct Observation {
  id @0 :UInt64;
  timestampMs @1 :UInt64;
  source @2 :Text;  # "web", "camera", "video", "user"
  contentType @3 :Text;  # "text", "image", "audio"
  contentData @4 :Data;
  metadataJson @5 :Text;
}

struct ActionCommand {
  id @0 :UInt64;
  timestampMs @1 :UInt64;
  kind @2 :Text;  # "browse", "speak", "verify"
  parametersJson @3 :Text;
  safetyLevel @4 :Text;
  expectedOutcome @5 :Text;
  originatingFrameId @6 :UInt64;
}

struct AccountabilityEvent {
  eventId @0 :UInt64;
  timestampMs @1 :UInt64;
  eventType @2 :Text;  # "action_executed", "action_blocked", etc.
  actionId @3 :Text;
  justificationTraceId @4 :Text;
  activeRole @5 :Text;
  activeContract @6 :Text;
  outcome @7 :Text;
  permitted @8 :Bool;
  explanation @9 :Text;
}

struct SessionSummary {
  sessionId @0 :UInt64;
  startMs @1 :UInt64;
  endMs @2 :UInt64;
  durationSeconds @3 :UInt32;
  
  # Exploration metrics
  pagesVisited @4 :UInt32;
  charactersRead @5 :UInt64;
  vocabularySize @6 :UInt32;
  relationsFormed @7 :UInt32;
  
  # Preference evolution
  topicCount @8 :UInt32;
  domainCount @9 :UInt32;
  explorationEntropy @10 :Float32;
  avgCuriosityScore @11 :Float32;
  
  # Mode
  mode @12 :Text;  # "seed_first", "curiosity_first"
  configJson @13 :Text;
}

struct ExpressionAuditRecord {
  recordId @0 :UInt64;
  timestampMs @1 :UInt64;
  outputText @2 :Text;
  expressionType @3 :Text;  # "DESCRIBE", "ANSWER", etc.
  
  # Evidence chain
  conceptIds @4 :List(UInt64);
  evidenceFrameIds @5 :List(UInt64);
  
  # Normative context
  normsApplied @6 :List(Text);
  valuesChecked @7 :List(Text);
  rolesActive @8 :List(Text);
  
  # Status
  status @9 :Text;  # "PERMITTED", "BLOCKED", "RESTRICTED"
  blockedReason @10 :Text;
  
  # Confidence
  overallConfidence @11 :Float32;
  groundedTokens @12 :UInt32;
  totalTokens @13 :UInt32;
}
