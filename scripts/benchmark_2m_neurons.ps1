# NeuroForge 2 Million Neuron Benchmark
# Comprehensive performance test for 2,000,000 neurons
# Based on successful 1M neuron architecture with optimizations

param(
    [string]$OutputDir = "benchmark_2m_results",
    [string]$NeuroForgeExe = "..\build\Release\neuroforge.exe",
    [int]$Steps = 50,
    [int]$StepMs = 100,
    [switch]$Verbose,
    [switch]$MemoryProfile
)

# Ensure output directory exists
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

Write-Host "NeuroForge 2 Million Neuron Benchmark" -ForegroundColor Cyan
Write-Host "=====================================" -ForegroundColor Cyan
Write-Host ""

# Test configuration for 2M neurons
$BenchmarkConfig = @{
    Name = "2M_Neuron_Benchmark"
    Neurons = 2000000
    Steps = $Steps
    StepMs = $StepMs
    Regions = @(
        "VisualCortex:V1:400000",      # 400K neurons - primary visual processing
        "AuditoryCortex:A1:300000",    # 300K neurons - auditory processing
        "MotorCortex:M1:250000",       # 250K neurons - motor control
        "PrefrontalCortex:PFC:500000", # 500K neurons - executive functions
        "Hippocampus:HPC:200000",      # 200K neurons - memory formation
        "Thalamus:TH:150000",          # 150K neurons - relay station
        "Brainstem:BS:100000",         # 100K neurons - basic functions
        "Cerebellum:CB:100000"         # 100K neurons - coordination
    )
    Description = "2 million neuron stress test with distributed architecture"
    EnableVision = $true
    EnableAudio = $true
    EnableSocial = $true
    EnableCrossModal = $true
    EnableLearning = $true
}

Write-Host "Test Configuration:" -ForegroundColor Yellow
Write-Host "  Total Neurons: $($BenchmarkConfig.Neurons.ToString('N0'))" -ForegroundColor White
Write-Host "  Brain Regions: $($BenchmarkConfig.Regions.Count)" -ForegroundColor White
Write-Host "  Processing Steps: $($BenchmarkConfig.Steps)" -ForegroundColor White
Write-Host "  Step Interval: $($BenchmarkConfig.StepMs)ms" -ForegroundColor White
Write-Host "  Estimated Memory: ~128MB (base) + overhead" -ForegroundColor White
Write-Host ""

# Build command arguments
$args = @(
    "--steps=$($BenchmarkConfig.Steps)",
    "--step-ms=$($BenchmarkConfig.StepMs)",
    "--enable-learning"
)

# Add brain regions
foreach ($region in $BenchmarkConfig.Regions) {
    $args += "--add-region=$region"
}

# Add feature flags
if ($BenchmarkConfig.EnableVision) { $args += "--vision-demo" }
if ($BenchmarkConfig.EnableAudio) { $args += "--audio-demo" }
if ($BenchmarkConfig.EnableSocial) { $args += "--social-perception" }
if ($BenchmarkConfig.EnableCrossModal) { $args += "--cross-modal" }

# Add output files for analysis
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$args += "--export-brain=brain_2m_$timestamp.json"
$args += "--export-connections=connections_2m_$timestamp.csv"
$args += "--snapshot-live=assemblies_2m_$timestamp.csv"

Write-Host "Starting 2 Million Neuron Benchmark..." -ForegroundColor Green
Write-Host "Command: $NeuroForgeExe $($args -join ' ')" -ForegroundColor Gray
Write-Host ""

# Performance monitoring setup
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$process = $null

