#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
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

int main() {
    HANDLE hRead, hWrite;
    // sa - permite herança do handle para processo filho
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    // Cria pipe anônimo com herança de handle
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        escrever_log("", "", "erro");
        std::cout << "Erro ao criar pipe." << std::endl;
        return 1;
    }

    // Obtém mensagem do usuário
    std::string mensagem;
    std::cout << "Digite a mensagem para enviar ao processo 2:" << std::endl;
    std::getline(std::cin, mensagem);

    // Converte handle para string para passar como argumento
    std::ostringstream oss;
    oss << (uintptr_t)hRead;
    std::string cmdLine = "processo2.exe " + oss.str();

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // Cria processo filho (processo2) passando o handle do pipe
    if (!CreateProcessA(
        NULL,               // Nome do módulo
        &cmdLine[0],        // Linha de comando
        NULL,               // Segurança do processo
        NULL,               // Segurança da thread
        TRUE,               // Herdar handles
        0,                  // Flags de criação
        NULL,               // Ambiente
        NULL,               // Diretório atual
        &si,                // Startup info
        &pi                 // Process info
    )) {
        escrever_log(mensagem, "", "erro");
        std::cout << "Erro ao criar processo filho." << std::endl;
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return 1;
    }

    DWORD written;
    // Escreve mensagem no pipe
    if (!WriteFile(hWrite, mensagem.c_str(), mensagem.size() + 1, &written, NULL)) {
        escrever_log(mensagem, "", "erro");
        std::cout << "Falha ao escrever no pipe." << std::endl;
        CloseHandle(hWrite);
        CloseHandle(hRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }

    // Aguarda processo2 terminar
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Obtém código de retorno do processo2
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    std::string mensagem_recebida = mensagem; // Assume sucesso inicialmente
    std::string status = "sucesso";

    // Se processo2 falhou ou retornou código de erro
    if (exitCode != 0) {
        mensagem_recebida = "";
        status = "erro";
    } else {
        // Lê o log gerado pelo processo2 para verificar o conteúdo recebido
        std::ifstream logFile("log.json");
        if (logFile.is_open()) {
            std::string line;
            std::string receivedContent;
            
            while (std::getline(logFile, line)) {
                // Procura pela mensagem recebida no JSON
                if (line.find("\"mensagem_recebida\":") != std::string::npos) {
                    size_t start = line.find('"', line.find(":") + 2) + 1;
                    size_t end = line.find('"', start);
                    if (start != std::string::npos && end != std::string::npos) {
                        receivedContent = line.substr(start, end - start);
                        break;
                    }
                }
            }
            logFile.close();

            // Verifica se a mensagem recebida é igual à enviada
            if (receivedContent != mensagem) {
                mensagem_recebida = receivedContent;
                status = "erro_verificacao";
                std::cout << "ERRO: Mensagem recebida difere da enviada!" << std::endl;
                std::cout << "Enviada: '" << mensagem << "'" << std::endl;
                std::cout << "Recebida: '" << receivedContent << "'" << std::endl;
            }
        }
    }

    // Escreve log final com verificação
    escrever_log(mensagem, mensagem_recebida, status);

    // Limpeza de handles
    CloseHandle(hWrite);
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (status == "sucesso") {
        std::cout << "Mensagem enviada e verificada com sucesso: " << mensagem << std::endl;
    } else {
        std::cout << "Falha na comunicação: " << status << std::endl;
    }

    return (status == "sucesso") ? 0 : 1;
}