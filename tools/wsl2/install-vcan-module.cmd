@echo off
rem Runs install-vcan-module.ps1 without needing a relaxed PowerShell execution policy.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-vcan-module.ps1" %*
exit /b %ERRORLEVEL%
