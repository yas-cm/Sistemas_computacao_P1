#include <winsock2.h>
#include <iostream>
#include <string>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

void escrever_log(const std::string& mensagem_enviada, const std::string& mensagem_recebida, const std::string& status) {
    std::ofstream log("log.json", std::ios::trunc);
    log << "{\n"
        << "    \"mensagem_enviada\": \"" << mensagem_enviada << "\",\n"
        << "    \"mensagem_recebida\": \"" << mensagem_recebida << "\",\n"
        << "    \"status\": \"" << status << "\"\n"
        << "}" << std::endl;
    log.close();
}

int main(int argc, char* argv[]) {
    // Verifica argumentos da linha de comando
    if (argc < 2) {
        escrever_log("", "", "erro");
        std::cout << "Uso: socket_client.exe <mensagem>" << std::endl;
        return 1;
    }

    std::string mensagem = argv[1];

    // Inicialização do Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        escrever_log(mensagem, "", "erro");
        std::cerr << "Falha ao inicializar Winsock." << std::endl;
        return 1;
    }

    // Cria socket TCP
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        escrever_log(mensagem, "", "erro");
        std::cerr << "Falha ao criar socket." << std::endl;
        WSACleanup();
        return 1;
    }

    // Configura endereço do servidor (localhost:8888)
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8888);

    // Conexão com o servidor
    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        escrever_log(mensagem, "", "erro");
        std::cerr << "Falha ao conectar ao servidor." << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    // Envio da mensagem
    if (send(client_socket, mensagem.c_str(), mensagem.size(), 0) == SOCKET_ERROR) {
        escrever_log(mensagem, "", "erro");
        std::cerr << "Falha ao enviar mensagem." << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    // Recebimento da resposta (servidor echo)
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::string resposta(buffer);
        escrever_log(mensagem, resposta, "sucesso");
        std::cout << "Resposta do servidor: " << resposta << std::endl;
    } else {
        escrever_log(mensagem, "", "erro");
        std::cerr << "Falha ao receber resposta." << std::endl;
    }

    // Limpeza de recursos
    closesocket(client_socket);
    WSACleanup();
    return 0;
}