@echo off
cd /d "%~dp0"
title Sistema de Comunicação entre Processos

echo ========================================
echo  Iniciando Sistema de IPC
echo ========================================

REM Finalizar processos existentes silenciosamente
echo Finalizando processos existentes...
taskkill /f /im socket_server.exe 2>nul
taskkill /f /im node.exe 2>nul
taskkill /f /im processo1.exe 2>nul
taskkill /f /im processo2.exe 2>nul
taskkill /f /im processo_escritor.exe 2>nul
taskkill /f /im processo_leitor.exe 2>nul

REM Compilar pipes
echo Compilando pipes...
g++ back\pipes\processo1.cpp -o back\pipes\processo1.exe
if %errorlevel% neq 0 (
    echo Erro ao compilar processo1.cpp
    pause
    exit /b 1
)
g++ back\pipes\processo2.cpp -o back\pipes\processo2.exe
if %errorlevel% neq 0 (
    echo Erro ao compilar processo2.cpp
    pause
    exit /b 1
)

REM Compilar sockets
echo Compilando sockets...
g++ back\sockets\socket_server.cpp -o back\sockets\socket_server.exe -lws2_32
if %errorlevel% neq 0 (
    echo Erro ao compilar socket_server.cpp
    pause
    exit /b 1
)
g++ back\sockets\socket_client.cpp -o back\sockets\socket_client.exe -lws2_32
if %errorlevel% neq 0 (
    echo Erro ao compilar socket_client.cpp
    pause
    exit /b 1
)

REM Compilar memória compartilhada
echo Compilando memoria compartilhada...
g++ back\shared_memory\processo_escritor.cpp -o back\shared_memory\processo_escritor.exe
if %errorlevel% neq 0 (
    echo Erro ao compilar processo_escritor.cpp
    pause
    exit /b 1
)
g++ back\shared_memory\processo_leitor.cpp -o back\shared_memory\processo_leitor.exe
if %errorlevel% neq 0 (
    echo Erro ao compilar processo_leitor.cpp
    pause
    exit /b 1
)

echo.
echo Compilacao concluida com sucesso!
echo.

REM Iniciar servidor socket em segundo plano SEM JANELA
echo Iniciando servidor socket...
start /B back\sockets\socket_server.exe

REM Aguardar um pouco para o socket server iniciar
timeout /t 2 /nobreak >nul

REM Iniciar servidor Node.js EM SEGUNDO PLANO
echo Iniciando servidor Node.js...
start /B node server.js

REM Aguardar um pouco para o servidor iniciar
echo Aguardando inicializacao do servidor...
timeout /t 3 /nobreak >nul

REM Abrir navegador no index.html
echo Abrindo navegador...
start "" http://localhost:3000/index.html

echo.
echo ========================================
echo  Sistema iniciado com sucesso!
echo ========================================
echo.
echo URL: http://localhost:3000/index.html
echo.
echo Esta janela pode ser fechada a qualquer momento
echo Todos os processos serao finalizados automaticamente
echo.
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
taskkill /f /im processo_escritor.exe 2>nul
taskkill /f /im processo_leitor.exe 2>nul

echo Todos os processos finalizados.
echo Pressione qualquer tecla para sair...
pause >nul