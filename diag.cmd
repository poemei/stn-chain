@echo off
cd /d "%~dp0"

echo Working directory:
cd

echo.
echo Genesis:
dir "genesis.block"

echo.
echo Transaction:
dir "selected.stnt"

echo.
"build\x64\Release\stn-chain.exe" --genesis "genesis.block" --transaction "selected.stnt" --data "chain.stns" --rpc-port 18473

pause