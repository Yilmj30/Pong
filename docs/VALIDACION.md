# Validación

## Ubuntu Linux con gcc — 25 de octubre de 2026

Entorno: Linux x86-64, gcc, Python 3.13.

| Prueba | Resultado |
|---|---|
| `make CFLAGS='-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror'` | Compila sin advertencias |
| `make test`: física (límites, rebote, colisión, punto y reinicio) | PASS |
| `make test`: integración TCP (registro, login, duplicados, fragmentación, dos partidas simultáneas, jugador ocupado, partida a 5 puntos, movimiento, abandono, desconexión, cola, presencia, ping, mensajes inválidos) | PASS |
| `make test`: servidor sin `users.conf` (registro abierto, partida completa, contenido del log) | PASS |
| Misma batería compilada con AddressSanitizer y UndefinedBehaviorSanitizer | PASS, sin errores de memoria ni comportamiento indefinido |
| Cliente `pong.py` contra el servidor: registro con nick y correo, emparejamiento, flechas, posiciones sincronizadas en ambos clientes y abandono | PASS |
| RTT de aplicación en loopback (20 PING/PONG) | mediana 0.05 ms, p95 0.07 ms |

Salida de `make test`:

```
PASS physics: limits, collision, wall, score, reset
Local application RTT: median=0.05ms p95=0.07ms, n=20
PASS TCP: register, auth, duplicate identity, fragmentation, two matches, busy rejection, full score, movement, leave, disconnect, queue cancel, presence, ping, malformed frame
PASS sin users.conf: registro abierto, partida completa y log con peticiones/respuestas
```

## macOS — 21 de octubre de 2026

Compilación con `-Werror`, pruebas de física, integración TCP, sanitizers y prueba gráfica con dos ventanas Tk reales (`python3 tests/gui_smoke.py`): PASS.

## AWS Academy

Despliegue y validación en la nube:
- **Fecha:** 25 de octubre de 2026.
- **Instancia:** AWS Academy Learner Lab, EC2 `t3.micro`, Ubuntu Server LTS (`i-013f9091ff96108f5`).
- **IP pública:** `54.242.247.239`, puerto TCP `8080`.
- **Servicio systemd:** `pong.service` en estado `active (running)` con arranque automático (`deploy/install.sh`).
- **Prueba en red:** Partida en vivo disputada exitosamente entre los clientes `YIL` y `FAB` con victoria por límite de puntos (5-1).
- **Evidencias gráficas:**
  - Servicio en ejecución: [docs/evidencias/systemctl_status_pong.png](evidencias/systemctl_status_pong.png)
  - Partida en vivo terminada en AWS: [docs/evidencias/partida_pong_aws.png](evidencias/partida_pong_aws.png)
