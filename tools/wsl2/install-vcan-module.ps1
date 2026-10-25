# PowerShell entry point: runs install-vcan-module.sh with Git Bash (the script
# itself only needs bash + docker). Pass -Rebuild to force a module rebuild.
#
# Git Bash is located from git.exe rather than from PATH, because on any machine
# with the WSL feature enabled C:\Windows\System32\bash.exe (the WSL launcher)
# shadows Git's bash.exe.
$ErrorActionPreference = "Stop"

$bash = $null
$git = Get-Command git.exe -ErrorAction SilentlyContinue
if ($git) {
    $candidate = Join-Path (Split-Path (Split-Path $git.Source -Parent) -Parent) "bin\bash.exe"
    if (Test-Path $candidate) { $bash = $candidate }
}
if (-not $bash) {
    foreach ($candidate in @(
            (Join-Path $env:ProgramFiles "Git\bin\bash.exe"),
            (Join-Path ${env:ProgramFiles(x86)} "Git\bin\bash.exe"),
            (Join-Path $env:LOCALAPPDATA "Programs\Git\bin\bash.exe"))) {
        if ($candidate -and (Test-Path $candidate)) { $bash = $candidate; break }
    }
}
if (-not $bash) {
    throw "Git Bash (bash.exe from Git for Windows) not found; install Git for Windows"
}

$script = Join-Path $PSScriptRoot "install-vcan-module.sh"
$scriptArgs = @()
foreach ($a in $args) {
    if ($a -eq "-Rebuild" -or $a -eq "--rebuild") { $scriptArgs += "--rebuild" }
}
& $bash $script @scriptArgs
exit $LASTEXITCODE
