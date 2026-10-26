# Despliegue en AWS Academy (EC2 con Ubuntu)

## 1. Crear la instancia

1. Entra a **AWS Academy**, abre el *Learner Lab* y pulsa **Start Lab**. Cuando el círculo esté verde, pulsa **AWS** para abrir la consola.
2. Ve a **EC2 → Instancias → Lanzar instancia**:
   - **Nombre:** `pong-server`
   - **Imagen (AMI):** Ubuntu Server (LTS)
   - **Tipo:** `t2.micro` o `t3.micro`
   - **Par de claves:** usa `vockey` o crea uno y descarga el `.pem`
3. En **Configuración de red → Editar**, deja la regla de **SSH (TCP 22)** y agrega otra:
   - **Tipo:** TCP personalizado · **Puerto:** `8080` · **Origen:** `0.0.0.0/0` (o solo las IP de los jugadores)
4. Pulsa **Lanzar instancia** y copia su **IP pública** (IPv4).

> El juego usa solo TCP; no hace falta abrir puertos UDP.

## 2. Subir el código e instalar el servidor

Opción A — desde el repositorio (más simple), conectándote con **EC2 Instance Connect** desde la consola (botón *Conectar*):

```sh
sudo apt update && sudo apt install -y git
git clone https://github.com/USUARIO/REPO.git pong   # repo privado: usar un token de acceso personal
cd pong
sudo sh deploy/install.sh
```

Opción B — copiando la carpeta desde tu PC (en Windows sirve PowerShell):

```sh
scp -i CLAVE.pem -r pong ubuntu@IP_PUBLICA:~/pong
ssh -i CLAVE.pem ubuntu@IP_PUBLICA
cd ~/pong
sudo sh deploy/install.sh
```

`install.sh` compila con gcc, instala el servidor en `/opt/pong` y lo deja como servicio de systemd que ejecuta:

```
/opt/pong/server 8080 /var/log/pong/server.log
```

El servicio arranca solo al encender la instancia y se reinicia si falla.

## 3. Comprobar

En la instancia:

```sh
make test                                  # pruebas automáticas
sudo systemctl status pong --no-pager      # debe decir active (running)
sudo tail -f /var/log/pong/server.log      # log en vivo (Ctrl+C para salir)
```

En dos computadores distintos:

```sh
python3 client/pong.py
```

Escribe la **IP pública** de la instancia, el puerto `8080`, un nick y un correo, pulsa **Conectar** y luego **Buscar partida**.

## 4. Evidencias para el README

Guarda en `docs/evidencias/`:

- `systemctl status pong` mostrando *active (running)*.
- Las dos ventanas del cliente jugando, con el mismo marcador.
- `tail` del log del servidor durante la partida.
- La regla del puerto 8080 en el grupo de seguridad.
- (Opcional) dos parejas jugando al mismo tiempo.

No subas el archivo `.pem` ni `users.conf`.

## Notas

- En AWS Academy la sesión del laboratorio dura unas horas. Al terminar, la instancia se detiene y al volver a iniciarla **la IP pública cambia**. Revísala antes de la demostración.
- Comandos útiles: `sudo systemctl restart pong`, `sudo systemctl stop pong`, `sudo journalctl -u pong -n 50`.
- **Cuentas reservadas (opcional):** antes de `install.sh`, ejecuta `python3 deploy/create_users.py ana:ana@mail.co`. Ese nick solo se podrá usar con el token generado. Los tokens viajan sin cifrar por TCP; si se usan por Internet, conviene conectar a través de un túnel SSH (`ssh -i CLAVE.pem -N -L 18080:127.0.0.1:8080 ubuntu@IP_PUBLICA` y en el cliente `127.0.0.1:18080`).
