# NeuroForge Scaling Test Runner
# Automated testing suite for neuron scaling from 1K to 100K neurons

param(
    [string]$OutputDir = "scaling_results",
    [string]$NeuroForgeExe = "..\build\Release\neuroforge.exe",
    [int]$MaxScale = 100000,
    [switch]$SkipBaseline,
    [switch]$Verbose
)

# Test configuration
$TestConfigs = @(
    @{
        Name = "Baseline"
        Neurons = 64
        Steps = 1000
        StepMs = 10
        Regions = @()
        Description = "Current default configuration"
    },
    @{
        Name = "1K_Scale"
        Neurons = 1000
        Steps = 1000
        StepMs = 10
        Regions = @(
            "VisualCortex:V1:250",
            "AuditoryCortex:A1:250",
            "MotorCortex:M1:250",
            "PrefrontalCortex:PFC:250"
        )
        Description = "1K neuron basic scaling test"
    },
    @{
        Name = "5K_Scale"
        Neurons = 5000
        Steps = 1000
        StepMs = 15
        Regions = @(
            "VisualCortex:V1:1000",
            "AuditoryCortex:A1:1000",
            "MotorCortex:M1:1000",
            "PrefrontalCortex:PFC:1000",
            "Hippocampus:HPC:1000"
        )
        Description = "5K neuron medium scaling test"
    },
    @{
        Name = "10K_Scale"
        Neurons = 10000
        Steps = 500
        StepMs = 20
        Regions = @(
            "VisualCortex:V1:2000",
            "AuditoryCortex:A1:1500",
            "MotorCortex:M1:1500",
            "PrefrontalCortex:PFC:2000",
            "Hippocampus:HPC:1500",
            "Thalamus:TH:1000",
            "Brainstem:BS:500"
        )
        Description = "10K neuron target minimum test"
        EnableVision = $true
        EnableAudio = $true
        EnableSocial = $true
    },
    @{
        Name = "25K_Scale"
        Neurons = 25000
        Steps = 300
        StepMs = 30
        Regions = @(
            "VisualCortex:V1:5000",
            "AuditoryCortex:A1:4000",
            "MotorCortex:M1:3000",
            "PrefrontalCortex:PFC:5000",
            "Hippocampus:HPC:3000",
            "Thalamus:TH:2000",
            "Brainstem:BS:1000",
            "Cerebellum:CB:2000"
        )
        Description = "25K neuron stress test"
        EnableVision = $true
        EnableAudio = $true
        EnableSocial = $true
        EnableCrossModal = $true
    },
    @{
        Name = "50K_Scale"
        Neurons = 50000
        Steps = 200
        StepMs = 50
        Regions = @(
            "VisualCortex:V1:10000",
            "AuditoryCortex:A1:8000",
            "MotorCortex:M1:6000",
            "PrefrontalCortex:PFC:10000",
            "Hippocampus:HPC:6000",
            "Thalamus:TH:4000",
            "Brainstem:BS:2000",
            "Cerebellum:CB:4000"
        )
        Description = "50K neuron high-performance test"
        EnableVision = $true
        EnableAudio = $true
        EnableSocial = $true
        EnableCrossModal = $true
    },
    @{
        Name = "100K_Scale"
        Neurons = 100000
        Steps = 100
        StepMs = 100
        Regions = @(
            "VisualCortex:V1:20000",
            "AuditoryCortex:A1:15000",
            "MotorCortex:M1:12000",
            "PrefrontalCortex:PFC:20000",
            "Hippocampus:HPC:12000",
            "Thalamus:TH:8000",
            "Brainstem:BS:5000",
            "Cerebellum:CB:8000"
        )
        Description = "100K neuron maximum scale test"
        EnableVision = $true
        EnableAudio = $true
        EnableSocial = $true
        EnableCrossModal = $true
    }
)

# Performance monitoring functions
function Start-PerformanceMonitoring {
    param([string]$ProcessName, [string]$LogFile)
    
    $script = @"
`$process = Get-Process -Name '$ProcessName' -ErrorAction SilentlyContinue
if (`$process) {
    while (!`$process.HasExited) {
        `$timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
        `$cpu = `$process.CPU
        `$memory = `$process.WorkingSet64 / 1MB
        `$threads = `$process.Threads.Count
        "`$timestamp,`$cpu,`$memory,`$threads" | Out-File -FilePath '$LogFile' -Append
        Start-Sleep -Milliseconds 100
    }
}
"@
    
    Start-Job -ScriptBlock ([scriptblock]::Create($script)) -Name "PerfMon_$ProcessName"
}

