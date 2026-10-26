"""Provision credentials locally; never commit users.conf."""
import argparse
import os
import secrets
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('users', nargs='+', help='nick:email')
p.add_argument('--output', default='users.conf')
a = p.parse_args()
rows = []
seen = set()
for user in a.users:
    nick, email = user.split(':', 1)
    if not nick.isascii() or not 1 <= len(nick) <= 32 or not all(c.isalnum() or c in '._-' for c in nick):
        p.error('Nickname invalido')
    if not email.isascii() or not 3 <= len(email) <= 64 or email.count('@') != 1 or any(c.isspace() for c in email):
        p.error('Correo invalido')
    if nick in seen:
        p.error('Nickname duplicado')
    seen.add(nick)
    rows.append(f'{nick} {email} {secrets.token_hex(24)}\n')
fd = os.open(a.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(fd, 'w') as f:
    f.writelines(rows)
print(f'Credenciales creadas en {Path(a.output).resolve()}. Distribuye cada token solo a su usuario.')
