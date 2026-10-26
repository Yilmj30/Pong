/*
 * protocol.h — MyAppGameProtocol (PGP: Pong Game Protocol), versión 2.
 *
 * Protocolo de capa de aplicación, codificación BINARIA, sobre TCP.
 * Este módulo es compartido por el servidor (PongServer) y el cliente
 * (PongClient): ambos deben compilar src/common/protocol.c.
 *
 * Todos los enteros multibyte viajan en orden de red (big-endian).
 * Las cadenas viajan como: [longitud u8][bytes ASCII imprimibles], sin '\0'.
 *
 * Formato de cada mensaje:
 *
 *   0        1        2        3        4        5        6        7
 *  +--------+--------+--------+--------+--------+--------+--------+--------+
 *  | MAGIC  |VERSION |  TYPE  | FLAGS  |       SEQ       |     LENGTH      |
 *  +--------+--------+--------+--------+--------+--------+--------+--------+
 *  |                  PAYLOAD (LENGTH bytes, 0..512)                ...
 *  +--------+--------+--------+--------+---
 *
 * Ver docs/PROTOCOLO.md para la especificación completa.
 */
#ifndef PONG_PROTOCOL_H
#define PONG_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------- */
/* Constantes generales                                             */
/* ---------------------------------------------------------------- */
#define PROTO_MAGIC 0x50u /* 'P' */
#define PROTO_VERSION 0x02u
#define PROTO_HEADER_SIZE 8
#define PROTO_MAX_PAYLOAD 512

#define PROTO_MAX_NICK 32
#define PROTO_MAX_EMAIL 64
#define PROTO_MAX_ERRTEXT 128

/* ---------------------------------------------------------------- */
/* Vocabulario de mensajes (campo TYPE)                             */
/* Bit más alto en 0 => Cliente -> Servidor                         */
/* Bit más alto en 1 => Servidor -> Cliente                         */
/* ---------------------------------------------------------------- */
enum {
  /* Cliente -> Servidor */
  MSG_REGISTER = 0x01,     /* nick, email: crea el perfil o lo retoma   */
  MSG_JOIN_QUEUE = 0x02,   /* (sin payload)                             */
  MSG_PADDLE_INPUT = 0x03, /* dirección de la paleta                    */
  MSG_LEAVE_MATCH = 0x04,  /* (sin payload) abandona la partida         */
  MSG_PING = 0x05,         /* timestamp u32                             */
  MSG_QUIT = 0x06,         /* (sin payload) cierre ordenado             */
  MSG_LOGIN = 0x07,        /* nick, email, token: cuenta preaprovisionada */
  MSG_LIST = 0x08,         /* (sin payload) pide la lista de usuarios   */
  MSG_CANCEL = 0x09,       /* (sin payload) sale de la cola             */

  /* Servidor -> Cliente */
  MSG_REGISTER_OK = 0x81, /* player_id u32                    */
  MSG_QUEUED = 0x82,      /* (sin payload)                    */
  MSG_MATCH_START = 0x83, /* datos de la partida y del campo  */
  MSG_GAME_STATE = 0x84,  /* instantánea del estado del juego */
  MSG_SCORE = 0x85,       /* alguien anotó un punto           */
  MSG_MATCH_END = 0x86,   /* fin de la partida                */
  MSG_PONG = 0x87,        /* eco del timestamp del PING       */
  MSG_USER = 0x88,        /* player_id u32, estado u8, nick   */
  MSG_USERS_BEGIN = 0x89, /* (sin payload) inicio de la lista */
  MSG_USERS_END = 0x8A,   /* (sin payload) fin de la lista    */
  MSG_ERROR = 0xFF        /* código de error + texto          */
};

/* Direcciones de la paleta (PADDLE_INPUT) */
enum { DIR_STOP = 0, DIR_UP = 1, DIR_DOWN = 2 };

/* Lados del campo */
enum { SIDE_LEFT = 0, SIDE_RIGHT = 1 };
#define SIDE_NONE 0xFFu

/* Razones de fin de partida (MATCH_END) */
enum { END_SCORE_LIMIT = 0, END_OPPONENT_LEFT = 1, END_SERVER_ERROR = 2 };

