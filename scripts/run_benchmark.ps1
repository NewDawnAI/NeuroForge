# PowerShell script to build and run NeuroForge performance benchmark
# Usage: .\run_benchmark.ps1 [build_type] [iterations]
# Example: .\run_benchmark.ps1 Release 5

param(
    [string]$BuildType = "Release",
    [int]$Iterations = 1,
    [switch]$CleanBuild = $false,
    [switch]$Verbose = $false
)

# Set error action preference
$ErrorActionPreference = "Stop"

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectRoot "build\benchmark"
$BinDir = Join-Path $BuildDir "bin"

Write-Host "NeuroForge Performance Benchmark Runner" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Build Type: $BuildType" -ForegroundColor Green
Write-Host "Iterations: $Iterations" -ForegroundColor Green
Write-Host "Project Root: $ProjectRoot" -ForegroundColor Yellow
Write-Host "Build Directory: $BuildDir" -ForegroundColor Yellow

# Function to check if command exists
function Test-Command {
    param($Command)
    try {
        Get-Command $Command -ErrorAction Stop | Out-Null
        return $true
    } catch {
        return $false
    }
}

# Check for required tools
Write-Host "`nChecking required tools..." -ForegroundColor Yellow

if (-not (Test-Command "cmake")) {
    Write-Error "CMake not found. Please install CMake and add it to PATH."
    exit 1
}

if (-not (Test-Command "msbuild") -and -not (Test-Command "ninja")) {
    Write-Error "Neither MSBuild nor Ninja found. Please install Visual Studio or Ninja build system."
    exit 1
}

# Create build directory
if ($CleanBuild -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

if (-not (Test-Path $BuildDir)) {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Change to build directory
Push-Location $BuildDir

try {
    # Configure CMake
    Write-Host "`nConfiguring CMake..." -ForegroundColor Yellow
    $CMakeArgs = @(
        "-S", $ScriptDir
        "-B", "."
        "-DCMAKE_BUILD_TYPE=$BuildType"
    )
    
    # Try to use Ninja if available, otherwise use default generator
    if (Test-Command "ninja") {
        $CMakeArgs += "-G", "Ninja"
        Write-Host "Using Ninja build system" -ForegroundColor Green
    } else {
        Write-Host "Using default build system (MSBuild)" -ForegroundColor Green
    }
    
    & cmake @CMakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
    
    # Build the project
    Write-Host "`nBuilding benchmark..." -ForegroundColor Yellow
    $BuildArgs = @("--build", ".", "--config", $BuildType)
    
    if ($Verbose) {
        $BuildArgs += "--verbose"
    }
    
    & cmake @BuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    
    # Check if executable exists
    $ExePath = Join-Path $BinDir "benchmark_performance.exe"
    if (-not (Test-Path $ExePath)) {
        # Try alternative path
        $ExePath = Join-Path $BuildDir "benchmark_performance.exe"
        if (-not (Test-Path $ExePath)) {
            throw "Benchmark executable not found at expected locations"
        }
    }
    
    Write-Host "`nBuild completed successfully!" -ForegroundColor Green
    Write-Host "Executable location: $ExePath" -ForegroundColor Yellow
    
    # Run benchmark iterations
    Write-Host "`nRunning benchmark ($Iterations iteration$(if($Iterations -ne 1){'s'}))..." -ForegroundColor Yellow
    
    $TotalStartTime = Get-Date
    $Results = @()
    
    for ($i = 1; $i -le $Iterations; $i++) {
        Write-Host "`n--- Iteration $i of $Iterations ---" -ForegroundColor Cyan
        
        $IterationStartTime = Get-Date
        
        # Run the benchmark
        & $ExePath
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Benchmark iteration $i failed with exit code $LASTEXITCODE"
            continue
        }
        
        $IterationEndTime = Get-Date
        $IterationDuration = ($IterationEndTime - $IterationStartTime).TotalSeconds
        
        $Results += [PSCustomObject]@{
            Iteration = $i
            Duration = $IterationDuration
            StartTime = $IterationStartTime
            EndTime = $IterationEndTime
        }
        
        Write-Host "Iteration $i completed in $([math]::Round($IterationDuration, 2)) seconds" -ForegroundColor Green
        
        # Brief pause between iterations
        if ($i -lt $Iterations) {
            Start-Sleep -Seconds 2
        }
    }
    
    $TotalEndTime = Get-Date
    $TotalDuration = ($TotalEndTime - $TotalStartTime).TotalSeconds
    
    # Display summary
    Write-Host "`n=== Benchmark Summary ===" -ForegroundColor Cyan
    Write-Host "Total iterations: $Iterations" -ForegroundColor Green
    Write-Host "Successful runs: $($Results.Count)" -ForegroundColor Green
    Write-Host "Total time: $([math]::Round($TotalDuration, 2)) seconds" -ForegroundColor Green
    
    if ($Results.Count -gt 0) {
        $AvgDuration = ($Results | Measure-Object -Property Duration -Average).Average
        $MinDuration = ($Results | Measure-Object -Property Duration -Minimum).Minimum
        $MaxDuration = ($Results | Measure-Object -Property Duration -Maximum).Maximum
        
        Write-Host "Average iteration time: $([math]::Round($AvgDuration, 2)) seconds" -ForegroundColor Green
        Write-Host "Fastest iteration: $([math]::Round($MinDuration, 2)) seconds" -ForegroundColor Green
        Write-Host "Slowest iteration: $([math]::Round($MaxDuration, 2)) seconds" -ForegroundColor Green
        
        # Save summary to file
        $SummaryFile = Join-Path $ProjectRoot "benchmark_summary.txt"
        $SummaryContent = @"
NeuroForge Benchmark Summary
===========================
Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Build Type: $BuildType
Total Iterations: $Iterations
Successful Runs: $($Results.Count)
Total Time: $([math]::Round($TotalDuration, 2)) seconds
Average Iteration Time: $([math]::Round($AvgDuration, 2)) seconds
Fastest Iteration: $([math]::Round($MinDuration, 2)) seconds
Slowest Iteration: $([math]::Round($MaxDuration, 2)) seconds

Individual Results:
"@
        
        foreach ($result in $Results) {
            $SummaryContent += "`nIteration $($result.Iteration): $([math]::Round($result.Duration, 2))s"
        }
        
        $SummaryContent | Out-File -FilePath $SummaryFile -Encoding UTF8
        Write-Host "`nSummary saved to: $SummaryFile" -ForegroundColor Yellow
    }
    
    Write-Host "`nBenchmark completed successfully!" -ForegroundColor Green
    
} catch {
    Write-Error "Error: $_"
    exit 1
} finally {
    # Return to original directory
    Pop-Location
}

# Check for result files
$ResultFiles = @(
    "benchmark_results.txt",
    "benchmark_summary.txt"
)

Write-Host "`nGenerated files:" -ForegroundColor Yellow
foreach ($file in $ResultFiles) {
    $filePath = Join-Path $ProjectRoot $file
    if (Test-Path $filePath) {
        Write-Host "  $filePath" -ForegroundColor Green
    }
}

Write-Host "`nBenchmark run completed!" -ForegroundColor Cyan