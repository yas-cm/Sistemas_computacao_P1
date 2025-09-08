#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <tchar.h> // Adicionado para suporte a TCHAR

// A mesma estrutura de dados do escritor para interpretar a memória corretamente.
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
    // --- PASSO 1: DEFINIR NOMES DOS OBJETOS ---
    // Utiliza os mesmos nomes definidos pelo processo escritor para encontrar os objetos.
    LPCTSTR shm_name = TEXT("MySharedMemory");
    LPCTSTR mutex_name = TEXT("MySharedMutex");

    HANDLE hMapFile;
    SharedData* pBuf;

    // --- PASSO 2: ABRIR A MEMÓRIA COMPARTILHADA EXISTENTE ---
    // OpenFileMapping tenta abrir um mapeamento de arquivo nomeado que já foi criado por outro processo.
    hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,   // Solicita acesso total de leitura/escrita
        FALSE,                 // Não herdar o nome
        shm_name);             // Nome do objeto a ser aberto

    if (hMapFile == NULL) {
        escrever_log("", "", "erro");
        std::cerr << "[Leitor] Erro ao abrir o file mapping: " << GetLastError() << std::endl;
        return 1;
    }

    // --- PASSO 3: MAPEAR A MEMÓRIA PARA O PROCESSO ---
    // Assim como no escritor, MapViewOfFile torna a memória acessível através do ponteiro `pBuf`.
    pBuf = (SharedData*)MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0, sizeof(SharedData));

    if (pBuf == NULL) {
        escrever_log("", "", "erro");
        std::cerr << "[Leitor] Erro ao mapear a view do arquivo: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }

    // --- PASSO 4: ABRIR O MUTEX EXISTENTE ---
    // OpenMutex abre um handle para o mutex que já foi criado pelo escritor.
    HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutex_name);
    if (hMutex == NULL) {
        escrever_log("", "", "erro");
        std::cerr << "[Leitor] Erro ao abrir o mutex: " << GetLastError() << std::endl;
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        return 1;
    }

    // --- PASSO 5: LER DA MEMÓRIA (SEÇÃO CRÍTICA) ---
    std::cout << "[Leitor] Aguardando para ler da memoria..." << std::endl;
    // Trava o mutex para garantir que o escritor não modifique os dados durante a leitura.
    WaitForSingleObject(hMutex, INFINITE); 

    // -- INÍCIO DA SEÇÃO CRÍTICA --
    std::string mensagem_recebida = pBuf->message;
    // -- FIM DA SEÇÃO CRÍTICA --
    
    // Libera o mutex para outros processos.
    ReleaseMutex(hMutex); 

    std::cout << "[Leitor] Mensagem recebida: " << mensagem_recebida << std::endl;

    // Escreve o log
    escrever_log(mensagem_recebida, mensagem_recebida, "sucesso");

    // --- PASSO 6: LIMPEZA DE RECURSOS ---
    // Fecha os handles e libera a memória mapeada.
    CloseHandle(hMutex);
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);

    return 0;
}