// admin.js — HTTP 手动管理接口（REST + X-Admin-Key 鉴权）
// 与计划书 §5.9 一致：/api/stats、/api/rooms、/api/connections、广播等。
import http from 'node:http';
import { MSG } from './protocol.js';
import { logger } from './logger.js';

export function startAdmin({ store, rooms, config }) {
  const { adminKey, allowRemote } = config.admin;
  if (!adminKey) {
    logger.warn('admin_disabled', { reason: 'adminKey 未配置' });
    return null;
  }

  const server = http.createServer((req, res) => {
    // 远程访问限制
    if (!allowRemote) {
      const ip = req.socket.remoteAddress;
      if (ip !== '127.0.0.1' && ip !== '::1' && ip !== '::ffff:127.0.0.1') {
        return sendJson(res, 403, { error: 'forbidden' });
      }
    }
    // 鉴权
    if (req.headers['x-admin-key'] !== adminKey) {
      return sendJson(res, 401, { error: 'unauthorized' });
    }

    const url = new URL(req.url, `http://${req.headers.host}`);
    const parts = url.pathname.split('/').filter(Boolean); // ['api', ...]

    try {
      if (parts[0] !== 'api') return sendJson(res, 404, { error: 'not_found' });
      if (req.method === 'GET' && parts[1] === 'stats') return sendJson(res, 200, stats());
      if (req.method === 'GET' && parts[1] === 'rooms' && !parts[2]) return sendJson(res, 200, roomsList());
      if (req.method === 'GET' && parts[1] === 'rooms' && parts[2]) return sendJson(res, 200, roomDetail(parts[2]));
      if (req.method === 'DELETE' && parts[1] === 'rooms' && parts[2]) {
        const room = store.getRoom(parts[2]);
        if (!room) return sendJson(res, 404, { error: 'room_not_found' });
        rooms.destroyRoom(room, 'admin');
        return sendJson(res, 200, { ok: true });
      }
      if (req.method === 'GET' && parts[1] === 'connections') return sendJson(res, 200, connsList());
      if (req.method === 'DELETE' && parts[1] === 'connections' && parts[2]) {
        const s = store.connections.get(parts[2]);
        if (!s) return sendJson(res, 404, { error: 'conn_not_found' });
        rooms.leaveRoom(s, 'admin');
        s.close();
        return sendJson(res, 200, { ok: true });
      }
      if (req.method === 'POST' && parts[1] === 'rooms' && parts[2] && parts[3] === 'broadcast') {
        const room = store.getRoom(parts[2]);
        if (!room) return sendJson(res, 404, { error: 'room_not_found' });
        let body = '';
        req.on('data', (c) => (body += c));
        req.on('end', () => {
          try {
            const payload = JSON.parse(body);
            rooms.broadcast(room, null, payload);
            sendJson(res, 200, { ok: true });
          } catch {
            sendJson(res, 400, { error: 'bad_json' });
          }
        });
        return;
      }
      return sendJson(res, 404, { error: 'not_found' });
    } catch (err) {
      logger.error('admin_error', { err: err.message });
      return sendJson(res, 500, { error: 'internal' });
    }
  });

  server.listen(config.server.adminPort, config.server.host, () => {
    logger.info('admin_listening', { host: config.server.host, port: config.server.adminPort });
  });

  function stats() {
    return {
      uptimeMs: Date.now() - (global.__bootTs || Date.now()),
      connections: store.connections.size,
      rooms: store.rooms.size,
    };
  }
  function roomsList() {
    return [...store.rooms.values()].map((r) => ({
      roomId: r.roomId, roomName: r.roomName, members: r.members.size, capacity: r.capacity,
      public: r.isPublic, owner: r.ownerClientId,
    }));
  }
  function roomDetail(id) {
    const r = store.getRoom(id);
    if (!r) return null;
    return {
      roomId: r.roomId, roomName: r.roomName, capacity: r.capacity, public: r.isPublic,
      owner: r.ownerClientId, joinCode: r.joinCode,
      members: [...r.members.values()].map((m) => ({
        clientId: m.clientId, displayName: m.displayName, isOwner: m.isOwner,
        roles: [...m.roles.values()].map((x) => x.roleId),
      })),
    };
  }
  function connsList() {
    return [...store.connections.values()].map((s) => ({
      connId: s.connId, clientId: s.clientId, deviceId: s.deviceId,
      authenticated: s.authenticated, roomId: s.roomId,
    }));
  }

  return server;
}

function sendJson(res, code, obj) {
  const body = JSON.stringify(obj);
  res.writeHead(code, { 'Content-Type': 'application/json; charset=utf-8', 'Content-Length': Buffer.byteLength(body) });
  res.end(body);
}

// 供 stats 使用
global.__bootTs = Date.now();
