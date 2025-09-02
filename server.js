const express = require("express");
const { spawn } = require("child_process");
const fs = require("fs");
const path = require("path");

const app = express();
app.use(express.json());

// Caminhos para os executáveis de cada tipo de comunicação
const commPaths = {
    pipes: path.join(__dirname, "back", "pipes", "processo1.exe"),
    socket: path.join(__dirname, "back", "sockets", "client.exe"), // exemplo futuro
    memcomp: path.join(__dirname, "back", "shared_memory", "processA.exe"), // exemplo futuro
};

const commDirs = {
    pipes: path.join(__dirname, "back", "pipes"),
    socket: path.join(__dirname, "back", "sockets"),
    memcomp: path.join(__dirname, "back", "shared_memory"),
};

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

    // Por enquanto só temos pipes implementado
    if (tipo !== "pipes") {
        return res.json({
            status: "erro",
            mensagem: "Tipo de comunicação em desenvolvimento",
            mensagem_enviada: mensagem,
            mensagem_recebida: "",
        });
    }

    const processo = spawn(commPaths[tipo], [], {
        cwd: commDirs[tipo],
    });

    // Envia a mensagem para o processo pelo stdin
    processo.stdin.write(mensagem + "\n");
    processo.stdin.end();

    // Aguarda o processo terminar e lê o log.json
    processo.on("close", () => {
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
});

// Servir arquivos estáticos do front-end
app.use(express.static(path.join(__dirname, "front")));

app.listen(3000, () => {
    console.log("Servidor rodando em http://localhost:3000");
});
