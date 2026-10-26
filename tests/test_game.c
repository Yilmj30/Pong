#include "game.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  Game g;
  game_init(&g);
  g.input[0] = 1;
  for (int i = 0; i < 100; i++)
    game_step(&g, 1.0f / 30);
  assert(g.p[0] == 50);
  g.input[0] = 2;
  for (int i = 0; i < 100; i++)
    game_step(&g, 1.0f / 30);
  assert(g.p[0] == 550);
  game_init(&g);
  g.x = 40;
  g.y = 300;
  g.vx = -300;
  game_step(&g, 0.05f);
  assert(g.vx > 0);
  g.y = 6;
  g.vy = -150;
  game_step(&g, 0.02f);
  assert(g.vy > 0);
  game_init(&g);
  g.x = 804;
  g.y = 10;
  assert(game_step(&g, 0.02f) == 0);
  assert(g.score[0] == 1 && g.x == 400);
  puts("PASS physics: limits, collision, wall, score, reset");
}
