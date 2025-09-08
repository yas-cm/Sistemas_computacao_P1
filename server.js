const express = require("express");
const { spawn, exec } = require("child_process");
const fs = require("fs");
const path = require("path");

const app = express();
app.use(express.json());

// Caminhos para os executáveis de cada tipo de comunicação
const commPaths = {
    pipes: path.join(__dirname, "back", "pipes", "processo1.exe"),
    socket: path.join(__dirname, "back", "sockets", "socket_client.exe"),
    memcomp: path.join(__dirname, "back", "shared_memory", "processA.exe"),
};

const commDirs = {
    pipes: path.join(__dirname, "back", "pipes"),
    socket: path.join(__dirname, "back", "sockets"),
    memcomp: path.join(__dirname, "back", "shared_memory"),
};

// Variáveis para controlar processos ativos
let activeProcesses = {
    pipes: null,
    socket: null,
    memcomp: null,
};

let socketServerProcess = null;

// Função para finalizar processos ativos
function cleanupProcesses() {
    Object.keys(activeProcesses).forEach((tipo) => {
        if (activeProcesses[tipo]) {
            activeProcesses[tipo].kill();
            activeProcesses[tipo] = null;
            console.log(`Processo ${tipo} finalizado`);
        }
    });
}

// Função para finalizar servidor socket
function stopSocketServer() {
    if (socketServerProcess) {
        // Usar taskkill para finalizar silenciosamente
        exec(`taskkill /f /im socket_server.exe /t`, (error) => {
            if (error) {
                console.log("Servidor socket já finalizado ou não encontrado");
            } else {
                console.log("Servidor socket finalizado");
            }
            socketServerProcess = null;
        });
    }
}

// Endpoint para enviar mensagem
app.post("/enviar", (req, res) => {
    const mensagem = req.body.mensagem || "";
    const tipo = req.body.tipo || "pipes";

    // Verifica se o tipo é suportado
    if (!commPaths[tipo] || !commDirs[tipo]) {
        return res.json({
            status: "erro",
            mensagem: "Tipo de comunicação não suportado",
            mensagem_enviada: mensagem,
            mensagem_recebida: "",
        });
    }

    // Finaliza processos anteriores antes de iniciar novo
    cleanupProcesses();

    // Processamento para Pipes
    if (tipo === "pipes") {
        const processo = spawn(commPaths[tipo], [], {
            cwd: commDirs[tipo],
            windowsHide: true, // Não mostrar janela do CMD
            stdio: ["pipe", "ignore", "ignore"], // Redirecionar stdin, ignorar stdout/stderr
        });

        activeProcesses.pipes = processo;

        // Envia a mensagem para o processo pelo stdin
        processo.stdin.write(mensagem + "\n");
        processo.stdin.end();

        // Aguarda o processo terminar e lê o log.json
        processo.on("close", () => {
            activeProcesses.pipes = null;
            const logPath = path.join(commDirs[tipo], "log.json");
            fs.readFile(logPath, "utf8", (err, data) => {
                if (err) {
                    return res.json({
                        status: "erro",
                        mensagem: "Erro ao ler arquivo de log",
                        mensagem_enviada: mensagem,
                        mensagem_recebida: "",
                    });
                }
                try {
                    const log = JSON.parse(data);
                    res.json({
                        status: log.status,
                        mensagem:
                            log.mensagem_recebida || log.mensagem_enviada || "",
                        mensagem_enviada: log.mensagem_enviada || mensagem,
                        mensagem_recebida: log.mensagem_recebida || "",
                    });
                } catch (parseError) {
                    res.json({
                        status: "erro",
                        mensagem: "Erro ao processar log",
                        mensagem_enviada: mensagem,
                        mensagem_recebida: "",
                    });
                }
            });
        });
    }
    // Processamento para Sockets
    else if (tipo === "socket") {
        const processo = spawn(commPaths[tipo], [mensagem], {
            cwd: commDirs[tipo],
            windowsHide: true, // Não mostrar janela do CMD
            stdio: ["ignore", "ignore", "ignore"], // Ignorar todo o stdio
        });

        activeProcesses.socket = processo;

        // Aguarda o processo terminar e lê o log.json
        processo.on("close", (code) => {
            activeProcesses.socket = null;
            const logPath = path.join(commDirs[tipo], "log.json");
            fs.readFile(logPath, "utf8", (err, data) => {
                if (err) {
                    return res.json({
                        status: "erro",
                        mensagem: "Erro ao ler arquivo de log",
                        mensagem_enviada: mensagem,
                        mensagem_recebida: "",
                    });
                }
                try {
                    const log = JSON.parse(data);
                    res.json({
                        status: log.status,
                        mensagem:
                            log.mensagem_recebida || log.mensagem_enviada || "",
                        mensagem_enviada: log.mensagem_enviada || mensagem,
                        mensagem_recebida: log.mensagem_recebida || "",
                    });
                } catch (parseError) {
                    res.json({
                        status: "erro",
                        mensagem: "Erro ao processar log",
                        mensagem_enviada: mensagem,
                        mensagem_recebida: "",
                    });
                }
            });
        });

        processo.on("error", (err) => {
            activeProcesses.socket = null;
            res.json({
                status: "erro",
                mensagem: "Erro ao executar processo socket",
                mensagem_enviada: mensagem,
                mensagem_recebida: "",
            });
        });
    }
    // Processamento para Memória Compartilhada (em desenvolvimento)
    else if (tipo === "memcomp") {
        return res.json({
            status: "erro",
            mensagem: "Tipo de comunicação em desenvolvimento",
            mensagem_enviada: mensagem,
            mensagem_recebida: "",
        });
    }
    // Tipo não reconhecido
    else {
        return res.json({
            status: "erro",
            mensagem: "Tipo de comunicação não implementado",
            mensagem_enviada: mensagem,
            mensagem_recebida: "",
        });
    }
});

