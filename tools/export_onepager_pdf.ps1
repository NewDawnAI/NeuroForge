$ErrorActionPreference = 'Stop'

# Resolve input HTML and output PDF paths
$in  = (Resolve-Path 'Lab_Log/Phase5_External_OnePager.html').Path
$out = Join-Path (Resolve-Path 'Lab_Log').Path 'Phase5_External_OnePager.pdf'

# Candidate browsers (Edge/Chrome) typical install paths
$candidates = @(
  'C:\Program Files\Microsoft\Edge\Application\msedge.exe',
  'C:\Program Files\Google\Chrome\Application\chrome.exe',
  'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
  'C:\Program Files (x86)\Google\Chrome\Application\chrome.exe'
) | Where-Object { Test-Path $_ }

if (-not $candidates -or $candidates.Count -eq 0) {
  throw 'No headless Edge/Chrome found at standard locations.'
}

$exe = $candidates[0]
$arguments = @(
  '--headless=new',
  '--disable-gpu',
  "--print-to-pdf=$out",
  '--virtual-time-budget=7000',
  '--no-sandbox',
  $in
)

Start-Process -FilePath $exe -ArgumentList $arguments -Wait -NoNewWindow

if (-not (Test-Path $out)) {
  throw 'PDF export failed (output not found).'
}

Write-Host ("Exported: {0} via {1}" -f $out, $exe)