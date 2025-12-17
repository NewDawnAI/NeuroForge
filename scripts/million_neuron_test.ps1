# NeuroForge 1 Million Neuron Assembly Test
# Comprehensive test script for massive-scale neural assembly formation

param(
    [string]$OutputDir = "million_neuron_results",
    [int]$Steps = 10,
    [int]$StepMs = 1000,
    [switch]$ExtendedTest,
    [switch]$SkipAssemblyAnalysis
)

# Test configuration
$TestConfig = @{
    TotalNeurons = 1000000
    Regions = @{
        "visual:V1" = 200000
        "pfc:PFC" = 200000
        "auditory:A1" = 150000
        "motor:M1" = 120000
        "hippocampus:HPC" = 120000
        "thalamus:TH" = 80000
        "ac:CB" = 80000
        "brainstem:BS" = 50000
    }
}

function Write-TestHeader {
    Write-Host "NeuroForge 1 Million Neuron Assembly Test" -ForegroundColor Green
    Write-Host "=========================================" -ForegroundColor Green
    Write-Host "Total Neurons: $($TestConfig.TotalNeurons)" -ForegroundColor Yellow
    Write-Host "Test Steps: $Steps" -ForegroundColor Yellow
    Write-Host "Step Duration: $StepMs ms" -ForegroundColor Yellow
    Write-Host "Output Directory: $OutputDir" -ForegroundColor Yellow
    Write-Host ""
}

function Test-SystemRequirements {
    Write-Host "Checking system requirements..." -ForegroundColor Cyan
    
    # Check available memory
    $memory = Get-CimInstance -ClassName Win32_ComputerSystem
    $totalMemoryGB = [math]::Round($memory.TotalPhysicalMemory / 1GB, 2)
    
    Write-Host "  Total RAM: $totalMemoryGB GB" -ForegroundColor Gray
    
    if ($totalMemoryGB -lt 8) {
        Write-Host "  WARNING: Less than 8GB RAM detected. Test may fail." -ForegroundColor Red
    } elseif ($totalMemoryGB -lt 16) {
        Write-Host "  CAUTION: Less than 16GB RAM. Monitor memory usage closely." -ForegroundColor Yellow
    } else {
        Write-Host "  RAM: Sufficient for 1M neuron test" -ForegroundColor Green
    }
    
    # Check available disk space
    $disk = Get-CimInstance -ClassName Win32_LogicalDisk | Where-Object { $_.DeviceID -eq "C:" }
    $freeSpaceGB = [math]::Round($disk.FreeSpace / 1GB, 2)
    
    Write-Host "  Free Disk Space: $freeSpaceGB GB" -ForegroundColor Gray
    
    if ($freeSpaceGB -lt 2) {
        Write-Host "  ERROR: Insufficient disk space. Need at least 2GB free." -ForegroundColor Red
        return $false
    } else {
        Write-Host "  Disk Space: Sufficient" -ForegroundColor Green
    }
    
    # Check NeuroForge executable
    if (!(Test-Path "build\Release\neuroforge.exe")) {
        Write-Host "  ERROR: NeuroForge executable not found" -ForegroundColor Red
        return $false
    } else {
        Write-Host "  NeuroForge Executable: Found" -ForegroundColor Green
    }
    
    Write-Host ""
    return $true
}

