// protocol.js — 消息类型常量 + 字段校验（协议单一定义，语义与计划书 §4 一致）
// 客户端 Qt C++ 侧有对应实现（src/net/protocol.h），字段名/语义必须保持一致。

// ---- 消息类型 ----
export const MSG = {
  // C → S
  HELLO: 'hello',
  CREATE_ROOM: 'create_room',
  JOIN_ROOM: 'join_room',
  LEAVE_ROOM: 'leave_room',
  LIST_ROOMS: 'list_rooms',
  ROOM_SETTINGS: 'room_settings',
  KICK_MEMBER: 'kick_member',
  DISBAND_ROOM: 'disband_room',
  ADD_ROLE: 'add_role',
  REMOVE_ROLE: 'remove_role',
  SKIN_UPDATE: 'skin_update',
  MOUSE: 'mouse',
  EMOTICON: 'emoticon',
  PING: 'ping',
  // S → C
  WELCOME: 'welcome',
  ROOM_CREATED: 'room_created',
  ROOM_JOINED: 'room_joined',
  ROOM_LEFT: 'room_left',
  ROOM_LIST: 'room_list',
  ROOM_SETTINGS_UPDATED: 'room_settings_updated',
  ROOM_CLOSED: 'room_closed',
  ROLE_ADDED: 'role_added',
  PEER_JOINED: 'peer_joined',
  PEER_LEFT: 'peer_left',
  PEER_KICKED: 'peer_kicked',
  PEER_SKIN: 'peer_skin',
  PEER_MOUSE: 'peer_mouse',
  PEER_EMOTICON: 'peer_emoticon',
  PONG: 'pong',
  ERROR: 'error',
};

// 服务器需要登录态/房间态后处理的消息
export const AUTH_REQUIRED = new Set([
  MSG.CREATE_ROOM, MSG.JOIN_ROOM, MSG.LEAVE_ROOM, MSG.LIST_ROOMS,
  MSG.ROOM_SETTINGS, MSG.KICK_MEMBER, MSG.DISBAND_ROOM,
  MSG.ADD_ROLE, MSG.REMOVE_ROLE, MSG.SKIN_UPDATE, MSG.MOUSE, MSG.EMOTICON,
]);

// 皮肤名白名单（防注入）
const SKIN_RE = /^[\w.-]+\.png$/;

// 返回 { ok:true } 或 { ok:false, code, message }
export function validate(msg, cfg) {
  const maxMsg = cfg.security.maxMessageSize;
  // 消息大小在传输层已限制（字节）；这里做结构校验
  if (!msg || typeof msg !== 'object') return fail('bad_message', '消息必须是 JSON 对象');
  const type = msg.type;
  if (typeof type !== 'string' || type.length === 0) return fail('bad_message', '缺少 type');

  switch (type) {
    case MSG.HELLO:
      // deviceId 是 36 字符 UUID，长度上限单独放宽（maxIdLen）
      return checkStr(msg, 'clientId', cfg) || checkDeviceId(msg, cfg)
        || checkStr(msg, 'displayName', cfg, true) || ok();
    case MSG.CREATE_ROOM:
      if (msg.roomName !== undefined && !isStr(msg.roomName, cfg.security.maxNameLen)) return fail('bad_field', 'roomName 非法');
      if (msg.capacity !== undefined && (!Number.isInteger(msg.capacity) || msg.capacity < 2 || msg.capacity > 32)) return fail('bad_field', 'capacity 非法');
      if (msg.public !== undefined && typeof msg.public !== 'boolean') return fail('bad_field', 'public 非法');
      return ok();
    case MSG.JOIN_ROOM:
      if (!isStr(msg.roomId, 32)) return fail('bad_field', 'roomId 非法');
      if (msg.joinCode !== undefined && !isStr(msg.joinCode, cfg.room.joinCodeLength + 8)) return fail('bad_field', 'joinCode 非法');
      return ok();
    case MSG.LEAVE_ROOM:
    case MSG.LIST_ROOMS:
      return ok();
    case MSG.ROOM_SETTINGS:
      return checkOwnerToken(msg, cfg) || ok();
    case MSG.KICK_MEMBER:
      return checkOwnerToken(msg, cfg) || checkStr(msg, 'targetClientId', cfg) || ok();
    case MSG.DISBAND_ROOM:
      return checkOwnerToken(msg, cfg) || ok();
    case MSG.ADD_ROLE:
      if (!Number.isInteger(msg.roleIndex) || msg.roleIndex < 0) return fail('bad_field', 'roleIndex 非法');
      if (msg.roleName !== undefined && !isStr(msg.roleName, cfg.security.maxNameLen)) return fail('bad_field', 'roleName 非法');
      if (msg.skin !== undefined && !SKIN_RE.test(msg.skin)) return fail('bad_field', 'skin 非法');
      return ok();
    case MSG.REMOVE_ROLE:
      if (!Number.isInteger(msg.roleIndex) || msg.roleIndex < 0) return fail('bad_field', 'roleIndex 非法');
      return ok();
    case MSG.SKIN_UPDATE:
      return checkRoleId(msg, cfg) || (!SKIN_RE.test(String(msg.skin || '')) ? fail('bad_field', 'skin 非法') : ok());
    case MSG.MOUSE: {
      const r1 = checkRoleId(msg, cfg);
      if (r1) return r1;
      if (typeof msg.dx !== 'number' || typeof msg.dy !== 'number' || !Number.isFinite(msg.dx) || !Number.isFinite(msg.dy))
        return fail('bad_field', 'dx/dy 非法');
      if (msg.eye !== undefined && (!Number.isInteger(msg.eye) || msg.eye < 0 || msg.eye > 4))
        return fail('bad_field', 'eye 非法');
      if (msg.es !== undefined && (typeof msg.es !== 'number' || !Number.isFinite(msg.es) || msg.es < 0 || msg.es > 3))
        return fail('bad_field', 'es 非法');
      return ok();
    }
    case MSG.EMOTICON:
      return checkRoleId(msg, cfg)
        || (!Number.isInteger(msg.index) || msg.index < 0 || msg.index > 15 ? fail('bad_field', 'index 非法') : ok());
    case MSG.PING:
      return ok();
    default:
      return fail('unknown_type', `未知消息类型: ${type}`);
  }
}

function ok() { return { ok: true }; }
function fail(code, message) { return { ok: false, code, message }; }
function isStr(v, max) { return typeof v === 'string' && v.length > 0 && v.length <= max; }
function checkStr(msg, key, cfg, optional = false) {
  if (msg[key] === undefined && optional) return null;
  return isStr(msg[key], cfg.security.maxNameLen) ? null : fail('bad_field', `${key} 非法`);
}
function checkDeviceId(msg, cfg) {
  const v = msg.deviceId;
  const max = cfg.security.maxIdLen ?? 64;
  return (typeof v === 'string' && v.length > 0 && v.length <= max)
    ? null : fail('bad_field', 'deviceId 非法');
}
function checkOwnerToken(msg, cfg) {
  return isStr(msg.ownerToken, 64) ? null : fail('bad_field', 'ownerToken 非法');
}
function checkRoleId(msg, cfg) {
  return isStr(msg.roleId, 64) ? null : fail('bad_field', 'roleId 非法');
}
