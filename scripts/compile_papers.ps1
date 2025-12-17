# NeuroForge Paper Compilation Script
# Compiles all LaTeX papers to PDF format

param(
    [switch]$InstallMiKTeX,
    [switch]$SkipCompilation,
    [string]$OutputDir = "compiled_papers"
)

function Write-Header {
    Write-Host "NeuroForge Paper Compilation" -ForegroundColor Green
    Write-Host "============================" -ForegroundColor Green
    Write-Host ""
}

function Test-LaTeXInstallation {
    Write-Host "Checking LaTeX installation..." -ForegroundColor Cyan
    
    try {
        $pdflatexVersion = pdflatex --version 2>&1 | Select-Object -First 1
        Write-Host "  Found: $pdflatexVersion" -ForegroundColor Green
        return $true
    } catch {
        Write-Host "  LaTeX not found. Please install MiKTeX or TeX Live." -ForegroundColor Red
        
        if ($InstallMiKTeX) {
            Write-Host "  Attempting to install MiKTeX..." -ForegroundColor Yellow
            try {
                winget install MiKTeX.MiKTeX
                Write-Host "  MiKTeX installation initiated. Please restart after installation." -ForegroundColor Yellow
                return $false
            } catch {
                Write-Host "  Failed to install MiKTeX automatically. Please install manually." -ForegroundColor Red
                return $false
            }
        }
        return $false
    }
}

function Compile-LaTeXPaper {
    param(
        [string]$PaperPath,
        [string]$OutputPath
    )
    
    $paperName = [System.IO.Path]::GetFileNameWithoutExtension($PaperPath)
    Write-Host "Compiling $paperName..." -ForegroundColor Cyan
    
    # Change to paper directory
    $paperDir = Split-Path $PaperPath -Parent
    $originalDir = Get-Location
    Set-Location $paperDir
    
    try {
        # First compilation
        Write-Host "  First pass..." -ForegroundColor Gray
        $result1 = pdflatex -interaction=nonstopmode $PaperPath 2>&1
        
        # Second compilation (for references)
        Write-Host "  Second pass..." -ForegroundColor Gray
        $result2 = pdflatex -interaction=nonstopmode $PaperPath 2>&1
        
        # Third compilation (for final formatting)
        Write-Host "  Final pass..." -ForegroundColor Gray
        $result3 = pdflatex -interaction=nonstopmode $PaperPath 2>&1
        
        # Check if PDF was created
        $pdfFile = $paperName + ".pdf"
        if (Test-Path $pdfFile) {
            # Move to output directory
            $outputFile = Join-Path $OutputPath $pdfFile
            Move-Item $pdfFile $outputFile -Force
            
            Write-Host "  ✓ Success: $outputFile" -ForegroundColor Green
            
            # Clean up auxiliary files
            Remove-Item "$paperName.aux" -ErrorAction SilentlyContinue
            Remove-Item "$paperName.log" -ErrorAction SilentlyContinue
            Remove-Item "$paperName.out" -ErrorAction SilentlyContinue
            Remove-Item "$paperName.bbl" -ErrorAction SilentlyContinue
            Remove-Item "$paperName.blg" -ErrorAction SilentlyContinue
            Remove-Item "$paperName.toc" -ErrorAction SilentlyContinue
            
            return $true
        } else {
            Write-Host "  ✗ Failed: PDF not generated" -ForegroundColor Red
            Write-Host "  Check log for errors:" -ForegroundColor Yellow
            if (Test-Path "$paperName.log") {
                Get-Content "$paperName.log" | Select-Object -Last 20 | ForEach-Object {
                    Write-Host "    $_" -ForegroundColor Yellow
                }
            }
            return $false
        }
    } finally {
        Set-Location $originalDir
    }
}

