# NeuroForge Autonomous Web Agent Demo
# This script launches the fully integrated Stage E system.

$BuildDir = "build-msvc/Release"
$ExePath = Join-Path $BuildDir "neuroforge.exe"

if (-not (Test-Path $ExePath)) {
    # Fallback to standard build directory if MSVC build is missing
    $BuildDir = "build"
    $ExePath = Join-Path $BuildDir "neuroforge.exe"
}

if (-not (Test-Path $ExePath)) {
    Write-Host "Error: neuroforge.exe not found. Please build the project first." -ForegroundColor Red
    exit 1
}

Write-Host "Starting NeuroForge Autonomous Web Agent..." -ForegroundColor Cyan
Write-Host "Target: YouTube (Rich multimodal input source)" -ForegroundColor Gray
Write-Host "Mode: Unified Substrate + Web Sandbox + Cross-Modal Processing" -ForegroundColor Gray
Write-Host "--------------------------------------------------------"

# Launch arguments:
# --sandbox=on          : Enables the Web Sandbox window
# --sandbox-url=...     : Sets the target website
# --steps=1000          : Runs for 1000 cognitive steps (approx 1-2 mins depending on speed)
# --autonomous=on       : Enables the M7 autonomy loop (curiosity/motivation)
# --unified-substrate=on: Ensures all cognitive systems (Language, Memory, etc.) are active

$Args = @(
    "--sandbox=on",
    "--sandbox-url=https://www.youtube.com", 
    "--steps=1000",
    "--autonomous=on",
    "--curiosity-threshold=0.2",
    "--hippocampal=on"
)

Start-Process -FilePath $ExePath -ArgumentList $Args -NoNewWindow -Wait

Write-Host "Demo Completed." -ForegroundColor Green
