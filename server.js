const express = require("express");
const { spawn, exec } = require("child_process");
const fs = require("fs");
const path = require("path");

const app = express();
app.use(express.json());

// Configuração dos caminhos dos executáveis C++ para cada tipo de IPC
const commPaths = {
    pipes: path.join(__dirname, "back", "pipes", "processo1.exe"),
    socket: path.join(__dirname, "back", "sockets", "socket_client.exe"),
    memcomp: path.join(
        __dirname,
        "back",
        "shared_memory",
        "processo_escritor.exe"
    ),
};

// Diretórios de trabalho para cada processo
const commDirs = {
    pipes: path.join(__dirname, "back", "pipes"),
    socket: path.join(__dirname, "back", "sockets"),
    memcomp: path.join(__dirname, "back", "shared_memory"),
};

// Controle de processos ativos por tipo de comunicação
let activeProcesses = {
    pipes: null,
    socket: null,
    memcomp: null,
};

let socketServerProcess = null; // Processo do servidor socket

// Limpa processos ativos para evitar conflitos
function cleanupProcesses() {
    Object.keys(activeProcesses).forEach((tipo) => {
        if (activeProcesses[tipo]) {
            activeProcesses[tipo].kill();
            activeProcesses[tipo] = null;
            console.log(`Processo ${tipo} finalizado`);
        }
    });
}

// Para o servidor socket usando taskkill do Windows
function stopSocketServer() {
    if (socketServerProcess) {
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

// Endpoint principal para enviar mensagens via IPC
app.post("/enviar", (req, res) => {
    const mensagem = req.body.mensagem || "";
    const tipo = req.body.tipo || "pipes";

    // Validação do tipo de comunicação
    if (!commPaths[tipo] || !commDirs[tipo]) {
        return res.json({
            status: "erro",
            mensagem: "Tipo de comunicação não suportado",
            mensagem_enviada: mensagem,
            mensagem_recebida: "",
        });
    }

    cleanupProcesses(); // Limpa processos anteriores

    // --- PROCESSAMENTO PARA PIPES ---
    if (tipo === "pipes") {
        const processo = spawn(commPaths[tipo], [], {
            cwd: commDirs[tipo],
            windowsHide: true, // Esconde janela do CMD
            stdio: ["pipe", "ignore", "ignore"], // stdin, stdout, stderr
        });

        activeProcesses.pipes = processo;

        // Envia mensagem via stdin para o processo1.exe
        processo.stdin.write(mensagem + "\n");
        processo.stdin.end();

        // Aguarda término e lê log.json gerado
        processo.on("close", () => {
            activeProcesses.pipes = null;
            const logPath = path.join(commDirs[tipo], "log.json");
            fs.readFile(logPath, "utf8", (err, data) => {
                if (err)
                    return handleLogError(
                        res,
                        mensagem,
                        "Erro ao ler arquivo de log"
                    );

                try {
                    const log = JSON.parse(data);
                    res.json({
                        status: log.status,
                        mensagem: log.mensagem_recebida || "",
                        mensagem_enviada: log.mensagem_enviada || mensagem,
                        mensagem_recebida: log.mensagem_recebida || "",
                    });
                } catch (parseError) {
                    handleLogError(res, mensagem, "Erro ao processar log");
                }
            });
        });
    }
    // --- PROCESSAMENTO PARA SOCKETS ---
    else if (tipo === "socket") {
        const processo = spawn(commPaths[tipo], [mensagem], {
            // Mensagem como argumento
            cwd: commDirs[tipo],
            windowsHide: true,
            stdio: ["ignore", "ignore", "ignore"],
        });

        activeProcesses.socket = processo;

        processo.on("close", (code) => {
            activeProcesses.socket = null;
            const logPath = path.join(commDirs[tipo], "log.json");
            fs.readFile(logPath, "utf8", (err, data) => {
                if (err)
                    return handleLogError(
                        res,
                        mensagem,
                        "Erro ao ler arquivo de log"
                    );

                try {
                    const log = JSON.parse(data);
                    res.json({
                        status: log.status,
                        mensagem: log.mensagem_recebida || "",
                        mensagem_enviada: log.mensagem_enviada || mensagem,
                        mensagem_recebida: log.mensagem_recebida || "",
                    });
                } catch (parseError) {
                    handleLogError(res, mensagem, "Erro ao processar log");
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
    // --- PROCESSAMENTO PARA MEMÓRIA COMPARTILHADA ---
    else if (tipo === "memcomp") {
        const processo = spawn(commPaths[tipo], [], {
            cwd: commDirs[tipo],
            windowsHide: true,
            stdio: ["pipe", "ignore", "ignore"],
        });

        activeProcesses.memcomp = processo;

        // Envia mensagem via stdin para o processo_escritor.exe
        processo.stdin.write(mensagem + "\n");
        processo.stdin.end();

        processo.on("close", () => {
            activeProcesses.memcomp = null;
            const logPath = path.join(commDirs[tipo], "log.json");
            fs.readFile(logPath, "utf8", (err, data) => {
                if (err)
                    return handleLogError(
                        res,
                        mensagem,
                        "Erro ao ler arquivo de log"
                    );

                try {
                    const log = JSON.parse(data);
                    res.json({
                        status: log.status,
                        mensagem: log.mensagem_recebida || "",
                        mensagem_enviada: log.mensagem_enviada || mensagem,
                        mensagem_recebida: log.mensagem_recebida || "",
                    });
                } catch (parseError) {
                    handleLogError(res, mensagem, "Erro ao processar log");
                }
            });
        });
    }
    // --- TIPO NÃO RECONHECIDO ---
    else {
        return res.json({
            status: "erro",
            mensagem: "Tipo de comunicação não implementado",
            mensagem_enviada: mensagem,
            mensagem_recebida: "",
        });
    }
});

// Função auxiliar para tratamento de erro de log
function handleLogError(res, mensagem, erro) {
    res.json({
        status: "erro",
        mensagem: erro,
        mensagem_enviada: mensagem,
        mensagem_recebida: "",
    });
}

// Endpoint para limpar/parar processos específicos ou todos
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

// Servir arquivos estáticos do frontend (HTML, CSS, JS)
app.use(express.static(path.join(__dirname, "front")));

// Rotas para servir a interface web
app.get("/", (req, res) => {
    res.sendFile(path.join(__dirname, "front", "html", "index.html"));
});

app.get("/index.html", (req, res) => {
    res.sendFile(path.join(__dirname, "front", "html", "index.html"));
});

// Inicializa servidor socket em segundo plano
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
                detached: true, // Processo independente
                windowsHide: true, // Janela escondida
                stdio: "ignore", // Ignora entrada/saída
            });
            socketServerProcess.unref(); // Permite que Node.js termine independentemente
            console.log(
                "Servidor socket iniciado silenciosamente na porta 8888"
            );
        } else {
            console.log(
                "Servidor socket não encontrado. Compile os arquivos C++ primeiro."
            );
        }
    } catch (error) {
        console.log("Erro ao iniciar servidor socket:", error.message);
    }
}

// Inicia servidor socket automaticamente
startSocketServer();

// Shutdown graceful - limpa recursos ao terminar
process.on("SIGINT", () => {
    console.log("\nFinalizando processos...");
    cleanupProcesses();
    stopSocketServer();
    process.exit(0);
});

// Inicia servidor web na porta 3000
app.listen(3000, () => {
    console.log("Servidor Node.js rodando em http://localhost:3000");
    console.log("Todos os processos serão executados sem abrir janelas do CMD");
});
