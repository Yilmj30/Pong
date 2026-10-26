#ifndef APP_H
#define APP_H
#include "game.h"
#define CAP 256
/* Eventos de dominio con valores logicos, desacoplados de la red */
typedef enum {
  EV_OK,
  EV_ERROR,
  EV_QUEUED,
  EV_START,
  EV_STATE,
  EV_SCORE,
  EV_END,
  EV_USER_BEGIN,
  EV_USER,
  EV_USER_END,
  EV_PONG
} EventKind;
typedef struct {
  EventKind kind;
  int to, code, side, winner, reason, status;
  uint32_t id;
  char text[129];
  Game game;
} Event;
typedef void (*Emit)(void *, const Event *);
typedef struct {
  int online, state, match, side;
  unsigned long order;
  char nick[33], email[65];
} Player;
typedef struct {
  int active, players[2];
  uint32_t id;
  float delay;
  Game game;
} Match;
typedef struct {
  char nick[33], email[65], token[65];
} Account;
typedef struct {
  Player players[CAP];
  Match matches[CAP / 2];
  Account accounts[CAP];
  int account_count;
  unsigned long order;
  uint32_t next_id;
  Emit emit;
  void *context;
} App;
void app_init(App *, Emit, void *);
int app_accounts(App *, const char *); /* 0 ok, 1 sin archivo, -1 invalido */
void app_connect(App *, int);
void app_disconnect(App *, int);
void app_register(App *, int, const char *, const char *);
void app_login(App *, int, const char *, const char *, const char *);
void app_list(App *, int);
void app_join(App *, int);
void app_cancel(App *, int);
void app_input(App *, int, int);
void app_leave(App *, int);
void app_tick(App *, float);
#endif