// Endpoint para limpar/parar processos
app.post("/limpar", (req, res) => {
    const tipo = req.body.tipo;

    if (tipo && activeProcesses[tipo]) {
        activeProcesses[tipo].kill();
        activeProcesses[tipo] = null;
        res.json({
            status: "sucesso",
            mensagem: `Processo ${tipo} finalizado`,
        });
    } else if (!tipo) {
        // Limpa todos os processos
        cleanupProcesses();
        res.json({
            status: "sucesso",
            mensagem: "Todos os processos finalizados",
        });
    } else {
        res.json({
            status: "erro",
            mensagem: `Nenhum processo ativo para ${tipo}`,
        });
    }
});

// Servir arquivos estáticos do front-end
app.use(express.static(path.join(__dirname, "front")));

// Rota para servir o index.html na raiz
app.get("/", (req, res) => {
    res.sendFile(path.join(__dirname, "front", "html", "index.html"));
});

// Rota para servir o index.html explicitamente
app.get("/index.html", (req, res) => {
    res.sendFile(path.join(__dirname, "front", "html", "index.html"));
});

// Inicializar servidor socket em segundo plano sem janela
function startSocketServer() {
    try {
        const socketServerPath = path.join(
            __dirname,
            "back",
            "sockets",
            "socket_server.exe"
        );
        const socketServerDir = path.join(__dirname, "back", "sockets");

        if (fs.existsSync(socketServerPath)) {
            console.log(
                "Iniciando servidor socket em segundo plano (sem janela)..."
            );
            socketServerProcess = spawn(socketServerPath, [], {
                cwd: socketServerDir,
                detached: true,
                windowsHide: true, // Não mostrar janela
                stdio: "ignore", // Ignorar toda a saída
            });
            socketServerProcess.unref();
            console.log(
                "Servidor socket iniciado silenciosamente na porta 8888"
            );
        } else {
            console.log(
                "Servidor socket não encontrado. Certifique-se de compilar os arquivos C++ primeiro."
            );
        }
    } catch (error) {
        console.log("Erro ao iniciar servidor socket:", error.message);
    }
}

// Iniciar servidor socket
startSocketServer();

// Graceful shutdown
process.on("SIGINT", () => {
    console.log("\nFinalizando processos...");
    cleanupProcesses();
    stopSocketServer();
    process.exit(0);
});

app.listen(3000, () => {
    console.log("Servidor Node.js rodando em http://localhost:3000");
    console.log("Todos os processos serão executados sem abrir janelas do CMD");
});
