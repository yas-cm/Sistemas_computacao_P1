@echo off
cd /d "%~dp0"

REM Compila os programas (descomente se quiser recompilar sempre)
REM g++ back\pipes\processo1.cpp -o back\pipes\processo1.exe
REM g++ back\pipes\processo2.cpp -o back\pipes\processo2.exe

REM Inicia o servidor Node.js
start cmd /k "node server.js"