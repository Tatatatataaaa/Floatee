// config.js — 默认配置 + config.json 覆盖 + 环境变量覆盖
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// 默认配置（与计划书 §5.3 一致）
export const DEFAULTS = {
  server: {
    host: '0.0.0.0',
    wsPort: 8765,
    tcpPort: 8764,
    adminPort: 8766,
    maxConnections: 1000,
  },
  room: {
    capacity: 8,
    maxRooms: 100,
    roomIdLength: 6,
    joinCodeLength: 4,
    emptyTtlMs: 300000,     // 空房 5 分钟销毁
    idleTimeoutMs: 30000,   // 连接 30s 无消息视为超时
  },
  throttle: {
    emoticonPerSec: 5,
    skinPerSec: 2,
    mousePerSec: 20,
  },
  security: {
    maxConnsPerDevice: 1,
    maxNameLen: 32,
    maxIdLen: 64,      // clientId/deviceId 上限（deviceId 为 36 字符 UUID）
    maxSkinLen: 64,
    maxChatLen: 256,
    maxMessageSize: 8192,
  },
  admin: {
    adminKey: '',
    allowRemote: false,
  },
};

function deepMerge(base, over) {
  if (!over || typeof over !== 'object') return base;
  const out = { ...base };
  for (const k of Object.keys(over)) {
    if (over[k] && typeof over[k] === 'object' && !Array.isArray(over[k]) && base[k] && typeof base[k] === 'object') {
      out[k] = deepMerge(base[k], over[k]);
    } else {
      out[k] = over[k];
    }
  }
  return out;
}

function loadConfig() {
  let config = DEFAULTS;
  // 尝试读取 config.json（server 目录）
  const cfgPath = path.join(__dirname, '..', 'config.json');
  if (fs.existsSync(cfgPath)) {
    try {
      const file = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
      config = deepMerge(DEFAULTS, file);
    } catch (err) {
      console.error('[config] 解析 config.json 失败，使用默认配置:', err.message);
    }
  }
  // 环境变量覆盖
  if (process.env.PORT) config.server.wsPort = parseInt(process.env.PORT, 10);
  if (process.env.TCP_PORT) config.server.tcpPort = parseInt(process.env.TCP_PORT, 10);
  if (process.env.ADMIN_PORT) config.server.adminPort = parseInt(process.env.ADMIN_PORT, 10);
  if (process.env.ADMIN_KEY) config.admin.adminKey = process.env.ADMIN_KEY;
  return config;
}

export const config = loadConfig();
