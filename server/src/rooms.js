// rooms.js — 房间逻辑（create/join/leave/disband/kick/settings）+ 自动管理 tick
// 与计划书 §5.5/§5.6/§5.7 一致。
import crypto from 'node:crypto';
import { MSG } from './protocol.js';
import { logger } from './logger.js';
import { Store, Session, Room, Member, RoleInfo } from './store.js';

// 随机串字符集：
// ALPHABET —— 通用（ownerToken 等，排除易混淆 0O1Il）
// DIGITS —— 房间号（6 位纯数字）
// CODE_ALPHABET —— 邀请码（4 位大写字母+数字，排除易混淆 0O1I）
const ALPHABET = '23456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
const DIGITS = '0123456789';
const CODE_ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
function randString(len, alphabet = ALPHABET) {
  let s = '';
  for (let i = 0; i < len; i++) s += alphabet[crypto.randomInt(0, alphabet.length)];
  return s;
}

export class RoomManager {
  /**
   * @param {Store} store
   * @param {object} config
   */
  constructor(store, config) {
    this.store = store;
    this.cfg = config;
    this.tickTimer = null;
  }

  /** 全局 tick：连接超时 / 空房回收（每 10s） */
  start() {
    this.tickTimer = setInterval(() => this.tick(), 10000);
    this.tickTimer.unref?.();
  }

  stop() {
    if (this.tickTimer) clearInterval(this.tickTimer);
    this.tickTimer = null;
  }

  tick() {
    const now = Date.now();
    const idleMs = this.cfg.room.idleTimeoutMs;
    const emptyMs = this.cfg.room.emptyTtlMs;

    // 1) 连接超时
    for (const s of [...this.store.connections.values()]) {
      if (!s.closed && now - s.lastSeen > idleMs) {
        logger.info('conn_timeout', { connId: s.connId, clientId: s.clientId });
        this.leaveRoom(s, 'timeout');
        s.close();
      }
    }
    // 2) 空房回收
    for (const r of [...this.store.rooms.values()]) {
      if (r.members.size === 0) {
        if (r.emptySince === null) r.emptySince = now;
        else if (now - r.emptySince >= emptyMs) this.destroyRoom(r, 'empty');
      }
    }
  }

  /** hello 握手：设备限制校验；返回 { ok } 或 { ok:false, code, message } */
  handshake(session, msg) {
    const maxDev = this.cfg.security.maxConnsPerDevice;
    if (maxDev > 0 && this.store.countDevice(msg.deviceId) >= maxDev) {
      return { ok: false, code: 'device_busy', message: '本设备已有联机进程' };
    }
    session.authenticated = true;
    session.clientId = msg.clientId;
    session.deviceId = msg.deviceId;
    session.displayName = msg.displayName || msg.clientId;
    session.version = msg.version || '';
    session.send({ type: MSG.WELCOME, clientId: session.clientId, serverTime: Date.now(), serverVersion: '0.1.0' });
    logger.info('hello', { connId: session.connId, clientId: session.clientId, deviceId: session.deviceId });
    return { ok: true };
  }

  createRoom(session, msg) {
    if (session.roomId) return err('already_in_room', '已在房间中，请先离开');
    if (this.store.rooms.size >= this.cfg.room.maxRooms) return err('room_full', '服务器房间数已达上限');

    let roomId;
    do { roomId = randString(this.cfg.room.roomIdLength, DIGITS); } while (this.store.rooms.has(roomId));

    const room = new Room(roomId, msg.roomName || `Room-${roomId}`, msg.capacity || this.cfg.room.capacity,
      msg.public === true, session.clientId);
    room.ownerToken = randString(32);
    room.joinCode = randString(this.cfg.room.joinCodeLength, CODE_ALPHABET);
    this.store.rooms.set(roomId, room);

    this.addMember(room, session, true);
    session.roomId = roomId;

    session.send({ type: MSG.ROOM_CREATED, roomId, roomName: room.roomName, capacity: room.capacity,
      public: room.isPublic, ownerToken: room.ownerToken, joinCode: room.joinCode });
    logger.info('room_created', { roomId, owner: session.clientId });
    return { ok: true };
  }

  joinRoom(session, msg) {
    if (session.roomId) return err('already_in_room', '已在房间中，请先离开');
    const room = this.store.getRoom(msg.roomId);
    if (!room) return err('room_not_found', '房间不存在或已关闭');
    if (room.members.size >= room.capacity) return err('room_full', '房间已满');
    if (!room.isPublic && room.joinCode !== msg.joinCode) return err('bad_join_code', '邀请码错误');

    this.addMember(room, session, false);
    session.roomId = room.roomId;

    // 给新成员发成员摘要
    session.send({
      type: MSG.ROOM_JOINED, roomId: room.roomId, roomName: room.roomName,
      members: this.memberSummary(room),
    });
    // 广播给房间内其他人
    this.broadcast(room, session, { type: MSG.PEER_JOINED, member: this.sessionMember(room, session) });
    logger.info('room_joined', { roomId: room.roomId, clientId: session.clientId });
    return { ok: true };
  }

