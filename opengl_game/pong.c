#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <arpa/inet.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include "libkuhl.h"

#define PI 3.14159265358979323846
#define BALL_WIDTH 0.025
#define PADDLE_HEIGHT 0.15
#define PADDLE_WIDTH 0.025
#define MAX_DEFLECT 75
#define MAX_PHONE_ANGLE 75

GLuint shader;               // id of GLSL shader
kuhl_geometry left_paddle;   // left paddle geom
kuhl_geometry right_paddle;  // right paddle geom
kuhl_geometry ball;          // ball geom

float speed = 1;       // horizontal velocity of ball
int paused = 1;        // if the game is paused or not
int p1_angle = 30;     // angles of player 1, in [0, 90]
int p2_angle = 30;     // angles of player 2, in [0, 90]
double last_drawn_at;  // time of the last drawn

int p1_score = 0;  // player 1 score
int p2_score = 0;  // player 2 score

struct BallInfo {
  int direction;  // -1 for left, 1 for right
  float angle;    // angle the ball if following, in radians
  float x_pos;    // x position of top left corner
  float y_pos;    // y position of top left corner
} ball_info = {.direction = -1,
               .angle = 0,
               .x_pos = -(BALL_WIDTH) / 2,
               .y_pos = 0.5 + (BALL_WIDTH) / 2};

// resets the ball info values back to default, increments scores
void reset_ball(int direction) {
  if (direction == -1) {
    p2_score += 1;
  } else {
    p1_score += 1;
  }
  if (p1_score >= 10 || p2_score >= 10) {
    paused = 1;
    p1_score = p2_score = 0;
  }

  speed = 1;
  ball_info.direction = direction;
  ball_info.angle = drand48() * (PI / 2);
  ball_info.x_pos = -(BALL_WIDTH) / 2;
  ball_info.y_pos = 0.5 + (BALL_WIDTH) / 2;
}

// calculates if the ball collides with a paddle.
// if it does, the relative distance from paddle center is returned in range
// [-1, 1] and collides is set to 1, if it doesn't, collides is set to 0 and
// garbage is returned.
// lower_y is the y coordinate of the paddle
float paddle_collision(float lower_y, int* collides) {
  float upper_y = lower_y + (PADDLE_HEIGHT) + (BALL_WIDTH);

  if (ball_info.y_pos >= lower_y && ball_info.y_pos <= upper_y) {
    *collides = 1;
    float x = ((ball_info.y_pos - ((BALL_WIDTH) / 2)) -
               ((lower_y + (PADDLE_HEIGHT)) / 2)) /
              (0.5 * (PADDLE_HEIGHT));

    if (x < -1) {
      x = -1;
    }
    if (x > 1) {
      x = 1;
    }
    return x;
  } else {
    *collides = 0;
    return 0;
  }
}

