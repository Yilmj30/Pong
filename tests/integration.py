"""Real TCP integration tests; temporary users/logs, no production credentials."""
import os
import socket
import struct
import subprocess
import tempfile
import time
from pathlib import Path

class Peer:
    def __init__(self, port):
        self.s = socket.create_connection(('127.0.0.1', port), timeout=3)
        self.s.settimeout(5)
        self.seq = 0
    def send(self, kind, payload=b'', fragmented=False):
        data = struct.pack('!BBBBHH', 80, 2, kind, 0, self.seq, len(payload)) + payload
        self.seq += 1
        if fragmented:
            for b in data:
                self.s.sendall(bytes([b]))
        else:
            self.s.sendall(data)
    def exact(self, n):
        out = b''
        while len(out) < n:
            data = self.s.recv(n-len(out))
            if not data:
                raise EOFError()
            out += data
        return out
    def until(self, kind):
        for _ in range(2000):
            magic, version, t, flags, seq, n = struct.unpack('!BBBBHH', self.exact(8))
            assert magic == 80 and version == 2 and flags == 0 and n <= 512
            payload = self.exact(n)
            if t == kind:
                return payload
        raise AssertionError('message missing')
    def login(self, nick, token='secret', fragmented=False):
        fields = (nick, nick+'@mail.test', token)
        p = b''.join(bytes([len(x)]) + x.encode() for x in fields)
        self.send(7, p, fragmented)
    def register(self, nick, email=None):
        fields = (nick, email or nick+'@mail.test')
        self.send(1, b''.join(bytes([len(x)]) + x.encode() for x in fields))
    def close(self):
        self.s.close()

