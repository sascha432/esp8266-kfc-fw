//
// Author: sascha_lammers@gmx.de
//
// Forward UDP packets on port 5001 to all clients connected to the websocket ws://<HOST>:5002/
// empty packets are ignored
//

const HOST = '192.168.0.3';
const UDP_PORT = 5001;
const WS_PORT = 5002;

const WebSocket = require('ws');

const wss = new WebSocket.Server({
  host: HOST,
  port: WS_PORT,
  perMessageDeflate: {
    zlibDeflateOptions: {
      chunkSize: 1024,
      memLevel: 7,
      level: 3
    },
    zlibInflateOptions: {
      chunkSize: 10 * 1024
    },
    clientNoContextTakeover: true,
    serverNoContextTakeover: true,
    serverMaxWindowBits: 10,
    concurrencyLimit: 10,
    threshold: 256
	}
});

const dgram = require('dgram');
const server = dgram.createSocket('udp4');

wss.on('listening', function() {
    const o = wss.options;
    console.log(`websocket ws://${o.host}:${o.port}/`);
});

function clients() {
    var count = 0;
    wss.clients.forEach(function each(client) {
        if (client.readyState === WebSocket.OPEN) {
            count++;
        }
    });
    return count;
}

function send_all(msg) {
    wss.clients.forEach(function each(client) {
        if (client.readyState === WebSocket.OPEN) {
            client.send(msg);
        }
    });
}

wss.on('connection', function(ws) {
    console.log('added client', clients());
    ws.on('close', function(ws) {
        console.log('removed client ', clients());
    });

});

server.on('error', (err) => {
    console.log(`server error:\n${err.stack}`);
    server.close();
});

server.on('message', (msg, rinfo) => {
    if (msg.length) {
        send_all(msg);
    }
});

server.on('listening', () => {
    const address = server.address();
    console.log(`server listening on UDP ${address.address}:${address.port}`);
});

server.bind(UDP_PORT);
