// rooms.test.js — 房间生命周期 / 凭证 / 权限 / 转发（node:test）
import { test, beforeEach } from 'node:test';
import assert from 'node:assert/strict';
import { Store, Session } from '../src/store.js';
import { RoomManager } from '../src/rooms.js';
import { createMessageHandler } from '../src/session.js';

// 最小配置
const config = {
  security: { maxConnsPerDevice: 1, maxNameLen: 32, maxSkinLen: 64 },
  room: { capacity: 8, maxRooms: 100, maxPublicRooms: 8, roomIdLength: 6, joinCodeLength: 4, emptyTtlMs: 300000, idleTimeoutMs: 30000 },
  throttle: { emoticonPerSec: 5, skinPerSec: 2, mousePerSec: 20 },
};

let store, rooms, handle;

// 每个测试独立环境，避免 clientId/房间状态互相污染
beforeEach(() => {
  store = new Store();
  rooms = new RoomManager(store, config);
  handle = createMessageHandler(store, rooms, config);
});

function newClient(displayName = 'user') {
  const sent = [];
  const session = new Session({ send: (o) => sent.push(o), close: () => {} }, config);
  store.addConnection(session);
  return { session, sent };
}

// 便捷：直接调用 handle（等价于收到网络消息）
function say(session, obj) {
  handle(session, obj);
}

test('握手 + welcome', () => {
  const { session, sent } = newClient();
  say(session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'Alice' });
  assert.equal(session.authenticated, true);
  assert.equal(sent.at(-1).type, 'welcome');
});

test('设备限制：同设备第二个连接被拒', () => {
  const c1 = newClient();
  say(c1.session, { type: 'hello', clientId: 'A', deviceId: 'devX', displayName: 'A' });
  const c2 = newClient();
  say(c2.session, { type: 'hello', clientId: 'B', deviceId: 'devX', displayName: 'B' });
  const last = c2.sent.at(-1);
  assert.equal(last.type, 'error');
  assert.equal(last.code, 'device_busy');
  // 未认证
  assert.equal(c2.session.authenticated, false);
});

test('创建房间返回 ownerToken + joinCode（无密码私密房邀请码为空）', () => {
  const { session, sent } = newClient();
  say(session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'Alice' });
  say(session, { type: 'create_room', roomName: 'friends', capacity: 8 });
  const r = sent.at(-1);
  assert.equal(r.type, 'room_created');
  assert.ok(r.roomId && r.ownerToken);
  assert.equal(r.joinCode, '');   // 无密码私密房：密码/邀请码为空
  assert.equal(session.roomId, r.roomId);
});

test('加入：错误邀请码拒绝，正确邀请码加入并广播 peer_joined', () => {
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'Alice' });
  say(a.session, { type: 'create_room', password: 'code123' });   // 设密码 → 邀请码=密码
  const created = a.sent.at(-1);
  assert.equal(created.joinCode, 'code123');

  // B 用错误码
  const b = newClient();
  say(b.session, { type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'Bob' });
  say(b.session, { type: 'join_room', roomId: created.roomId, joinCode: 'wrong' });
  assert.equal(b.sent.at(-1).code, 'bad_join_code');

  // B 用正确码（旧客户端路径：joinCode）
  say(b.session, { type: 'join_room', roomId: created.roomId, joinCode: created.joinCode });
  assert.equal(b.sent.at(-1).type, 'room_joined');
  assert.equal(b.session.roomId, created.roomId);
  // A 收到 peer_joined
  const peerMsg = a.sent.find((m) => m.type === 'peer_joined');
  assert.ok(peerMsg);
  assert.equal(peerMsg.member.clientId, 'B');
});

