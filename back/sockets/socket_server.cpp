#include <winsock2.h>
#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <atomic>
#include <csignal>

#pragma comment(lib, "ws2_32.lib")

std::atomic<bool> server_running(true);

void escrever_log(const std::string& mensagem_enviada, const std::string& mensagem_recebida, const std::string& status) {
    std::ofstream log("log.json", std::ios::trunc);
    log << "{\n"
        << "    \"mensagem_enviada\": \"" << mensagem_enviada << "\",\n"
        << "    \"mensagem_recebida\": \"" << mensagem_recebida << "\",\n"
        << "    \"status\": \"" << status << "\"\n"
        << "}" << std::endl;
    log.close();
}

void signal_handler(int signal) {
    server_running = false;
    std::cout << "Recebido sinal de desligamento. Finalizando servidor..." << std::endl;
}

void handle_client(SOCKET client_socket) {
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::string mensagem_recebida(buffer);
        
        // Envia a mesma mensagem de volta (echo) - sem "ACK: "
        send(client_socket, mensagem_recebida.c_str(), mensagem_recebida.size(), 0);
        
        escrever_log(mensagem_recebida, mensagem_recebida, "sucesso");
        std::cout << "Mensagem recebida e retornada: " << mensagem_recebida << std::endl;
    } else {
        std::cout << "Conexão fechada pelo cliente ou erro na recepção." << std::endl;
    }
    
    closesocket(client_socket);
}

int main() {
    // Configurar handler para Ctrl+C
    std::signal(SIGINT, signal_handler);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        escrever_log("", "", "erro");
        std::cerr << "Falha ao inicializar Winsock." << std::endl;
        return 1;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        escrever_log("", "", "erro");
        std::cerr << "Falha ao criar socket." << std::endl;
        WSACleanup();
        return 1;
    }

    // Configurar socket para reutilizar endereço
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        std::cerr << "Erro ao configurar socket options." << std::endl;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        escrever_log("", "", "erro");
        std::cerr << "Falha ao bind socket. Porta 8888可能 estar em uso." << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        escrever_log("", "", "erro");
        std::cerr << "Falha ao escutar no socket." << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Servidor socket iniciado na porta 8888..." << std::endl;
    std::cout << "Pressione Ctrl+C para finalizar o servidor." << std::endl;
    escrever_log("", "", "servidor_iniciado");

    while (server_running) {
        // Configurar timeout para aceitar conexões (para poder verificar server_running)
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        
        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(0, &readfds, NULL, NULL, &timeout);
        
        if (activity == SOCKET_ERROR) {
            std::cerr << "Erro no select." << std::endl;
            break;
        } else if (activity > 0 && FD_ISSET(server_socket, &readfds)) {
            // Aceitar conexão
            sockaddr_in client_addr;
            int client_size = sizeof(client_addr);
            SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_size);
            
            if (client_socket != INVALID_SOCKET) {
                std::cout << "Cliente conectado: " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;
                std::thread(handle_client, client_socket).detach();
            } else {
                std::cerr << "Falha ao aceitar conexão." << std::endl;
            }
        }
        // Se activity == 0, timeout ocorreu - verificar server_running novamente
    }

    std::cout << "Finalizando servidor socket..." << std::endl;
    
    // Limpar recursos
    closesocket(server_socket);
    WSACleanup();
    
    std::cout << "Servidor finalizado com sucesso." << std::endl;
    return 0;
}