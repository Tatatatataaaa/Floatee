// rooms.test.js — 房间生命周期 / 凭证 / 权限 / 转发（node:test）
import { test, beforeEach } from 'node:test';
import assert from 'node:assert/strict';
import { Store, Session } from '../src/store.js';
import { RoomManager } from '../src/rooms.js';
import { createMessageHandler } from '../src/session.js';

// 最小配置
const config = {
  security: { maxConnsPerDevice: 1, maxNameLen: 32, maxSkinLen: 64 },
  room: { capacity: 8, maxRooms: 100, roomIdLength: 6, joinCodeLength: 4, emptyTtlMs: 300000, idleTimeoutMs: 30000 },
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

test('创建房间返回 ownerToken + joinCode', () => {
  const { session, sent } = newClient();
  say(session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'Alice' });
  say(session, { type: 'create_room', roomName: 'friends', capacity: 8 });
  const r = sent.at(-1);
  assert.equal(r.type, 'room_created');
  assert.ok(r.roomId && r.ownerToken && r.joinCode);
  assert.equal(session.roomId, r.roomId);
});

test('加入：错误邀请码拒绝，正确邀请码加入并广播 peer_joined', () => {
  const a = newClient();
  say(a.session, { type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'Alice' });
  say(a.session, { type: 'create_room' });
  const created = a.sent.at(-1);

  // B 用错误码
  const b = newClient();
  say(b.session, { type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'Bob' });
  say(b.session, { type: 'join_room', roomId: created.roomId, joinCode: 'wrong' });
  assert.equal(b.sent.at(-1).code, 'bad_join_code');

  // B 用正确码
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
  say(a.session, { type: 'create_room' });
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
  say(a.session, { type: 'create_room' });
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
  say(a.session, { type: 'create_room' });
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
  say(a.session, { type: 'create_room' });
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
