import express from "express";
import { WebSocketServer } from "ws";
import cors from "cors";
import { spawn } from "node:child_process";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { existsSync } from "node:fs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const binaryPath = path.join(__dirname, "order_book.exe");
const loggerBinaryPath = path.join(__dirname, "merkle_logger.exe");

const clients = new Set();
const app = express();
const PORT = 4000;
const WS_PORT = 4001;
const wss = new WebSocketServer({ port: WS_PORT });

// 1. Core Matching Engine Process
const engineProcess = spawn(binaryPath, []);

engineProcess.on("error", (err) => {
    console.error(`[C++ Engine Process Error]: ${err.message}`);
});

// 2. Standalone Asynchronous Verifiable Merkle Logger Child Process
let loggerProcess = null;
if (existsSync(loggerBinaryPath)) {
    loggerProcess = spawn(loggerBinaryPath, []);
    loggerProcess.on("error", (err) => {
        console.error(`[Merkle Logger Error]: ${err.message}`);
    });

    loggerProcess.stdout.on("data", (data) => {
        const lines = data.toString().split(/\r?\n/).filter((l) => l.trim().length > 0);
        for (const line of lines) {
            console.log(`[Merkle Logger]: ${line}`);
            broadcastToClients(line);
        }
    });

    loggerProcess.stderr.on("data", (data) => {
        console.log(`[Merkle Logger Info]: ${data.toString().trim()}`);
    });

    loggerProcess.on("close", (code) => {
        console.log(`[Merkle Logger] exited with code ${code}`);
    });
} else {
    console.warn(`[Merkle Logger]: Binary not found at ${loggerBinaryPath}. Run start.bat to compile it.`);
}

let logCounter = 0;

engineProcess.stdout.on("data", (data) => {
    const lines = data.toString().split(/\r?\n/).filter((line) => line.trim().length > 0);
    for (const line of lines) {
        logCounter++;
        // Sample logging so terminal doesn't choke during high-frequency throughput
        if (logCounter % 20 === 0 || line.startsWith("BOOK_") || line.startsWith("ENGINE_") || logCounter < 50) {
            console.log(`[C++ Engine]: ${line}`);
        }
        broadcastToClients(line);
    }
});

engineProcess.stderr.on("data", (data) => {
    console.error(`[C++ Error]: ${data.toString()}`);
});

engineProcess.on("close", (code) => {
    console.log(`[C++ Engine] exited with code ${code}`);
});

app.use(cors());
app.use(express.json());

app.get("/", (req, res) => {
    res.send("Hello World, Trading Engine Running");
});

app.post("/api/order", (req, res) => {
    const { order } = req.body;
    if (!order) return res.status(400).json({ error: "Order payload required" });
    sendToEngine(order);
    res.status(200).json({ status: "Order sent to engine" });
});

app.post("/api/orders", (req, res) => {
    const { orders } = req.body;
    if (!Array.isArray(orders)) return res.status(400).json({ error: "Orders array required" });
    for (const ord of orders) {
        sendToEngine(ord);
    }
    res.status(200).json({ status: `${orders.length} orders dispatched` });
});

app.listen(PORT, () => {
    console.log(`Server running on port http://localhost:${PORT}`);
    console.log(`Websocket Server running on port ws://localhost:${WS_PORT}`);
});

wss.on("connection", (ws) => {
    console.log("Frontend connected via WebSocket");
    clients.add(ws);

    ws.on("message", (raw) => {
        try {
            const str = raw.toString().trim();
            if (!str) return;
            if (str.startsWith("[") && str.endsWith("]")) {
                const arr = JSON.parse(str);
                for (const item of arr) sendToEngine(item);
            } else if (str.startsWith("{")) {
                const obj = JSON.parse(str);
                if (obj.action === "order" && obj.order) {
                    sendToEngine(obj.order);
                } else if (obj.action === "orders" && Array.isArray(obj.orders)) {
                    for (const item of obj.orders) sendToEngine(item);
                } else if (obj.action === "clear" || obj.action === "reset" || obj.action === "snapshot") {
                    sendToEngine(obj.action);
                } else {
                    sendToEngine(obj);
                }
            } else {
                sendToEngine(str);
            }
        } catch (e) {
            console.error("Error handling WS message:", e.message);
        }
    });

    ws.on("error", (err) => {
        console.error("WS Client error:", err.message);
    });

    ws.on("close", () => {
        clients.delete(ws);
        console.log("Frontend disconnected");
    });
});

function broadcastToClients(data) {
    for (const client of clients) {
        if (client.readyState === 1) {
            client.send(data);
        }
    }
}

function sendToEngine(order) {
    const payload = typeof order === "object" ? JSON.stringify(order) : String(order);
    if (engineProcess.stdin && engineProcess.stdin.writable) {
        engineProcess.stdin.write(payload + "\n");
    } else {
        console.error("C++ engine stdin is not writable");
    }
}

process.on("SIGINT", () => {
    if (engineProcess && !engineProcess.killed) {
        engineProcess.kill();
    }
    if (loggerProcess && !loggerProcess.killed) {
        loggerProcess.kill();
    }
    process.exit();
});
