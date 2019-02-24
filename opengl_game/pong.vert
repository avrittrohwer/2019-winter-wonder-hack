#version 150 // GLSL 150 = OpenGL 3.2

in vec3 in_Position;

uniform mat4 ProjectionView;
uniform mat4 Model;

void main() {
  gl_Position = ProjectionView * Model * vec4(in_Position.xyz, 1);
}
