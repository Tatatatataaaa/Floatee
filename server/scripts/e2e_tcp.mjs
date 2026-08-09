// 临时端到端测试：TCP 端口（JSON 行协议）
// 用法: node scripts/e2e_tcp.mjs
import net from 'node:net';

const HOST = '127.0.0.1';
const PORT = 8764;

function client(label) {
  const socket = net.createConnection({ host: HOST, port: PORT });
  let buf = '';
  const sent = [];
  socket.on('data', (c) => {
    buf += c.toString('utf8');
    let i;
    while ((i = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, i).trim();
      buf = buf.slice(i + 1);
      if (line) {
        const msg = JSON.parse(line);
        console.log(`[${label}] ← ${msg.type}`, JSON.stringify(msg));
      }
    }
  });
  return {
    socket,
    say(obj) { console.log(`[${label}] → ${obj.type}`); socket.write(JSON.stringify(obj) + '\n'); },
    close() { socket.end(); },
  };
}

const a = client('A');
a.socket.on('connect', () => {
  a.say({ type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'Alice' });
  // create_room 在收到 welcome 后发送（用定时简单处理）
  setTimeout(() => {
    a.say({ type: 'create_room', roomName: 'test' });
  }, 300);
  setTimeout(() => {
    const b = client('B');
    b.socket.on('connect', () => {
      b.say({ type: 'hello', clientId: 'B', deviceId: 'devB', displayName: 'Bob' });
      setTimeout(() => {
        // 用 A 的 joinCode（这里 A 不知道，先直接 join 错误码验证）
        b.say({ type: 'join_room', roomId: 'XXXXXX', joinCode: 'wrong' });
      }, 300);
    });
  }, 800);
  // 5s 后结束
  setTimeout(() => process.exit(0), 5000);
});
