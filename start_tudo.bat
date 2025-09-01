@echo off
cd /d "%~dp0"

REM Compila os programas (descomente se quiser recompilar sempre)
g++ back\pipes\processo1.cpp -o back\pipes\processo1.exe
g++ back\pipes\processo2.cpp -o back\pipes\processo2.exe

REM Inicia o servidor Node.js
start cmd /k "node server.js"

REM Abre o navegador na página inicial
start "" http://localhost:3000/html/index.html