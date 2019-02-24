const http = require("http");
const fs = require("fs");
const net = require("net");
const path = require("path");
const os = require("os");
const WebSocket = require("ws");
const replaceStream = require("replacestream");

const web_port = 8080;
const wss_port = 9000;
const socket_path = "/tmp/pong.sock";
const host_ip = os.networkInterfaces()["wlp2s0"][0].address; // WARNING: very specific to my laptop
console.log("Host ip: " + host_ip);
let should_start = 0;

const default_angle = format_angle(30);
const p1 = {
  ip: undefined,
  angle: default_angle,
  score: 0,
};
const p2 = {
  ip: undefined,
  angle: default_angle,
  score: 0,
};

// formats a number be 2 characters wide
function format_angle(num) {
  return ("0" + num).slice(-2);
}

// create a UNIX domain socket to communicate with the game
socket_server = net
  .createServer(c => {
    console.log("Game connected");

    // collect the scores, send back phone angles
    c.on("data", data => {
      const [p1_score, p2_score] = data.toString().split(",");
      if (p1_score != p1.score) {
        p1.score = p1_score;
      }
      if (p2_score != p2.score) {
        p2.score = p2_score;
      }
      if (should_start) {
        c.write("s");
        should_start = 0;
      }

      c.write(`p:${p1.angle},${p2.angle}`);
    });

    c.on("end", () => {
      console.log("Game disconnected");
    });

    // shutdown gracefully on errors
    c.on("error", () => {
      console.log("Socket error, exiting");
      socket_server.close();
      process.exit();
    });
  })
  .listen(socket_path);

// handle SIGINT to clean up socket file
process.on("SIGINT", () => {
  socket_server.close();
  process.exit();
});

// create web server to send client code
http
  .createServer((_, res) => {
    res.writeHead(200, { "Content-Type": "text/html" });
    fs.createReadStream(path.join(__dirname, "index.html"))
      .pipe(replaceStream("HOST_IP", host_ip)) // put the correct host IP in script section
      .pipe(res);
  })
  .listen(web_port);

// create web socket server for clients to update angles on
wss_server = new WebSocket.Server({ port: wss_port }).on("connection", (ws, req) => {
  const ip = req.connection.remoteAddress;

  // register player
  if (p1.ip === undefined) {
    p1.ip = ip;
    console.log("Player 1 connected");
    ws.send(JSON.stringify({ type: "player_num", msg: 1 }));
  } else if (p2.ip === undefined) {
    p2.ip = ip;
    console.log("Player 2 connected");
    ws.send(JSON.stringify({ type: "player_num", msg: 2 }));
  } else {
    // if a 3rd player is trying to connect, immediately close the connection
    ws.close(1013); // Try Again Later code
  }

  // update angles on messages, send back score
  ws.on("message", msg => {
    const player = ip === p1.ip ? p1 : p2;
    const data = JSON.parse(msg);
    switch (data.type) {
      case "start":
        should_start = 1;
      case "angle":
        player.angle = format_angle(data.msg);
    }

    ws.send(JSON.stringify({ type: "score", msg: player.score }));
  });

  // handle players ending wss connection
  ws.on("close", () => {
    let player, p_num;
    if (ip === p1.ip) {
      player = p1;
      p_num = 1;
    } else {
      player = p2;
      p_num = 2;
    }

    player.ip = undefined;
    player.angle = default_angle;
    player.score = 0;
    console.log(`Player ${p_num} disconnected`);
  });
});
