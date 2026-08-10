// index.js — Floatee 联机中转服务器入口
// 装配：HTTP admin + WebSocket(ws) + TCP(测试) 三种传输，共享一套房间/协议逻辑。
import { WebSocketServer } from 'ws';
import net from 'node:net';
import { config } from './config.js';
import { Store, Session } from './store.js';
import { RoomManager } from './rooms.js';
import { createMessageHandler } from './session.js';
import { startAdmin } from './admin.js';
import { logger } from './logger.js';
import { MSG } from './protocol.js';

const store = new Store();
const rooms = new RoomManager(store, config);
const handle = createMessageHandler(store, rooms, config);
rooms.start();

let totalConns = 0;

/** 绑定一个新的传输连接 */
function attach(session, transport) {
  if (store.connections.size >= config.server.maxConnections) {
    transport.send({ type: MSG.ERROR, code: 'server_full', message: '服务器连接数已达上限' });
    transport.close();
    return;
  }
  store.addConnection(session);
  totalConns++;
  logger.info('conn_open', { connId: session.connId, total: totalConns });

  transport.onClose(() => {
    // 用独立的 cleaned 标志：session.close()（如握手失败 device_busy 时）会
    // 置 closed=true，但连接仍要从表里移除，否则会残留导致同设备后续
    // 一直 device_busy。
    if (session.cleaned) return;
    session.cleaned = true;
    rooms.leaveRoom(session, 'disconnected');
    store.removeConnection(session);
    logger.info('conn_close', { connId: session.connId, clientId: session.clientId });
  });
}

// ── WebSocket 传输（正式） ──────────────────────────────────────────
const wss = new WebSocketServer({ host: config.server.host, port: config.server.wsPort });
wss.on('connection', (ws) => {
  const session = new Session(wsTransport(ws), config);
  attach(session, {
    send: (o) => { if (ws.readyState === 1) ws.send(JSON.stringify(o)); },
    close: () => { try { ws.close(); } catch {} },
    onClose: (cb) => ws.on('close', cb),
  });
  ws.on('message', (data) => onMessage(session, data.toString()));
  ws.on('error', () => {});
});

function wsTransport(ws) {
  return {
    send(obj) { if (ws.readyState === 1) ws.send(JSON.stringify(obj)); },
    close() { try { ws.close(); } catch {} },
  };
}

// ── TCP 传输（客户端测试/备用，JSON 行协议） ──────────────────────
const tcp = net.createServer((socket) => {
  const session = new Session(tcpTransport(socket), config);
  let buf = '';
  attach(session, {
    send: (o) => socket.write(JSON.stringify(o) + '\n'),
    close: () => { try { socket.end(); } catch {} },
    onClose: (cb) => socket.on('close', cb),
  });
  socket.on('data', (chunk) => {
    buf += chunk.toString('utf8');
    let idx;
    while ((idx = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, idx).trim();
      buf = buf.slice(idx + 1);
      if (line) onMessage(session, line);
    }
    if (buf.length > 65536) { buf = ''; session.close(); }
  });
  socket.on('error', () => {});
});

function tcpTransport(socket) {
  return {
    send(obj) { socket.write(JSON.stringify(obj) + '\n'); },
    close() { try { socket.end(); } catch {} },
  };
}

tcp.listen(config.server.tcpPort, config.server.host, () => {
  logger.info('tcp_listening', { host: config.server.host, port: config.server.tcpPort });
});

// ── 消息解析与分发 ────────────────────────────────────────────────
function onMessage(session, text) {
  if (!text) return;
  if (Buffer.byteLength(text, 'utf8') > config.security.maxMessageSize) {
    session.send({ type: MSG.ERROR, code: 'too_large', message: '消息过大' });
    return;
  }
  let msg;
  try {
    msg = JSON.parse(text);
  } catch {
    session.send({ type: MSG.ERROR, code: 'bad_json', message: 'JSON 解析失败' });
    return;
  }
  handle(session, msg);
}

// ── 管理接口 ───────────────────────────────────────────────────────
startAdmin({ store, rooms, config });

logger.info('floatee_server_started', {
  wsPort: config.server.wsPort,
  tcpPort: config.server.tcpPort,
  adminPort: config.server.adminPort,
  maxRooms: config.room.maxRooms,
  maxConnsPerDevice: config.security.maxConnsPerDevice,
});