function Start-MillionNeuronTest {
    Write-Host "Starting 1 Million Neuron Test..." -ForegroundColor Cyan
    Write-Host "This may take 30-60 minutes. Please be patient." -ForegroundColor Yellow
    Write-Host ""
    
    # Build command arguments
    $args = @(
        "--steps=$Steps",
        "--step-ms=$StepMs",
        "--enable-learning"
    )
    
    # Add regions
    foreach ($region in $TestConfig.Regions.GetEnumerator()) {
        $args += "--add-region=$($region.Key):$($region.Value)"
    }
    
    # Add data export options
    $connectivityFile = Join-Path $OutputDir "million_neuron_connectivity.csv"
    $memoryDbFile = Join-Path $OutputDir "million_neuron_test.db"
    $brainFile = Join-Path $OutputDir "million_neuron_brain.json"
    
    $args += "--snapshot-csv=$connectivityFile"
    $args += "--memory-db=$memoryDbFile"
    $args += "--save-brain=$brainFile"
    
    if ($ExtendedTest) {
        $liveFile = Join-Path $OutputDir "live_connectivity.csv"
        $args += "--snapshot-live=$liveFile"
        $args += "--snapshot-interval=5000"
        $args += "--memdb-interval=2000"
    }
    
    # Execute test with performance monitoring
    $outputFile = Join-Path $OutputDir "test_output.txt"
    $errorFile = Join-Path $OutputDir "test_error.txt"
    
    Write-Host "Command: build\Release\neuroforge.exe $($args -join ' ')" -ForegroundColor Yellow
    Write-Host ""
    
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        # Start the process
        $process = Start-Process -FilePath "build\Release\neuroforge.exe" -ArgumentList $args -PassThru -NoNewWindow -RedirectStandardOutput $outputFile -RedirectStandardError $errorFile
        
        # Monitor progress
        $lastSize = 0
        while (!$process.HasExited) {
            Start-Sleep -Seconds 10
            
            # Check output file growth
            if (Test-Path $outputFile) {
                $currentSize = (Get-Item $outputFile).Length
                if ($currentSize -gt $lastSize) {
                    Write-Host "  Processing... (Output: $([math]::Round($currentSize/1KB, 1)) KB)" -ForegroundColor Gray
                    $lastSize = $currentSize
                }
            }
            
            # Check memory usage
            try {
                $memUsage = $process.WorkingSet64 / 1MB
                Write-Host "  Memory Usage: $([math]::Round($memUsage, 1)) MB" -ForegroundColor Gray
            } catch {
                # Process may have exited
            }
        }
        
        $stopwatch.Stop()
        $exitCode = $process.ExitCode
        
        Write-Host ""
        if ($exitCode -eq 0) {
            Write-Host "✓ Test completed successfully!" -ForegroundColor Green
            Write-Host "  Execution time: $($stopwatch.Elapsed.ToString('hh\:mm\:ss'))" -ForegroundColor Green
            return $true
        } else {
            Write-Host "✗ Test failed with exit code: $exitCode" -ForegroundColor Red
            Write-Host "  Check error file: $errorFile" -ForegroundColor Red
            return $false
        }
        
    } catch {
        $stopwatch.Stop()
        Write-Host "✗ Test failed with exception: $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

function Start-AssemblyAnalysis {
    if ($SkipAssemblyAnalysis) {
        Write-Host "Skipping assembly analysis (--SkipAssemblyAnalysis specified)" -ForegroundColor Yellow
        return
    }
    
    Write-Host "Starting Assembly Analysis..." -ForegroundColor Cyan
    
    $connectivityFile = Join-Path $OutputDir "million_neuron_connectivity.csv"
    
    if (!(Test-Path $connectivityFile)) {
        Write-Host "  ERROR: Connectivity file not found: $connectivityFile" -ForegroundColor Red
        return
    }
    
    # Check if Python is available
    try {
        $pythonVersion = python --version 2>&1
        Write-Host "  Python: $pythonVersion" -ForegroundColor Gray
    } catch {
        Write-Host "  WARNING: Python not found. Skipping assembly analysis." -ForegroundColor Yellow
        return
    }
    
    # Check if assembly detector exists
    if (!(Test-Path "scripts\assembly_detector.py")) {
        Write-Host "  ERROR: Assembly detector script not found" -ForegroundColor Red
        return
    }
    
    # Create Python analysis script
    $analysisScript = @"
import sys
import os
sys.path.append('scripts')

try:
    from assembly_detector import AssemblyDetector
    import time
    
    print("Initializing assembly detector for 1M neurons...")
    detector = AssemblyDetector(
        connectivity_threshold=0.05,
        min_assembly_size=5
    )
    
    print("Loading connectivity data...")
    start_time = time.time()
    success = detector.load_connectivity_data("$connectivityFile")
    load_time = time.time() - start_time
    
    if not success:
        print("ERROR: Failed to load connectivity data")
        sys.exit(1)
    
    print(f"Data loaded in {load_time:.1f} seconds")
    
    print("Detecting assemblies using DBSCAN...")
    start_time = time.time()
    assemblies = detector.detect_assemblies_dbscan(eps=0.1, min_samples=5)
    detection_time = time.time() - start_time
    
    print(f"Assembly detection completed in {detection_time:.1f} seconds")
    print(f"Detected {len(assemblies)} assemblies")
    
    if len(assemblies) > 0:
        print("Analyzing assembly hierarchy...")
        hierarchy = detector.analyze_assembly_hierarchy()
        
        print("Generating analysis report...")
        report_file = os.path.join("$OutputDir", "million_neuron_assembly_report.json")
        report = detector.generate_report(report_file)
        
        print("Creating visualizations...")
        viz_file = os.path.join("$OutputDir", "million_neuron_assemblies.png")
        detector.visualize_assemblies(viz_file, figsize=(16, 12))
        
        print(f"Analysis complete!")
        print(f"Report saved to: {report_file}")
        print(f"Visualization saved to: {viz_file}")
    else:
        print("No assemblies detected")
    
except ImportError as e:
    print(f"ERROR: Missing Python dependencies: {e}")
    print("Install required packages: pip install numpy pandas scikit-learn matplotlib networkx")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: Assembly analysis failed: {e}")
    sys.exit(1)
"@
    
    $scriptFile = Join-Path $OutputDir "assembly_analysis.py"
    $analysisScript | Out-File -FilePath $scriptFile -Encoding UTF8
    
    Write-Host "  Running assembly analysis..." -ForegroundColor Gray
    
    try {
        $result = python $scriptFile 2>&1
        Write-Host $result -ForegroundColor Gray
    } catch {
        Write-Host "  ERROR: Assembly analysis failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Generate-TestSummary {
    Write-Host "Generating Test Summary..." -ForegroundColor Cyan
    
    $summaryFile = Join-Path $OutputDir "test_summary.txt"
    $summary = @"
NeuroForge 1 Million Neuron Assembly Test Summary
================================================

Test Configuration:
- Total Neurons: $($TestConfig.TotalNeurons)
- Test Steps: $Steps
- Step Duration: $StepMs ms
- Extended Test: $ExtendedTest

Region Distribution:
"@
    
    foreach ($region in $TestConfig.Regions.GetEnumerator()) {
        $summary += "`n- $($region.Key): $($region.Value) neurons"
    }
    
    $summary += @"

Generated Files:
- Test Output: test_output.txt
- Test Errors: test_error.txt
- Connectivity Data: million_neuron_connectivity.csv
- Memory Database: million_neuron_test.db
- Brain State: million_neuron_brain.json
"@
    
    if (!$SkipAssemblyAnalysis) {
        $summary += @"
- Assembly Report: million_neuron_assembly_report.json
- Assembly Visualization: million_neuron_assemblies.png
"@
    }
    
    $summary += @"

Test completed: $(Get-Date)
"@
    
    $summary | Out-File -FilePath $summaryFile -Encoding UTF8
    Write-Host "  Summary saved to: $summaryFile" -ForegroundColor Green
}

# Main execution
function Main {
    Write-TestHeader
    
    # Create output directory
    if (!(Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir | Out-Null
        Write-Host "Created output directory: $OutputDir" -ForegroundColor Green
    }
    
    # Check system requirements
    if (!(Test-SystemRequirements)) {
        Write-Host "System requirements not met. Aborting test." -ForegroundColor Red
        exit 1
    }
    
    # Execute the test
    $testSuccess = Start-MillionNeuronTest
    
    if ($testSuccess) {
        # Analyze assemblies
        Start-AssemblyAnalysis
        
        # Generate summary
        Generate-TestSummary
        
        Write-Host ""
        Write-Host "🎉 1 Million Neuron Assembly Test Complete!" -ForegroundColor Green
        Write-Host "Results saved to: $OutputDir" -ForegroundColor Yellow
    } else {
        Write-Host ""
        Write-Host "❌ Test failed. Check output files for details." -ForegroundColor Red
        exit 1
    }
}

# Execute main function
Main