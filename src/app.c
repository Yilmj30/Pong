#include "app.h"
#include <stdio.h>
#include <string.h>
static void emit(App *a, Event e) { a->emit(a->context, &e); }
static void error(App *a, int i, int code, const char *t) {
  Event e = {.kind = EV_ERROR, .to = i, .code = code};
  snprintf(e.text, sizeof e.text, "%s", t);
  emit(a, e);
}
void app_init(App *a, Emit cb, void *ctx) {
  memset(a, 0, sizeof *a);
  a->emit = cb;
  a->context = ctx;
  a->next_id = 1;
}
/* Devuelve 0 si cargo el archivo, 1 si no existe (solo registro abierto) y
 * -1 si el archivo tiene un formato invalido. */
int app_accounts(App *a, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 1;
  char line[256];
  while (fgets(line, sizeof line, f)) {
    Account x;
    char extra;
    if (line[0] == '#' || line[0] == '\n')
      continue;
    if (a->account_count == CAP || sscanf(line, "%32s %64s %64s %c", x.nick,
                                          x.email, x.token, &extra) != 3) {
      fclose(f);
      return -1;
    }
    for (int j = 0; j < a->account_count; j++)
      if (!strcmp(x.nick, a->accounts[j].nick)) {
        fclose(f);
        return -1;
      }
    a->accounts[a->account_count++] = x;
  }
  fclose(f);
  return 0;
}
void app_connect(App *a, int i) {
  memset(&a->players[i], 0, sizeof(Player));
  a->players[i].online = 1;
  a->players[i].match = -1;
}
void app_list(App *a, int i) {
  if (!a->players[i].state) {
    error(a, i, 5, "Autenticacion requerida");
    return;
  }
  emit(a, (Event){.kind = EV_USER_BEGIN, .to = i});
  for (int j = 0; j < CAP; j++)
    if (a->players[j].online && a->players[j].state) {
      Event e = {.kind = EV_USER,
                 .to = i,
                 .id = (uint32_t)j + 1,
                 .status = a->players[j].state};
      snprintf(e.text, sizeof e.text, "%s", a->players[j].nick);
      emit(a, e);
    }
  emit(a, (Event){.kind = EV_USER_END, .to = i});
}
static void presence(App *a) {
  for (int i = 0; i < CAP; i++)
    if (a->players[i].online && a->players[i].state)
      app_list(a, i);
}
static int same_secret(const char *x, const char *y) {
  unsigned diff = (unsigned)(strlen(x) ^ strlen(y));
  for (int i = 0; i < 64; i++)
    diff |= (unsigned char)x[i] ^ (unsigned char)y[i];
  return diff == 0;
}
/* Deja la sesion i autenticada con el perfil indicado. */
static void enter(App *a, int i, const char *nick, const char *email) {
  for (int j = 0; j < CAP; j++)
    if (j != i && a->players[j].online && a->players[j].state &&
        !strcmp(nick, a->players[j].nick)) {
      error(a, i, 7, "Usuario ya conectado");
      return;
    }
  Player *p = &a->players[i];
  snprintf(p->nick, sizeof p->nick, "%s", nick);
  snprintf(p->email, sizeof p->email, "%s", email);
  p->state = 1;
  emit(a, (Event){.kind = EV_OK, .to = i, .id = (uint32_t)i + 1});
  presence(a);
}
static int valid_nick(const char *s) {
  size_t n = strlen(s);
  if (n < 1 || n > 32)
    return 0;
  for (size_t k = 0; k < n; k++) {
    char c = s[k];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
      return 0;
  }
  return 1;
}
static int valid_email(const char *s) {
  const char *at = strchr(s, '@');
  size_t n = strlen(s);
  return n >= 3 && n <= 64 && at && at != s && at[1] && !strchr(at + 1, '@');
}
void app_register(App *a, int i, const char *nick, const char *email) {
  if (a->players[i].state) {
    error(a, i, 6, "Sesion ya autenticada");
    return;
  }
  if (!valid_nick(nick) || !valid_email(email)) {
    error(a, i, 4, "Nick (A-Z a-z 0-9 _ - .) o correo invalido");
    return;
  }
  for (int j = 0; j < a->account_count; j++) {
    Account *u = &a->accounts[j];
    if (strcmp(nick, u->nick))
      continue;
    if (u->token[0]) {
      error(a, i, 7, "Nick reservado: use LOGIN con su token");
      return;
    }
    if (strcmp(email, u->email)) {
      error(a, i, 7, "Nick ya registrado con otro correo");
      return;
    }
    enter(a, i, nick, email); /* el perfil ya existia: se retoma */
    return;
  }
  if (a->account_count == CAP) {
    error(a, i, 9, "No hay cupo para nuevos perfiles");
    return;
  }
  Account *u = &a->accounts[a->account_count];
  memset(u, 0, sizeof *u);
  snprintf(u->nick, sizeof u->nick, "%s", nick);
  snprintf(u->email, sizeof u->email, "%s", email);
  a->account_count++;
  enter(a, i, nick, email);
}
void app_login(App *a, int i, const char *nick, const char *email,
               const char *token) {
  if (a->players[i].state) {
    error(a, i, 6, "Sesion ya autenticada");
    return;
  }
  int ok = 0;
  char secret[65] = {0};
  snprintf(secret, sizeof secret, "%s", token);
  for (int j = 0; j < a->account_count; j++) {
    Account *u = &a->accounts[j];
    char expected[65] = {0};
    snprintf(expected, sizeof expected, "%s", u->token);
    /* Los perfiles creados con REGISTER no tienen token: nunca validan aqui. */
    if (u->token[0] && !strcmp(nick, u->nick) && !strcmp(email, u->email) &&
        same_secret(secret, expected))
      ok = 1;
  }
  if (!ok) {
    error(a, i, 11, "Credenciales incorrectas");
    return;
  }
  enter(a, i, nick, email);
}
static void finish(App *a, int m, int reason, int winner) {
  Match *q = &a->matches[m];
  for (int k = 0; k < 2; k++) {
    int i = q->players[k];
    a->players[i].state = a->players[i].online ? 1 : 0;
    a->players[i].match = -1;
    emit(a, (Event){.kind = EV_END,
                    .to = i,
                    .reason = reason,
                    .winner = winner,
                    .game = q->game});
  }
  q->active = 0;
}
void app_leave(App *a, int i) {
  int m = a->players[i].match;
  if (m < 0) {
    error(a, i, 8, "No esta en partida");
    return;
  }
  finish(a, m, 1, 1 - a->players[i].side);
  presence(a);
}
void app_disconnect(App *a, int i) {
  Player *p = &a->players[i];
  p->online = 0;
  if (p->match >= 0)
    finish(a, p->match, 1, 1 - p->side);
  p->state = 0;
  presence(a);
}
void app_cancel(App *a, int i) {
  if (a->players[i].state != 2) {
    error(a, i, 8, "No esta en cola");
    return;
  }
  a->players[i].state = 1;
  presence(a);
}
void app_join(App *a, int i) {
  Player *p = &a->players[i];
  if (p->state != 1) {
    error(a, i, p->state ? 8 : 5, "No disponible para otra partida");
    return;
  }
  p->state = 2;
  p->order = ++a->order;
  emit(a, (Event){.kind = EV_QUEUED, .to = i});
  int other = -1;
  for (int j = 0; j < CAP; j++)
    if (j != i && a->players[j].online && a->players[j].state == 2 &&
        (other < 0 || a->players[j].order < a->players[other].order))
      other = j;
  if (other >= 0) {
    int m;
    for (m = 0; m < CAP / 2 && a->matches[m].active; m++) {
    }
    if (m == CAP / 2) {
      p->state = 1;
      error(a, i, 9, "Servidor lleno");
      presence(a);
      return;
    }
    Match *q = &a->matches[m];
    memset(q, 0, sizeof *q);
    q->active = 1;
    q->id = a->next_id++;
    q->players[0] = other;
    q->players[1] = i;
    q->delay = 1;
    game_init(&q->game);
    for (int k = 0; k < 2; k++) {
      int id = q->players[k];
      a->players[id].state = 3;
      a->players[id].match = m;
      a->players[id].side = k;
      Event e = {.kind = EV_START, .to = id, .side = k, .id = q->id};
      snprintf(e.text, sizeof e.text, "%s", a->players[q->players[1 - k]].nick);
      emit(a, e);
    }
  }
  presence(a);
}
void app_input(App *a, int i, int dir) {
  Player *p = &a->players[i];
  if (p->match >= 0)
    a->matches[p->match].game.input[p->side] = (uint8_t)dir;
}
void app_tick(App *a, float dt) {
  int changed = 0;
  for (int m = 0; m < CAP / 2; m++) {
    Match *q = &a->matches[m];
    if (!q->active)
      continue;
    if (q->delay > 0) {
      q->delay -= dt;
      continue;
    }
    int point = game_step(&q->game, dt);
    for (int k = 0; k < 2; k++) {
      int to = q->players[k];
      if (point >= 0)
        emit(a,
             (Event){
                 .kind = EV_SCORE, .to = to, .side = point, .game = q->game});
      emit(a, (Event){.kind = EV_STATE, .to = to, .game = q->game});
    }
    if (q->game.score[0] >= 5 || q->game.score[1] >= 5) {
      finish(a, m, 0, q->game.score[0] >= 5 ? 0 : 1);
      changed = 1;
    }
  }
  if (changed)
    presence(a);
}