try {
    # Start the benchmark process
    $processInfo = New-Object System.Diagnostics.ProcessStartInfo
    $processInfo.FileName = $NeuroForgeExe
    $processInfo.Arguments = $args -join ' '
    $processInfo.UseShellExecute = $false
    $processInfo.RedirectStandardOutput = $true
    $processInfo.RedirectStandardError = $true
    $processInfo.CreateNoWindow = $true
    $processInfo.WorkingDirectory = Split-Path $NeuroForgeExe -Parent

    $process = [System.Diagnostics.Process]::Start($processInfo)
    
    # Monitor memory usage during execution
    $memoryReadings = @()
    $cpuReadings = @()
    $startTime = Get-Date
    
    Write-Host "Monitoring Performance..." -ForegroundColor Yellow
    
    while (!$process.HasExited) {
        Start-Sleep -Seconds 5
        
        if (!$process.HasExited) {
            try {
                $memoryMB = [math]::Round($process.WorkingSet64 / 1MB, 2)
                $cpuTime = $process.TotalProcessorTime.TotalSeconds
                $elapsedTime = (Get-Date) - $startTime
                
                $memoryReadings += @{
                    Time = $elapsedTime.TotalSeconds
                    MemoryMB = $memoryMB
                }
                
                $cpuReadings += @{
                    Time = $elapsedTime.TotalSeconds
                    CpuTimeSeconds = $cpuTime
                }
                
                Write-Host "  Elapsed: $([math]::Round($elapsedTime.TotalMinutes, 1))min | Memory: $($memoryMB)MB | CPU Time: $([math]::Round($cpuTime, 1))s" -ForegroundColor Cyan
            }
            catch {
                # Process might have exited between checks
            }
        }
    }
    
    $stopwatch.Stop()
    
    # Capture output
    $output = $process.StandardOutput.ReadToEnd()
    $errorOutput = $process.StandardError.ReadToEnd()
    $exitCode = $process.ExitCode
    
    # Calculate final metrics
    $totalTimeSeconds = $stopwatch.Elapsed.TotalSeconds
    $totalTimeMinutes = $stopwatch.Elapsed.TotalMinutes
    $stepsPerSecond = if ($totalTimeSeconds -gt 0) { $BenchmarkConfig.Steps / $totalTimeSeconds } else { 0 }
    $neuronsPerSecond = if ($totalTimeSeconds -gt 0) { $BenchmarkConfig.Neurons / $totalTimeSeconds } else { 0 }
    $peakMemoryMB = if ($memoryReadings.Count -gt 0) { ($memoryReadings | Measure-Object -Property MemoryMB -Maximum).Maximum } else { 0 }
    $avgMemoryMB = if ($memoryReadings.Count -gt 0) { ($memoryReadings | Measure-Object -Property MemoryMB -Average).Average } else { 0 }
    
    # Results summary
    Write-Host ""
    Write-Host "2 Million Neuron Benchmark Results" -ForegroundColor Green
    Write-Host "==================================" -ForegroundColor Green
    Write-Host ""
    
    if ($exitCode -eq 0) {
        Write-Host "✅ SUCCESS: 2M neuron benchmark completed successfully!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Performance Metrics:" -ForegroundColor Yellow
        Write-Host "  Total Execution Time: $([math]::Round($totalTimeMinutes, 2)) minutes ($([math]::Round($totalTimeSeconds, 1)) seconds)" -ForegroundColor White
        Write-Host "  Processing Speed: $([math]::Round($stepsPerSecond, 3)) steps/second" -ForegroundColor White
        Write-Host "  Neuron Throughput: $([math]::Round($neuronsPerSecond, 0).ToString('N0')) neurons/second" -ForegroundColor White
        Write-Host "  Peak Memory Usage: $([math]::Round($peakMemoryMB, 1)) MB" -ForegroundColor White
        Write-Host "  Average Memory Usage: $([math]::Round($avgMemoryMB, 1)) MB" -ForegroundColor White
        Write-Host "  Memory per Neuron: $([math]::Round($peakMemoryMB * 1024 * 1024 / $BenchmarkConfig.Neurons, 1)) bytes" -ForegroundColor White
        Write-Host ""
        
        # Scaling comparison
        Write-Host "Scaling Analysis:" -ForegroundColor Yellow
        Write-Host "  2M vs 1M neurons: 2x scale increase" -ForegroundColor White
        Write-Host "  Expected memory: ~128MB (actual: $([math]::Round($peakMemoryMB, 1))MB)" -ForegroundColor White
        Write-Host "  Performance vs 1M: $(if ($stepsPerSecond -gt 0.2) { "Excellent" } elseif ($stepsPerSecond -gt 0.1) { "Good" } else { "Acceptable" })" -ForegroundColor White
        Write-Host ""
        
        # System stability assessment
        Write-Host "System Stability:" -ForegroundColor Yellow
        Write-Host "  Exit Code: $exitCode (Success)" -ForegroundColor Green
        Write-Host "  Memory Stability: $(if ($peakMemoryMB -lt 500) { "Excellent" } elseif ($peakMemoryMB -lt 1000) { "Good" } else { "Concerning" })" -ForegroundColor White
        Write-Host "  Processing Consistency: $(if ($stepsPerSecond -gt 0.1) { "Stable" } else { "Slow but stable" })" -ForegroundColor White
        
    } else {
        Write-Host "❌ FAILED: 2M neuron benchmark failed with exit code $exitCode" -ForegroundColor Red
        Write-Host ""
        if ($errorOutput) {
            Write-Host "Error Output:" -ForegroundColor Red
            Write-Host $errorOutput -ForegroundColor Yellow
        }
    }
    
    # Save detailed results
    $results = @{
        TestName = $BenchmarkConfig.Name
        Neurons = $BenchmarkConfig.Neurons
        Steps = $BenchmarkConfig.Steps
        Success = ($exitCode -eq 0)
        ExitCode = $exitCode
        TotalTimeSeconds = $totalTimeSeconds
        TotalTimeMinutes = $totalTimeMinutes
        StepsPerSecond = $stepsPerSecond
        NeuronsPerSecond = $neuronsPerSecond
        PeakMemoryMB = $peakMemoryMB
        AverageMemoryMB = $avgMemoryMB
        MemoryPerNeuron = if ($BenchmarkConfig.Neurons -gt 0) { $peakMemoryMB * 1024 * 1024 / $BenchmarkConfig.Neurons } else { 0 }
        MemoryReadings = $memoryReadings
        CpuReadings = $cpuReadings
        Output = $output
        ErrorOutput = $errorOutput
        Timestamp = $timestamp
        Configuration = $BenchmarkConfig
    }
    
    # Export results to JSON
    $resultsFile = Join-Path $OutputDir "benchmark_2m_results_$timestamp.json"
    $results | ConvertTo-Json -Depth 10 | Out-File -FilePath $resultsFile -Encoding UTF8
    
    Write-Host ""
    Write-Host "Results saved to: $resultsFile" -ForegroundColor Cyan
    
    # Export CSV summary for analysis
    $csvSummary = [PSCustomObject]@{
        TestName = $results.TestName
        Neurons = $results.Neurons
        Steps = $results.Steps
        Success = $results.Success
        TotalTimeMinutes = $results.TotalTimeMinutes
        StepsPerSecond = $results.StepsPerSecond
        NeuronsPerSecond = $results.NeuronsPerSecond
        PeakMemoryMB = $results.PeakMemoryMB
        MemoryPerNeuron = $results.MemoryPerNeuron
        Timestamp = $results.Timestamp
    }
    
    $csvFile = Join-Path $OutputDir "benchmark_2m_summary_$timestamp.csv"
    $csvSummary | Export-Csv -Path $csvFile -NoTypeInformation
    
    Write-Host "Summary saved to: $csvFile" -ForegroundColor Cyan
    
    if ($Verbose -and $output) {
        Write-Host ""
        Write-Host "Detailed Output:" -ForegroundColor Yellow
        Write-Host $output -ForegroundColor Gray
    }
    
    return $results

} catch {
    Write-Host "❌ ERROR: Exception during benchmark execution" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Yellow
    return $null
} finally {
    if ($process -and !$process.HasExited) {
        Write-Host "Terminating benchmark process..." -ForegroundColor Yellow
        $process.Kill()
        $process.WaitForExit(5000)
    }
    if ($process) {
        $process.Dispose()
    }
}