test('add_role 后角色广播；转发只发给其他人', () => {
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'Alice' });
  say(a.session, { type: 'create_room', password: 'code123' });
  const roomId = a.sent.at(-1).roomId;
  const b = newClient();
  say(b.session, { type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'Bob' });
  const b2 = newClient();
  say(b2.session, { type: 'hello', clientId: 'C', deviceId: 'devC', displayName: 'Carol' });
  const code = a.sent.find((m) => m.type === 'room_created').joinCode;
  say(b.session, { type: 'join_room', roomId, joinCode: code });
  say(b2.session, { type: 'join_room', roomId, joinCode: code });

  // A 注册角色并上报 mouse
  say(a.session, { type: 'add_role', roleIndex: 0, roleName: 'Alice', skin: 'Tata.png' });
  a.sent.length = 0; b.sent.length = 0; b2.sent.length = 0;
  say(a.session, { type: 'mouse', roleId: 'A/0', dx: 10, dy: -20, eye: 1 });
  assert.equal(b.sent.at(-1).type, 'peer_mouse');
  assert.equal(b2.sent.at(-1).type, 'peer_mouse');
  assert.equal(b.sent.at(-1).eye, 1);
  // 自己收不到自己的转发
  assert.equal(a.sent.find((m) => m.type === 'peer_mouse'), undefined);
});

test('非房主不能 kick/settings', () => {
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'A' });
  say(a.session, { type: 'create_room', password: 'code123' });
  const rc = a.sent.find((m) => m.type === 'room_created');
  const b = newClient();
  say(b.session, { type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'B' });
  say(b.session, { type: 'join_room', roomId: rc.roomId, joinCode: rc.joinCode });

  say(b.session, { type: 'kick_member', ownerToken: 'fake', targetClientId: 'A' });
  assert.equal(b.sent.at(-1).code, 'no_permission');
  say(b.session, { type: 'room_settings', ownerToken: 'fake', capacity: 4 });
  assert.equal(b.sent.at(-1).code, 'no_permission');
});

test('房主 kick_member 生效', () => {
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'A' });
  say(a.session, { type: 'create_room', password: 'code123' });
  const rc = a.sent.find((m) => m.type === 'room_created');
  const b = newClient();
  say(b.session, { type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'B' });
  say(b.session, { type: 'join_room', roomId: rc.roomId, joinCode: rc.joinCode });

  a.sent.length = 0; b.sent.length = 0;
  say(a.session, { type: 'kick_member', ownerToken: rc.ownerToken, targetClientId: 'B' });
  assert.equal(b.sent.at(-1).type, 'peer_kicked');
  assert.equal(b.session.roomId, null);
  assert.equal(a.sent.find((m) => m.type === 'peer_left').roleId, 'B/0');
});

test('房主离开转移给最早加入者', () => {
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'A' });
  say(a.session, { type: 'create_room', password: 'code123' });
  const rc = a.sent.find((m) => m.type === 'room_created');
  const b = newClient();
  say(b.session, { type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'B' });
  say(b.session, { type: 'join_room', roomId: rc.roomId, joinCode: rc.joinCode });
  const c = newClient();
  say(c.session, { type: 'hello', clientId: 'C', deviceId: 'devC', displayName: 'C' });
  say(c.session, { type: 'join_room', roomId: rc.roomId, joinCode: rc.joinCode });

  b.sent.length = 0;
  rooms.leaveRoom(a.session, 'disconnected');   // 房主 A 离开
  const upd = b.sent.find((m) => m.type === 'room_settings_updated');
  assert.ok(upd, '新房主应收到 room_settings_updated');
  assert.equal(upd.youAreOwner, true);
  // 新房主 B 的 ownerToken 已更换
  const room = store.getRoom(rc.roomId);
  assert.equal(room.ownerClientId, 'B');
});

test('空房回收：成员归零后 TTL 到期销毁', () => {
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'Z1', deviceId: 'devZ1', displayName: 'Z1' });
  say(a.session, { type: 'create_room' });
  const roomId = a.sent.find((m) => m.type === 'room_created').roomId;
  rooms.leaveRoom(a.session, 'left');
  assert.equal(store.getRoom(roomId).members.size, 0);
  // 手动触发 tick（emptyTtlMs 较小化测试）
  const room = store.getRoom(roomId);
  room.emptySince = Date.now() - config.room.emptyTtlMs - 1;
  rooms.tick();
  assert.equal(store.getRoom(roomId), null);
});

