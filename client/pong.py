"""Cliente grafico para Pong en red (Tkinter) usando el protocolo binario PGP."""
import struct
import time
import tkinter as tk
from tkinter import messagebox, ttk
from wire import Wire, text

class PongClient:
    def __init__(self, root):
        self.root = root
        self.net = None
        self.player_id = None
        self.playing = False
        self.keys = set()
        self.direction = 0
        self.side = 0
        self.last_ping = 0
        self.users = []
        root.title('Pong · Cliente / Servidor')
        root.configure(bg='#101827')
        self.status = tk.StringVar(value='Escribe tu nick y correo para crear tu perfil (el token es opcional)')
        form = ttk.Frame(root, padding=12)
        form.pack(fill='x')
        self.fields = {}
        for index, (name, default) in enumerate([('Servidor', '127.0.0.1'), ('Puerto', '8080'), ('Usuario', ''), ('Correo', ''), ('Token (opcional)', '')]):
            ttk.Label(form, text=name).grid(row=0, column=index, sticky='w')
            entry = ttk.Entry(form, width=18 if name != 'Puerto' else 7, show='*' if name.startswith('Token') else '')
            entry.insert(0, default)
            entry.grid(row=1, column=index, padx=4)
            self.fields[name] = entry
        self.connect_button = ttk.Button(form, text='Conectar', command=self.connect)
        self.connect_button.grid(row=1, column=5)
        self.join = ttk.Button(form, text='Buscar partida', command=lambda: self.send(2), state='disabled')
        self.join.grid(row=2, column=0, pady=10)
        self.cancel = ttk.Button(form, text='Salir de la cola', command=lambda: self.send(9), state='disabled')
        self.cancel.grid(row=2, column=1)
        self.leave = ttk.Button(form, text='Abandonar', command=lambda: self.send(4), state='disabled')
        self.leave.grid(row=2, column=2)
        ttk.Button(form, text='Desconectar', command=self.disconnect).grid(row=2, column=3)
        self.canvas = tk.Canvas(root, width=800, height=600, bg='#101827', highlightthickness=0, takefocus=True)
        self.canvas.pack(padx=12)
        self.canvas.create_line(400, 0, 400, 600, fill='#34445f', dash=(8, 12))
        self.score = self.canvas.create_text(400, 50, text='0    0', fill='#ffffff', font=('Helvetica', 36, 'bold'))
        self.names = self.canvas.create_text(400, 95, text='Flechas ↑ y ↓ para mover tu paleta', fill='#a9b9d2', font=('Helvetica', 14))
        self.ball = self.canvas.create_oval(395, 295, 405, 305, fill='#ffffff', outline='')
        self.paddles = [self.canvas.create_rectangle(x-5, 250, x+5, 350, fill=color, outline='') for x, color in [(20, '#48ded0'), (780, '#ffb86b')]]
        self.people = ttk.Label(root, text='Usuarios conectados: —', wraplength=780)
        self.people.pack(padx=12, pady=8)
        ttk.Label(root, textvariable=self.status, wraplength=780).pack(padx=12, pady=(0, 12))
        root.bind('<KeyPress-Up>', lambda e: self.key('up', True))
        root.bind('<KeyRelease-Up>', lambda e: self.key('up', False))
        root.bind('<KeyPress-Down>', lambda e: self.key('down', True))
        root.bind('<KeyRelease-Down>', lambda e: self.key('down', False))
        root.bind('<FocusOut>', lambda e: self.release_keys())
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(16, self.poll)

    def send(self, kind, payload=b''):
        if self.net:
            self.net.send(kind, payload)

    def connect(self):
        if self.net:
            return
        try:
            nick = self.fields['Usuario'].get().strip()
            email = self.fields['Correo'].get().strip()
            token = self.fields['Token (opcional)'].get().strip()
            if len(nick) > 32:
                raise ValueError('Usuario de maximo 32 caracteres')
            if token:  # cuenta preaprovisionada: LOGIN
                kind, payload = 7, text(nick) + text(email) + text(token)
            else:      # registro: crea el perfil con nick y correo
                kind, payload = 1, text(nick) + text(email)
            self.net = Wire(self.fields['Servidor'].get().strip(), int(self.fields['Puerto'].get()))
            self.send(kind, payload)
            self.connect_button.configure(state='disabled')
            self.status.set('Registrando perfil…' if kind == 1 else 'Autenticando…')
        except (OSError, ValueError) as exc:
            messagebox.showerror('Conexion', str(exc))

    def key(self, key, down):
        if not self.playing:
            return
        if down:
            self.keys.add(key)
        else:
            self.keys.discard(key)
        direction = 1 if 'up' in self.keys else 2 if 'down' in self.keys else 0
        if direction != self.direction:
            self.direction = direction
            self.send(3, bytes([direction]))

    def release_keys(self):
        self.keys.clear()
        if self.direction:
            self.direction = 0
            self.send(3, b'\x00')

    def message(self, kind, p):
        if kind == 0x81:
            self.player_id, = struct.unpack('!I', p)
            self.status.set(f'Perfil listo (id {self.player_id}). Busca una partida; el servidor asignara un rival.')
        elif kind == 0x82:
            self.join.configure(state='disabled')
            self.cancel.configure(state='normal')
            self.status.set('Esperando rival…')
        elif kind == 0x83:
            match, self.side, win, width, height, pw, ph, ball = struct.unpack('!IBBHHHHH', p[:16])
            opponent = p[17:17+p[16]].decode('ascii')
            self.playing = True
            self.canvas.itemconfigure(self.score, text='0    0')
            self.canvas.itemconfigure(self.names, text=f'{self.fields["Usuario"].get()} vs {opponent} · Gana quien llegue a {win}')
            self.join.configure(state='disabled')
            self.cancel.configure(state='disabled')
            self.leave.configure(state='normal')
            self.canvas.focus_set()
            self.status.set(f'Partida {match}. Tu paleta: {"izquierda" if self.side == 0 else "derecha"}. Flechas ↑ / ↓.')
        elif kind == 0x84:
            tick, x, y, left, right, sl, sr = struct.unpack('!IHHHHBB', p)
            self.canvas.coords(self.ball, x-5, y-5, x+5, y+5)
            for item, px, py in zip(self.paddles, (20, 780), (left, right)):
                self.canvas.coords(item, px-5, py-50, px+5, py+50)
            self.canvas.itemconfigure(self.score, text=f'{sl}    {sr}')
        elif kind == 0x86:
            reason, winner, sl, sr = p
            self.release_keys()
            self.playing = False
            self.leave.configure(state='disabled')
            self.canvas.itemconfigure(self.score, text=f'{sl}    {sr}')
            result = 'Ganaste' if winner == self.side else 'Perdiste' if winner < 2 else 'Sin ganador'
            self.status.set(f'{result} · {sl}–{sr} · {"limite de puntos" if reason == 0 else "abandono o desconexion" if reason == 1 else "servidor detenido"}')
        elif kind == 0x89:
            self.users = []
        elif kind == 0x88:
            ident, state = struct.unpack('!IB', p[:5])
            nick = p[6:6+p[5]].decode('ascii')
            self.users.append(f'{nick}: { {1:"disponible",2:"esperando",3:"jugando"}[state]}')
            if ident == self.player_id:
                self.join.configure(state='normal' if state == 1 else 'disabled')
                self.cancel.configure(state='normal' if state == 2 else 'disabled')
        elif kind == 0x8a:
            self.people.configure(text='Usuarios conectados: ' + ' · '.join(self.users))
        elif kind == 0xff:
            self.status.set('Error: ' + p[2:2+p[1]].decode('ascii'))
            if self.player_id is None:
                self.disconnect(keep_status=True)

    def poll(self):
        try:
            if self.net:
                if time.monotonic() - self.last_ping > 10:
                    self.send(5, struct.pack('!I', int(time.monotonic()*1000) & 0xffffffff))
                    self.last_ping = time.monotonic()
                messages = self.net.pump()
                for kind, payload in messages:
                    self.message(kind, payload)
        except (OSError, ValueError, KeyError, struct.error) as exc:
            self.status.set(f'Conexion terminada: {exc}')
            self.disconnect(keep_status=True)
        self.root.after(16, self.poll)

    def disconnect(self, keep_status=False):
        if self.net:
            self.net.close()
        self.net = None
        self.player_id = None
        self.playing = False
        self.keys.clear()
        self.direction = 0
        self.join.configure(state='disabled')
        self.cancel.configure(state='disabled')
        self.leave.configure(state='disabled')
        self.connect_button.configure(state='normal')
        self.people.configure(text='Usuarios conectados: —')
        if not keep_status:
            self.status.set('Desconectado')

    def close(self):
        self.disconnect()
        self.root.destroy()

if __name__ == '__main__':
    PongClient(tk.Tk()).root.mainloop()
