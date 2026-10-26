/*
 * protocol.c — Implementación de MyAppGameProtocol (PGP v2).
 * Serialización binaria explícita byte a byte (no se envían structs crudos,
 * así se evitan problemas de padding y de endianness entre plataformas).
 */
#include "protocol.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Orden de bytes de red (big-endian)                                  */
/* ------------------------------------------------------------------ */
static void put_u16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)v;
}

static void put_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static uint16_t get_u16(const uint8_t *p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t get_u32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Cadena: [len u8][bytes]. maxlen limita len. */
static int put_str(uint8_t *p, size_t cap, size_t *off, const char *s,
                   size_t maxlen) {
  size_t n = strlen(s);
  if (n > maxlen || *off + 1 + n > cap)
    return -1;
  p[(*off)++] = (uint8_t)n;
  memcpy(p + *off, s, n);
  *off += n;
  return 0;
}

static int get_str(const uint8_t *p, size_t len, size_t *off, char *out,
                   size_t maxlen) {
  if (*off + 1 > len)
    return -1;
  size_t n = p[(*off)++];
  if (n > maxlen || *off + n > len)
    return -1;
  for (size_t i = 0; i < n; i++) {
    uint8_t ch = p[*off + i];
    if (ch < 0x20 || ch > 0x7E) /* solo ASCII imprimible */
      return -1;
  }
  memcpy(out, p + *off, n);
  out[n] = '\0';
  *off += n;
  return 0;
}

/* ------------------------------------------------------------------ */
/* Nombres legibles                                                    */
/* ------------------------------------------------------------------ */
const char *proto_type_name(uint8_t t) {
  switch (t) {
  case MSG_REGISTER:
    return "REGISTER";
  case MSG_JOIN_QUEUE:
    return "JOIN_QUEUE";
  case MSG_PADDLE_INPUT:
    return "PADDLE_INPUT";
  case MSG_LEAVE_MATCH:
    return "LEAVE_MATCH";
  case MSG_PING:
    return "PING";
  case MSG_QUIT:
    return "QUIT";
  case MSG_LOGIN:
    return "LOGIN";
  case MSG_LIST:
    return "LIST_USERS";
  case MSG_CANCEL:
    return "CANCEL_QUEUE";
  case MSG_USER:
    return "USER";
  case MSG_USERS_BEGIN:
    return "USERS_BEGIN";
  case MSG_USERS_END:
    return "USERS_END";
  case MSG_REGISTER_OK:
    return "REGISTER_OK";
  case MSG_QUEUED:
    return "QUEUED";
  case MSG_MATCH_START:
    return "MATCH_START";
  case MSG_GAME_STATE:
    return "GAME_STATE";
  case MSG_SCORE:
    return "SCORE";
  case MSG_MATCH_END:
    return "MATCH_END";
  case MSG_PONG:
    return "PONG";
  case MSG_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

const char *proto_error_name(uint8_t c) {
  switch (c) {
  case ERR_BAD_HEADER:
    return "Cabecera invalida (magic/version)";
  case ERR_UNKNOWN_TYPE:
    return "Tipo de mensaje desconocido";
  case ERR_MALFORMED:
    return "Mensaje mal formado";
  case ERR_INVALID_FIELD:
    return "Nick o email con formato invalido";
  case ERR_NOT_REGISTERED:
    return "Debe registrarse primero";
  case ERR_ALREADY_REGISTERED:
    return "Ya esta registrado";
  case ERR_NICK_TAKEN:
    return "El nick ya esta en uso";
  case ERR_INVALID_STATE:
    return "Mensaje no valido en el estado actual";
  case ERR_SERVER_FULL:
    return "Servidor lleno";
  case ERR_INTERNAL:
    return "Error interno del servidor";
  default:
    return "Error desconocido";
  }
}

const char *proto_dir_name(uint8_t d) {
  switch (d) {
  case DIR_STOP:
    return "STOP";
  case DIR_UP:
    return "UP";
  case DIR_DOWN:
    return "DOWN";
  default:
    return "?";
  }
}

const char *proto_end_reason_name(uint8_t r) {
  switch (r) {
  case END_SCORE_LIMIT:
    return "limite de puntos";
  case END_OPPONENT_LEFT:
    return "abandono/desconexion";
  case END_SERVER_ERROR:
    return "error del servidor";
  default:
    return "?";
  }
}

/* ------------------------------------------------------------------ */
/* Cabecera                                                            */
/* ------------------------------------------------------------------ */
void proto_encode_header(uint8_t out[PROTO_HEADER_SIZE],
                         const msg_header_t *h) {
  out[0] = h->magic;
  out[1] = h->version;
  out[2] = h->type;
  out[3] = h->flags;
  put_u16(out + 4, h->seq);
  put_u16(out + 6, h->length);
}

int proto_decode_header(const uint8_t in[PROTO_HEADER_SIZE], msg_header_t *h) {
  h->magic = in[0];
  h->version = in[1];
  h->type = in[2];
  h->flags = in[3];
  h->seq = get_u16(in + 4);
  h->length = get_u16(in + 6);
  if (h->magic != PROTO_MAGIC || h->version != PROTO_VERSION)
    return -1;
  return 0;
}

int proto_enc_register(uint8_t *p, size_t cap, const msg_register_t *m) {
  size_t off = 0;
  if (put_str(p, cap, &off, m->nick, PROTO_MAX_NICK) < 0 ||
      put_str(p, cap, &off, m->email, PROTO_MAX_EMAIL) < 0)
    return -1;
  return (int)off;
}

int proto_dec_register(const uint8_t *p, size_t len, msg_register_t *m) {
  size_t off = 0;
  if (get_str(p, len, &off, m->nick, PROTO_MAX_NICK) < 0 ||
      get_str(p, len, &off, m->email, PROTO_MAX_EMAIL) < 0 || off != len)
    return -1;
  return 0;
}

/* REGISTER_OK / PING / PONG: [u32] */
int proto_enc_u32(uint8_t *p, size_t cap, uint32_t v) {
  if (cap < 4)
    return -1;
  put_u32(p, v);
  return 4;
}

int proto_dec_u32(const uint8_t *p, size_t len, uint32_t *v) {
  if (len != 4)
    return -1;
  *v = get_u32(p);
  return 0;
}

/* PADDLE_INPUT: [dir u8] */
int proto_enc_paddle_input(uint8_t *p, size_t cap, uint8_t dir) {
  if (cap < 1 || dir > DIR_DOWN)
    return -1;
  p[0] = dir;
  return 1;
}

int proto_dec_paddle_input(const uint8_t *p, size_t len, uint8_t *dir) {
  if (len != 1 || p[0] > DIR_DOWN)
    return -1;
  *dir = p[0];
  return 0;
}

/* MATCH_START: [match_id u32][side u8][win_score u8][field_w u16][field_h u16]
 *              [paddle_w u16][paddle_h u16][ball_size u16][opp_len
 * u8][opponent] */
#define MATCH_START_FIXED 16
int proto_enc_match_start(uint8_t *p, size_t cap, const msg_match_start_t *m) {
  if (cap < MATCH_START_FIXED)
    return -1;
  put_u32(p, m->match_id);
  p[4] = m->side;
  p[5] = m->win_score;
  put_u16(p + 6, m->field_w);
  put_u16(p + 8, m->field_h);
  put_u16(p + 10, m->paddle_w);
  put_u16(p + 12, m->paddle_h);
  put_u16(p + 14, m->ball_size);
  size_t off = MATCH_START_FIXED;
  if (put_str(p, cap, &off, m->opponent, PROTO_MAX_NICK) < 0)
    return -1;
  return (int)off;
}

int proto_dec_match_start(const uint8_t *p, size_t len, msg_match_start_t *m) {
  if (len < MATCH_START_FIXED + 1)
    return -1;
  m->match_id = get_u32(p);
  m->side = p[4];
  m->win_score = p[5];
  m->field_w = get_u16(p + 6);
  m->field_h = get_u16(p + 8);
  m->paddle_w = get_u16(p + 10);
  m->paddle_h = get_u16(p + 12);
  m->ball_size = get_u16(p + 14);
  size_t off = MATCH_START_FIXED;
  if (get_str(p, len, &off, m->opponent, PROTO_MAX_NICK) < 0 || off != len)
    return -1;
  if (m->side > SIDE_RIGHT)
    return -1;
  return 0;
}

/* GAME_STATE (14 bytes): [tick u32][ball_x u16][ball_y u16]
 *                        [paddle_l u16][paddle_r u16][score_l u8][score_r u8]
 */
#define GAME_STATE_SIZE 14
int proto_enc_game_state(uint8_t *p, size_t cap, const msg_game_state_t *m) {
  if (cap < GAME_STATE_SIZE)
    return -1;
  put_u32(p, m->tick);
  put_u16(p + 4, m->ball_x);
  put_u16(p + 6, m->ball_y);
  put_u16(p + 8, m->paddle_y[0]);
  put_u16(p + 10, m->paddle_y[1]);
  p[12] = m->score[0];
  p[13] = m->score[1];
  return GAME_STATE_SIZE;
}

int proto_dec_game_state(const uint8_t *p, size_t len, msg_game_state_t *m) {
  if (len != GAME_STATE_SIZE)
    return -1;
  m->tick = get_u32(p);
  m->ball_x = get_u16(p + 4);
  m->ball_y = get_u16(p + 6);
  m->paddle_y[0] = get_u16(p + 8);
  m->paddle_y[1] = get_u16(p + 10);
  m->score[0] = p[12];
  m->score[1] = p[13];
  return 0;
}

/* SCORE (3 bytes): [scorer u8][score_l u8][score_r u8] */
int proto_enc_score(uint8_t *p, size_t cap, const msg_score_t *m) {
  if (cap < 3)
    return -1;
  p[0] = m->scorer;
  p[1] = m->score[0];
  p[2] = m->score[1];
  return 3;
}

int proto_dec_score(const uint8_t *p, size_t len, msg_score_t *m) {
  if (len != 3 || p[0] > SIDE_RIGHT)
    return -1;
  m->scorer = p[0];
  m->score[0] = p[1];
  m->score[1] = p[2];
  return 0;
}

/* MATCH_END (4 bytes): [reason u8][winner u8][score_l u8][score_r u8] */
int proto_enc_match_end(uint8_t *p, size_t cap, const msg_match_end_t *m) {
  if (cap < 4)
    return -1;
  p[0] = m->reason;
  p[1] = m->winner;
  p[2] = m->score[0];
  p[3] = m->score[1];
  return 4;
}

int proto_dec_match_end(const uint8_t *p, size_t len, msg_match_end_t *m) {
  if (len != 4)
    return -1;
  m->reason = p[0];
  m->winner = p[1];
  m->score[0] = p[2];
  m->score[1] = p[3];
  return 0;
}

/* ERROR: [code u8][text_len u8][text] */
int proto_enc_error(uint8_t *p, size_t cap, const msg_error_t *m) {
  if (cap < 1)
    return -1;
  p[0] = m->code;
  size_t off = 1;
  if (put_str(p, cap, &off, m->text, PROTO_MAX_ERRTEXT) < 0)
    return -1;
  return (int)off;
}

int proto_dec_error(const uint8_t *p, size_t len, msg_error_t *m) {
  if (len < 2)
    return -1;
  m->code = p[0];
  size_t off = 1;
  if (get_str(p, len, &off, m->text, PROTO_MAX_ERRTEXT) < 0 || off != len)
    return -1;
  return 0;
}
