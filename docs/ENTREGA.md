# Guía de entrega

## 1. Crear el repositorio privado en GitHub

1. En github.com pulsa **New repository**, ponle un nombre (por ejemplo `pong-telematica`), marca **Private** y **no** agregues README ni .gitignore.
2. En **Settings → Collaborators** invita a tu compañera y al docente, si el curso lo pide.

## 2. Subir el código

**Opción A — GitHub Desktop (Windows, sin consola).** Instala GitHub Desktop, entra con tu cuenta, usa *File → Add local repository*, elige la carpeta `pong` y acepta crear el repositorio. Haz *Commit to main* y luego *Publish repository*, dejando marcado **Keep this code private**.

**Opción B — Consola con git:**

```sh
cd pong
git init -b main
git add .
git status          # verifica que NO aparezcan users.conf, *.pem ni *.log
git commit -m "Pong cliente/servidor con protocolo binario PGP"
git remote add origin https://github.com/USUARIO/pong-telematica.git
git push -u origin main
```

Si git pide identidad: `git config user.name "Tu nombre"` y `git config user.email "tu@correo"`.

Al subirlo, la pestaña **Actions** ejecuta `make test` en Ubuntu. Debe quedar en verde.

## 3. Antes de entregar

- [ ] Nombres de los integrantes en el README.
- [ ] Servidor desplegado en AWS ([AWS.md](AWS.md)) y evidencias en `docs/evidencias/`.
- [ ] Sección *Despliegue en AWS Academy* del README completada con la IP y las capturas.
- [ ] Enlace del repositorio entregado en el buzón de Interactiva Virtual.
- [ ] Ningún commit después de la hora de cierre.

## Qué preparar para la sustentación

- Explicar la cabecera binaria de 8 bytes y un mensaje de ejemplo byte a byte (sección 3 de PROTOCOLO.md).
- Por qué TCP y no UDP, y su costo (*head-of-line blocking*).
- Cómo atiende el servidor varias partidas a la vez con `poll()` y por qué no usamos hilos.
- Demostración: dos clientes jugando contra el servidor en AWS, el log del servidor en vivo, un abandono y un nick duplicado rechazado.