def run():
    with tempfile.TemporaryDirectory() as tmp:
        users = Path(tmp)/'users'
        users.write_text(''.join(f'{n} {n}@mail.test secret\n' for n in ('alice','bob','carol','dave')))
        probe = socket.socket();probe.bind(('127.0.0.1',0));port=probe.getsockname()[1];probe.close()
        process = subprocess.Popen([os.environ.get('PONG_SERVER_BIN','./server'),str(port),str(Path(tmp)/'log')],env={**os.environ,'PONG_USERS_FILE':str(users)},stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        peers=[]
        try:
            for _ in range(100):
                try:
                    a=Peer(port);peers.append(a);break
                except OSError:
                    time.sleep(.03)
            else:
                raise AssertionError(process.stderr.read().decode())
            a.send(2);assert a.until(255)[0]==5
            a.login('alice','wrong');assert a.until(255)[0]==11
            a.login('alice',fragmented=True);a.until(129)
            dup=Peer(port);peers.append(dup);dup.login('alice');assert dup.until(255)[0]==7
            b,c,d=[Peer(port) for _ in range(3)];peers += [b,c,d]
            for peer,nick in zip((b,c,d),('bob','carol','dave')):
                peer.login(nick);peer.until(129)
            a.send(2);a.until(130);b.send(2);b.until(130)
            sa=a.until(131);sb=b.until(131);assert sa[:4]==sb[:4] and sa[4]!=sb[4]
            a.send(2);assert a.until(255)[0]==8
            c.send(2);c.until(130);d.send(2);d.until(130)
            sc=c.until(131);sd=d.until(131);assert sc[:4]==sd[:4] and sc[:4]!=sa[:4]
            # Complete match with stationary paddles. Compare both players' terminal score.
            ea=a.until(134);eb=b.until(134);assert ea==eb and ea[0]==0 and max(ea[2:])==5
            a.send(2);a.until(130);b.send(2);b.until(130);a.until(131);b.until(131)
            a.send(3,b'\x01');st=a.until(132);assert struct.unpack('!IHHHHBB',st)[3] < 300
            a.send(4);assert a.until(134)==b.until(134)
            a.send(2);a.until(130);b.send(2);b.until(130);a.until(131);b.until(131)
            b.close();assert a.until(134)[0]==1
            a.send(2);a.until(130);a.send(9);a.send(8);a.until(137)
            found=[]
            while True:
                h=struct.unpack('!BBBBHH',a.exact(8));p=a.exact(h[-1])
                if h[2]==136:found.append(p)
                if h[2]==138:break
            assert found
            a.send(5,struct.pack('!I',123));assert a.until(135)==struct.pack('!I',123)
            a.send(3,b'\xff');assert a.until(255)[0]==3
            bad=Peer(port);peers.append(bad);bad.s.sendall(struct.pack('!BBBBHH',80,2,7,0,0,513));assert bad.s.recv(1)==b''
            # Registro abierto: el jugador crea su perfil con nick y correo.
            e=Peer(port);peers.append(e);e.register('erin');e.until(129)
            e.register('erin');assert e.until(255)[0]==6
            e2=Peer(port);peers.append(e2);e2.register('erin');assert e2.until(255)[0]==7
            e2.register('alice');assert e2.until(255)[0]==7
            e2.register('bad!');assert e2.until(255)[0]==4
            e2.register('frank','sin-arroba');assert e2.until(255)[0]==4
            e2.login('erin','x');assert e2.until(255)[0]==11
            e.close();time.sleep(.1)
            e3=Peer(port);peers.append(e3);e3.register('erin','otro@mail.test');assert e3.until(255)[0]==7
            e3.register('erin');e3.until(129)
            f=Peer(port);peers.append(f);f.register('frank');f.until(129)
            e3.send(2);e3.until(130);f.send(2);f.until(130);e3.until(131);f.until(131)
            e3.send(4);assert e3.until(134)==f.until(134)
            samples=[]
            for value in range(20):
                start=time.perf_counter();a.send(5,struct.pack('!I',value));assert a.until(135)==struct.pack('!I',value)
                samples.append((time.perf_counter()-start)*1000)
            print(f'Local application RTT: median={sorted(samples)[10]:.2f}ms p95={sorted(samples)[18]:.2f}ms, n=20')
            print('PASS TCP: register, auth, duplicate identity, fragmentation, two matches, busy rejection, full score, movement, leave, disconnect, queue cancel, presence, ping, malformed frame')
        finally:
            for peer in peers:peer.close()
            process.terminate()
            try:process.wait(timeout=5)
            except subprocess.TimeoutExpired:process.kill();process.wait()
            errors=process.stderr.read().decode()
            assert process.returncode == 0, f'Server exit {process.returncode}: {errors}'
            assert 'runtime error:' not in errors and 'ERROR: AddressSanitizer' not in errors, errors

def run_without_accounts():
    """Sin users.conf el servidor arranca y acepta registro abierto."""
    with tempfile.TemporaryDirectory() as tmp:
        probe=socket.socket();probe.bind(('127.0.0.1',0));port=probe.getsockname()[1];probe.close()
        env={**os.environ,'PONG_USERS_FILE':str(Path(tmp)/'no-existe')}
        process=subprocess.Popen([os.environ.get('PONG_SERVER_BIN','./server'),str(port),str(Path(tmp)/'log')],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
        peers=[]
        try:
            for _ in range(100):
                try:
                    a=Peer(port);peers.append(a);break
                except OSError:
                    time.sleep(.03)
            else:
                raise AssertionError(process.stderr.read().decode())
            b=Peer(port);peers.append(b)
            a.register('ana');a.until(129);b.register('beto');b.until(129)
            a.send(2);a.until(130);b.send(2);b.until(130)
            sa=a.until(131);sb=b.until(131);assert sa[:4]==sb[:4]
            ea=a.until(134);eb=b.until(134);assert ea==eb and max(ea[2:])==5
            log=(Path(tmp)/'log').read_text()
            for word in ('READY','CONNECT client=0 from=127.0.0.1','REGISTER client=0 nick=ana','AUTH_OK','RECV','SEND','MATCH_START','GAME_EVENT'):
                assert word in log, word
            print('PASS sin users.conf: registro abierto, partida completa y log con peticiones/respuestas')
        finally:
            for peer in peers:peer.close()
            process.terminate();process.wait(timeout=5)
            errors=process.stderr.read().decode()
            assert process.returncode == 0, errors
            assert 'runtime error:' not in errors and 'ERROR: AddressSanitizer' not in errors, errors

if __name__=='__main__':
    run()
    run_without_accounts()
