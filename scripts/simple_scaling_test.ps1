# Simple NeuroForge Scaling Test
# Tests neuron scaling with basic performance monitoring

param(
    [int]$MaxNeurons = 10000,
    [string]$OutputDir = "scaling_results"
)

# Create output directory
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

Write-Host "NeuroForge Simple Scaling Test" -ForegroundColor Green
Write-Host "==============================" -ForegroundColor Green
Write-Host ""

# Test configurations
$tests = @(
    @{ Name="Baseline"; Neurons=64; Steps=100; Args=@("--steps=100", "--step-ms=10", "--enable-learning") },
    @{ Name="1K_Test"; Neurons=1000; Steps=100; Args=@("--steps=100", "--step-ms=10", "--enable-learning", "--add-region=visual:V1:250", "--add-region=auditory:A1:250", "--add-region=motor:M1:250", "--add-region=pfc:PFC:250") },
    @{ Name="5K_Test"; Neurons=5000; Steps=50; Args=@("--steps=50", "--step-ms=20", "--enable-learning", "--add-region=visual:V1:1000", "--add-region=auditory:A1:1000", "--add-region=motor:M1:1000", "--add-region=pfc:PFC:1000", "--add-region=hippocampus:HPC:1000") },
    @{ Name="10K_Test"; Neurons=10000; Steps=25; Args=@("--steps=25", "--step-ms=50", "--enable-learning", "--add-region=visual:V1:2000", "--add-region=auditory:A1:1500", "--add-region=motor:M1:1500", "--add-region=pfc:PFC:2000", "--add-region=hippocampus:HPC:1500", "--add-region=thalamus:TH:1000", "--add-region=brainstem:BS:500") }
)

$results = @()

foreach ($test in $tests) {
    if ($test.Neurons -gt $MaxNeurons) {
        Write-Host "Skipping $($test.Name) - exceeds MaxNeurons limit" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "Running $($test.Name) test ($($test.Neurons) neurons)..." -ForegroundColor Cyan
    
    $outputFile = Join-Path $OutputDir "$($test.Name)_output.txt"
    $errorFile = Join-Path $OutputDir "$($test.Name)_error.txt"
    
    # Measure execution time
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        $process = Start-Process -FilePath "build\Release\neuroforge.exe" -ArgumentList $test.Args -Wait -PassThru -NoNewWindow -RedirectStandardOutput $outputFile -RedirectStandardError $errorFile
        $exitCode = $process.ExitCode
        $stopwatch.Stop()
        
        $result = @{
            TestName = $test.Name
            Neurons = $test.Neurons
            Steps = $test.Steps
            ExecutionTimeMs = $stopwatch.ElapsedMilliseconds
            ExitCode = $exitCode
            Success = ($exitCode -eq 0)
            StepsPerSecond = if ($stopwatch.ElapsedMilliseconds -gt 0) { ($test.Steps * 1000.0) / $stopwatch.ElapsedMilliseconds } else { 0 }
        }
        
        if ($result.Success) {
            Write-Host "  ✓ Success: $($result.ExecutionTimeMs)ms, $([math]::Round($result.StepsPerSecond, 2)) steps/sec" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Failed: Exit code $exitCode" -ForegroundColor Red
        }
        
        $results += $result
        
    } catch {
        $stopwatch.Stop()
        Write-Host "  ✗ Exception: $($_.Exception.Message)" -ForegroundColor Red
        
        $result = @{
            TestName = $test.Name
            Neurons = $test.Neurons
            Steps = $test.Steps
            ExecutionTimeMs = $stopwatch.ElapsedMilliseconds
            ExitCode = -1
            Success = $false
            Error = $_.Exception.Message
            StepsPerSecond = 0
        }
        $results += $result
    }
    
    Write-Host ""
}

# Generate summary
Write-Host "Scaling Test Summary" -ForegroundColor Green
Write-Host "===================" -ForegroundColor Green

$successfulTests = $results | Where-Object { $_.Success }
$failedTests = $results | Where-Object { !$_.Success }

Write-Host "Total tests: $($results.Count)"
Write-Host "Successful: $($successfulTests.Count)"
Write-Host "Failed: $($failedTests.Count)"
Write-Host ""

if ($successfulTests.Count -gt 0) {
    Write-Host "Performance Results:" -ForegroundColor Cyan
    Write-Host "Neurons    Steps/Sec   Time(ms)   Test" -ForegroundColor Gray
    Write-Host "-------    ---------   --------   ----" -ForegroundColor Gray
    
    foreach ($test in $successfulTests | Sort-Object Neurons) {
        $neuronsStr = $test.Neurons.ToString().PadLeft(7)
        $stepsSecStr = [math]::Round($test.StepsPerSecond, 2).ToString().PadLeft(9)
        $timeStr = $test.ExecutionTimeMs.ToString().PadLeft(8)
        Write-Host "$neuronsStr    $stepsSecStr   $timeStr   $($test.TestName)"
    }
    
    # Calculate scaling efficiency
    if ($successfulTests.Count -gt 1) {
        $baseline = $successfulTests | Sort-Object Neurons | Select-Object -First 1
        $largest = $successfulTests | Sort-Object Neurons | Select-Object -Last 1
        
        $neuronRatio = $largest.Neurons / $baseline.Neurons
        $timeRatio = $largest.ExecutionTimeMs / $baseline.ExecutionTimeMs
        $efficiency = $neuronRatio / $timeRatio
        
        Write-Host ""
        Write-Host "Scaling Analysis:" -ForegroundColor Yellow
        Write-Host "  Neuron scale increase: $([math]::Round($neuronRatio, 1))x"
        Write-Host "  Time increase: $([math]::Round($timeRatio, 1))x"
        Write-Host "  Scaling efficiency: $([math]::Round($efficiency, 2)) (1.0 = linear scaling)"
    }
}

if ($failedTests.Count -gt 0) {
    Write-Host ""
    Write-Host "Failed Tests:" -ForegroundColor Red
    foreach ($test in $failedTests) {
        Write-Host "  $($test.TestName) ($($test.Neurons) neurons): Exit code $($test.ExitCode)"
    }
}

# Save results to CSV
$csvFile = Join-Path $OutputDir "scaling_results.csv"
$results | Export-Csv -Path $csvFile -NoTypeInformation
Write-Host ""
Write-Host "Results saved to: $csvFile" -ForegroundColor Yellow

# Determine maximum successful scale
$maxSuccessful = ($successfulTests | Measure-Object -Property Neurons -Maximum).Maximum
if ($maxSuccessful) {
    Write-Host "Maximum successful scale: $maxSuccessful neurons" -ForegroundColor Green
} else {
    Write-Host "No tests completed successfully" -ForegroundColor Red
}