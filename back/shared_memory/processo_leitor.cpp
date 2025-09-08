#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <tchar.h>
#include <stdexcept>
#include <cctype>

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
    };

    try {
        // --- PASSO 1: ABRIR MEMÓRIA COMPARTILHADA ---
        hMapFile = OpenFileMapping(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            shm_name
        );
        
        if (hMapFile == NULL) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND) {
                throw std::runtime_error("Memória compartilhada não encontrada. Execute o escritor primeiro.");
            }
            throw std::runtime_error("OpenFileMapping falhou: " + std::to_string(error));
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

        // --- PASSO 3: ABRIR MUTEX ---
        hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutex_name);
        if (hMutex == NULL) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND) {
                throw std::runtime_error("Mutex não encontrado. Execute o escritor primeiro.");
            }
            throw std::runtime_error("OpenMutex falhou: " + std::to_string(error));
        }

        // --- PASSO 4: LER DA MEMÓRIA ---
        std::cout << "[Leitor] Aguardando para ler da memoria (timeout: 10s)..." << std::endl;
        
        DWORD waitResult = WaitForSingleObject(hMutex, 10000);
        if (waitResult == WAIT_TIMEOUT) {
            throw std::runtime_error("Timeout: Não foi possível acessar a memória em 10 segundos");
        }
        if (waitResult != WAIT_OBJECT_0) {
            throw std::runtime_error("Falha ao adquirir mutex: " + std::to_string(GetLastError()));
        }
        mutex_acquired = true;

        std::string mensagem_recebida;
        
        // REGIÃO CRÍTICA - com proteção manual
        try {
            if (pBuf->message[0] == '\0') {
                throw std::runtime_error("Memória compartilhada está vazia");
            }
            
            bool valid_string = true;
            size_t length = 0;
            for (size_t i = 0; i < sizeof(pBuf->message); i++) {
                if (pBuf->message[i] == '\0') {
                    length = i;
                    break;
                }
                if (!std::isprint(static_cast<unsigned char>(pBuf->message[i])) && 
                    !std::isspace(static_cast<unsigned char>(pBuf->message[i]))) {
                    valid_string = false;
                    break;
                }
            }
            
            if (!valid_string || length == 0) {
                throw std::runtime_error("Dados na memória estão corrompidos ou inválidos");
            }
            
            mensagem_recebida = pBuf->message;
            std::cout << "[Leitor] Mensagem recebida: " << mensagem_recebida << std::endl;
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