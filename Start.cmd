@echo off
if not exist "%~dp0release\OrangeStudy\OrangeStudy.exe" (
  echo Please build first: powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Package
  pause
  exit /b 1
)
start "Orange Study" /D "%~dp0release\OrangeStudy" "%~dp0release\OrangeStudy\OrangeStudy.exe" --data-dir "%~dp0data"
