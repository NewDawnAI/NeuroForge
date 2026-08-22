# Run Stage D Meta-Cognition Experiment
$buildDir = "build"
$exePath = "$buildDir/neuroforge.exe"

if (-not (Test-Path $exePath)) {
    # Fallback to Release folder if using MSVC generator structure
    $exePath = "$buildDir/Release/neuroforge.exe"
}

if (-not (Test-Path $exePath)) {
    Write-Host "Executable not found at $exePath"
    exit 1
}

Write-Host "Starting NeuroForge with Meta-Cognition (Stage D)..."
# Run for 2000 steps to allow for multiple regulation cycles (every 500 steps)
# Enable embodiment to generate variable CIP/Reward
# Enable Phase 15 to generate ethics data
& $exePath --embodiment=on --unified-substrate=on --phase15=on --steps=2000 --memory-db=stage_d_test.db