test('私密房密码统一：joinCode=password，新旧客户端均可加入，空密码凭房号直入', () => {
  // 带密码私密房
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'A' });
  say(a.session, { type: 'create_room', roomName: 'sec', password: 'abc123' });
  const rc = a.sent.find((m) => m.type === 'room_created');
  assert.equal(rc.joinCode, 'abc123');   // 邀请码 = 密码

  // 旧客户端路径：发 joinCode
  const b = newClient();
  say(b.session, { type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'B' });
  say(b.session, { type: 'join_room', roomId: rc.roomId, joinCode: 'abc123' });
  assert.equal(b.sent.at(-1).type, 'room_joined');

  // 新客户端路径：发 password
  const c = newClient();
  say(c.session, { type: 'hello', clientId: 'C', deviceId: 'devC', displayName: 'C' });
  say(c.session, { type: 'join_room', roomId: rc.roomId, password: 'abc123' });
  assert.equal(c.sent.at(-1).type, 'room_joined');

  // 无凭证被拒
  const d = newClient();
  say(d.session, { type: 'hello', clientId: 'D', deviceId: 'devD', displayName: 'D' });
  say(d.session, { type: 'join_room', roomId: rc.roomId });
  assert.equal(d.sent.at(-1).code, 'bad_join_code');

  // 空密码私密房：凭房间号直入
  const e = newClient();
  say(e.session, { type: 'hello', clientId: 'E', deviceId: 'devE', displayName: 'E' });
  say(e.session, { type: 'create_room', roomName: 'open' });
  const rc2 = e.sent.find((m) => m.type === 'room_created');
  assert.equal(rc2.joinCode, '');
  const f = newClient();
  say(f.session, { type: 'hello', clientId: 'F', deviceId: 'devF', displayName: 'F' });
  say(f.session, { type: 'join_room', roomId: rc2.roomId });
  assert.equal(f.sent.at(-1).type, 'room_joined');
});

test('公共房：上限限制 + 密码鉴权 + 列表只列公共房且不泄露密码', () => {
  rooms.cfg.room.maxPublicRooms = 2;

  // 创建 2 个公共房（其一带密码）+ 1 个私密房
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'A' });
  say(a.session, { type: 'create_room', roomName: 'pub1', public: true });
  const pub1 = a.sent.find((m) => m.type === 'room_created');

  const b = newClient();
  say(b.session, { type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'B' });
  say(b.session, { type: 'create_room', roomName: 'pub2', public: true, password: 'pw123' });
  const pub2 = b.sent.find((m) => m.type === 'room_created');

  const c = newClient();
  say(c.session, { type: 'hello', clientId: 'C', deviceId: 'devC', displayName: 'C' });
  say(c.session, { type: 'create_room', roomName: 'priv' });   // 默认私密
  const priv = c.sent.find((m) => m.type === 'room_created');

  // 超过公共房上限 → 拒绝
  const d = newClient();
  say(d.session, { type: 'hello', clientId: 'D', deviceId: 'devD', displayName: 'D' });
  say(d.session, { type: 'create_room', roomName: 'pub3', public: true });
  assert.equal(d.sent.at(-1).code, 'public_room_full');

  // 列表：只列公共房、含 hasPassword 标记、不含 password 本体
  say(a.session, { type: 'list_rooms' });
  const listMsg = a.sent.find((m) => m.type === 'room_list');
  const ids = listMsg.rooms.map((r) => r.roomId);
  assert.ok(ids.includes(pub1.roomId) && ids.includes(pub2.roomId));
  assert.ok(!ids.includes(priv.roomId), '私密房不应出现在列表');
  const pwRoom = listMsg.rooms.find((r) => r.roomId === pub2.roomId);
  assert.equal(pwRoom.hasPassword, true);
  assert.equal('password' in pwRoom, false, '列表不得泄露密码');

  // 公共带密码房：错误密码拒绝，正确密码加入
  const e = newClient();
  say(e.session, { type: 'hello', clientId: 'E', deviceId: 'devE', displayName: 'E' });
  say(e.session, { type: 'join_room', roomId: pub2.roomId, password: 'wrong' });
  assert.equal(e.sent.at(-1).code, 'bad_password');
  say(e.session, { type: 'join_room', roomId: pub2.roomId, password: 'pw123' });
  assert.equal(e.sent.at(-1).type, 'room_joined');

  // 公共免密房可直接加入（无需邀请码/密码）
  const f = newClient();
  say(f.session, { type: 'hello', clientId: 'F', deviceId: 'devF', displayName: 'F' });
  say(f.session, { type: 'join_room', roomId: pub1.roomId });
  assert.equal(f.sent.at(-1).type, 'room_joined');
});
