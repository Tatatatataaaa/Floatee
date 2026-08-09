// logger.js — 结构化日志（时间/级别/事件/上下文）
const LEVELS = { debug: 10, info: 20, warn: 30, error: 40 };
let level = LEVELS[process.env.LOG_LEVEL || 'info'] ?? LEVELS.info;

function ts() {
  return new Date().toISOString();
}

export const logger = {
  setLevel(l) {
    level = LEVELS[l] ?? LEVELS.info;
  },
  debug(evt, ctx = {}) {
    if (level <= LEVELS.debug) console.log(`${ts()} [debug] ${evt}`, ctx);
  },
  info(evt, ctx = {}) {
    if (level <= LEVELS.info) console.log(`${ts()} [info ] ${evt}`, ctx);
  },
  warn(evt, ctx = {}) {
    if (level <= LEVELS.warn) console.warn(`${ts()} [warn ] ${evt}`, ctx);
  },
  error(evt, ctx = {}) {
    if (level <= LEVELS.error) console.error(`${ts()} [error] ${evt}`, ctx);
  },
};