// draws the scene
void display() {
  viewmat_begin_frame();

  // find the viewport size
  int viewport[4];
  viewmat_get_viewport(viewport, 0);
  glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

  // clear the scene
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // get view, projection matrix
  float view_mat[16], projection_mat[16];
  viewmat_get(view_mat, projection_mat, 0);

  // pre-multiply the view and projection matrices
  float projection_view_mat[16];
  mat4f_mult_mat4f_new(projection_view_mat, projection_mat, view_mat);

  // send projection view matrix to shader
  glUseProgram(shader);
  glUniformMatrix4fv(kuhl_get_uniform("ProjectionView"),
                     1,  // number of matrices
                     0,  // transpose
                     projection_view_mat);

  // calculate how far up the bottom of the paddles should be translated
  float model_mat[16];

  float left_paddle_translate =
      p1_angle * ((1 - (PADDLE_HEIGHT)) / (MAX_PHONE_ANGLE));
  float right_paddle_translate =
      p2_angle * ((1 - (PADDLE_HEIGHT)) / (MAX_PHONE_ANGLE));

  // make translate matrix for left paddle and draw
  mat4f_translate_new(model_mat, 0, left_paddle_translate, 0);
  glUniformMatrix4fv(kuhl_get_uniform("Model"),
                     1,  // number of matrices
                     0,  // transpose
                     model_mat);
  kuhl_geometry_draw(&left_paddle);

  // make translate matrix for right paddle and draw
  mat4f_translate_new(model_mat, 0, right_paddle_translate, 0);
  glUniformMatrix4fv(kuhl_get_uniform("Model"),
                     1,  // number of matrices
                     0,  // transpose
                     model_mat);
  kuhl_geometry_draw(&right_paddle);

  // get time since last frame
  float delta_time = (glfwGetTime() - last_drawn_at) / speed;

  // calc change in ball position
  float delta_x = (float)ball_info.direction * delta_time;
  float delta_y = (float)ball_info.direction * delta_x * tan(ball_info.angle);

  float next_x = ball_info.x_pos + delta_x;
  float next_y = ball_info.y_pos + delta_y;

  // check if ball should bounce off top or bottom
  if (next_y >= 1.0 || next_y <= (BALL_WIDTH)) {
    ball_info.angle = PI - ball_info.angle;
  }

  // check if the ball should bounce off left paddle
  if (((next_x - (BALL_WIDTH)) >= -1.0 &&
       (next_x - (BALL_WIDTH)) <= -1.0 + (PADDLE_WIDTH)) ||
      (ball_info.x_pos >= -1.0 + (PADDLE_WIDTH) && next_x <= -1.0)) {
    int collides;
    float angle_factor = paddle_collision(left_paddle_translate, &collides);
    if (collides) {
      speed -= 0.02;
      ball_info.direction = 1;
      ball_info.angle = angle_factor * MAX_DEFLECT;
    }
  }

  // check if the ball should bounce off right paddle
  if ((next_x >= 1.0 - (PADDLE_WIDTH) && next_x <= 1.0) ||
      (ball_info.x_pos <= 1.0 - (PADDLE_WIDTH) && next_x >= 1.0)) {
    int collides;
    float angle_factor = paddle_collision(right_paddle_translate, &collides);
    if (collides) {
      speed -= 0.02;
      ball_info.direction = -1;
      ball_info.angle = angle_factor * MAX_DEFLECT;
    }
  }

  ball_info.x_pos = next_x;
  ball_info.y_pos = next_y;

  // check if player 1 should lose a point
  if (next_x <= -1.3) {
    reset_ball(-1);
  }

  // check if player 2 should lose a point
  if (next_x >= 1.3) {
    reset_ball(1);
  }

  // create translate matrix, draw
  mat4f_translate_new(model_mat, ball_info.x_pos - ((BALL_WIDTH) / 2),
                      ball_info.y_pos - ((BALL_WIDTH) / 2), 0);
  glUniformMatrix4fv(kuhl_get_uniform("Model"),
                     1,  // number of matrices
                     0,  // transpose
                     model_mat);
  kuhl_geometry_draw(&ball);

  glUseProgram(0);
  viewmat_end_frame();
}

// initializes left paddle kuhl_geometry
// 0 +--+ 1
//   |  |
//   |  |
//   |  |
//   |  |
// 3 +--+ 2
void init_left_paddle() {
  // paddle is 4 points in a rectangle
  kuhl_geometry_new(&left_paddle, shader, 4, GL_TRIANGLES);

  float x_offset = -1 + (PADDLE_WIDTH);
  // clang-format off
  float vertex_data[] = {
    -1, PADDLE_HEIGHT, 0,
    x_offset, PADDLE_HEIGHT, 0,
    x_offset, 0, 0,
    -1, 0, 0
  };
  // clang-format on
  kuhl_geometry_attrib(&left_paddle, vertex_data, 3, "in_Position", 1);

  // clang-format off
  unsigned int index_data[] = {
    0, 1, 2,
    0, 2, 3
  };
  // clang-format on
  kuhl_geometry_indices(&left_paddle, index_data, 6);
}

