#ifndef GAME_H
#define GAME_H
#include <stdint.h>
typedef struct {
  float x, y, vx, vy, p[2];
  uint8_t input[2], score[2];
  uint32_t tick;
} Game;
void game_init(Game *g);
int game_step(Game *g, float dt);
#endif