function Stop-PerformanceMonitoring {
    param([string]$ProcessName)
    
    Get-Job -Name "PerfMon_$ProcessName" -ErrorAction SilentlyContinue | Stop-Job
    Get-Job -Name "PerfMon_$ProcessName" -ErrorAction SilentlyContinue | Remove-Job
}

function Measure-TestExecution {
    param([hashtable]$Config, [string]$OutputPath)
    
    Write-Host "Running test: $($Config.Name) ($($Config.Neurons) neurons)" -ForegroundColor Cyan
    Write-Host "Description: $($Config.Description)" -ForegroundColor Gray
    
    # Build command line arguments
    $args = @(
        "--steps=$($Config.Steps)",
        "--step-ms=$($Config.StepMs)",
        "--enable-learning"
    )
    
    # Add regions
    foreach ($region in $Config.Regions) {
        $args += "--add-region=$region"
    }
    
    # Add demo flags
    if ($Config.EnableVision) { $args += "--vision-demo" }
    if ($Config.EnableAudio) { $args += "--audio-demo" }
    if ($Config.EnableSocial) { $args += "--social-perception" }
    if ($Config.EnableCrossModal) { $args += "--cross-modal" }
    
    # Performance monitoring setup
    $perfLogFile = Join-Path $OutputPath "$($Config.Name)_performance.csv"
    "Timestamp,CPU,Memory_MB,Threads" | Out-File -FilePath $perfLogFile
    
    # Execute test with timing
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        Write-Host "Command: $NeuroForgeExe $($args -join ' ')" -ForegroundColor Yellow
        
        # Start performance monitoring
        $process = Start-Process -FilePath $NeuroForgeExe -ArgumentList $args -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $OutputPath "$($Config.Name)_output.txt") -RedirectStandardError (Join-Path $OutputPath "$($Config.Name)_error.txt")
        
        # Monitor performance
        Start-PerformanceMonitoring -ProcessName $process.ProcessName -LogFile $perfLogFile
        
        # Wait for completion with timeout
        $timeoutMs = ($Config.Steps * $Config.StepMs * 10) + 60000  # 10x expected time + 1 minute buffer
        $completed = $process.WaitForExit($timeoutMs)
        
        $stopwatch.Stop()
        
        if (!$completed) {
            Write-Host "Test timed out after $($timeoutMs/1000) seconds" -ForegroundColor Red
            $process.Kill()
            $exitCode = -1
        } else {
            $exitCode = $process.ExitCode
        }
        
        Stop-PerformanceMonitoring -ProcessName $process.ProcessName
        
        # Collect results
        $result = @{
            TestName = $Config.Name
            Neurons = $Config.Neurons
            Steps = $Config.Steps
            ExecutionTimeMs = $stopwatch.ElapsedMilliseconds
            ExitCode = $exitCode
            Success = ($exitCode -eq 0)
            StepsPerSecond = if ($stopwatch.ElapsedMilliseconds -gt 0) { ($Config.Steps * 1000.0) / $stopwatch.ElapsedMilliseconds } else { 0 }
            Timeout = !$completed
        }
        
        # Analyze performance data
        if (Test-Path $perfLogFile) {
            $perfData = Import-Csv $perfLogFile | Where-Object { $_.Memory_MB -ne "Memory_MB" }
            if ($perfData.Count -gt 0) {
                $result.PeakMemoryMB = ($perfData | Measure-Object -Property Memory_MB -Maximum).Maximum
                $result.AvgMemoryMB = ($perfData | Measure-Object -Property Memory_MB -Average).Average
                $result.MaxThreads = ($perfData | Measure-Object -Property Threads -Maximum).Maximum
            }
        }
        
        return $result
        
    } catch {
        $stopwatch.Stop()
        Write-Host "Test failed with exception: $($_.Exception.Message)" -ForegroundColor Red
        
        return @{
            TestName = $Config.Name
            Neurons = $Config.Neurons
            Steps = $Config.Steps
            ExecutionTimeMs = $stopwatch.ElapsedMilliseconds
            ExitCode = -2
            Success = $false
            Error = $_.Exception.Message
            StepsPerSecond = 0
        }
    }
}

