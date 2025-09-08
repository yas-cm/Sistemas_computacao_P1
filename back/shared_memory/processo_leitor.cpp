#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <tchar.h>
#include <stdexcept>
#include <cctype>

// Mesma estrutura do escritor - deve ser idêntica
struct SharedData {
    char message[256];
};

// Função de log idêntica ao escritor
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
    // Mesmos nomes do escritor
    LPCTSTR shm_name = TEXT("MySharedMemory");
    LPCTSTR mutex_name = TEXT("MySharedMutex");
    
    HANDLE hMapFile = NULL;
    SharedData* pBuf = NULL;
    HANDLE hMutex = NULL;
    bool mutex_acquired = false;

    // Sistema de cleanup automático
    auto cleanup = [&]() {
        if (mutex_acquired && hMutex) ReleaseMutex(hMutex);
        if (hMutex) CloseHandle(hMutex);
        if (pBuf) UnmapViewOfFile(pBuf);
        if (hMapFile) CloseHandle(hMapFile);
    };

    try {
        // --- PASSO 1: ABRIR MEMÓRIA COMPARTILHADA EXISTENTE ---
        hMapFile = OpenFileMapping(
            FILE_MAP_ALL_ACCESS,    // Acesso completo
            FALSE,                  // Não herdar
            shm_name                // Nome da memória
        );
        
        if (hMapFile == NULL) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND) {
                throw std::runtime_error("Execute o escritor primeiro");
            }
            throw std::runtime_error("OpenFileMapping falhou");
        }

        // --- PASSO 2: MAPEAR MEMÓRIA COMPARTILHADA ---
        pBuf = (SharedData*)MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            sizeof(SharedData)
        );
        
        if (pBuf == NULL) throw std::runtime_error("MapViewOfFile falhou");

        // --- PASSO 3: ABRIR MUTEX EXISTENTE ---
        hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutex_name);
        if (hMutex == NULL) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND) throw std::runtime_error("Mutex não encontrado");
            throw std::runtime_error("OpenMutex falhou");
        }

        // --- PASSO 4: LER DA MEMÓRIA COMPARTILHADA ---
        std::cout << "[Leitor] Aguardando mutex para ler (timeout: 10s)..." << std::endl;
        
        DWORD waitResult = WaitForSingleObject(hMutex, 10000);
        if (waitResult == WAIT_TIMEOUT) throw std::runtime_error("Timeout: Não conseguiu acessar");
        if (waitResult != WAIT_OBJECT_0) throw std::runtime_error("Falha ao adquirir mutex");
        
        mutex_acquired = true;

        std::string mensagem_recebida;
        
        // REGIÃO CRÍTICA - Leitura protegida por mutex
        try {
            // Verificar se há dados válidos
            if (pBuf->message[0] == '\0') throw std::runtime_error("Memória vazia");
            
            // Validação rigorosa dos dados
            bool valid_string = true;
            size_t length = 0;
            
            for (size_t i = 0; i < sizeof(pBuf->message); i++) {
                if (pBuf->message[i] == '\0') {
                    length = i;
                    break;
                }
                // Verificar se é caractere imprimível ou espaço
                if (!std::isprint(static_cast<unsigned char>(pBuf->message[i])) && 
                    !std::isspace(static_cast<unsigned char>(pBuf->message[i]))) {
                    valid_string = false;
                    break;
                }
            }
            
            if (!valid_string || length == 0) throw std::runtime_error("Dados corrompidos");
            
            mensagem_recebida = pBuf->message;
            std::cout << "[Leitor] Mensagem recebida: " << mensagem_recebida << std::endl;
        }
        catch (...) {
            if (mutex_acquired) ReleaseMutex(hMutex);
            mutex_acquired = false;
            throw;
        }
        
        ReleaseMutex(hMutex);
        mutex_acquired = false;

        // --- PASSO 5: LOG DE SUCESSO ---
        escrever_log(mensagem_recebida, mensagem_recebida, "sucesso");
        
        cleanup();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[Leitor] ERRO: " << e.what() << std::endl;
        escrever_log("", "", "erro");
        cleanup();
        return 1;
    }
}