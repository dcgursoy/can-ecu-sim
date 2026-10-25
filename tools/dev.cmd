@echo off
rem Runs tools\dev.ps1 without needing a relaxed PowerShell execution policy.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0dev.ps1" %*
exit /b %ERRORLEVEL%