# Main execution
function Main {
    Write-Host "NeuroForge Scaling Test Suite" -ForegroundColor Green
    Write-Host "=============================" -ForegroundColor Green
    
    # Validate executable
    if (!(Test-Path $NeuroForgeExe)) {
        Write-Host "Error: NeuroForge executable not found at: $NeuroForgeExe" -ForegroundColor Red
        Write-Host "Please build the project or specify correct path with -NeuroForgeExe parameter" -ForegroundColor Red
        exit 1
    }
    
    # Create output directory
    if (!(Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir | Out-Null
    }
    
    $results = @()
    $startTime = Get-Date
    
    # Filter tests based on MaxScale
    $testsToRun = $TestConfigs | Where-Object { $_.Neurons -le $MaxScale }
    
    if ($SkipBaseline) {
        $testsToRun = $testsToRun | Where-Object { $_.Name -ne "Baseline" }
    }
    
    Write-Host "Running $($testsToRun.Count) scaling tests up to $MaxScale neurons" -ForegroundColor Yellow
    Write-Host ""
    
    foreach ($config in $testsToRun) {
        $result = Measure-TestExecution -Config $config -OutputPath $OutputDir
        $results += $result
        
        # Display immediate results
        if ($result.Success) {
            Write-Host "✓ $($result.TestName): $($result.ExecutionTimeMs)ms, $([math]::Round($result.StepsPerSecond, 2)) steps/sec" -ForegroundColor Green
            if ($result.PeakMemoryMB) {
                Write-Host "  Memory: $([math]::Round($result.PeakMemoryMB, 1))MB peak, $([math]::Round($result.AvgMemoryMB, 1))MB avg" -ForegroundColor Gray
            }
        } else {
            Write-Host "✗ $($result.TestName): FAILED" -ForegroundColor Red
            if ($result.Timeout) {
                Write-Host "  Reason: Timeout" -ForegroundColor Red
            } elseif ($result.Error) {
                Write-Host "  Reason: $($result.Error)" -ForegroundColor Red
            } else {
                Write-Host "  Exit Code: $($result.ExitCode)" -ForegroundColor Red
            }
        }
        Write-Host ""
    }
    
    # Generate summary report
    $endTime = Get-Date
    $totalTime = $endTime - $startTime
    
    $summaryFile = Join-Path $OutputDir "scaling_test_summary.csv"
    $results | Export-Csv -Path $summaryFile -NoTypeInformation
    
    Write-Host "Scaling Test Summary" -ForegroundColor Green
    Write-Host "===================" -ForegroundColor Green
    Write-Host "Total execution time: $($totalTime.ToString('hh\:mm\:ss'))"
    Write-Host "Tests completed: $($results.Count)"
    Write-Host "Successful tests: $(($results | Where-Object Success).Count)"
    Write-Host "Failed tests: $(($results | Where-Object { !$_.Success }).Count)"
    Write-Host ""
    Write-Host "Results saved to: $summaryFile" -ForegroundColor Yellow
    
    # Display scaling analysis
    $successfulTests = $results | Where-Object Success | Sort-Object Neurons
    if ($successfulTests.Count -gt 1) {
        Write-Host "Scaling Analysis:" -ForegroundColor Cyan
        foreach ($test in $successfulTests) {
            $neuronsPerSec = if ($test.ExecutionTimeMs -gt 0) { ($test.Neurons * 1000.0) / $test.ExecutionTimeMs } else { 0 }
            Write-Host "  $($test.Neurons.ToString().PadLeft(6)) neurons: $([math]::Round($test.StepsPerSecond, 2).ToString().PadLeft(8)) steps/sec, $([math]::Round($neuronsPerSec, 0).ToString().PadLeft(8)) neurons/sec"
        }
    }
    
    # Identify maximum successful scale
    $maxSuccessful = ($successfulTests | Measure-Object -Property Neurons -Maximum).Maximum
    if ($maxSuccessful) {
        Write-Host ""
        Write-Host "Maximum successful scale: $maxSuccessful neurons" -ForegroundColor Green
    }
    
    # Check for failures and provide recommendations
    $failedTests = $results | Where-Object { !$_.Success }
    if ($failedTests.Count -gt 0) {
        Write-Host ""
        Write-Host "Failed Tests Analysis:" -ForegroundColor Red
        foreach ($test in $failedTests) {
            $errorMsg = if ($test.Error) { $test.Error } else { "Exit code $($test.ExitCode)" }
            Write-Host "  $($test.TestName) ($($test.Neurons) neurons): $errorMsg"
        }
    }
}

# Execute main function
Main