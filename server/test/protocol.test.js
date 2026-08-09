// protocol.test.js — 字段校验与限流相关（node:test）
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { validate, MSG } from '../src/protocol.js';
import { config } from '../src/config.js';

const cfg = config;

test('hello 合法', () => {
  assert.deepEqual(validate({ type: MSG.HELLO, clientId: 'A', deviceId: 'dev1', displayName: 'Tata' }, cfg), { ok: true });
});
test('hello 缺 clientId 拒绝', () => {
  assert.equal(validate({ type: MSG.HELLO, deviceId: 'dev1' }, cfg).ok, false);
});
test('create_room 默认合法', () => {
  assert.equal(validate({ type: MSG.CREATE_ROOM, roomName: 'friends' }, cfg).ok, true);
});
test('create_room capacity 非法', () => {
  assert.equal(validate({ type: MSG.CREATE_ROOM, capacity: 1 }, cfg).ok, false);
  assert.equal(validate({ type: MSG.CREATE_ROOM, capacity: 99 }, cfg).ok, false);
});
test('join_room 缺 roomId 拒绝', () => {
  assert.equal(validate({ type: MSG.JOIN_ROOM, joinCode: 'x' }, cfg).ok, false);
});
test('skin 白名单', () => {
  assert.equal(validate({ type: MSG.SKIN_UPDATE, roleId: 'A/0', skin: 'hollowknight.png' }, cfg).ok, true);
  assert.equal(validate({ type: MSG.SKIN_UPDATE, roleId: 'A/0', skin: '../evil.png' }, cfg).ok, false);
});
test('mouse 合法/非法', () => {
  assert.equal(validate({ type: MSG.MOUSE, roleId: 'A/0', dx: 1.5, dy: -2, eye: 0 }, cfg).ok, true);
  assert.equal(validate({ type: MSG.MOUSE, roleId: 'A/0', dx: 'x', dy: 0 }, cfg).ok, false);
  assert.equal(validate({ type: MSG.MOUSE, roleId: 'A/0', dx: 0, dy: 0, eye: 9 }, cfg).ok, false);
});
test('emoticon index 范围 0..15', () => {
  assert.equal(validate({ type: MSG.EMOTICON, roleId: 'A/0', index: 0 }, cfg).ok, true);
  assert.equal(validate({ type: MSG.EMOTICON, roleId: 'A/0', index: 15 }, cfg).ok, true);
  assert.equal(validate({ type: MSG.EMOTICON, roleId: 'A/0', index: 16 }, cfg).ok, false);
});
test('未知类型拒绝', () => {
  assert.equal(validate({ type: 'nope' }, cfg).ok, false);
});
test('kick_member 缺 ownerToken 拒绝', () => {
  assert.equal(validate({ type: MSG.KICK_MEMBER, targetClientId: 'B' }, cfg).ok, false);
});
