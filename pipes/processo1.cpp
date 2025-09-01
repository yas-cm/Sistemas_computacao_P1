#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
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

int main() {
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        escrever_log("", "erro");
        std::cout << "Erro ao criar pipe." << std::endl;
        return 1;
    }

    std::string mensagem;
    std::cout << "Digite a mensagem para enviar ao processo 2:" << std::endl;
    std::getline(std::cin, mensagem);

    std::ostringstream oss;
    oss << (uintptr_t)hRead;
    std::string cmdLine = "processo2.exe " + oss.str();

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessA(
        NULL,
        &cmdLine[0],
        NULL,
        NULL,
        TRUE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        escrever_log(mensagem, "erro");
        std::cout << "Erro ao criar processo filho." << std::endl;
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return 1;
    }

    DWORD written;
    if (!WriteFile(hWrite, mensagem.c_str(), mensagem.size() + 1, &written, NULL)) {
        escrever_log(mensagem, "erro");
        std::cout << "Falha ao escrever no pipe." << std::endl;
        CloseHandle(hWrite);
        CloseHandle(hRead);
        return 1;
    }

    escrever_log(mensagem, "sucesso");

    CloseHandle(hWrite);
    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    std::cout << "Mensagem enviada: " << mensagem << std::endl;
    return 0;
}
