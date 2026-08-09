// 验证：创建房间后 list_rooms 能列出（临时）
import net from 'node:net';

function client(label) {
  const s = net.createConnection({ host: '127.0.0.1', port: 8764 });
  let b = '';
  s.on('data', (c) => {
    b += c;
    let i;
    while ((i = b.indexOf('\n')) >= 0) {
      const l = b.slice(0, i).trim();
      b = b.slice(i + 1);
      if (l) console.log(`[${label}] <- ${l}`);
    }
  });
  return {
    s,
    say(o) { console.log(`[${label}] -> ${o.type}`); s.write(JSON.stringify(o) + '\n'); },
  };
}

const a = client('A');
a.s.on('connect', () => {
  a.say({ type: 'hello', clientId: 'A', deviceId: 'devA', displayName: 'Alice' });
  setTimeout(() => a.say({ type: 'create_room', roomName: 'friends' }), 200);
  setTimeout(() => {
    const p = client('probe');
    p.s.on('connect', () => {
      p.say({ type: 'hello', clientId: 'probe', deviceId: 'probe-dev' });
      setTimeout(() => p.say({ type: 'list_rooms' }), 200);
      setTimeout(() => { p.s.end(); a.s.end(); process.exit(0); }, 800);
    });
  }, 600);
});