// initializes right paddle kuhl_geometry
// 0 +--+ 1
//   |  |
//   |  |
//   |  |
//   |  |
// 3 +--+ 2
void init_right_paddle() {
  // paddle is 4 points in a rectangle
  kuhl_geometry_new(&right_paddle, shader, 4, GL_TRIANGLES);

  float x_offset = 1 - (PADDLE_WIDTH);
  // clang-format off
  float vertex_data[] = {
    x_offset, PADDLE_HEIGHT, 0,
    1, PADDLE_HEIGHT, 0,
    1, 0, 0,
    x_offset, 0, 0
  };
  // clang-format on
  kuhl_geometry_attrib(&right_paddle, vertex_data, 3, "in_Position", 1);

  // clang-format off
  unsigned int index_data[] = {
    0, 1, 2,
    0, 2, 3
  };
  // clang-format on
  kuhl_geometry_indices(&right_paddle, index_data, 6);
}

// initializes ball geometry so the ball is centered at the origin
// 0 +--+ 1
//   |  |
// 3 +--+ 2
void init_ball() {
  // ball is 4 points in a square
  kuhl_geometry_new(&ball, shader, 4, GL_TRIANGLES);

  float half_width = (BALL_WIDTH) / 2;
  // clang-format off
  float vertex_data[] = {
    -half_width, BALL_WIDTH, 0,
    half_width, BALL_WIDTH, 0,
    half_width, 0, 0,
    -half_width, 0, 0
  };
  // clang-format on
  kuhl_geometry_attrib(&ball, vertex_data, 3, "in_Position", 1);

  // clang-format off
  unsigned int index_data[] = {
    0, 1, 2,
    0, 2, 3
  };
  // clang-format on
  kuhl_geometry_indices(&ball, index_data, 6);
}

// handles keyboard input
void keyboard(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (action != GLFW_PRESS) return;

  switch (key) {
    case GLFW_KEY_SPACE:
      paused = !paused;
      break;
    case GLFW_KEY_Q:
    case GLFW_KEY_ESCAPE:
      glfwSetWindowShouldClose(window, GL_TRUE);
      break;
  }
}

int main(int argc, char** argv) {
  // init rand function
  srand48(0);

  int sock, bytes, len;
  struct sockaddr_un node_server;
  char msg[16];

  // create socket to communicate with node server
  if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    printf("error: could not create socket\n");
    exit(1);
  }

  // connect to unix socket file
  node_server.sun_family = AF_UNIX;
  strcpy(node_server.sun_path, "/tmp/pong.sock");
  len = strlen(node_server.sun_path) + sizeof(node_server.sun_family);
  if (connect(sock, (struct sockaddr*)&node_server, len) == -1) {
    printf("error: could not connect to /tmp/pong.sock\n");
    exit(1);
  }

  // init GLFW, GLEW
  kuhl_ogl_init(&argc, argv, 1024, 512, 32, 4);
  glfwSetKeyCallback(kuhl_get_window(), keyboard);

  // compile shader program
  shader = kuhl_create_program("pong.vert", "pong.frag");

  // init geometry
  init_left_paddle();
  init_right_paddle();
  init_ball();

  // init cam position
  float initCamPos[3] = {0, 0.5, 1};   // location of camera
  float initCamLook[3] = {0, 0.5, 0};  // a point the camera is facing at
  float initCamUp[3] = {0, 1, 0};  // a vector indicating which direction is up
  viewmat_init(initCamPos, initCamLook, initCamUp);

  // start game loop
  last_drawn_at = glfwGetTime();
  display();
  while (!glfwWindowShouldClose(kuhl_get_window())) {
    // send scores to node server
    sprintf(msg, "%d,%d", p1_score, p2_score);
    if (send(sock, msg, strlen(msg), 0) == -1) {
      printf("error: could not send message to socket\n");
      exit(1);
    }

    // get either a start message or phone angle message
    if ((bytes = recv(sock, msg, 16, 0)) > 0) {
      if (msg[0] == 's') {
        paused = 0;
      } else {
        msg[7] = '\0';
        sscanf(msg, "p:%d,%d", &p1_angle, &p2_angle);
      }
    } else {
      printf("error: could not receive message\n");
      exit(1);
    }

    if (paused) {
      // if paused, sleep for a little but and check in with node server to for
      // command to start
      useconds_t sleep_time = 2000;
      usleep(sleep_time);
    } else {
      // draw frame
      display();
      last_drawn_at = glfwGetTime();
    }

    glfwPollEvents();
  }

  exit(0);
}