function Create-CompilationSummary {
    param(
        [hashtable]$Results,
        [string]$OutputDir
    )
    
    $summary = @"
# NeuroForge Paper Compilation Summary

**Compilation Date**: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

## Papers Compiled

"@
    
    foreach ($paper in $Results.Keys) {
        $status = if ($Results[$paper]) { "✓ Success" } else { "✗ Failed" }
        $summary += "`n- **$paper**: $status"
    }
    
    $summary += @"

## Generated Files

"@
    
    if (Test-Path $OutputDir) {
        Get-ChildItem $OutputDir -Filter "*.pdf" | ForEach-Object {
            $size = [math]::Round($_.Length / 1KB, 1)
            $summary += "`n- $($_.Name) ($size KB)"
        }
    }
    
    $summary += @"

## Usage Instructions

1. **NeurIPS Paper**: Submit to NeurIPS 2024 conference
2. **ICLR Paper**: Submit to ICLR 2024 conference  
3. **IEEE Paper**: Submit to IEEE Transactions or conference

## Quality Checklist

- [ ] All figures display correctly
- [ ] References are properly formatted
- [ ] Mathematical equations render properly
- [ ] Tables are well-formatted
- [ ] Page limits are respected
- [ ] Author information is anonymized (for review)

## Submission Notes

- Ensure all figures are high-resolution (300 DPI minimum)
- Verify that all referenced figures exist in publication_figures/
- Check that experimental data matches reported results
- Validate that all citations are complete and accurate

**Generated by NeuroForge Paper Compilation System**
"@
    
    $summaryFile = Join-Path $OutputDir "COMPILATION_SUMMARY.md"
    $summary | Out-File -FilePath $summaryFile -Encoding UTF8
    Write-Host "Compilation summary saved to: $summaryFile" -ForegroundColor Yellow
}

function Main {
    Write-Header
    
    # Create output directory
    if (!(Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir | Out-Null
        Write-Host "Created output directory: $OutputDir" -ForegroundColor Green
    }
    
    # Check LaTeX installation
    if (!(Test-LaTeXInstallation)) {
        Write-Host "Cannot proceed without LaTeX installation." -ForegroundColor Red
        Write-Host "Please install MiKTeX or TeX Live and try again." -ForegroundColor Yellow
        Write-Host "Or run with -InstallMiKTeX to attempt automatic installation." -ForegroundColor Yellow
        exit 1
    }
    
    if ($SkipCompilation) {
        Write-Host "Skipping compilation (--SkipCompilation specified)" -ForegroundColor Yellow
        return
    }
    
    # Papers to compile
    $papers = @{
        "NeurIPS_Paper" = "papers\neurips_unified_neural_substrate.tex"
        "ICLR_Paper" = "papers\iclr_biological_learning_integration.tex"
        "IEEE_Paper" = "papers\ieee_technical_implementation.tex"
    }
    
    $results = @{}
    $successCount = 0
    
    # Compile each paper
    foreach ($paperName in $papers.Keys) {
        $paperPath = $papers[$paperName]
        
        if (Test-Path $paperPath) {
            $success = Compile-LaTeXPaper -PaperPath $paperPath -OutputPath $OutputDir
            $results[$paperName] = $success
            if ($success) { $successCount++ }
        } else {
            Write-Host "Paper not found: $paperPath" -ForegroundColor Red
            $results[$paperName] = $false
        }
        Write-Host ""
    }
    
    # Generate summary
    Create-CompilationSummary -Results $results -OutputDir $OutputDir
    
    # Final report
    Write-Host "Compilation Complete!" -ForegroundColor Green
    Write-Host "Successfully compiled: $successCount / $($papers.Count) papers" -ForegroundColor Yellow
    
    if ($successCount -eq $papers.Count) {
        Write-Host "🎉 All papers compiled successfully!" -ForegroundColor Green
        Write-Host "PDFs are ready for submission." -ForegroundColor Green
    } else {
        Write-Host "⚠️  Some papers failed to compile. Check logs for details." -ForegroundColor Yellow
    }
    
    Write-Host ""
    Write-Host "Output directory: $OutputDir" -ForegroundColor Cyan
    Write-Host "Generated files:" -ForegroundColor Cyan
    if (Test-Path $OutputDir) {
        Get-ChildItem $OutputDir | ForEach-Object {
            Write-Host "  $($_.Name)" -ForegroundColor Gray
        }
    }
}

# Execute main function
Main