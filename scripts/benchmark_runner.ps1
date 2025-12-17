# NeuroForge Benchmark Runner
# Automated validation suite for post-M7 neural substrate system

param(
    [string]$TestCategory = "all",
    [string]$OutputDir = "benchmark_results",
    [switch]$Verbose = $false
)

# Configuration
$NeuroForgeExe = ".\Release\neuroforge.exe"
$ResultsDir = Join-Path $PWD $OutputDir
$LogFile = Join-Path $ResultsDir "benchmark_log.txt"

# Ensure results directory exists
if (!(Test-Path $ResultsDir)) {
    New-Item -ItemType Directory -Path $ResultsDir -Force | Out-Null
}

# Logging function
function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $LogEntry = "[$Timestamp] [$Level] $Message"
    Write-Host $LogEntry
    Add-Content -Path $LogFile -Value $LogEntry
}

# Test execution function
function Run-Test {
    param(
        [string]$TestName,
        [string]$Command,
        [hashtable]$SuccessCriteria,
        [int]$TimeoutMinutes = 10
    )
    
    Write-Log "Starting test: $TestName" "TEST"
    $StartTime = Get-Date
    
    try {
        # Execute test command
        Write-Log "Executing: $Command" "CMD"
        $Process = Start-Process -FilePath "powershell" -ArgumentList "-Command", $Command -PassThru -NoNewWindow -RedirectStandardOutput "$ResultsDir\$TestName.out" -RedirectStandardError "$ResultsDir\$TestName.err"
        
        # Wait for completion with timeout
        $Completed = $Process.WaitForExit($TimeoutMinutes * 60 * 1000)
        
        if (!$Completed) {
            $Process.Kill()
            Write-Log "Test $TestName TIMEOUT after $TimeoutMinutes minutes" "ERROR"
            return @{ Status = "TIMEOUT"; Duration = (Get-Date) - $StartTime }
        }
        
        $ExitCode = $Process.ExitCode
        $Duration = (Get-Date) - $StartTime
        
        if ($ExitCode -eq 0) {
            # Parse output for success criteria
            $Output = Get-Content "$ResultsDir\$TestName.out" -Raw
            $TestResult = Validate-TestOutput -Output $Output -Criteria $SuccessCriteria -TestName $TestName
            $TestResult.Duration = $Duration
            $TestResult.ExitCode = $ExitCode
            
            Write-Log "Test $TestName completed: $($TestResult.Status)" "RESULT"
            return $TestResult
        } else {
            Write-Log "Test $TestName FAILED with exit code $ExitCode" "ERROR"
            return @{ Status = "FAILED"; Duration = $Duration; ExitCode = $ExitCode }
        }
        
    } catch {
        Write-Log "Test $TestName ERROR: $($_.Exception.Message)" "ERROR"
        return @{ Status = "ERROR"; Duration = (Get-Date) - $StartTime; Error = $_.Exception.Message }
    }
}

# Output validation function
function Validate-TestOutput {
    param(
        [string]$Output,
        [hashtable]$Criteria,
        [string]$TestName
    )
    
    $Result = @{ Status = "PASS"; Metrics = @{} }
    
    # Extract learning statistics
    if ($Output -match "Total Updates: (\d+)") {
        $Result.Metrics.TotalUpdates = [int]$Matches[1]
    }
    
    if ($Output -match "Active Synapses: (\d+)") {
        $Result.Metrics.ActiveSynapses = [int]$Matches[1]
    }
    
    if ($Output -match "Avg Weight Change: ([\d\.e\-\+]+)") {
        $Result.Metrics.AvgWeightChange = [double]$Matches[1]
    }
    
    # Check success criteria
    foreach ($Criterion in $Criteria.GetEnumerator()) {
        $MetricName = $Criterion.Key
        $Threshold = $Criterion.Value
        
        if ($Result.Metrics.ContainsKey($MetricName)) {
            $ActualValue = $Result.Metrics[$MetricName]
            if ($ActualValue -lt $Threshold) {
                $Result.Status = "FAIL"
                Write-Log "Criterion failed: $MetricName = $ActualValue (required: $Threshold)" "FAIL"
            } else {
                Write-Log "Criterion passed: $MetricName = $ActualValue (required: $Threshold)" "PASS"
            }
        } else {
            $Result.Status = "INCONCLUSIVE"
            Write-Log "Metric not found: $MetricName" "WARN"
        }
    }
    
    return $Result
}

# Benchmark test definitions
$ScalabilityTests = @{
    "B1.1_50K_Stability" = @{
        Command = "$NeuroForgeExe --vision-demo --audio-demo --motor-cortex --enable-learning --hebbian-rate=0.01 --stdp-rate=0.005 --steps=100"
        Criteria = @{ TotalUpdates = 1000000; ActiveSynapses = 30000 }
        Timeout = 15
    }
    "B1.2_75K_CrossModal" = @{
        Command = "$NeuroForgeExe --vision-demo --audio-demo --motor-cortex --cross-modal --substrate-mode=native --autonomous-mode=on --enable-learning --hebbian-rate=0.01 --steps=150"
        Criteria = @{ TotalUpdates = 2000000; ActiveSynapses = 30000 }
        Timeout = 20
    }
}

$LearningTests = @{
    "B2.1_Visual_Audio_Association" = @{
        Command = "$NeuroForgeExe --vision-demo --audio-demo --cross-modal --enable-learning --hebbian-rate=0.01 --steps=50"
        Criteria = @{ TotalUpdates = 500000; ActiveSynapses = 15000 }
        Timeout = 10
    }
    "B2.2_Spatial_Navigation" = @{
        Command = "$NeuroForgeExe --maze-demo --enable-learning --hebbian-rate=0.01 --hippocampal-snapshots=on --steps=100"
        Criteria = @{ TotalUpdates = 100000; ActiveSynapses = 5000 }
        Timeout = 15
    }
}

