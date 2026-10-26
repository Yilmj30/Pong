# Pong en red — Proyecto N1, Internet: Arquitectura y Protocolos

**Integrantes:**
- Fabiola Valencia Barrios
- Yilmer David Murillo Jordán

## Introducción

Este proyecto implementa una versión en red del clásico **Pong** con arquitectura **cliente/servidor**. Cada jugador ejecuta un cliente gráfico (**PongClient**, en Python con Tkinter) que se conecta por Internet a un servidor (**PongServer**, en C). El servidor es la autoridad del juego. Registra a los jugadores, los empareja de a dos, simula cada partida, valida las entradas y envía a ambos jugadores el estado actualizado.

Para la comunicación diseñamos un protocolo propio de capa de aplicación, **MyAppGameProtocol** (Pong Game Protocol, PGP). Tiene **codificación binaria**, va sobre **TCP/IP** y usa directamente la **API de sockets Berkeley**. El servidor es concurrente: atiende hasta 256 conexiones y 128 partidas simultáneas.

## Desarrollo

### Estructura del repositorio

| Archivo | Responsabilidad |
|---|---|
| `src/server.c` | Sockets Berkeley (`socket`, `bind`, `listen`, `accept`, `recv`, `send`), bucle `poll()`, búferes por conexión y logger |
| `src/app.c`, `app.h` | Perfiles, registro, sesiones, presencia, cola FIFO y partidas |
| `src/game.c`, `game.h` | Física: paletas, bola, rebotes, colisiones y puntos (sin código de red) |
| `src/protocol.c`, `protocol.h` | Codificación binaria del protocolo |
| `client/wire.py` | Protocolo binario y socket del cliente |
| `client/pong.py` | Interfaz: registro, lista de usuarios, controles y dibujo del juego |
| `tests/` | Pruebas de la física (C) y de integración con sockets TCP reales (Python) |
| `deploy/` | Instalación en Ubuntu (EC2) como servicio systemd |
| `docs/PROTOCOLO.md` | **Especificación completa del protocolo con diagramas UML** |
| `docs/AWS.md` | Despliegue en AWS Academy |

### Compilación y ejecución

Requisitos: `gcc`, `make` y Python 3 con Tkinter (en Ubuntu: `sudo apt install build-essential python3-tk`).

```sh
make                          # compila ./server con gcc
./server 8080 server.log      # ./server <PORT> <LogFile>
```

En otras dos terminales, o en dos computadores distintos:

```sh
python3 client/pong.py
```

El cliente también funciona directamente en **Windows**, sin WSL. Instala Python desde python.org (ya trae Tkinter) y ejecuta `py client\pong.py`.

En cada ventana escribe la IP del servidor, el puerto, un **nick** y un **correo**, y pulsa **Conectar**. Así se crea el perfil. El campo *Token* se deja vacío. Luego pulsa **Buscar partida**. El servidor empareja a los jugadores por orden de llegada.

- Las flechas **↑ / ↓** mueven tu paleta. Al soltarlas, la paleta se detiene.
- Gana el primero que llegue a **5 puntos**.
- **Abandonar** le da la victoria al rival. **Salir de la cola** cancela la espera.
- La lista inferior muestra quién está disponible, esperando o jugando.

**Cuentas reservadas (opcional).** Si se quiere que un nick solo lo pueda usar una persona, el administrador lo reserva con `python3 deploy/create_users.py ana:ana@mail.co`. Esto genera `users.conf` con un token secreto, y ese jugador escribe su token en el cliente. Sin `users.conf`, el servidor funciona solo con registro abierto.

### Protocolo (resumen)

La especificación completa está en **[docs/PROTOCOLO.md](docs/PROTOCOLO.md)**. Incluye el servicio, el formato binario, el vocabulario, los errores, las reglas de procedimiento, la máquina de estados, los diagramas de secuencia y la arquitectura.

- **Cabecera binaria de 8 bytes:** `MAGIC (0x50) | VERSION (2) | TYPE | FLAGS | SEQ u16 | LENGTH u16`, enteros en big-endian, payload de hasta 512 bytes.
- **Cliente → servidor:** `REGISTER`, `LOGIN`, `LIST_USERS`, `JOIN_QUEUE`, `CANCEL_QUEUE`, `PADDLE_INPUT`, `LEAVE_MATCH`, `PING`, `QUIT`.
- **Servidor → cliente:** `REGISTER_OK`, `QUEUED`, `MATCH_START`, `GAME_STATE`, `SCORE`, `MATCH_END`, `PONG`, `USER`/`USERS_BEGIN`/`USERS_END`, `ERROR`.
- **Servidor autoritativo:** el cliente solo envía la dirección de su paleta. El servidor simula y envía `GAME_STATE` 30 veces por segundo a ambos jugadores.

### ¿Por qué TCP (`SOCK_STREAM`)?

