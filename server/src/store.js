// store.js — 内存数据模型（Store/Session/Room/Member/RoleInfo）
// 与计划书 §5.4 一致。Session 持有传输对象（WebSocket 或 TCP），
// 通过 transport.send(obj) 发送、transport.close() 关闭。

let connSeq = 0;

export class Session {
  constructor(transport, config) {
    this.connId = `c${++connSeq}`;
    this.transport = transport;
    this.authenticated = false;
    this.clientId = null;
    this.deviceId = null;
    this.displayName = '';
    this.version = '';
    this.roomId = null;            // 当前房间（null = 不在房间）
    this.roles = new Map();        // roleIndex -> RoleInfo
    this.lastSeen = Date.now();
    // 限流滑动窗口（记录时间戳）
    this.counters = { emoticon: [], skin: [], mouse: [], chat: [] };
    this.closed = false;   // session.close() 标记（防重复 close）
    this.cleaned = false;  // 已从连接表移除（防重复清理，不受 close 影响）
  }

  send(obj) {
    if (this.closed) return false;
    try {
      this.transport.send(obj);
      return true;
    } catch (err) {
      return false;
    }
  }

  close() {
    if (this.closed) return;
    this.closed = true;
    try { this.transport.close(); } catch { /* ignore */ }
  }
}

export class RoleInfo {
  constructor(roleId, roleName = '', skin = '', hue = 0, sat = 1, light = 1) {
    this.roleId = roleId;
    this.roleName = roleName;
    this.skin = skin;
    this.hue = hue;      // 皮肤 HSL 调整（随 add_role 同步）
    this.sat = sat;
    this.light = light;
  }
}

export class Room {
  constructor(roomId, roomName, capacity, isPublic, ownerClientId) {
    this.roomId = roomId;
    this.roomName = roomName;
    this.capacity = capacity;
    this.isPublic = isPublic;
    this.createdAt = Date.now();
    this.ownerClientId = ownerClientId;
    this.ownerToken = '';          // 房主管理令牌（create 时生成）
    this.joinCode = '';            // 唯一邀请码
    this.password = '';            // 公共房可选密码（私密房忽略；不随列表返回）
    this.members = new Map();      // clientId -> Member
    this.emptySince = null;        // 空房 TTL 起点
  }
}

export class Member {
  constructor(clientId, displayName, isOwner) {
    this.clientId = clientId;
    this.displayName = displayName;
    this.joinedAt = Date.now();
    this.isOwner = isOwner;
    this.roles = new Map();        // roleIndex -> RoleInfo
  }
}

export class Store {
  constructor() {
    this.connections = new Map();  // connId -> Session
    this.rooms = new Map();        // roomId -> Room
  }

  addConnection(session) {
    this.connections.set(session.connId, session);
  }

  removeConnection(session) {
    this.connections.delete(session.connId);
  }

  /** 统计某 deviceId 当前已认证且在线的连接数 */
  countDevice(deviceId) {
    let n = 0;
    for (const s of this.connections.values()) {
      if (s.authenticated && s.deviceId === deviceId) n++;
    }
    return n;
  }

  getRoom(roomId) {
    return this.rooms.get(roomId) || null;
  }

  roomByJoinCode(code) {
    for (const r of this.rooms.values()) if (r.joinCode === code) return r;
    return null;
  }
}
