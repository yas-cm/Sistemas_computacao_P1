#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>

void escrever_log(const std::string& mensagem, const std::string& status) {
    std::ofstream log("log.json", std::ios::trunc);
    log << "{\n"
        << "    \"tipo_comunicacao\": \"pipes\",\n"
        << "    \"mensagem\": \"" << mensagem << "\",\n"
        << "    \"status\": \"" << status << "\"\n"
        << "}" << std::endl;
    log.close();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        escrever_log("", "erro");
        std::cout << "Handle do pipe não fornecido." << std::endl;
        return 1;
    }

    HANDLE hRead = (HANDLE)(uintptr_t)std::stoull(argv[1]);
    char buffer[1024] = {0};
    DWORD read = 0;

    if (!ReadFile(hRead, buffer, sizeof(buffer) - 1, &read, NULL)) {
        escrever_log("", "erro");
        std::cout << "Falha ao ler do pipe." << std::endl;
        CloseHandle(hRead);
        return 1;
    }
    buffer[read] = '\0';

    escrever_log(buffer, "sucesso");

    std::cout << "Mensagem recebida: " << buffer << std::endl;

    CloseHandle(hRead);
    system("pause");
    return 0;
}


