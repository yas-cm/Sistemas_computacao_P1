#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>

// Função para escrever log em formato JSON
void escrever_log(const std::string& mensagem_enviada,
                 const std::string& mensagem_recebida,
                 const std::string& status) {
    std::ofstream log("log.json", std::ios::trunc);
    log << "{\n"
        << "    \"mensagem_enviada\": \"" << mensagem_enviada << "\",\n"
        << "    \"mensagem_recebida\": \"" << mensagem_recebida << "\",\n"
        << "    \"status\": \"" << status << "\"\n"
        << "}" << std::endl;
    log.close();
}

int main(int argc, char* argv[]) {
    // Verifica se recebeu o handle do pipe como argumento
    if (argc < 2) {
        escrever_log("", "", "erro");
        std::cout << "Handle do pipe não fornecido." << std::endl;
        return 1;
    }

    // Converte argumento string para handle
    HANDLE hRead = (HANDLE)(uintptr_t)std::stoull(argv[1]);
    char buffer[1024] = {0};
    DWORD read = 0;

    // Lê mensagem do pipe
    if (!ReadFile(hRead, buffer, sizeof(buffer) - 1, &read, NULL)) {
        escrever_log("", "", "erro");
        std::cout << "Falha ao ler do pipe." << std::endl;
        CloseHandle(hRead);
        return 1;
    }
    
    // Verifica se leu algum dado
    if (read == 0) {
        escrever_log("", "", "erro_vazio");
        std::cout << "Nenhum dado recebido do pipe." << std::endl;
        CloseHandle(hRead);
        return 1;
    }
    
    buffer[read] = '\0'; // Adiciona terminador nulo

    std::string mensagem_recebida(buffer);
    
    // Verificação adicional: mensagem não pode estar vazia
    if (mensagem_recebida.empty()) {
        escrever_log("", "", "erro_vazio");
        std::cout << "Mensagem recebida está vazia." << std::endl;
        CloseHandle(hRead);
        return 1;
    }

    // Escreve log com mensagem recebida (usa mesma mensagem para enviada/recebida pois é one-way)
    escrever_log(mensagem_recebida, mensagem_recebida, "sucesso");

    std::cout << "Mensagem recebida: " << mensagem_recebida << std::endl;
    std::cout << "Tamanho: " << mensagem_recebida.size() << " bytes" << std::endl;

    CloseHandle(hRead);
    return 0; // Retorna 0 indicando sucesso
}