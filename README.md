# Sistema Integrado de Comunicação entre Processos (IPC)

## 📋 Descrição do Projeto

Este projeto implementa um sistema integrado de Comunicação entre Processos (IPC) que demonstra três mecanismos distintos de comunicação: **Pipes Anônimos**, **Sockets Locais** e **Memória Compartilhada**. O sistema consiste em um backend desenvolvido em C++ que implementa a lógica de IPC e um frontend web em HTML/JavaScript que fornece uma interface gráfica para interação com os mecanismos.

## 🏗️ Arquitetura do Sistema

### Estrutura de Diretórios

```bash
sistemas_computacao_p1/
├── back/                           # Backend em C++
│   ├── pipes/                      # Pipes Anônimos
│   │   ├── processo1.cpp           # Processo escritor
│   │   └── processo2.cpp           # Processo leitor
│   ├── sockets/                    # Sockets Locais
│   │   ├── socket_server.cpp       # Servidor socket
│   │   └── socket_client.cpp       # Cliente socket
│   └── shared_memory/              # Memória Compartilhada
│       ├── processo_escritor.cpp   # Processo escritor
│       └── processo_leitor.cpp     # Processo leitor
├── front/                          # Frontend
│   ├── html/
│   │   └── index.html              # Interface principal
│   └── style/
│       └── style.css               # Estilos da interface
├── server.js                       # Servidor Node.js
├── start_tudo.bat                  # Script de inicialização (Windows)
└── README.md
```

## 🔧 Mecanismos de IPC Implementados

### 1. Pipes Anônimos

-   **Processo1**: cria o pipe, inicia o Processo2 e escreve a mensagem.
-   **Processo2**: lê a mensagem e gera o log.
-   **Sincronização**: uso de `WaitForSingleObject`.

### 2. Sockets Locais

-   **Servidor**: multithreaded na porta 8888.
-   **Cliente**: envia mensagens e recebe de volta (echo).
-   **Protocolo**: TCP.

### 3. Memória Compartilhada

-   **Escritor**: cria a memória e controla acesso via mutex.
-   **Leitor**: acessa e lê a memória compartilhada.
-   **Sincronização**: mutex para evitar conflitos.

---

## 🚀 Como Compilar e Executar

### Pré-requisitos

-   Compilador C++ (g++ ou MinGW no Windows)
-   Node.js
-   Navegador web moderno

### Compilação

#### Pipes

```bash
g++ back/pipes/processo1.cpp -o back/pipes/processo1.exe
g++ back/pipes/processo2.cpp -o back/pipes/processo2.exe
```

#### Sockets

```bash
g++ back/sockets/socket_server.cpp -o back/sockets/socket_server.exe -lws2_32
g++ back/sockets/socket_client.cpp -o back/sockets/socket_client.exe -lws2_32
```

#### Memoria Compartilhada

```bash
g++ back/shared_memory/processo_escritor.cpp -o back/shared_memory/processo_escritor.exe
g++ back/shared_memory/processo_leitor.cpp -o back/shared_memory/processo_leitor.exe
```

#### Ou use

```bash
start_tudo.bat
```
