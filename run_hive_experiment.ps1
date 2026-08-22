# Run Hive Experiment
$buildDir = "build"
$exePath = "$buildDir/production_substrate_deployment.exe"

if (-not (Test-Path $exePath)) {
    Write-Host "Executable not found at $exePath"
    exit 1
}

# Start Node 1 (Main)
Write-Host "Starting Node 1..."
Start-Process -FilePath $exePath -ArgumentList "--embodiment=on", "--hive=on", "--phase15=on", "--hive-id=node_1", "--steps=2000", "--memory-db=neuroforge.db", "--unified-substrate=on" -NoNewWindow

# Start Node 2 (Peer) - Delayed slightly
Start-Sleep -Seconds 2
Write-Host "Starting Node 2..."
Start-Process -FilePath $exePath -ArgumentList "--embodiment=on", "--hive=on", "--phase15=on", "--hive-id=node_2", "--steps=2000", "--memory-db=neuroforge.db", "--unified-substrate=on" -NoNewWindow
