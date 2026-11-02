# UI snapshot review pack generator.
#
# Reads a batch produced by "run.bat --ui-snapshots" (build/ui-snapshots/<UTC>) and turns it into
# a small review package:
#   1. Selects the pages worth confirming (default scope: main window input states + settings).
#   2. Composes light/dark comparison sheets, so the whole scope fits in a few images.
#   3. Compares PNG bytes with the previous batch and reports changed / added / removed pages.
#   4. Writes review/index.html, review/summary.json and prints the sheet list for delivery.
#
# PNG output of the Qt snapshot runner is deterministic, so a file hash is a reliable change signal.
# This file must stay ASCII-only (Windows PowerShell 5.1 reads non-BOM files as ANSI).

[CmdletBinding()]
param(
    # Regenerate a fresh batch first (equivalent to running run.bat --ui-snapshots).
    [switch]$Generate,
    # Batch directory. Default: build/ui-snapshots/LATEST.txt.
    [string]$Batch = '',
    # core = main window input states + settings pages, all = every captured page.
    [ValidateSet('core', 'all')]
    [string]$Scope = 'core',
    # Comma separated id patterns overriding -Scope, e.g. "json-*,update".
    [string]$Ids = '',
    # Batch to diff against. Default: the batch right before the current one. Pass "none" to skip.
    [string]$Baseline = '',
    # Output directory. Default: <batch>/review.
    [string]$OutDir = '',
    # Tile width in pixels for each screenshot inside a sheet.
    [ValidateRange(120, 2000)]
    [int]$TileWidth = 620,
    # How many pages per sheet (each page is one light/dark row).
    [ValidateRange(1, 12)]
    [int]$RowsPerSheet = 4,
    # Print every page id in the batch and whether it falls inside the review scope, then exit.
    [switch]$List
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$RepoRoot = [System.IO.Path]::GetDirectoryName($PSScriptRoot)
$SnapshotRoot = Join-Path $RepoRoot 'build/ui-snapshots'
$LatestFile = Join-Path $SnapshotRoot 'LATEST.txt'

# Split-Path in PowerShell 5.1 rejects -LiteralPath with -Leaf, and LATEST.txt stores forward slashes.
function Get-Leaf([string]$Path) { return [System.IO.Path]::GetFileName($Path.TrimEnd('\', '/')) }

function Resolve-LatestBatch {
    if (-not (Test-Path -LiteralPath $LatestFile)) {
        throw "Missing $LatestFile - run build.bat or this script with -Generate first."
    }
    return (Get-Content -LiteralPath $LatestFile -Raw).Trim()
}

if ($Generate) {
    # The runner defaults to main+settings; -Scope all asks for every page group.
    $runnerArgs = if ($Scope -eq 'all') { '--ui-snapshots --pages=all' } else { '--ui-snapshots' }
    # Native cmd.exe inherits the process working directory, not the PowerShell location, so use the full path.
    $runner = Join-Path $RepoRoot 'run.bat'
    & cmd.exe /c "`"$runner`" $runnerArgs"
    if ($LASTEXITCODE -ne 0) { throw "run.bat $runnerArgs failed with code $LASTEXITCODE" }
}

if (-not $Batch) { $Batch = Resolve-LatestBatch }
if (-not (Test-Path -LiteralPath $Batch)) { throw "Batch directory not found: $Batch" }

$ManifestPath = Join-Path $Batch 'manifest.json'
if (-not (Test-Path -LiteralPath $ManifestPath)) { throw "manifest.json not found in $Batch" }

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$entries = @($manifest.images)
$runnerGroups = @($manifest.groups)

$coreIds = @('launcher', 'calculator', 'search-empty', 'clipboard-history', 'screenshot-history',
    'directory', 'floating-icon')
$corePrefixes = @('settings-')

function Test-InScope([string]$Id) {
    if ($Ids) {
        foreach ($pattern in $Ids.Split(',')) {
            if ($Id -like $pattern.Trim()) { return $true }
        }
        return $false
    }
    if ($Scope -eq 'all') { return $true }
    if ($coreIds -contains $Id) { return $true }
    foreach ($prefix in $corePrefixes) { if ($Id -like "$prefix*") { return $true } }
    return $false
}

if ($List) {
    Write-Host "Batch:  $Batch"
    Write-Host "Groups: $(if ($runnerGroups) { $runnerGroups -join ',' } else { 'schema-1 batch (every page)' })"
    foreach ($id in @($entries | ForEach-Object { $_.id } | Sort-Object -Unique)) {
        $first = $entries | Where-Object { $_.id -eq $id } | Select-Object -First 1
        Write-Host ("{0}`t{1}x{2}`t{3}" -f $id, $first.width, $first.height,
            $(if (Test-InScope $id) { 'in scope' } else { '-' }))
    }
    return
}

# Existing batches ordered by name (names are UTC timestamps).
$batches = @(Get-ChildItem -LiteralPath $SnapshotRoot -Directory |
    Where-Object { $_.Name -match '^\d{8}-\d{6}-\d{3}$' } |
    Sort-Object Name | ForEach-Object { $_.FullName })

if (-not $Baseline) {
    # Compare by leaf name: LATEST.txt uses forward slashes, FullName uses backslashes.
    $leaf = Get-Leaf $Batch
    $index = [Array]::IndexOf($batches, ($batches | Where-Object { (Get-Leaf $_) -eq $leaf } | Select-Object -First 1))
    if ($index -gt 0) { $Baseline = $batches[$index - 1] } else { $Baseline = 'none' }
}

# Page order follows the manifest so related states stay next to each other.
$pages = @()
$order = 0
foreach ($entry in $entries) {
    if ($pages.Name -contains $entry.id) { continue }
    if (-not (Test-InScope $entry.id)) { continue }
    $pages += [pscustomobject]@{ Name = $entry.id; Order = $order++ }
}
if ($pages.Count -eq 0) {
    # A core-only batch simply has no json/capture pages; say so instead of blaming the filter.
    throw ("No pages matched. Batch groups: $(if ($runnerGroups) { $runnerGroups -join ',' } else { 'all' }). " +
        'Use -Ids for ids listed by -List, or -Generate -Scope all to capture every group.')
}

$skipped = @($entries | ForEach-Object { $_.id } | Sort-Object -Unique |
    Where-Object { $pages.Name -notcontains $_ })

function Get-PngHash([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

# --- change detection against the baseline batch -------------------------------------------------
$baselineBatch = if ($Baseline -eq 'none') { $null } else { $Baseline }
$changed = New-Object System.Collections.Generic.List[string]
$added = New-Object System.Collections.Generic.List[string]
$removed = New-Object System.Collections.Generic.List[string]
$dropped = New-Object System.Collections.Generic.List[string]
$missing = New-Object System.Collections.Generic.List[string]

if ($baselineBatch) {
    $baselineManifest = Join-Path $baselineBatch 'manifest.json'
    $baselineIds = @()
    if (Test-Path -LiteralPath $baselineManifest) {
        $baselineIds = @(Get-Content -LiteralPath $baselineManifest -Raw | ConvertFrom-Json).images |
            ForEach-Object { $_.id } | Sort-Object -Unique
    }
    $batchIds = @($entries | ForEach-Object { $_.id } | Sort-Object -Unique)
    foreach ($page in $pages) {
        $differs = $false
        $absent = $false
        foreach ($theme in 'light', 'dark') {
            $file = "$theme-$($page.Name).png"
            $current = Get-PngHash (Join-Path $Batch $file)
            if (-not $current) { $absent = $true; continue }
            $before = Get-PngHash (Join-Path $baselineBatch $file)
            if (-not $before) { $differs = $true }
            elseif ($before -ne $current) { $differs = $true }
        }
        if ($absent) { $missing.Add($page.Name) }
        if ($baselineIds -notcontains $page.Name) { $added.Add($page.Name) }
        elseif ($differs) { $changed.Add($page.Name) }
    }
    foreach ($id in $baselineIds) {
        # Distinguish "the runner stopped capturing it" from "this review scope ignores it".
        if ($batchIds -notcontains $id) { $removed.Add($id) }
        elseif ($pages.Name -notcontains $id) { $dropped.Add($id) }
    }
}

$changedSet = @($changed)
$addedSet = @($added)

# --- contact sheet rendering ---------------------------------------------------------------------
if (-not $OutDir) { $OutDir = Join-Path $Batch 'review' }
if (Test-Path -LiteralPath $OutDir) { Remove-Item -LiteralPath $OutDir -Recurse -Force }
New-Item -ItemType Directory -Path $OutDir | Out-Null

$bg = [System.Drawing.Color]::FromArgb(255, 244, 244, 245)
$ink = [System.Drawing.Color]::FromArgb(255, 24, 24, 27)
$muted = [System.Drawing.Color]::FromArgb(255, 113, 113, 122)
$mark = [System.Drawing.Color]::FromArgb(255, 217, 119, 6)
$frame = [System.Drawing.Color]::FromArgb(255, 212, 212, 216)
$font = New-Object System.Drawing.Font('Segoe UI', 20, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$smallFont = New-Object System.Drawing.Font('Segoe UI', 15, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)

$padding = 28
$gutter = 26
$labelHeight = 34
$gap = 14
$headerHeight = 74
$sheetWidth = $padding * 2 + $TileWidth * 2 + $gutter

function Get-Tile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    $image = [System.Drawing.Image]::FromFile($Path)
    # Downscale only: enlarging tiny widgets (floating icon) would just blur them and hide real pixels.
    $scale = [Math]::Min(1.0, $TileWidth / [double]$image.Width)
    $size = New-Object System.Drawing.Bitmap([int]($image.Width * $scale), [int]($image.Height * $scale))
    $size.SetResolution($image.HorizontalResolution, $image.VerticalResolution)
    $g = [System.Drawing.Graphics]::FromImage($size)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($image, 0, 0, $size.Width, $size.Height)
    $g.Dispose()
    $image.Dispose()
    return $size
}

$stamp = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd HH:mm') + ' UTC'
$batchName = Get-Leaf $Batch
$sheets = New-Object System.Collections.Generic.List[object]
$pageRuns = [Math]::Max(1, [Math]::Ceiling($pages.Count / $RowsPerSheet))

for ($run = 0; $run -lt $pageRuns; $run++) {
    $chunk = @($pages | Where-Object { $_.Order -ge ($run * $RowsPerSheet) -and $_.Order -lt (($run + 1) * $RowsPerSheet) })
    $tiles = @()
    foreach ($page in $chunk) {
        $tiles += [pscustomobject]@{
            Name  = $page.Name
            Light = (Get-Tile (Join-Path $Batch "light-$($page.Name).png"))
            Dark  = (Get-Tile (Join-Path $Batch "dark-$($page.Name).png"))
        }
    }

    $rows = @()
    foreach ($tile in $tiles) {
        $lh = $(if ($tile.Light) { $tile.Light.Height } else { 80 })
        $dh = $(if ($tile.Dark) { $tile.Dark.Height } else { 80 })
        $rows += ($labelHeight + [Math]::Max($lh, $dh) + $gap)
    }
    $sheetHeight = [int]($padding * 2 + $headerHeight + ($rows | Measure-Object -Sum).Sum)
    $sheetHeight = [Math]::Max($sheetHeight, 400)

    $canvas = New-Object System.Drawing.Bitmap($sheetWidth, $sheetHeight)
    $canvas.SetResolution(96, 96)
    $g = [System.Drawing.Graphics]::FromImage($canvas)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit
    $g.Clear($bg)

    $brush = New-Object System.Drawing.SolidBrush($ink)
    $mutedBrush = New-Object System.Drawing.SolidBrush($muted)
    $markBrush = New-Object System.Drawing.SolidBrush($mark)
    $framePen = New-Object System.Drawing.Pen($frame, 1)

    $g.DrawString("TypeGo UI review - $($chunk[0].Name) .. $($chunk[-1].Name)", $font, $brush, $padding, $padding)
    $g.DrawString("$batchName | $(if ($Ids) { "ids=$Ids" } else { "$Scope scope" }) | sheet $($run + 1)/$pageRuns of $($pages.Count) pages | $stamp | baseline $(if ($baselineBatch) { Get-Leaf $baselineBatch } else { 'none' })",
        $smallFont, $mutedBrush, $padding, ($padding + 34))

    $y = $padding + $headerHeight
    foreach ($tile in $tiles) {
        $rowHeight = [Math]::Max(
            $(if ($tile.Light) { $tile.Light.Height } else { 80 }),
            $(if ($tile.Dark) { $tile.Dark.Height } else { 80 }))

        $badge = ''
        if ($changedSet -contains $tile.Name) { $badge = ' [changed]' }
        elseif ($addedSet -contains $tile.Name) { $badge = ' [new]' }
        $g.DrawString("$($tile.Name)$badge", $smallFont, $(if ($badge) { $markBrush } else { $brush }), $padding, $y)
        $labelY = $y + $labelHeight

        $columns = @(
            @{ img = $tile.Light; x = $padding; tag = 'light' },
            @{ img = $tile.Dark; x = ($padding + $TileWidth + $gutter); tag = 'dark' }
        )
        foreach ($column in $columns) {
            if ($column.img) {
                $g.DrawImage($column.img, $column.x, $labelY)
                $g.DrawRectangle($framePen, $column.x - 0.5, $labelY - 0.5, $column.img.Width + 1, $column.img.Height + 1)
            }
            else {
                $g.DrawString("$($column.tag) missing", $smallFont, $markBrush, $column.x, $labelY)
            }
        }

        $y += $labelHeight + $rowHeight + $gap
    }

    $sheetName = "sheet-{0:d2}.png" -f ($run + 1)
    $sheetPath = Join-Path $OutDir $sheetName
    $canvas.Save($sheetPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $brush.Dispose(); $mutedBrush.Dispose(); $markBrush.Dispose(); $framePen.Dispose()
    $tiles | ForEach-Object { if ($_.Light) { $_.Light.Dispose() }; if ($_.Dark) { $_.Dark.Dispose() } }
    $canvas.Dispose()

    $sheets.Add([pscustomobject]@{
            file  = "review/$sheetName"
            abs   = $sheetPath
            pages = @($tiles | ForEach-Object { $_.Name })
            width = $sheetWidth
            height = $sheetHeight
        })
}

# --- review gallery + machine readable summary ---------------------------------------------------
$gallery = New-Object System.Text.StringBuilder
[void]$gallery.AppendLine('<!doctype html><html lang="zh-CN"><meta charset="utf-8">')
[void]$gallery.AppendLine('<meta name="viewport" content="width=device-width,initial-scale=1">')
[void]$gallery.AppendLine("<title>TypeGo UI review - $batchName</title>")
[void]$gallery.AppendLine('<style>body{font:16px/1.5 system-ui;margin:0 auto;padding:16px;max-width:1100px;background:#f4f4f5;color:#18181b}')
[void]$gallery.AppendLine('img{width:100%;height:auto;display:block;border-radius:12px;border:1px solid #d4d4d8}')
[void]$gallery.AppendLine('h2{margin-top:32px}code{background:#e4e4e7;padding:2px 6px;border-radius:6px}')
[void]$gallery.AppendLine('@media(prefers-color-scheme:dark){body{background:#18181b;color:#f4f4f5}code{background:#27272a}img{border-color:#3f3f46}}</style>')
[void]$gallery.AppendLine("<h1>TypeGo UI review</h1><p>Batch <code>$batchName</code> | scope <code>$Scope</code> | baseline <code>$(if ($baselineBatch) { Get-Leaf $baselineBatch } else { 'none' })</code></p>")
if ($changedSet) { [void]$gallery.AppendLine("<p><strong>changed:</strong> $($changedSet -join ', ')</p>") }
if ($addedSet) { [void]$gallery.AppendLine("<p><strong>new:</strong> $($addedSet -join ', ')</p>") }
if ($removed) { [void]$gallery.AppendLine("<p><strong>no longer captured:</strong> $($removed -join ', ')</p>") }
if ($dropped) { [void]$gallery.AppendLine("<p><strong>outside this scope:</strong> $($dropped -join ', ')</p>") }
foreach ($sheet in $sheets) {
    [void]$gallery.AppendLine("<h2>$($sheet.file)</h2><p>$($sheet.pages -join ' | ')</p>")
    [void]$gallery.AppendLine("<img loading='lazy' alt='$($sheet.file)' src='$($sheet.file)'>")
}
[void]$gallery.AppendLine("<p>Full-resolution single pages: <code>$Batch</code></p></html>")
Set-Content -LiteralPath (Join-Path $OutDir 'index.html') -Value $gallery.ToString() -Encoding UTF8

$summary = [ordered]@{
    batch         = $Batch
    baseline      = $(if ($baselineBatch) { $baselineBatch } else { 'none' })
    scope         = $Scope
    ids           = $(if ($Ids) { $Ids } else { '' })
    pageTotal     = $pages.Count
    pages         = @($pages | ForEach-Object { $_.Name })
    skipped       = @($skipped)
    changed       = $changedSet
    added         = $addedSet
    removed       = @($removed)
    dropped       = @($dropped)
    missing       = @($missing)
    sheets        = @($sheets | ForEach-Object { $_.abs })
    gallery       = Join-Path $OutDir 'index.html'
    generatedUtc  = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
}
$summaryPath = Join-Path $OutDir 'summary.json'
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "Batch:    $Batch"
Write-Host "Groups:   $(if ($runnerGroups) { $runnerGroups -join ',' } else { 'schema-1 batch (all pages)' })"
Write-Host "Baseline: $(if ($baselineBatch) { $baselineBatch } else { 'none' })"
Write-Host "Scope:    $(if ($Ids) { "ids=$Ids" } else { $Scope }) ($($pages.Count) pages, $($sheets.Count) sheets)"
if ($changedSet) { Write-Host "Changed:  $($changedSet -join ', ')" }
if ($addedSet) { Write-Host "New:      $($addedSet -join ', ')" }
if ($removed) { Write-Host "Gone:     $($removed -join ', ')" }
Write-Host "Filtered: $(@($skipped).Count) ids outside scope, $(@($dropped).Count) ids not reviewed vs baseline"
if ($missing) { Write-Host "Missing:  $(@($missing) -join ', ')" }
foreach ($sheet in $sheets) { Write-Host "Sheet:    $($sheet.abs)" }
Write-Host "Gallery:  $(Join-Path $OutDir 'index.html')"
Write-Host "Summary:  $summaryPath"
