# Run a command inside the dev container image from PowerShell.
#
#   tools\dev.ps1                      # interactive shell
#   tools\dev.ps1 west build ...       # one command (arguments are passed through untouched)
#   tools\dev.ps1 -Build               # (re)build the image first
#
# If PowerShell refuses to run scripts, use tools\dev.cmd, which runs this file
# with -ExecutionPolicy Bypass.
#
# No param() block on purpose: PowerShell would otherwise bind west's own flags
# (-b, -p, -d, -v ...) to this script's parameters. $args is used verbatim.
#
# Environment:
#   CAN_ECU_SIM_IMAGE        image to run (default can-ecu-sim-dev:local)
#   CAN_ECU_SIM_DOCKER_ARGS  extra `docker run` flags, split on spaces
$ErrorActionPreference = "Stop"
$Image = if ($env:CAN_ECU_SIM_IMAGE) { $env:CAN_ECU_SIM_IMAGE } else { "can-ecu-sim-dev:local" }
$Platform = "linux/amd64"   # the Zephyr SDK tarball and gcc-multilib are x86_64 only
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$cmd = @($args)
$build = $false
if ($cmd.Count -gt 0 -and ($cmd[0] -eq "-Build" -or $cmd[0] -eq "--build")) {
    $build = $true
    $cmd = @($cmd | Select-Object -Skip 1)
}

if ($build) {
    & docker build --platform $Platform -t $Image -f (Join-Path $RepoRoot ".devcontainer\Dockerfile") $RepoRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($cmd.Count -eq 0) { exit 0 }
}

# Docker Desktop's WSL2 VM forgets the out-of-tree vcan module whenever it
# restarts; reinstall it from the cached build when it is missing (~10 s).
$ErrorActionPreference = "Continue"
& wsl.exe -d docker-desktop -e grep -q '^vcan ' /proc/modules 2>$null
$vcanLoaded = ($LASTEXITCODE -eq 0)
$ErrorActionPreference = "Stop"
if (-not $vcanLoaded) {
    & (Join-Path $PSScriptRoot "wsl2\install-vcan-module.ps1")
    if ($LASTEXITCODE -ne 0) { Write-Warning "vcan is not available in the Docker VM; see tools/wsl2/" }
}

$tty = if ($cmd.Count -eq 0 -and -not [Console]::IsInputRedirected) { "-it" } else { "-i" }
$extra = @()
if ($env:CAN_ECU_SIM_DOCKER_ARGS) { $extra = $env:CAN_ECU_SIM_DOCKER_ARGS -split " " }

& docker run --rm $tty --platform $Platform --cap-add=NET_ADMIN --hostname can-ecu-sim `
    -v "${RepoRoot}:/workspaces/can-ecu-sim" -w /workspaces/can-ecu-sim `
    @extra $Image @cmd
exit $LASTEXITCODE