  addMember(room, session, isOwner) {
    const m = new Member(session.clientId, session.displayName, isOwner);
    room.members.set(session.clientId, m);
    room.emptySince = null;
  }

  leaveRoom(session, reason = 'left') {
    if (!session.roomId) return;
    const room = this.store.getRoom(session.roomId);
    session.roomId = null;
    if (!room) return;
    room.members.delete(session.clientId);
    this.removeRoles(room, session);
    session.send({ type: MSG.ROOM_LEFT });

    if (room.members.size === 0) {
      room.emptySince = Date.now();   // 启动空房倒计时
      return;
    }
    // 房主转移：若离开者是房主，转给最早加入者
    if (room.ownerClientId === session.clientId) {
      this.transferOwner(room, session.clientId);
    }
    this.broadcast(room, null, { type: MSG.PEER_LEFT, roleId: `${session.clientId}/0` });
  }

  removeRoles(room, session) {
    for (const [idx] of session.roles) {
      room.members.get(session.clientId)?.roles.delete(idx);
    }
    session.roles.clear();
  }

  transferOwner(room, oldOwner) {
    const next = [...room.members.values()].sort((a, b) => a.joinedAt - b.joinedAt)[0];
    if (!next) return;
    room.ownerClientId = next.clientId;
    room.ownerToken = randString(32);
    next.isOwner = true;
    const s = this.sessionOf(next.clientId);
    s?.send({ type: MSG.ROOM_SETTINGS_UPDATED, roomId: room.roomId, ownerToken: room.ownerToken, youAreOwner: true });
    logger.info('owner_transferred', { roomId: room.roomId, newOwner: next.clientId });
  }

  destroyRoom(room, reason) {
    const roomId = room.roomId;
    for (const m of room.members.values()) {
      const s = this.sessionOf(m.clientId);
      if (s) {
        s.roomId = null;
        s.send({ type: MSG.ROOM_CLOSED, roomId, reason });
      }
    }
    this.store.rooms.delete(roomId);
    logger.info('room_destroyed', { roomId, reason });
  }

  // ---- 房主操作（ownerToken 校验） ----

  roomSettings(session, msg) {
    const room = this.requireOwner(session, msg);
    if (!room.ok) return room;
    const r = room.room;
    if (msg.capacity !== undefined) r.capacity = Math.max(2, Math.min(32, msg.capacity));
    if (msg.public !== undefined) r.isPublic = !!msg.public;
    if (msg.roomName !== undefined) r.roomName = String(msg.roomName).slice(0, this.cfg.security.maxNameLen);
    this.broadcast(r, null, { type: MSG.ROOM_SETTINGS_UPDATED, roomId: r.roomId, capacity: r.capacity, public: r.isPublic, roomName: r.roomName });
    return { ok: true };
  }

  kickMember(session, msg) {
    const room = this.requireOwner(session, msg);
    if (!room.ok) return room;
    const r = room.room;
    const target = r.members.get(msg.targetClientId);
    if (!target) return err('member_not_found', '成员不在房间中');
    if (target.clientId === r.ownerClientId) return err('cannot_kick_owner', '不能踢房主');

    const t = this.sessionOf(target.clientId);
    r.members.delete(target.clientId);
    this.removeRoles(r, t);
    if (t) {
      t.roomId = null;
      t.send({ type: MSG.PEER_KICKED, roomId: r.roomId, reason: 'kicked' });
    }
    this.broadcast(r, null, { type: MSG.PEER_LEFT, roleId: `${target.clientId}/0` });
    logger.info('member_kicked', { roomId: r.roomId, target: target.clientId });
    return { ok: true };
  }

  disbandRoom(session, msg) {
    const room = this.requireOwner(session, msg);
    if (!room.ok) return room;
    this.destroyRoom(room.room, 'disbanded');
    return { ok: true };
  }

  requireOwner(session, msg) {
    const room = this.store.getRoom(session.roomId || '');
    if (!room) return { ok: false, ...err('not_in_room', '不在房间中') };
    if (room.ownerToken !== msg.ownerToken) return { ok: false, ...err('no_permission', '无权限（仅房主可操作）') };
    return { ok: true, room };
  }

  // ---- 角色 ----

