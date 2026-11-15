@echo off
echo Starting Backend and Frontend...

start "Order Book Backend" cmd /k "cd /d %~dp0backend && node index.js"
start "Order Book Frontend" cmd /k "cd /d %~dp0frontend && npm run dev"

echo Both services launched in separate windows.
