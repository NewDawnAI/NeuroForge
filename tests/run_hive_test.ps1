# Test Hive Telepathy with 3 nodes

$root = "C:\Users\ashis\Desktop\NeuroForge"
$exe = "$root\build\neuroforge.exe"
$db = "$root\hive_test.db"
$steps = 200

if (Test-Path $db) { Remove-Item $db }

Write-Host "Starting Hive Test with 3 nodes..."
Write-Host "Exe: $exe"
Write-Host "DB: $db"

# Start Node 1
$job1 = Start-Job -ScriptBlock {
    param($exe, $db, $steps, $root)
    Set-Location $root
    & $exe --unified-substrate --embodiment --hive --hive-id="node_1" --memory-db=$db --steps=$steps --phase15
} -ArgumentList $exe, $db, $steps, $root

# Start Node 2
$job2 = Start-Job -ScriptBlock {
    param($exe, $db, $steps, $root)
    Set-Location $root
    Start-Sleep -Seconds 2 # Wait for DB init
    & $exe --unified-substrate --embodiment --hive --hive-id="node_2" --memory-db=$db --steps=$steps --phase15
} -ArgumentList $exe, $db, $steps, $root

# Start Node 3
$job3 = Start-Job -ScriptBlock {
    param($exe, $db, $steps, $root)
    Set-Location $root
    Start-Sleep -Seconds 4
    & $exe --unified-substrate --embodiment --hive --hive-id="node_3" --memory-db=$db --steps=$steps --phase15
} -ArgumentList $exe, $db, $steps, $root

Write-Host "Jobs started. Waiting for completion..."
$jobs = $job1, $job2, $job3
Wait-Job -Job $jobs

Write-Host "Collecting output..."
$out1 = Receive-Job -Job $job1 -Keep
$out2 = Receive-Job -Job $job2 -Keep
$out3 = Receive-Job -Job $job3 -Keep

# Check for Telepathy messages
$telepathy_count = ($out1 + $out2 + $out3) | Select-String "\[Telepathy\]" | Measure-Object | Select-Object -ExpandProperty Count

Write-Host "Total Telepathy Messages Received: $telepathy_count"

if ($telepathy_count -gt 0) {
    Write-Host "SUCCESS: Telepathy verified!" -ForegroundColor Green
    
    Write-Host "`n--- Sample Node 1 Output ---"
    $out1 | Select-String "Telepathy" -Context 0,1 | Select-Object -First 5
} else {
    Write-Host "FAILURE: No telepathy messages found." -ForegroundColor Red
    Write-Host "`n--- Node 1 Output ---"
    $out1 | Select-Object -Last 20
}

Remove-Job -Job $jobs
