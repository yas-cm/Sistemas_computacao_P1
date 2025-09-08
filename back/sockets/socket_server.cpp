#include <winsock2.h>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

#pragma comment(lib, "ws2_32.lib")

std::atomic<bool> server_running(true);

// Thread Pool para gerenciar conexões
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

public:
    ThreadPool(size_t threads) : stop(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });
                        if (this->stop && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    template<class F>
    void enqueue(F&& task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) return;
            tasks.emplace(std::forward<F>(task));
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
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

void handle_client(SOCKET client_socket) {
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        std::string mensagem_recebida(buffer);
        
        // Envia a mesma mensagem de volta (echo)
        send(client_socket, mensagem_recebida.c_str(), mensagem_recebida.size(), 0);
        
        escrever_log(mensagem_recebida, mensagem_recebida, "sucesso");
        std::cout << "Mensagem recebida: " << mensagem_recebida << std::endl;
    }
    
    closesocket(client_socket);
}

int main() {
    // Inicializar thread pool com 4 threads
    ThreadPool pool(4);
    std::cout << "Thread Pool inicializado com 4 threads" << std::endl;

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
        std::cerr << "Falha ao bind socket. Porta 8888 pode estar em uso." << std::endl;
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
    std::cout << "Usando Thread Pool para gerenciar conexões" << std::endl;
    escrever_log("", "", "servidor_iniciado");

    // Configurar timeout para aceitar conexões
    fd_set readfds;
    timeval timeout;

    while (server_running) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(0, &readfds, NULL, NULL, &timeout);
        
        if (activity == SOCKET_ERROR) {
            std::cerr << "Erro no select." << std::endl;
            break;
        } else if (activity > 0 && FD_ISSET(server_socket, &readfds)) {
            sockaddr_in client_addr;
            int client_size = sizeof(client_addr);
            SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_size);
            
            if (client_socket != INVALID_SOCKET) {
                // Usar thread pool em vez de criar thread para cada conexão
                pool.enqueue([client_socket] {
                    handle_client(client_socket);
                });
                std::cout << "Conexao aceita e enfileirada no thread pool" << std::endl;
            }
        }
    }

    std::cout << "Finalizando servidor socket..." << std::endl;
    
    // O destrutor do ThreadPool fará o join das threads automaticamente
    closesocket(server_socket);
    WSACleanup();
    
    std::cout << "Servidor finalizado com sucesso." << std::endl;
    return 0;
}