import os,sys,socket,subprocess,tempfile,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1] / 'client'))
import tkinter as tk
from pong import PongClient
with tempfile.TemporaryDirectory() as d:
    users=Path(d)/'users';users.write_text('alice alice@mail.test token-a\nbob bob@mail.test token-b\n')
    probe=socket.socket();probe.bind(('127.0.0.1',0));port=probe.getsockname()[1];probe.close()
    proc=subprocess.Popen([str(Path(__file__).resolve().parents[1] / 'server'),str(port),d+'/log'],env={**os.environ,'PONG_USERS_FILE':str(users)},stdout=subprocess.DEVNULL)
    time.sleep(.2)
    root=tk.Tk();root2=tk.Toplevel(root);a=PongClient(root);b=PongClient(root2)
    try:
        for client,nick,token in [(a,'alice','token-a'),(b,'bob','token-b')]:
            for key,value in [('Puerto',str(port)),('Usuario',nick),('Correo',nick+'@mail.test'),('Token (opcional)',token)]:
                client.fields[key].delete(0,'end');client.fields[key].insert(0,value)
            client.connect()
        def pump(seconds):
            deadline=time.monotonic()+seconds
            while time.monotonic()<deadline:
                root.update();time.sleep(.01)
        pump(.5)
        assert a.player_id and b.player_id
        a.send(2);b.send(2);pump(1.5)
        assert a.playing and b.playing
        assert str(a.join['state'])=='disabled' and str(b.join['state'])=='disabled'
        a.key('up',True);pump(.4);a.key('up',False);pump(.1)
        assert a.canvas.coords(a.paddles[0])[1] < 250
        assert a.canvas.coords(a.paddles[0])==b.canvas.coords(b.paddles[0])
        assert a.canvas.itemcget(a.score,'text')==b.canvas.itemcget(b.score,'text')
        a.send(4);pump(.3);assert not a.playing and not b.playing
        assert str(a.join['state'])=='normal'
        print('PASS GUI: login, match, disabled busy action, arrows, synchronized positions and score, return to lobby')
    finally:
        a.disconnect();b.disconnect();root.destroy();proc.terminate();proc.wait()