  addRole(session, msg) {
    if (!session.roomId) return err('not_in_room', '不在房间中');
    const room = this.store.getRoom(session.roomId);
    const m = room?.members.get(session.clientId);
    if (!m) return err('not_in_room', '不在房间中');
    if (session.roles.has(msg.roleIndex)) return err('role_exists', '角色已存在');
    const roleId = `${session.clientId}/${msg.roleIndex}`;
    const role = new RoleInfo(roleId, msg.roleName || session.displayName,
      msg.skin || '', msg.hue ?? 0, msg.sat ?? 1, msg.light ?? 1);
    session.roles.set(msg.roleIndex, role);
    m.roles.set(msg.roleIndex, role);
    // 确认给发送者（客户端据此才开始上报 mouse/skin，避免时序竞争）
    session.send({ type: MSG.ROLE_ADDED, roleId });
    // 广播新角色给房间内其他人
    this.broadcast(room, session, { type: MSG.PEER_JOINED, member: this.sessionMember(room, session) });
    return { ok: true };
  }

  removeRole(session, msg) {
    if (!session.roomId) return err('not_in_room', '不在房间中');
    const room = this.store.getRoom(session.roomId);
    const role = session.roles.get(msg.roleIndex);
    if (!role) return err('role_not_found', '角色不存在');
    session.roles.delete(msg.roleIndex);
    room.members.get(session.clientId)?.roles.delete(msg.roleIndex);
    this.broadcast(room, session, { type: MSG.PEER_LEFT, roleId: role.roleId });
    return { ok: true };
  }

  // ---- 事件转发（校验 roleId 归属） ----

  skinUpdate(session, msg) {
    if (!this.hasRole(session, msg.roleId)) return err('role_not_found', '角色不属于本连接');
    this.setRoleField(session, msg.roleId, 'skin', msg.skin);
    this.broadcastEvent(session, { type: MSG.PEER_SKIN, roleId: msg.roleId, skin: msg.skin });
    return { ok: true };
  }

  mouse(session, msg) {
    if (!this.hasRole(session, msg.roleId)) return err('role_not_found', '角色不属于本连接');
    this.broadcastEvent(session, { type: MSG.PEER_MOUSE, roleId: msg.roleId, dx: msg.dx, dy: msg.dy, eye: msg.eye ?? 0, es: msg.es ?? 0 });
    return { ok: true };
  }

  emoticon(session, msg) {
    if (!this.hasRole(session, msg.roleId)) return err('role_not_found', '角色不属于本连接');
    this.broadcastEvent(session, { type: MSG.PEER_EMOTICON, roleId: msg.roleId, index: msg.index });
    return { ok: true };
  }

  chat(session, msg) {
    if (!this.hasRole(session, msg.roleId)) return err('role_not_found', '角色不属于本连接');
    this.broadcastEvent(session, { type: MSG.PEER_CHAT, roleId: msg.roleId, text: msg.text });
    return { ok: true };
  }

  hasRole(session, roleId) {
    return [...session.roles.values()].some((r) => r.roleId === roleId);
  }

  setRoleField(session, roleId, field, value) {
    for (const r of session.roles.values()) if (r.roleId === roleId) { r[field] = value; break; }
  }

  listRooms(session) {
    const rooms = [...this.store.rooms.values()]
      .filter((r) => r.members.size > 0)   // 只列非空房
      .map((r) => ({ roomId: r.roomId, roomName: r.roomName, members: r.members.size, capacity: r.capacity, public: r.isPublic }));
    session.send({ type: MSG.ROOM_LIST, rooms });
    return { ok: true };
  }

  // ---- 辅助 ----

  sessionOf(clientId) {
    for (const s of this.store.connections.values()) if (s.clientId === clientId) return s;
    return null;
  }

  sessionMember(room, session) {
    return {
      clientId: session.clientId,
      displayName: session.displayName,
      roles: [...session.roles.values()].map((r) => ({ roleId: r.roleId, roleName: r.roleName, skin: r.skin, hue: r.hue, sat: r.sat, light: r.light })),
    };
  }

  memberSummary(room) {
    return [...room.members.values()].map((m) => ({
      clientId: m.clientId,
      displayName: m.displayName,
      roles: [...m.roles.values()].map((r) => ({ roleId: r.roleId, roleName: r.roleName, skin: r.skin, hue: r.hue, sat: r.sat, light: r.light })),
    }));
  }

  /** 广播给房间内除 exclude 以外的所有人 */
  broadcast(room, exclude, obj) {
    for (const m of room.members.values()) {
      if (exclude && m.clientId === exclude.clientId) continue;
      const s = this.sessionOf(m.clientId);
      s?.send(obj);
    }
  }

  broadcastEvent(session, obj) {
    const room = this.store.getRoom(session.roomId || '');
    if (!room) return;
    this.broadcast(room, session, obj);
  }
}

function err(code, message) {
  return { code, message };
}
