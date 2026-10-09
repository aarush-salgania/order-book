import express from "express";
import { WebSocketServer } from "ws";
import http from "node:http"
import cors from "cors";
import { spawn } from "node:child_process";

const clients = new Set();
const app = express();
const PORT = 4000;
const WS_PORT = 4001;
const wss = new WebSocketServer({ port: WS_PORT });

const engineProcess = spawn('./order_book.exe', []); // Binary file to be created yet

engineProcess.stdout.on("data", (data) => {
    const message = data.toString().trim();
    console.log(`[C++ Engine]: ${message}`);

    // Broadcast the C++ engine output to all connected React clients
    broadcastToClients(message);
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
})

app.post("/api/order", (req, res) => {
    const { order } = req.body;
    if (!order) return res.status(400).json({ error: "Order payload required" })
    sendToEngine(order);
    res.status(200).json({ status: "Order sent to engine" });
})

app.listen(PORT, () => {
    console.log(`Server running on port http://localhost:${PORT}`)
    console.log(`Websocket Server running on port ws://localhost:${WS_PORT}`)
})

wss.on("connection", (ws) => {
    console.log("Frontend connected via WebSocket");
    clients.add(ws);

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
function sendToEngine(orderString) {
    if (engineProcess.stdin.writable) {
        engineProcess.stdin.write(orderString + "\n");
    } else {
        console.error("C++ engine stdin is not writable");
    }
}
