$ErrorActionPreference = "Continue"

$src = "$PSScriptRoot\src"
$obs_src = "$PSScriptRoot\..\obs-studio"
$obs_bin = "C:\Program Files\obs-studio\bin\64bit"
$build = "$PSScriptRoot\build"
$mingw = "C:\msys64\mingw64\bin"

Remove-Item -Recurse -Force $build -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $build | Out-Null

$env:Path = "$mingw;$env:Path"

$incs = @(
    "-I$obs_src\libobs",
    "-I$obs_src\libobs\graphics",
    "-I$obs_src\frontend\api",
    "-I$src"
)

$cxx_flags = @("-O2", "-std=c++17", "-DWIN32", "-D_WINDOWS")

# Step 1: Generate obs.dll import lib
Write-Host "Step 1: obs.dll import library..."
$obsDef = "$build\obs.def"
$obsImplib = "$build\libobs.dll.a"
$defContent = cmd /c "$mingw\gendef.exe - `"$obs_bin\obs.dll`" 2>nul" | Out-File -FilePath $obsDef -Encoding ASCII
if ((-not (Test-Path $obsDef)) -or ((Get-Item $obsDef).Length -eq 0)) {
    Write-Host "ERROR: gendef did not produce obs.def"
    exit 1
}
$null = cmd /c "$mingw\dlltool.exe -d `"$obsDef`" -l `"$obsImplib`" -D `"$obs_bin\obs.dll`"" 2>&1
Write-Host "  obs: $((Get-Item $obsImplib).Length) bytes"

# Step 2: Generate obs-frontend-api.dll import lib
Write-Host "Step 2: obs-frontend-api.dll import library..."
$feDll = "$obs_bin\obs-frontend-api.dll"
$feDef = "$build\frontend.def"
$feImplib = "$build\libfrontend.dll.a"
cmd /c "$mingw\gendef.exe - `"$feDll`" 2>nul" | Out-File -FilePath $feDef -Encoding ASCII
$null = cmd /c "$mingw\dlltool.exe -d `"$feDef`" -l `"$feImplib`" -D `"$feDll`"" 2>&1
Write-Host "  frontend: $((Get-Item $feImplib).Length) bytes"

# Step 3: Compile
Write-Host "Step 3: Compiling..."
$objs = @()
$sources = @("warp-mesh.cpp", "editor-window.cpp", "plugin-main.cpp")
foreach ($f in $sources) {
    $obj = "$build\$($f).o"
    $args = @("-c", "$src\$f", "-o", $obj) + $cxx_flags + $incs
    $out = & "$mingw\g++.exe" @args 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED: $f"
        Write-Host $out
        exit 1
    }
    Write-Host "  $f OK"
    $objs += $obj
}

# Step 4: Link
Write-Host "Step 4: Linking..."
$link_args = @(
    "-shared",
    "-o", "$build\hltn-advanced.dll",
    $objs,
    $obsImplib,
    $feImplib,
    "-lgdi32",
    "-lcomdlg32",
    "-static-libgcc",
    "-static-libstdc++",
    "-Wl,--subsystem,windows"
)
$out = & "$mingw\g++.exe" @link_args 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "LINK FAILED:"
    Write-Host $out
    exit 1
}

$dllPath = "$build\hltn-advanced.dll"
if (Test-Path $dllPath) {
    Write-Host "  hltn-advanced.dll: $((Get-Item $dllPath).Length) bytes"
} else {
    Write-Host "ERROR: DLL not created"
    exit 1
}

# Step 5: Runtime DLLs + data
Write-Host "Step 5: Runtime DLLs + data..."
$runtime = @(
    "$mingw\libgcc_s_seh-1.dll",
    "$mingw\libstdc++-6.dll",
    "$mingw\libwinpthread-1.dll"
)
foreach ($r in $runtime) {
    Copy-Item $r -Destination $build -Force
    Write-Host "  $(Split-Path $r -Leaf)"
}
$dataDir = "$build\data"
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null
Copy-Item "$PSScriptRoot\data\edge_blend.effect" -Destination $dataDir -Force
Write-Host "  edge_blend.effect"

Write-Host "`nBUILD SUCCESS!"
Write-Host "Output: $dllPath"
