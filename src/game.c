#include "game.h"
#include <string.h>
void game_init(Game *g) {
  memset(g, 0, sizeof *g);
  g->x = 400;
  g->y = 300;
  g->vx = 300;
  g->vy = 150;
  g->p[0] = g->p[1] = 300;
}
int game_step(Game *g, float dt) {
  g->tick++;
  for (int i = 0; i < 2; i++) {
    g->p[i] += (g->input[i] == 1 ? -350 : g->input[i] == 2 ? 350 : 0) * dt;
    if (g->p[i] < 50)
      g->p[i] = 50;
    if (g->p[i] > 550)
      g->p[i] = 550;
  }
  /* Substeps prevent the ball from tunnelling through a paddle. */
  for (int k = 0; k < 4; k++) {
    float old = g->x;
    g->x += g->vx * dt / 4;
    g->y += g->vy * dt / 4;
    if (g->y < 5) {
      g->y = 10 - g->y;
      g->vy = -g->vy;
    }
    if (g->y > 595) {
      g->y = 1190 - g->y;
      g->vy = -g->vy;
    }
    int side = g->vx < 0 ? 0 : 1;
    float plane = side ? 770 : 30;
    if ((side ? (old <= plane && g->x >= plane)
              : (old >= plane && g->x <= plane)) &&
        g->y + 5 >= g->p[side] - 50 && g->y - 5 <= g->p[side] + 50) {
      g->x = plane;
      g->vx = -g->vx;
      g->vy = (g->y - g->p[side]) * 5;
    }
    if (g->x < -5 || g->x > 805) {
      int scorer = g->x < 0 ? 1 : 0;
      g->score[scorer]++;
      g->x = 400;
      g->y = 300;
      g->vx = scorer ? -300 : 300;
      g->vy = 150;
      return scorer;
    }
  }
  return -1;
}
