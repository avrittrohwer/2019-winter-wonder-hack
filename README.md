# 2019-winter-wonder-hack

This is a pong game with the graphics stack written in C using OpenGL and helper libraries at
located at https://gitlab.com/kuhl/opengl-examples. The paddles are controlled with devices that
have the `DeviceOrientationEvent` and `WebSocket` web APIs enabled.

The networking is orchestrated by a Node.js server which acts as a web server to which the devices
initially connect to and also as a Web Socket server which the devices subsequently communicate on.
The Node.js server communicates with the OpenGL process via a UNIX domain socket. The network usage
is very inefficient and unstable, but its a hackathon project after all.
