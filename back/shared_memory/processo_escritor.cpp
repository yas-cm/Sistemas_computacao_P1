#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <tchar.h>
#include <stdexcept>

// Estrutura de dados compartilhados - tamanho fixo para segurança
struct SharedData {
    char message[256]; // Buffer fixo de 256 caracteres
};

// Função de log padronizada (igual às outras IPC)
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
    // Nomes dos objetos de sincronização
    LPCTSTR shm_name = TEXT("MySharedMemory");
    LPCTSTR mutex_name = TEXT("MySharedMutex");
    
    // Handles para recursos do Windows
    HANDLE hMapFile = NULL;
    SharedData* pBuf = NULL;
    HANDLE hMutex = NULL;
    HANDLE hProcess = NULL;
    HANDLE hThread = NULL;
    bool mutex_acquired = false;

    // Sistema de cleanup automático (RAII pattern)
    auto cleanup = [&]() {
        if (mutex_acquired && hMutex) ReleaseMutex(hMutex);
        if (hMutex) CloseHandle(hMutex);
        if (pBuf) UnmapViewOfFile(pBuf);
        if (hMapFile) CloseHandle(hMapFile);
        if (hProcess) CloseHandle(hProcess);
        if (hThread) CloseHandle(hThread);
    };

    try {
        // --- PASSO 1: CRIAR MEMÓRIA COMPARTILHADA ---
        hMapFile = CreateFileMapping(
            INVALID_HANDLE_VALUE,   // Usar memória do sistema
            NULL,                   // Segurança padrão
            PAGE_READWRITE,         // Permissões de leitura/escrita
            0,                      // Tamanho alto (0)
            sizeof(SharedData),     // Tamanho baixo (tamanho da struct)
            shm_name                // Nome único
        );
        
        if (hMapFile == NULL) throw std::runtime_error("CreateFileMapping falhou");

        // Aviso se memória já existia (pode ser reutilização normal)
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            std::cout << "Aviso: Memória compartilhada já existia" << std::endl;
        }

        // --- PASSO 2: MAPEAR A MEMÓRIA NO ESPAÇO DE ENDEREÇAMENTO ---
        pBuf = (SharedData*)MapViewOfFile(
            hMapFile,           // Handle do file mapping
            FILE_MAP_ALL_ACCESS,// Acesso completo
            0,                  // Offset alto
            0,                  // Offset baixo
            sizeof(SharedData)  // Número de bytes para mapear
        );
        
        if (pBuf == NULL) throw std::runtime_error("MapViewOfFile falhou");

        // --- PASSO 3: CRIAR MUTEX PARA SINCRONIZAÇÃO ---
        hMutex = CreateMutex(NULL, FALSE, mutex_name);
        if (hMutex == NULL) throw std::runtime_error("CreateMutex falhou");

        // --- PASSO 4: OBTER MENSAGEM DO USUÁRIO ---
        std::string mensagem;
        std::cout << "Digite a mensagem para enviar via Memoria Compartilhada:" << std::endl;
        std::getline(std::cin, mensagem);

        // Validações de entrada
        if (mensagem.empty()) throw std::runtime_error("Mensagem vazia");
        if (mensagem.size() >= sizeof(pBuf->message)) {
            throw std::runtime_error("Mensagem muito grande! Máximo: " + 
                                   std::to_string(sizeof(pBuf->message) - 1) + " caracteres");
        }

        // --- PASSO 5: INICIAR PROCESSO LEITOR ---
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        std::string cmdLine = "processo_leitor.exe";

        if (!CreateProcessA(NULL, &cmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            throw std::runtime_error("CreateProcess falhou");
        }
        
        hProcess = pi.hProcess;
        hThread = pi.hThread;

        // --- PASSO 6: ESCREVER NA MEMÓRIA COMPARTILHADA ---
        std::cout << "Aguardando mutex para escrever (timeout: 10s)..." << std::endl;
        
        DWORD waitResult = WaitForSingleObject(hMutex, 10000);
        if (waitResult == WAIT_TIMEOUT) throw std::runtime_error("Timeout: Mutex ocupado");
        if (waitResult != WAIT_OBJECT_0) throw std::runtime_error("Falha ao adquirir mutex");
        
        mutex_acquired = true;

        // REGIÃO CRÍTICA - Escrita protegida por mutex
        try {
            ZeroMemory(pBuf->message, sizeof(pBuf->message)); // Limpa buffer
            
            CopyMemory((PVOID)pBuf->message, mensagem.c_str(), mensagem.size() + 1);
            
            // Verificação de integridade após escrita
            if (strncmp(pBuf->message, mensagem.c_str(), mensagem.size()) != 0) {
                throw std::runtime_error("Dados corrompidos na escrita");
            }
            
            std::cout << "Mensagem escrita e verificada: " << pBuf->message << std::endl;
        }
        catch (...) {
            if (mutex_acquired) ReleaseMutex(hMutex);
            mutex_acquired = false;
            throw;
        }
        
        ReleaseMutex(hMutex);
        mutex_acquired = false;

        // --- PASSO 7: AGUARDAR PROCESSO LEITOR TERMINAR ---
        std::cout << "Aguardando processo leitor (timeout: 15s)..." << std::endl;
        
        waitResult = WaitForSingleObject(pi.hProcess, 15000);
        if (waitResult == WAIT_TIMEOUT) throw std::runtime_error("Timeout: Leitor não terminou");
        if (waitResult != WAIT_OBJECT_0) throw std::runtime_error("Erro ao aguardar leitor");

        // Verificar se leitor terminou com sucesso
        DWORD exitCode;
        if (!GetExitCodeProcess(pi.hProcess, &exitCode)) throw std::runtime_error("Falha ao obter exit code");
        if (exitCode != 0) throw std::runtime_error("Leitor falhou com código: " + std::to_string(exitCode));

        std::cout << "Processo leitor terminou com sucesso." << std::endl;

        // --- PASSO 8: LOG DE SUCESSO ---
        escrever_log(mensagem, mensagem, "sucesso");
        
        cleanup();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "ERRO: " << e.what() << std::endl;
        escrever_log("", "", "erro");
        cleanup();
        return 1;
    }
}