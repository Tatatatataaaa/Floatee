// session.js — 连接的消息分发：握手/心跳/限流/路由
import { MSG, validate, AUTH_REQUIRED } from './protocol.js';
import { logger } from './logger.js';

export function createMessageHandler(store, rooms, config) {
  return function handle(session, msg) {
    // 心跳与存活时间
    session.lastSeen = Date.now();

    // 基础校验
    const v = validate(msg, config);
    if (!v.ok) {
      session.send({ type: MSG.ERROR, code: v.code, message: v.message });
      return;
    }

    // 握手
    if (msg.type === MSG.HELLO) {
      const r = rooms.handshake(session, msg);
      if (!r.ok) {
        session.send({ type: MSG.ERROR, code: r.code, message: r.message });
        session.close();   // 握手失败（如 device_busy）直接断开，不留挂起连接
      }
      return;
    }

    if (!session.authenticated) {
      session.send({ type: MSG.ERROR, code: 'not_authed', message: '请先发送 hello' });
      return;
    }

    if (msg.type === MSG.PING) {
      session.send({ type: MSG.PONG });
      return;
    }

    if (AUTH_REQUIRED.has(msg.type)) {
      // 限流
      const bucket = { [MSG.EMOTICON]: 'emoticon', [MSG.SKIN_UPDATE]: 'skin', [MSG.MOUSE]: 'mouse', [MSG.CHAT]: 'chat' }[msg.type];
      if (bucket && !checkRate(session, bucket, config.throttle[`${bucket}PerSec`])) {
        session.send({ type: MSG.ERROR, code: 'rate_limited', message: '消息过于频繁' });
        return;
      }
    }

    let r = { ok: true };
    switch (msg.type) {
      case MSG.CREATE_ROOM: r = rooms.createRoom(session, msg); break;
      case MSG.JOIN_ROOM: r = rooms.joinRoom(session, msg); break;
      case MSG.LEAVE_ROOM: rooms.leaveRoom(session, 'left'); break;
      case MSG.LIST_ROOMS: r = rooms.listRooms(session); break;
      case MSG.ROOM_SETTINGS: r = rooms.roomSettings(session, msg); break;
      case MSG.KICK_MEMBER: r = rooms.kickMember(session, msg); break;
      case MSG.DISBAND_ROOM: r = rooms.disbandRoom(session, msg); break;
      case MSG.ADD_ROLE: r = rooms.addRole(session, msg); break;
      case MSG.REMOVE_ROLE: r = rooms.removeRole(session, msg); break;
      case MSG.SKIN_UPDATE: r = rooms.skinUpdate(session, msg); break;
      case MSG.MOUSE: r = rooms.mouse(session, msg); break;
      case MSG.EMOTICON: r = rooms.emoticon(session, msg); break;
      case MSG.CHAT: r = rooms.chat(session, msg); break;
      default:
        r = { ok: false, code: 'unknown_type', message: `未处理: ${msg.type}` };
    }
    if (!r.ok) session.send({ type: MSG.ERROR, code: r.code, message: r.message });
  };
}

/** 滑动窗口限流：bucket 内 1s 内不超过 limit 次 */
function checkRate(session, bucket, limit) {
  const now = Date.now();
  const arr = session.counters[bucket] ?? (session.counters[bucket] = []);
  while (arr.length && now - arr[0] > 1000) arr.shift();
  if (arr.length >= limit) return false;
  arr.push(now);
  return true;
}

export { logger };
