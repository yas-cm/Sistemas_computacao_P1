const express = require("express");
const { spawn } = require("child_process");
const fs = require("fs");
const path = require("path");

const app = express();
app.use(express.json());

// Caminho absoluto para a pasta dos executáveis e log
const pipesDir = path.join(__dirname, "back", "pipes");

// Endpoint para enviar mensagem via pipes
app.post("/enviar", (req, res) => {
    const mensagem = req.body.mensagem || "";
    const processo1 = spawn(path.join(pipesDir, "processo1.exe"), [], {
        cwd: pipesDir,
    });

    // Envia a mensagem para o processo1 pelo stdin
    processo1.stdin.write(mensagem + "\n");
    processo1.stdin.end();

    // Aguarda o processo terminar e lê o log.json
    processo1.on("close", () => {
        fs.readFile(path.join(pipesDir, "log.json"), "utf8", (err, data) => {
            if (err) {
                return res.json({ status: "erro", mensagem: "" });
            }
            try {
                const log = JSON.parse(data);
                res.json({ status: log.status, mensagem: log.mensagem });
            } catch {
                res.json({ status: "erro", mensagem: "" });
            }
        });
    });
});

// Servir arquivos estáticos do front-end
app.use(express.static(path.join(__dirname, "front")));

app.listen(3000, () => {
    console.log("Servidor rodando em http://localhost:3000");
});
