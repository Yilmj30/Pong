"""Binary PGP codec and TCP adapter. No game or UI dependencies."""
import socket
import struct

class Wire:
    def __init__(self, host, port):
        self.socket = socket.create_connection((host, port), timeout=5)
        self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.socket.setblocking(False)
        self.rx = bytearray()
        self.tx = bytearray()
        self.seq = 0

    def send(self, kind, payload=b''):
        self.tx.extend(struct.pack('!BBBBHH', 0x50, 2, kind, 0, self.seq, len(payload)))
        self.tx.extend(payload)
        self.seq = (self.seq + 1) & 65535

    def pump(self):
        if self.tx:
            try:
                sent = self.socket.send(self.tx)
                del self.tx[:sent]
            except BlockingIOError:
                pass
        try:
            data = self.socket.recv(65536)
            if not data:
                raise ConnectionError('El servidor cerro la conexion')
            self.rx.extend(data)
        except BlockingIOError:
            pass
        messages = []
        while len(self.rx) >= 8:
            magic, version, kind, flags, seq, length = struct.unpack('!BBBBHH', self.rx[:8])
            if magic != 0x50 or version != 2 or flags or length > 512:
                raise ConnectionError('Cabecera invalida')
            if len(self.rx) < 8 + length:
                break
            messages.append((kind, bytes(self.rx[8:8 + length])))
            del self.rx[:8 + length]
        return messages

    def close(self):
        self.socket.close()


def text(value):
    data = value.encode('ascii')
    if not 1 <= len(data) <= 64:
        raise ValueError('Campos ASCII de 1 a 64 caracteres')
    return bytes([len(data)]) + data
