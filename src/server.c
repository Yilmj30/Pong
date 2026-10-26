#define _POSIX_C_SOURCE 200809L
#include "app.h"
#include "protocol.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#define BUFFER 65536
typedef struct {
  int fd, bad, attempts;
  char peer[64]; /* "ip:puerto" del cliente */
  uint16_t seq;
  size_t used, out;
  unsigned char rx[2048], tx[BUFFER];
  double last, blocked;
} Connection;
typedef struct {
  App app;
  Connection con[CAP];
  FILE *log;
  int listener;
} Server;
static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}
static void logline(Server *s, const char *fmt, ...) {
  char msg[512], stamp[40];
  time_t t = time(NULL);
  struct tm tm;
  localtime_r(&t, &tm);
  strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &tm);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  fprintf(stdout, "%s %s\n", stamp, msg);
  fprintf(s->log, "%s %s\n", stamp, msg);
  fflush(s->log);
  fflush(stdout);
}
static void packet(Server *s, int i, int type, const uint8_t *p, int n) {
  Connection *c = &s->con[i];
  if (c->fd < 0 || c->bad)
    return;
  if (c->out + 8 + (size_t)n > BUFFER) {
    c->bad = 1;
    return;
  }
  msg_header_t h = {PROTO_MAGIC, PROTO_VERSION, (uint8_t)type,
                    0,           c->seq++,      (uint16_t)n};
  proto_encode_header(c->tx + c->out, &h);
  if (n)
    memcpy(c->tx + c->out + 8, p, (size_t)n);
  if (!c->out)
    c->blocked = now();
  c->out += 8 + (size_t)n;
  logline(s, "SEND client=%d type=0x%02x %s seq=%u len=%d", i, type,
          proto_type_name((uint8_t)type), h.seq, n);
}
/* Limitar coordenadas enviadas para evitar desbordamiento */
static uint16_t coordinate(float value) {
  if (value < 0)
    return 0;
  if (value > 65535)
    return 65535;
  return (uint16_t)value;
}
static void event(void *ctx, const Event *e) {
  Server *s = ctx;
  if (e->kind == EV_ERROR)
    logline(s, "ERROR client=%d code=%d text=%s", e->to, e->code, e->text);
  if (e->kind == EV_START)
    logline(s, "MATCH_START client=%d match=%u side=%d opponent=%s", e->to,
            e->id, e->side, e->text);
  if (e->kind == EV_SCORE || e->kind == EV_END)
    logline(s, "GAME_EVENT client=%d kind=%d score=%u-%u winner=%d reason=%d",
            e->to, e->kind, e->game.score[0], e->game.score[1], e->winner,
            e->reason);
  uint8_t b[512];
  int n = 0, type = 0;
  switch (e->kind) {
  case EV_OK:
    logline(s, "AUTH_OK client=%d nick=%s player_id=%u", e->to,
            s->app.players[e->to].nick, e->id);
    type = MSG_REGISTER_OK;
    n = proto_enc_u32(b, sizeof b, e->id);
    break;
  case EV_ERROR: {
    msg_error_t m = {0};
    m.code = (uint8_t)e->code;
    snprintf(m.text, sizeof m.text, "%s", e->text);
    type = MSG_ERROR;
    n = proto_enc_error(b, sizeof b, &m);
    break;
  }
  case EV_QUEUED:
    type = MSG_QUEUED;
    break;
  case EV_START: {
    msg_match_start_t m = {.match_id = e->id,
                           .side = (uint8_t)e->side,
                           .win_score = 5,
                           .field_w = 800,
                           .field_h = 600,
                           .paddle_w = 10,
                           .paddle_h = 100,
                           .ball_size = 10};
    snprintf(m.opponent, sizeof m.opponent, "%.32s", e->text);
    type = MSG_MATCH_START;
    n = proto_enc_match_start(b, sizeof b, &m);
    break;
  }
  case EV_STATE: {
    msg_game_state_t m = {
        .tick = e->game.tick,
        .ball_x = coordinate(e->game.x),
        .ball_y = coordinate(e->game.y),
        .paddle_y = {coordinate(e->game.p[0]), coordinate(e->game.p[1])},
        .score = {e->game.score[0], e->game.score[1]}};
    type = MSG_GAME_STATE;
    n = proto_enc_game_state(b, sizeof b, &m);
    break;
  }
  case EV_SCORE: {
    msg_score_t m = {(uint8_t)e->side, {e->game.score[0], e->game.score[1]}};
    type = MSG_SCORE;
    n = proto_enc_score(b, sizeof b, &m);
    break;
  }
  case EV_END: {
    msg_match_end_t m = {(uint8_t)e->reason,
                         (uint8_t)e->winner,
                         {e->game.score[0], e->game.score[1]}};
    type = MSG_MATCH_END;
    n = proto_enc_match_end(b, sizeof b, &m);
    break;
  }
  case EV_USER_BEGIN:
    type = MSG_USERS_BEGIN;
    break;
  case EV_USER_END:
    type = MSG_USERS_END;
    break;
  case EV_USER:
    type = MSG_USER;
    n = proto_enc_u32(b, sizeof b, e->id);
    b[n++] = (uint8_t)e->status;
    b[n++] = (uint8_t)strlen(e->text);
    memcpy(b + n, e->text, strlen(e->text));
    n += (int)strlen(e->text);
    break;
  case EV_PONG:
    type = MSG_PONG;
    n = proto_enc_u32(b, sizeof b, e->id);
    break;
  }
  if (n >= 0)
    packet(s, e->to, type, b, n);
}
static void error(Server *s, int i, int code, const char *text) {
  Event e = {.kind = EV_ERROR, .to = i, .code = code};
  snprintf(e.text, sizeof e.text, "%s", text);
  event(s, &e);
}
static int string(const uint8_t *p, size_t n, size_t *off, char *out,
                  size_t max) {
  if (*off >= n)
    return -1;
  size_t len = p[(*off)++];
  if (!len || len > max || *off + len > n)
    return -1;
  for (size_t k = 0; k < len; k++)
    if (p[*off + k] < 33 || p[*off + k] > 126)
      return -1;
  memcpy(out, p + *off, len);
  out[len] = 0;
  *off += len;
  return 0;
}
static void dispatch(Server *s, int i, msg_header_t h, const uint8_t *p) {
  uint8_t dir;
  uint32_t ts;
  switch (h.type) {
  case MSG_LOGIN: {
    char nick[33] = {0}, email[65] = {0}, token[65] = {0};
    size_t off = 0;
    if (string(p, h.length, &off, nick, 32) ||
        string(p, h.length, &off, email, 64) ||
        string(p, h.length, &off, token, 64) || off != h.length)
      goto malformed;
    if (++s->con[i].attempts > 10) {
      s->con[i].bad = 1;
      return;
    }
    logline(s, "LOGIN client=%d nick=%s", i, nick); /* el token no se registra */
    app_login(&s->app, i, nick, email, token);
    break;
  }
  case MSG_REGISTER: {
    char nick[33] = {0}, email[65] = {0};
    size_t off = 0;
    if (string(p, h.length, &off, nick, 32) ||
        string(p, h.length, &off, email, 64) || off != h.length)
      goto malformed;
    logline(s, "REGISTER client=%d nick=%s email=%s", i, nick, email);
    app_register(&s->app, i, nick, email);
    break;
  }
  case MSG_JOIN_QUEUE:
    if (h.length)
      goto malformed;
    app_join(&s->app, i);
    break;
  case MSG_LIST:
    if (h.length)
      goto malformed;
    app_list(&s->app, i);
    break;
  case MSG_CANCEL:
    if (h.length)
      goto malformed;
    app_cancel(&s->app, i);
    break;
  case MSG_PADDLE_INPUT:
    if (proto_dec_paddle_input(p, h.length, &dir))
      goto malformed;
    logline(s, "INPUT client=%d direction=%u", i, dir);
    app_input(&s->app, i, dir);
    break;
  case MSG_LEAVE_MATCH:
    if (h.length)
      goto malformed;
    app_leave(&s->app, i);
    break;
  case MSG_PING:
    if (proto_dec_u32(p, h.length, &ts))
      goto malformed;
    event(s, &(Event){.kind = EV_PONG, .to = i, .id = ts});
    break;
  case MSG_QUIT:
    if (h.length)
      goto malformed;
    s->con[i].bad = 1;
    break;
  default:
    error(s, i, 2, "Tipo desconocido");
    break;
  }
  return;
malformed:
  error(s, i, 3, "Payload invalido");
}
static void read_client(Server *s, int i) {
  Connection *c = &s->con[i];
  ssize_t n = recv(c->fd, c->rx + c->used, sizeof c->rx - c->used, 0);
  if (n <= 0) {
    if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
      c->bad = 1;
    return;
  }
  c->used += (size_t)n;
  c->last = now();
  while (c->used >= 8 && !c->bad) {
    msg_header_t h;
    if (proto_decode_header(c->rx, &h) || h.flags || h.length > 512) {
      c->bad = 1;
      logline(s, "BAD_HEADER client=%d", i);
      return;
    }
    size_t total = 8 + h.length;
    if (c->used < total)
      break;
    logline(s, "RECV client=%d type=0x%02x %s seq=%u len=%u", i, h.type,
            proto_type_name(h.type), h.seq,
            h.length);
    dispatch(s, i, h, c->rx + 8);
    memmove(c->rx, c->rx + total, c->used - total);
    c->used -= total;
  }
}
static void write_client(Server *s, int i) {
  Connection *c = &s->con[i];
  if (!c->out)
    return;
  ssize_t n = send(c->fd, c->tx, c->out, 0);
  if (n > 0) {
    memmove(c->tx, c->tx + n, c->out - (size_t)n);
    c->out -= (size_t)n;
  } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
    c->bad = 1;
}
int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Uso: %s <PORT> <LogFile>\n", argv[0]);
    return 1;
  }
  char *end;
  long port = strtol(argv[1], &end, 10);
  if (*end || port < 1 || port > 65535)
    return 1;
  Server *s = calloc(1, sizeof *s);
  if (!s)
    return 1;
  s->log = fopen(argv[2], "a");
  if (!s->log) {
    perror("log");
    free(s);
    return 1;
  }
  app_init(&s->app, event, s);
  const char *users = getenv("PONG_USERS_FILE");
  if (!users)
    users = "users.conf";
  int accounts = app_accounts(&s->app, users);
  if (accounts < 0) {
    fprintf(stderr, "Formato invalido en %s (filas: nick correo token)\n",
            users);
    fclose(s->log);
    free(s);
    return 1;
  }
  for (int i = 0; i < CAP; i++)
    s->con[i].fd = -1;
  signal(SIGPIPE, SIG_IGN);
  sigset_t stops;
  sigemptyset(&stops);
  sigaddset(&stops, SIGINT);
  sigaddset(&stops, SIGTERM);
  sigprocmask(SIG_BLOCK, &stops, NULL);
  s->listener = socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  setsockopt(s->listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons((uint16_t)port),
                             .sin_addr = {.s_addr = htonl(INADDR_ANY)}};
  if (bind(s->listener, (struct sockaddr *)&addr, sizeof addr) ||
      listen(s->listener, 32)) {
    perror("listen/bind");
    return 1;
  }
  fcntl(s->listener, F_SETFL, O_NONBLOCK);
  logline(s, "READY TCP port=%ld log=%s cuentas_con_token=%d%s", port,
          argv[2], s->app.account_count,
          accounts == 1 ? " (sin users.conf: solo registro abierto)" : "");
  double next = now();
  for (;;) {
    sigset_t pending;
    sigpending(&pending);
    if (sigismember(&pending, SIGTERM) || sigismember(&pending, SIGINT))
      break;
    struct pollfd fds[CAP + 1];
    fds[0] = (struct pollfd){s->listener, POLLIN, 0};
    for (int i = 0; i < CAP; i++)
      fds[i + 1] = (struct pollfd){
          s->con[i].fd, (short)(POLLIN | (s->con[i].out ? POLLOUT : 0)), 0};
    int rc = poll(fds, CAP + 1, 10);
    if (rc < 0 && errno != EINTR)
      break;
    if (fds[0].revents & POLLIN) {
      struct sockaddr_in ca;
      socklen_t cl = sizeof ca;
      int fd = accept(s->listener, (struct sockaddr *)&ca, &cl);
      if (fd >= 0) {
        int i;
        for (i = 0; i < CAP && s->con[i].fd >= 0; i++) {
        }
        if (i == CAP) {
          logline(s, "REJECT servidor lleno (%d conexiones)", CAP);
          close(fd);
        }
        else {
          fcntl(fd, F_SETFL, O_NONBLOCK);
          setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
          memset(&s->con[i], 0, sizeof(Connection));
          s->con[i].fd = fd;
          s->con[i].last = now();
          app_connect(&s->app, i);
          char ip[INET_ADDRSTRLEN] = "?";
          inet_ntop(AF_INET, &ca.sin_addr, ip, sizeof ip);
          snprintf(s->con[i].peer, sizeof s->con[i].peer, "%s:%u", ip,
                   ntohs(ca.sin_port));
          logline(s, "CONNECT client=%d from=%s", i, s->con[i].peer);
        }
      }
    }
    for (int i = 0; i < CAP; i++) {
      Connection *c = &s->con[i];
      if (c->fd < 0)
        continue;
      short ev = fds[i + 1].revents;
      if (ev & POLLIN)
        read_client(s, i);
      if (ev & POLLOUT)
        write_client(s, i);
      if (ev & (POLLERR | POLLHUP | POLLNVAL))
        c->bad = 1;
      if (now() - c->last > 60 || (c->out && now() - c->blocked > 5))
        c->bad = 1;
      if (c->bad) {
        close(c->fd);
        c->fd = -1;
        app_disconnect(&s->app, i);
        logline(s, "DISCONNECT client=%d from=%s", i, c->peer);
      }
    }
    double t = now();
    if (t >= next) {
      app_tick(&s->app, 1.0f / 30);
      next += 1.0 / 30;
      if (t - next > 0.1)
        next = t + 1.0 / 30;
    }
  }
  for (int m = 0; m < CAP / 2; m++)
    if (s->app.matches[m].active) {
      Match *q = &s->app.matches[m];
      for (int k = 0; k < 2; k++)
        event(s, &(Event){.kind = EV_END,
                          .to = q->players[k],
                          .reason = 2,
                          .winner = 255,
                          .game = q->game});
    }
  for (int i = 0; i < CAP; i++)
    if (s->con[i].fd >= 0)
      write_client(s, i);
  logline(s, "STOP");
  close(s->listener);
  for (int i = 0; i < CAP; i++)
    if (s->con[i].fd >= 0)
      close(s->con[i].fd);
  fclose(s->log);
  free(s);
  return 0;
}
