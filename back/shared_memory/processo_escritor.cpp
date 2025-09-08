#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <tchar.h>
#include <stdexcept>

// Estrutura que define o layout dos dados na memória compartilhada.
struct SharedData {
    char message[256];
};

void escrever_log(const std::string& mensagem_enviada, const std::string& mensagem_recebida, const std::string& status) {
    std::ofstream log("log.json", std::ios::trunc);
    log << "{\n"
        << "    \"mensagem_enviada\": \"" << mensagem_enviada << "\",\n"
        << "    \"mensagem_recebida\": \"" << mensagem_recebida << "\",\n"
        << "    \"status\": \"" << status << "\"\n"
        << "}" << std::endl;
    log.close();
}

int main() {
    LPCTSTR shm_name = TEXT("MySharedMemory");
    LPCTSTR mutex_name = TEXT("MySharedMutex");
    HANDLE hMapFile = NULL;
    SharedData* pBuf = NULL;
    HANDLE hMutex = NULL;
    HANDLE hProcess = NULL;
    HANDLE hThread = NULL;
    bool mutex_acquired = false;

    // Função de limpeza garantida
    auto cleanup = [&]() {
        if (mutex_acquired && hMutex) {
            ReleaseMutex(hMutex);
            mutex_acquired = false;
        }
        if (hMutex) CloseHandle(hMutex);
        if (pBuf) UnmapViewOfFile(pBuf);
        if (hMapFile) CloseHandle(hMapFile);
        if (hProcess) CloseHandle(hProcess);
        if (hThread) CloseHandle(hThread);
    };

    try {
        // --- PASSO 1: CRIAR MEMÓRIA COMPARTILHADA ---
        hMapFile = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(SharedData),
            shm_name
        );
        
        if (hMapFile == NULL) {
            throw std::runtime_error("CreateFileMapping falhou: " + std::to_string(GetLastError()));
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cout << "Aviso: Memória compartilhada já existia, reutilizando..." << std::endl;
        }

        // --- PASSO 2: MAPEAR MEMÓRIA ---
        pBuf = (SharedData*)MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(SharedData)
        );
        
        if (pBuf == NULL) {
            throw std::runtime_error("MapViewOfFile falhou: " + std::to_string(GetLastError()));
        }

        // --- PASSO 3: CRIAR MUTEX ---
        hMutex = CreateMutex(NULL, FALSE, mutex_name);
        if (hMutex == NULL) {
            throw std::runtime_error("CreateMutex falhou: " + std::to_string(GetLastError()));
        }

        // --- PASSO 4: OBTER MENSAGEM ---
        std::string mensagem;
        std::cout << "Digite a mensagem para enviar via Memoria Compartilhada:" << std::endl;
        std::getline(std::cin, mensagem);

        if (mensagem.empty()) {
            throw std::runtime_error("Mensagem vazia não é permitida");
        }
        
        if (mensagem.size() >= sizeof(pBuf->message)) {
            throw std::runtime_error("Mensagem muito grande! Máximo: " + 
                                   std::to_string(sizeof(pBuf->message) - 1) + " caracteres");
        }

        // --- PASSO 5: INICIAR PROCESSO LEITOR ---
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        std::string cmdLine = "processo_leitor.exe";

        if (!CreateProcessA(
            NULL,
            &cmdLine[0],
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &si,
            &pi
        )) {
            throw std::runtime_error("CreateProcess falhou: " + std::to_string(GetLastError()));
        }
        
        hProcess = pi.hProcess;
        hThread = pi.hThread;

        // --- PASSO 6: ESCREVER NA MEMÓRIA ---
        std::cout << "Aguardando para escrever na memoria (timeout: 10s)..." << std::endl;
        
        DWORD waitResult = WaitForSingleObject(hMutex, 10000);
        if (waitResult == WAIT_TIMEOUT) {
            throw std::runtime_error("Timeout: Mutex ocupado por mais de 10 segundos");
        }
        if (waitResult != WAIT_OBJECT_0) {
            throw std::runtime_error("Falha ao adquirir mutex: " + std::to_string(GetLastError()));
        }
        mutex_acquired = true;

        // REGIÃO CRÍTICA - com proteção manual
        try {
            ZeroMemory(pBuf->message, sizeof(pBuf->message));
            
            CopyMemory((PVOID)pBuf->message, mensagem.c_str(), mensagem.size() + 1);
            
            if (strncmp(pBuf->message, mensagem.c_str(), mensagem.size()) != 0) {
                throw std::runtime_error("Falha na verificação da escrita - dados corrompidos");
            }
            
            std::cout << "Mensagem escrita e verificada com sucesso: " << pBuf->message << std::endl;
        }
        catch (...) {
            // Libera mutex em caso de erro na região crítica
            if (mutex_acquired) {
                ReleaseMutex(hMutex);
                mutex_acquired = false;
            }
            throw; // Re-lança a exceção
        }
        
        // Liberação normal do mutex
        ReleaseMutex(hMutex);
        mutex_acquired = false;

        // --- PASSO 7: AGUARDAR PROCESSO LEITOR ---
        std::cout << "Aguardando processo leitor terminar..." << std::endl;
        
        waitResult = WaitForSingleObject(pi.hProcess, 15000);
        if (waitResult == WAIT_TIMEOUT) {
            throw std::runtime_error("Timeout: Processo leitor não terminou em 15 segundos");
        }
        if (waitResult != WAIT_OBJECT_0) {
            throw std::runtime_error("Erro ao aguardar processo leitor: " + std::to_string(GetLastError()));
        }

        DWORD exitCode;
        if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
            throw std::runtime_error("Falha ao obter código de saída do leitor: " + std::to_string(GetLastError()));
        }
        
        if (exitCode != 0) {
            throw std::runtime_error("Processo leitor falhou com código: " + std::to_string(exitCode));
        }

        std::cout << "Processo leitor terminou com sucesso." << std::endl;

        // --- PASSO 8: LOG DE SUCESSO ---
        escrever_log(mensagem, mensagem, "sucesso");
        
        cleanup();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERRO CRÍTICO: " << e.what() << std::endl;
        escrever_log("", "", "erro");
        cleanup();
        return 1;
    }
}