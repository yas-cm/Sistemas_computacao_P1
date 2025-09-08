#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <tchar.h> // Adicionado para suporte a TCHAR

// Estrutura que define o layout dos dados na memória compartilhada.
// Deve ser idêntica em ambos os processos.
struct SharedData {
    char message[256];
};

// Função utilitária para gerar um arquivo de log em formato JSON,
// permitindo que a interface gráfica (frontend) monitore o resultado.
void escrever_log(const std::string& mensagem_enviada, const std::string& mensagem_recebida, const std::string& status) {
    std::ofstream log("log.json", std::ios::trunc); // std::ios::trunc apaga o conteúdo anterior do arquivo.
    log << "{\n"
        << "    \"mensagem_enviada\": \"" << mensagem_enviada << "\",\n"
        << "    \"mensagem_recebida\": \"" << mensagem_recebida << "\",\n"
        << "    \"status\": \"" << status << "\"\n"
        << "}" << std::endl;
    log.close();
}

int main() {
    // --- PASSO 1: DEFINIR NOMES DOS OBJETOS ---
    // Define nomes únicos para os objetos de memória e de sincronização.
    // O processo leitor usará esses mesmos nomes para se conectar a eles.
    LPCTSTR shm_name = TEXT("MySharedMemory");
    LPCTSTR mutex_name = TEXT("MySharedMutex");

    HANDLE hMapFile;
    SharedData* pBuf;

    // --- PASSO 2: CRIAR A MEMÓRIA COMPARTILHADA ---
    // CreateFileMapping cria um "mapeamento de arquivo" na memória, que serve como nosso espaço compartilhado.
    // INVALID_HANDLE_VALUE indica que a memória é volátil (não associada a um arquivo em disco).
    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // Usar o arquivo de paginação do sistema
        NULL,                    // Segurança padrão
        PAGE_READWRITE,          // Acesso de leitura/escrita
        0,                       // Tamanho máximo (high-order DWORD)
        sizeof(SharedData),      // Tamanho do nosso bloco de dados
        shm_name);               // Nome do objeto de mapeamento

    if (hMapFile == NULL) {
        escrever_log("", "", "erro");
        std::cerr << "Erro ao criar o file mapping: " << GetLastError() << std::endl;
        return 1;
    }

    // --- PASSO 3: MAPEAR A MEMÓRIA PARA O PROCESSO ---
    // MapViewOfFile torna a memória compartilhada acessível dentro do espaço de endereçamento deste processo.
    // A partir daqui, `pBuf` é um ponteiro para a área compartilhada.
    pBuf = (SharedData*)MapViewOfFile(
        hMapFile,                // Handle para o mapeamento
        FILE_MAP_ALL_ACCESS,     // Permissão total de leitura/escrita
        0, 0, sizeof(SharedData));

    if (pBuf == NULL) {
        escrever_log("", "", "erro");
        std::cerr << "Erro ao mapear a view do arquivo: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }

    // --- PASSO 4: CRIAR O MUTEX PARA SINCRONIZAÇÃO ---
    // CreateMutex cria um objeto de exclusão mútua (mutex).
    // Ele funcionará como um "token": apenas o processo que possuir o token poderá escrever ou ler.
    HANDLE hMutex = CreateMutex(NULL, FALSE, mutex_name);
    if (hMutex == NULL) {
        escrever_log("", "", "erro");
        std::cerr << "Erro ao criar o mutex: " << GetLastError() << std::endl;
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        return 1;
    }

    // --- PASSO 5: OBTER DADOS E INICIAR O PROCESSO FILHO ---
    std::string mensagem;
    std::cout << "Digite a mensagem para enviar via Memoria Compartilhada:" << std::endl;
    std::getline(std::cin, mensagem);

    // CreateProcessA inicia um novo processo (nosso leitor).
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::string cmdLine = "processo_leitor.exe";

    if (!CreateProcessA(NULL, &cmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        escrever_log(mensagem, "", "erro");
        std::cerr << "Erro ao criar processo filho." << std::endl;
        CloseHandle(hMutex); // Limpeza de recursos em caso de falha
        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        return 1;
    }

    // --- PASSO 6: ESCREVER NA MEMÓRIA (SEÇÃO CRÍTICA) ---
    std::cout << "Aguardando para escrever na memoria..." << std::endl;
    // WaitForSingleObject aguarda até que o mutex esteja livre e o trava.
    WaitForSingleObject(hMutex, INFINITE); 
    
    // -- INÍCIO DA SEÇÃO CRÍTICA --
    // Apenas este processo pode executar este trecho de código por vez.
    CopyMemory((PVOID)pBuf->message, mensagem.c_str(), (mensagem.size() + 1));
    // -- FIM DA SEÇÃO CRÍTICA --
    
    // ReleaseMutex libera o mutex, permitindo que outros processos o utilizem.
    ReleaseMutex(hMutex); 
    std::cout << "Mensagem escrita na memoria." << std::endl;

    // --- PASSO 7: AGUARDAR E FINALIZAR ---
    // Aguarda o processo leitor terminar sua execução antes de continuar.
    WaitForSingleObject(pi.hProcess, INFINITE);
    std::cout << "Processo leitor terminou." << std::endl;

    // Escreve o log final de sucesso.
    escrever_log(mensagem, mensagem, "sucesso");

    // --- PASSO 8: LIMPEZA DE RECURSOS ---
    // Fecha todos os handles e libera a memória mapeada para evitar vazamentos de recursos.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hMutex);
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);

    return 0;
}