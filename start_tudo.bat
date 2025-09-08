@echo off
cd /d "%~dp0"
title Sistema de Comunicação entre Processos

REM Finalizar processos existentes silenciosamente
echo Finalizando processos existentes...
taskkill /f /im socket_server.exe 2>nul
taskkill /f /im node.exe 2>nul
taskkill /f /im processo1.exe 2>nul
taskkill /f /im processo2.exe 2>nul

REM Compilar pipes
echo Compilando pipes...
g++ back\pipes\processo1.cpp -o back\pipes\processo1.exe
g++ back\pipes\processo2.cpp -o back\pipes\processo2.exe

REM Compilar sockets
echo Compilando sockets...
g++ back\sockets\socket_server.cpp -o back\sockets\socket_server.exe -lws2_32
g++ back\sockets\socket_client.cpp -o back\sockets\socket_client.exe -lws2_32

REM Iniciar servidor socket em segundo plano SEM JANELA
echo Iniciando servidor socket...
start /B back\sockets\socket_server.exe

REM Iniciar servidor Node.js EM SEGUNDO PLANO
echo Iniciando servidor Node.js...
timeout 2
start /B node server.js

REM Aguardar um pouco para o servidor iniciar
echo Aguardando inicialização do servidor...
timeout 3

REM Abrir navegador no index.html
echo Abrindo navegador...
start "" http://localhost:3000/index.html

echo.
echo ========================================
echo Sistema iniciado com sucesso!
echo.
echo URL: http://localhost:3000/index.html
echo.
echo Esta janela pode ser fechada a qualquer momento
echo Todos os processos serao finalizados automaticamente
echo ========================================
echo.

REM Esperar usuario pressionar qualquer tecla para finalizar
echo Pressione qualquer tecla para finalizar o sistema...
pause >nul

REM Finalizar processos ao sair
echo.
echo Finalizando processos...
taskkill /f /im socket_server.exe 2>nul
taskkill /f /im node.exe 2>nul
taskkill /f /im processo1.exe 2>nul
taskkill /f /im processo2.exe 2>nul

echo Todos os processos finalizados.
echo Pressione qualquer tecla para sair...
pause >nul