$EmergentTests = @{
    "B3.1_Autonomous_Learning" = @{
        Command = "$NeuroForgeExe --autonomous-mode=on --substrate-mode=native --vision-demo --eliminate-scaffolds=on --curiosity-threshold=0.3 --enable-learning --steps=200"
        Criteria = @{ TotalUpdates = 1000000; ActiveSynapses = 20000 }
        Timeout = 25
    }
    "B3.2_Assembly_Formation" = @{
        Command = "$NeuroForgeExe --vision-demo --audio-demo --cross-modal --enable-learning --hebbian-rate=0.01 --stdp-rate=0.005 --steps=100"
        Criteria = @{ TotalUpdates = 1500000; ActiveSynapses = 25000 }
        Timeout = 15
    }
}

$ContinuityTests = @{
    "B4.1_Long_Duration" = @{
        Command = "$NeuroForgeExe --vision-demo --enable-learning --hebbian-rate=0.01 --steps=500"
        Criteria = @{ TotalUpdates = 5000000; ActiveSynapses = 10000 }
        Timeout = 30
    }
}

# Main execution logic
Write-Log "NeuroForge Benchmark Suite Starting" "START"
Write-Log "Test Category: $TestCategory" "CONFIG"
Write-Log "Output Directory: $ResultsDir" "CONFIG"

$AllTests = @{}
if ($TestCategory -eq "all" -or $TestCategory -eq "scalability") {
    $AllTests += $ScalabilityTests
}
if ($TestCategory -eq "all" -or $TestCategory -eq "learning") {
    $AllTests += $LearningTests
}
if ($TestCategory -eq "all" -or $TestCategory -eq "emergent") {
    $AllTests += $EmergentTests
}
if ($TestCategory -eq "all" -or $TestCategory -eq "continuity") {
    $AllTests += $ContinuityTests
}

$Results = @{}
$TotalTests = $AllTests.Count
$CurrentTest = 0

foreach ($Test in $AllTests.GetEnumerator()) {
    $CurrentTest++
    Write-Log "Progress: $CurrentTest/$TotalTests" "PROGRESS"
    
    $TestName = $Test.Key
    $TestConfig = $Test.Value
    
    $Result = Run-Test -TestName $TestName -Command $TestConfig.Command -SuccessCriteria $TestConfig.Criteria -TimeoutMinutes $TestConfig.Timeout
    $Results[$TestName] = $Result
}

# Generate summary report
Write-Log "Generating benchmark report..." "REPORT"

$PassCount = ($Results.Values | Where-Object { $_.Status -eq "PASS" }).Count
$FailCount = ($Results.Values | Where-Object { $_.Status -eq "FAIL" }).Count
$ErrorCount = ($Results.Values | Where-Object { $_.Status -eq "ERROR" -or $_.Status -eq "TIMEOUT" }).Count

$ReportPath = Join-Path $ResultsDir "benchmark_report.md"
$Report = @"
# NeuroForge Benchmark Report

**Date**: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
**Test Category**: $TestCategory
**Total Tests**: $TotalTests
**Results**: $PassCount PASS, $FailCount FAIL, $ErrorCount ERROR

## Summary

| Status | Count | Percentage |
|--------|-------|------------|
| PASS   | $PassCount | $([math]::Round($PassCount/$TotalTests*100, 1))% |
| FAIL   | $FailCount | $([math]::Round($FailCount/$TotalTests*100, 1))% |
| ERROR  | $ErrorCount | $([math]::Round($ErrorCount/$TotalTests*100, 1))% |

## Detailed Results

"@

foreach ($Result in $Results.GetEnumerator()) {
    $TestName = $Result.Key
    $TestResult = $Result.Value
    
    $Report += @"

### $TestName
- **Status**: $($TestResult.Status)
- **Duration**: $($TestResult.Duration)
- **Exit Code**: $($TestResult.ExitCode)

"@
    
    if ($TestResult.Metrics) {
        $Report += "**Metrics**:`n"
        foreach ($Metric in $TestResult.Metrics.GetEnumerator()) {
            $Report += "- $($Metric.Key): $($Metric.Value)`n"
        }
    }
    
    if ($TestResult.Error) {
        $Report += "**Error**: $($TestResult.Error)`n"
    }
}

$Report += @"

## Recommendations

"@

if ($PassCount -eq $TotalTests) {
    $Report += "✅ **All tests passed** - System ready for deployment validation"
} elseif ($PassCount / $TotalTests -gt 0.8) {
    $Report += "⚠️ **Most tests passed** - Minor issues need attention before deployment"
} else {
    $Report += "❌ **Significant issues detected** - System requires improvement before validation"
}

Set-Content -Path $ReportPath -Value $Report
Write-Log "Benchmark report generated: $ReportPath" "COMPLETE"

# Display summary
Write-Host "`n=== BENCHMARK SUMMARY ===" -ForegroundColor Cyan
Write-Host "Total Tests: $TotalTests" -ForegroundColor White
Write-Host "PASS: $PassCount" -ForegroundColor Green
Write-Host "FAIL: $FailCount" -ForegroundColor Red
Write-Host "ERROR: $ErrorCount" -ForegroundColor Yellow
Write-Host "Report: $ReportPath" -ForegroundColor Cyan
Write-Host "=========================" -ForegroundColor Cyan

Write-Log "NeuroForge Benchmark Suite Complete" "COMPLETE"