/* Códigos de error (ERROR) */
enum {
  ERR_BAD_HEADER = 1,         /* magic/versión inválidos (fatal)       */
  ERR_UNKNOWN_TYPE = 2,       /* TYPE no reconocido                    */
  ERR_MALFORMED = 3,          /* payload con longitud/forma incorrecta */
  ERR_INVALID_FIELD = 4,      /* nick o email con formato inválido     */
  ERR_NOT_REGISTERED = 5,     /* operación que exige registro previo   */
  ERR_ALREADY_REGISTERED = 6, /* REGISTER repetido en la misma sesión  */
  ERR_NICK_TAKEN = 7,         /* nick en uso por otro jugador          */
  ERR_INVALID_STATE = 8,      /* mensaje no válido en el estado actual */
  ERR_SERVER_FULL = 9,        /* sin cupos de conexión (fatal)         */
  ERR_INTERNAL = 10,          /* error interno del servidor            */
  ERR_BAD_CREDENTIALS = 11    /* LOGIN con nick/correo/token erróneos  */
};

/* ---------------------------------------------------------------- */
/* Estructuras en memoria (NO se envían tal cual: se serializan)    */
/* ---------------------------------------------------------------- */
typedef struct {
  uint8_t magic;
  uint8_t version;
  uint8_t type;
  uint8_t flags;   /* reservado, debe ser 0 */
  uint16_t seq;    /* contador por emisor, para trazabilidad en logs */
  uint16_t length; /* longitud del payload */
} msg_header_t;

typedef struct {
  char nick[PROTO_MAX_NICK + 1];
  char email[PROTO_MAX_EMAIL + 1];
} msg_register_t;

typedef struct {
  uint32_t match_id;
  uint8_t side;      /* SIDE_LEFT / SIDE_RIGHT: lado del receptor */
  uint8_t win_score; /* puntos para ganar */
  uint16_t field_w, field_h;
  uint16_t paddle_w, paddle_h;
  uint16_t ball_size;
  char opponent[PROTO_MAX_NICK + 1];
} msg_match_start_t;

typedef struct {
  uint32_t tick;
  uint16_t ball_x, ball_y; /* centro de la bola, en píxeles del campo   */
  uint16_t paddle_y[2];    /* centro vertical de cada paleta [izq, der] */
  uint8_t score[2];        /* [izq, der] */
} msg_game_state_t;

typedef struct {
  uint8_t scorer; /* SIDE_LEFT / SIDE_RIGHT */
  uint8_t score[2];
} msg_score_t;

typedef struct {
  uint8_t reason; /* END_* */
  uint8_t winner; /* SIDE_LEFT / SIDE_RIGHT / SIDE_NONE */
  uint8_t score[2];
} msg_match_end_t;

typedef struct {
  uint8_t code;
  char text[PROTO_MAX_ERRTEXT + 1];
} msg_error_t;

/* ---------------------------------------------------------------- */
/* Nombres legibles (para logs y depuración) */
const char *proto_type_name(uint8_t type);
const char *proto_error_name(uint8_t code);
const char *proto_dir_name(uint8_t dir);
const char *proto_end_reason_name(uint8_t reason);

/* Cabecera */
void proto_encode_header(uint8_t out[PROTO_HEADER_SIZE], const msg_header_t *h);
int proto_decode_header(const uint8_t in[PROTO_HEADER_SIZE], msg_header_t *h);

/* Serialización de payloads.
 * proto_enc_*: escriben en p (capacidad cap); devuelven bytes escritos o -1.
 * proto_dec_*: leen exactamente len bytes; devuelven 0 si ok o -1 si está mal
 * formado. */
int proto_enc_register(uint8_t *p, size_t cap, const msg_register_t *m);
int proto_dec_register(const uint8_t *p, size_t len, msg_register_t *m);

/* REGISTER_OK (player_id), PING y PONG (timestamp) comparten forma: un u32 */
int proto_enc_u32(uint8_t *p, size_t cap, uint32_t v);
int proto_dec_u32(const uint8_t *p, size_t len, uint32_t *v);

int proto_enc_paddle_input(uint8_t *p, size_t cap, uint8_t dir);
int proto_dec_paddle_input(const uint8_t *p, size_t len, uint8_t *dir);

int proto_enc_match_start(uint8_t *p, size_t cap, const msg_match_start_t *m);
int proto_dec_match_start(const uint8_t *p, size_t len, msg_match_start_t *m);

int proto_enc_game_state(uint8_t *p, size_t cap, const msg_game_state_t *m);
int proto_dec_game_state(const uint8_t *p, size_t len, msg_game_state_t *m);

int proto_enc_score(uint8_t *p, size_t cap, const msg_score_t *m);
int proto_dec_score(const uint8_t *p, size_t len, msg_score_t *m);

int proto_enc_match_end(uint8_t *p, size_t cap, const msg_match_end_t *m);
int proto_dec_match_end(const uint8_t *p, size_t len, msg_match_end_t *m);

int proto_enc_error(uint8_t *p, size_t cap, const msg_error_t *m);
int proto_dec_error(const uint8_t *p, size_t len, msg_error_t *m);

#endif /* PONG_PROTOCOL_H */
