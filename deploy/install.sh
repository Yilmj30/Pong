#!/bin/sh
# Run on Ubuntu EC2 from the uploaded project: sudo sh deploy/install.sh
set -eu
if [ "$(id -u)" -ne 0 ]; then echo 'Ejecuta con sudo'; exit 1; fi
apt-get update
apt-get install -y build-essential
make clean all
if ! id pong >/dev/null 2>&1; then useradd --system --home /opt/pong --shell /usr/sbin/nologin pong; fi
install -d -o root -g pong -m 750 /opt/pong
install -d -o pong -g pong -m 750 /var/log/pong
install -o root -g pong -m 750 server /opt/pong/server
# users.conf es opcional: solo hace falta para reservar nicks con token.
if [ -f users.conf ]; then install -o root -g pong -m 640 users.conf /opt/pong/users.conf; fi
install -m 644 deploy/pong.service /etc/systemd/system/pong.service
systemctl daemon-reload
systemctl enable --now pong
systemctl status pong --no-pager