- El registro, la cola, el inicio y fin de partida y los puntos tienen que llegar **siempre y en orden**. TCP lo garantiza sin que tengamos que programar confirmaciones ni retransmisiones.
- Las entradas se envían solo cuando cambia la dirección. Si se perdiera un "detener", la paleta seguiría moviéndose; con TCP no pasa.
- El tráfico es pequeño: 22 bytes por estado, unos 660 B/s por jugador. Además activamos `TCP_NODELAY` para que cada mensaje salga sin esperas.
- **Costo:** si se pierde un paquete, los siguientes esperan su retransmisión (*head-of-line blocking*). En un juego más exigente, los `GAME_STATE` podrían ir por UDP, porque un estado nuevo reemplaza al perdido, y el resto seguiría por TCP. Para Pong preferimos la simplicidad y la consistencia de TCP.

### Concurrencia

El servidor atiende a todos los clientes y partidas a la vez con **multiplexación de E/S**: un bucle con `poll()` sobre sockets no bloqueantes. El enunciado permite usar hilos, pero no lo exige. Escogimos `poll()` porque:

- no hay memoria compartida entre hilos, así que no necesitamos mutex ni hay condiciones de carrera;
- cada conexión tiene su propio búfer de salida (máximo 64 KiB), así que un cliente lento no frena a los demás;
- la simulación a 30 Hz corre en el mismo ciclo que la red.

Las pruebas lo comprueban con dos partidas simultáneas e independientes.

### Logger

Cada evento se imprime en la terminal y se agrega al archivo indicado en la línea de comandos: conexiones (con IP y puerto), cada petición recibida (`RECV`), cada respuesta enviada (`SEND`), registros, movimientos de paleta, inicio de partida, puntos, finales y errores. Por ejemplo:

```
2026-10-25 14:13:38 CONNECT client=0 from=127.0.0.1:55556
2026-10-25 14:13:38 RECV client=0 type=0x01 REGISTER seq=0 len=16
2026-10-25 14:13:38 REGISTER client=0 nick=ana email=ana@mail.co
2026-10-25 14:13:38 SEND client=0 type=0x81 REGISTER_OK seq=0 len=4
```

Como se registra cada `GAME_STATE`, el archivo crece unas 60 líneas por segundo por partida.

### Pruebas

```sh
make test
```

- **Física:** límites de las paletas, rebote en paredes, colisión con la paleta, punto y reinicio de la bola.
- **Integración con sockets TCP reales:**
  - registro: perfil nuevo, nick duplicado, nick reservado, datos inválidos y retomar un perfil;
  - login con token correcto e incorrecto;
  - mensajes fragmentados byte a byte;
  - dos partidas simultáneas y rechazo de una segunda partida para un jugador ocupado;
  - movimiento de paletas y partida completa hasta 5 puntos, con el mismo resultado en ambos clientes;
  - abandono, desconexión, cancelación de la cola, presencia, ping y mensajes inválidos;
  - arranque sin `users.conf` y contenido del log.

Resultados en Ubuntu con gcc: ver [docs/VALIDACION.md](docs/VALIDACION.md).

### Despliegue en AWS Academy

Los pasos están en [docs/AWS.md](docs/AWS.md): una instancia EC2 con Ubuntu, el puerto TCP 8080 abierto en el grupo de seguridad y el servidor instalado como servicio systemd.

**Evidencias del despliegue:**
- IP pública usada en la demostración: _completar_
- Captura de `systemctl status pong`: _agregar en `docs/evidencias/`_
- Captura de dos clientes jugando contra el servidor en AWS: _agregar en `docs/evidencias/`_

## Conclusiones

- Diseñar el protocolo antes de programar fue clave. Con la cabecera binaria fija, el campo `LENGTH` y la tabla de mensajes por estado, servidor y cliente (en lenguajes distintos) se entendieron desde el primer intento.
- Que el servidor sea la única autoridad garantiza que ambos jugadores vean siempre el mismo marcador y que un cliente no pueda hacer trampa. El cliente solo envía intenciones y dibuja lo que recibe.
- TCP simplificó mucho el diseño: no tuvimos que programar confirmaciones ni ordenar mensajes. A cambio aceptamos posibles pausas por retransmisión, que en Pong no afectan la experiencia.
- La multiplexación con `poll()` nos dio concurrencia real sin los problemas típicos de los hilos (bloqueos y condiciones de carrera), y separar la física, la lógica de partidas y la red hizo que cada parte se pudiera probar por separado.
- TCP es un flujo de bytes, no de mensajes. Hubo que acumular los datos recibidos hasta completar cada mensaje, y las pruebas con envío byte a byte lo confirmaron.

## Referencias

- Enunciado del curso: *Pong — Proyecto N1, Internet: Arquitectura y Protocolos*, versión 1.0, 2026.
- B. Hall, *Beej's Guide to Network Programming*. https://beej.us/guide/bgnet/
- B. Hall, *Beej's Guide to C Programming*. https://beej.us/guide/bgc/
- GeeksforGeeks, *TCP Server-Client implementation in C*. https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
- W. Eddy (ed.), *RFC 9293 — Transmission Control Protocol*, IETF, 2022. https://www.rfc-editor.org/rfc/rfc9293.html
- Wikipedia, *Pong*. https://en.wikipedia.org/wiki/Pong
- Python Software Foundation, *tkinter — Python interface to Tcl/Tk*. https://docs.python.org/3/library/tkinter.html
- Amazon Web Services, *Connect to your EC2 instance using EC2 Instance Connect*. https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-instance-connect-methods.html
