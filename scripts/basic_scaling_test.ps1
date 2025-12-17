# Basic NeuroForge Scaling Test
param([int]$MaxNeurons = 10000)

Write-Host "NeuroForge Basic Scaling Test" -ForegroundColor Green
Write-Host ""

# Test baseline (64 neurons)
Write-Host "Testing Baseline (64 neurons)..." -ForegroundColor Cyan
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$process = Start-Process -FilePath "build\Release\neuroforge.exe" -ArgumentList @("--steps=100", "--step-ms=10", "--enable-learning") -Wait -PassThru -NoNewWindow
$stopwatch.Stop()
$baselineTime = $stopwatch.ElapsedMilliseconds
$baselineSuccess = ($process.ExitCode -eq 0)

if ($baselineSuccess) {
    $baselineStepsPerSec = (100 * 1000.0) / $baselineTime
    Write-Host "  Success: $baselineTime ms, $([math]::Round($baselineStepsPerSec, 2)) steps/sec" -ForegroundColor Green
} else {
    Write-Host "  Failed: Exit code $($process.ExitCode)" -ForegroundColor Red
}
Write-Host ""

# Test 1K neurons
if ($MaxNeurons -ge 1000) {
    Write-Host "Testing 1K neurons..." -ForegroundColor Cyan
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $args = @("--steps=100", "--step-ms=10", "--enable-learning", "--add-region=visual:V1:250", "--add-region=auditory:A1:250", "--add-region=motor:M1:250", "--add-region=pfc:PFC:250")
    $process = Start-Process -FilePath "build\Release\neuroforge.exe" -ArgumentList $args -Wait -PassThru -NoNewWindow
    $stopwatch.Stop()
    $test1KTime = $stopwatch.ElapsedMilliseconds
    $test1KSuccess = ($process.ExitCode -eq 0)

    if ($test1KSuccess) {
        $test1KStepsPerSec = (100 * 1000.0) / $test1KTime
        Write-Host "  Success: $test1KTime ms, $([math]::Round($test1KStepsPerSec, 2)) steps/sec" -ForegroundColor Green
    } else {
        Write-Host "  Failed: Exit code $($process.ExitCode)" -ForegroundColor Red
    }
    Write-Host ""
}

# Test 5K neurons
if ($MaxNeurons -ge 5000) {
    Write-Host "Testing 5K neurons..." -ForegroundColor Cyan
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $args = @("--steps=50", "--step-ms=20", "--enable-learning", "--add-region=visual:V1:1000", "--add-region=auditory:A1:1000", "--add-region=motor:M1:1000", "--add-region=pfc:PFC:1000", "--add-region=hippocampus:HPC:1000")
    $process = Start-Process -FilePath "build\Release\neuroforge.exe" -ArgumentList $args -Wait -PassThru -NoNewWindow
    $stopwatch.Stop()
    $test5KTime = $stopwatch.ElapsedMilliseconds
    $test5KSuccess = ($process.ExitCode -eq 0)

    if ($test5KSuccess) {
        $test5KStepsPerSec = (50 * 1000.0) / $test5KTime
        Write-Host "  Success: $test5KTime ms, $([math]::Round($test5KStepsPerSec, 2)) steps/sec" -ForegroundColor Green
    } else {
        Write-Host "  Failed: Exit code $($process.ExitCode)" -ForegroundColor Red
    }
    Write-Host ""
}

# Test 10K neurons
if ($MaxNeurons -ge 10000) {
    Write-Host "Testing 10K neurons..." -ForegroundColor Cyan
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $args = @("--steps=25", "--step-ms=50", "--enable-learning", "--add-region=visual:V1:2000", "--add-region=auditory:A1:1500", "--add-region=motor:M1:1500", "--add-region=pfc:PFC:2000", "--add-region=hippocampus:HPC:1500", "--add-region=thalamus:TH:1000", "--add-region=brainstem:BS:500")
    $process = Start-Process -FilePath "build\Release\neuroforge.exe" -ArgumentList $args -Wait -PassThru -NoNewWindow
    $stopwatch.Stop()
    $test10KTime = $stopwatch.ElapsedMilliseconds
    $test10KSuccess = ($process.ExitCode -eq 0)

    if ($test10KSuccess) {
        $test10KStepsPerSec = (25 * 1000.0) / $test10KTime
        Write-Host "  Success: $test10KTime ms, $([math]::Round($test10KStepsPerSec, 2)) steps/sec" -ForegroundColor Green
    } else {
        Write-Host "  Failed: Exit code $($process.ExitCode)" -ForegroundColor Red
    }
    Write-Host ""
}

Write-Host "Basic scaling test completed" -ForegroundColor Green