param([string]$VulkanInclude = "", [string]$OutputDir = "")
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (!$OutputDir) { $OutputDir = Join-Path $repoRoot 'build' }
$OutputDir = Join-Path $OutputDir 'graphics/nvidia/ngx'
if (!$VulkanInclude -and $env:VULKAN_SDK) { $VulkanInclude = Join-Path $env:VULKAN_SDK 'Include' }
if (!(Test-Path -LiteralPath (Join-Path $VulkanInclude 'vulkan/vulkan.h'))) {
    throw 'Pass -VulkanInclude <directory containing vulkan/vulkan.h>, or set VULKAN_SDK.'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ Build Tools to build the optional NGX adapter.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsRoot) { throw 'Visual Studio C++ x64 tools were not found.' }
$sdkLib = Join-Path $repoRoot 'third_party/ngx/lib/nvsdk_ngx_s.lib'
$scratch = Join-Path $repoRoot '.agents/ngx'
New-Item -ItemType Directory -Force -Path (Split-Path $sdkLib), $scratch, $OutputDir | Out-Null
if (!(Test-Path -LiteralPath $sdkLib)) {
    Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/NVIDIA/DLSS/9981c6350dc58c0ce21f024fd65e31e851c31689/lib/Windows_x86_64/x64/nvsdk_ngx_s.lib' -OutFile $sdkLib
}
$commands = @"
@echo off
call "$vsRoot\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /LD /MT /O2 /EHsc /I "$repoRoot\third_party\ngx\include" /I "$VulkanInclude" /Fo"$scratch\ngx_sdk.obj" "$repoRoot\runtime\nvn\vulkan\temporal\nvidia\ngx_sdk.cpp" "$sdkLib" /link /OUT:"$OutputDir\mhgu-nvngx.dll" /IMPLIB:"$scratch\mhgu-nvngx.lib" advapi32.lib shell32.lib ole32.lib crypt32.lib wintrust.lib shlwapi.lib version.lib user32.lib
"@
$batch = Join-Path $scratch 'build.cmd'
[IO.File]::WriteAllText($batch, $commands, [Text.Encoding]::Default)
& $batch
if ($LASTEXITCODE -ne 0) { throw 'NGX adapter build failed.' }
