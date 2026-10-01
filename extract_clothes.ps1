$file = "$env:USERPROFILE\OneDrive\Escritorio\Proyectos\Red redemptio\Ejemplos\RedEM_PreBuild-main\RedEM_PreBuild\server-data\resources\redemrp_clothes_store\utils\test.lua"
$out = "$env:USERPROFILE\OneDrive\Escritorio\Proyectos\Red redemptio\RDR2Coop.Client.Cpp\clothes_db.h"

Write-Host "Extracting hashes from test.lua..."

$categories = @{}
$currentCat = ""
$currentGender = ""
$lines = Get-Content $file

foreach ($line in $lines) {
    if ($line -match "^\s+\['(male|female)'\]") {
        $currentGender = $Matches[1]
        continue
    }
    if ($line -match "^\s+\['(\w+)'\]\s*=\s*\{" -and $currentGender) {
        $currentCat = $Matches[1]
        if (-not $categories.ContainsKey("$currentGender/$currentCat")) {
            $categories["$currentGender/$currentCat"] = @()
        }
        continue
    }
    if ($line -match "\['hash'\]\s*=\s*(\d+)" -and $currentCat) {
        $hash = [uint32]$Matches[1]
        $categories["$currentGender/$currentCat"] += $hash
    }
}

$h = @"
#pragma once
#include <cstdint>
// Auto-generated clothes database from RedEM
// Index = texture_id - 1 (for categories with 1 model)
// Usage: hash = CLOTHES_[GENDER]_[CAT][texture_idx - 1]

"@

foreach ($key in $categories.Keys | Sort-Object) {
    $gender, $cat = $key -split '/'
    $arr = $categories[$key]
    if ($arr.Count -eq 0) { continue }
    $name = "CLOTHES_" + $gender.ToUpper() + "_" + $cat.ToUpper()
    $h += "static const uint32_t $name" + "[$($arr.Count)] = {`n    "
    $h += ($arr -join ", ")
    $h += "`n};`n`n"
}

$h += "// Total: $($categories.Count) categories`n"
Set-Content $out $h

$total = 0; foreach ($v in $categories.Values) { $total += $v.Count }
Write-Host "Done! $total hashes in $($categories.Count) categories -> $out"
