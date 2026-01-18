#!/usr/bin/env python3
import argparse
import json
import os
import signal
import sys
import threading
import time
import queue
from subprocess import Popen, PIPE

from flask import Flask, Response, request
from flask_socketio import SocketIO, emit

# -----------------------------
# SSE broker for fallback transport
# -----------------------------
class SSEBroker:
    def __init__(self):
        self._clients: list[queue.Queue] = []
        self._lock = threading.Lock()
    def add_client(self) -> queue.Queue:
        q = queue.Queue()
        with self._lock:
            self._clients.append(q)
        return q
    def remove_client(self, q: queue.Queue):
        with self._lock:
            if q in self._clients:
                self._clients.remove(q)
    def broadcast(self, data: str):
        dead = []
        with self._lock:
            for q in list(self._clients):
                try:
                    q.put_nowait(data)
                except Exception:
                    dead.append(q)
            for q in dead:
                if q in self._clients:
                    self._clients.remove(q)

# Global broker instance
broker = SSEBroker()

# -----------------------------
# Producer that streams generator JSON to WebSocket clients and SSE
# -----------------------------
class Producer:
    def __init__(self, socketio: SocketIO, brand_color: str | None, preset: str, extras: list[str], no_artifacts: bool):
        self.socketio = socketio
        self.brand_color = brand_color
        self.preset = preset
        self.extras = [x for x in extras if x != "--"]
        self.no_artifacts = no_artifacts
        self._proc: Popen | None = None
        self._lock = threading.Lock()
        self._reader: threading.Thread | None = None
        self._stderr: threading.Thread | None = None

    def _build_cmd(self) -> list[str]:
        cmd = [sys.executable, os.path.join('scripts', 'generate_sweep_small_multiples.py')]
        metric_shorthands = {'-Pa', '-Ps', '-Pm', '-Pv'}
        extras_set = set(self.extras)
        if metric_shorthands.isdisjoint(extras_set):
            preset_map = {'accuracy': '-Pa', 'strength': '-Ps', 'max': '-Pm', 'avg': '-Pv'}
            cmd.append(preset_map.get(self.preset, '-Pa'))
        cmd.extend(['--phase-c-log-json', '--json-only'])
        if self.brand_color and ('--brand-color' not in extras_set):
            cmd.extend(['--brand-color', self.brand_color])
        if self.no_artifacts:
            cmd.extend(['--png-out', os.devnull])
        if self.extras:
            cmd.extend(self.extras)
        return cmd

    def start(self):
        with self._lock:
            self.stop()  # ensure no duplicate
            cmd = self._build_cmd()
            self._proc = Popen(cmd, stdout=PIPE, stderr=PIPE, bufsize=1, universal_newlines=True)
            self._reader = threading.Thread(target=self._pump_stdout, daemon=True)
            self._stderr = threading.Thread(target=self._pump_stderr, daemon=True)
            self._reader.start(); self._stderr.start()

    def _pump_stdout(self):
        assert self._proc is not None
        for line in self._proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except Exception:
                obj = {"type": "log", "message": line}
            # Socket.IO
            self.socketio.emit('event', obj)
            # SSE
            broker.broadcast(json.dumps(obj))
        done_evt = {"type": "producer_done", "ts": time.time()}
        self.socketio.emit('event', done_evt)
        broker.broadcast(json.dumps(done_evt))

    def _pump_stderr(self):
        if not self._proc or not self._proc.stderr:
            return
        for line in self._proc.stderr:
            line = line.strip()
            if not line:
                continue
            evt = {"type": "stderr", "message": line}
            self.socketio.emit('event', evt)
            broker.broadcast(json.dumps(evt))

    def stop(self):
        if self._proc and self._proc.poll() is None:
            try:
                if os.name == 'nt':
                    self._proc.terminate()
                else:
                    self._proc.send_signal(signal.SIGINT)
            except Exception:
                pass
            self._proc = None

# -----------------------------
# Flask + Socket.IO app
# -----------------------------
app = Flask(__name__)
# Remove wildcard CORS to prevent cross-origin websocket hijacking
socketio = SocketIO(app)

INDEX_HTML = """
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>NeuroForge Live Dashboard (Phase 2)</title>
  <style>
    body { font-family: system-ui, Segoe UI, Roboto, Helvetica, Arial, sans-serif; margin:0; background:#0b0c10; color:#f2f4f8; }
    header { padding:12px 16px; background:#111318; border-bottom:1px solid #23262d; display:flex; gap:12px; align-items:center; }
    select, label { color:#0b0c10; }
    .wrap { display:flex; gap:16px; padding:16px; }
    .panel { background:#141720; border:1px solid #23262d; border-radius:8px; padding:12px; flex:1; min-width:320px; }
    .panel h2 { margin:0 0 8px 0; font-size:14px; color:#c3c7cf; }
    table { width:100%; border-collapse: collapse; font-size:12px; }
    th, td { border-bottom:1px dashed #2a2f3a; padding:6px 8px; vertical-align: top; }
    th { text-align:left; color:#aeb4bf; }
    #heat { width:420px; height:420px; background:#0f1117; border:1px solid #23262d; border-radius:6px; }
    .meta { color:#aeb4bf; font-size:12px; }
  </style>
  <!-- Try local Socket.IO first; if not available, fall back to CDN. Chart.js remains via CDN. -->
  <script>
    (function(){
      function loadScript(src, onload, onerror){
        var s=document.createElement('script');
        s.src=src; s.async=true;
        s.onload=onload || function(){};
        s.onerror=onerror || function(){};
        document.head.appendChild(s);
      }
      if (typeof window.io === 'undefined'){
        loadScript('/static/socket.io.min.js', function(){ /* local loaded */ }, function(){
          loadScript('https://cdn.jsdelivr.net/npm/socket.io@4/dist/socket.io.min.js', function(){ /* CDN loaded */ }, function(){
            console.warn('Socket.IO client unavailable; will use SSE fallback');
          });
        });
      }
    })();
  </script>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
  <header>
    <h1 style=\"margin:0; font-size:18px;\">NeuroForge Live Dashboard (Phase 2 – Flask + Socket.IO)</h1>
    <div style=\"flex:1\"></div>
    <label style=\"color:#f2f4f8;\">Preset:
      <select id=\"preset\">
        <option value=\"accuracy\">accuracy</option>
        <option value=\"strength\">strength</option>
        <option value=\"max\">max</option>
        <option value=\"avg\">avg</option>
      </select>
    </label>
    <label style=\"margin-left:12px; color:#f2f4f8;\">
      <input type=\"checkbox\" id=\"noart\"/> --no-artifacts
    </label>
  </header>
  <div class=\"wrap\">
    <div class=\"panel\" style=\"max-width:460px;\">
      <h2>Heatmap</h2>
      <canvas id=\"heat\" width=\"420\" height=\"420\"></canvas>
      <div class=\"meta\" id=\"heat_meta\">Waiting for events…</div>
    </div>
    <div class=\"panel\">
      <h2>Live JSON Events</h2>
      <table>
        <thead><tr><th style=\"width:120px;\">Time</th><th style=\"width:120px;\">Type</th><th>Payload</th></tr></thead>
        <tbody id=\"log\"></tbody>
      </table>
    </div>
  </div>
  <div class=\"wrap\">
    <div class=\"panel\">
      <h2>Live Metric</h2>
      <canvas id=\"ts\" height=\"200\"></canvas>
    </div>
  </div>
<script>
(function(){
  const logBody = document.getElementById('log');
  const canvas = document.getElementById('heat');
  const ctx = canvas.getContext('2d');
  const meta = document.getElementById('heat_meta');
  const presetSel = document.getElementById('preset');
  const noart = document.getElementById('noart');

  const N = 10; let best = {i:-1,j:-1};
  function drawGrid(){
    const w=canvas.width,h=canvas.height; const cw=Math.floor(w/N), ch=Math.floor(h/N);
    ctx.clearRect(0,0,w,h);
    for (let i=0;i<N;i++) for (let j=0;j<N;j++) {
      const t=(i+j)/(2*(N-1)); const r=Math.floor(15+30*t), g=Math.floor(55+120*t), b=Math.floor(90+150*t);
      ctx.fillStyle = `rgb(${r},${g},${b})`; ctx.fillRect(j*cw, i*ch, cw-1, ch-1);
    }
    if (best.i>=0){ ctx.strokeStyle='#f2f4f8'; ctx.lineWidth=3; ctx.strokeRect(best.j*cw+1.5,best.i*ch+1.5,cw-3,ch-3); }
  }
  drawGrid();

  function row(ts, type, payload){
    const tr=document.createElement('tr');
    const td1=document.createElement('td'); td1.textContent=new Date(ts).toLocaleTimeString();
    const td2=document.createElement('td'); td2.textContent=type||'';
    const td3=document.createElement('td'); td3.textContent=(payload.length>240?payload.slice(0,240)+'…':payload);
    tr.append(td1,td2,td3); logBody.prepend(tr);
  }

  // 200ms debounce for heat updates
  let lastDraw=0; function updateBest(i,j){
    const now=Date.now(); if(now-lastDraw<200){ return; }
    lastDraw=now; best.i=i; best.j=j; drawGrid(); meta.textContent=`Best cell @ [${i}, ${j}]`;
  }

  // Unified event handler
  function handle(o){
    const ev = (o && (o.type || o.event)) ? (o.type || o.event) : 'event';
    row(Date.now(), ev, JSON.stringify(o));
    if (ev === 'panel_best'){
      // Prefer indices if provided; otherwise, derive simple indices from decay/seq_window
      let bi = (typeof o.best_i === 'number') ? o.best_i : -1;
      let bj = (typeof o.best_j === 'number') ? o.best_j : -1;
      if (bi < 0 && typeof o.decay === 'number'){
        // Map higher decay to lower row index (0..9)
        bi = Math.max(0, Math.min(9, Math.round((1 - Math.min(1, Math.max(0, o.decay))) * 9)));
      }
      if (bj < 0){
        if (typeof o.seq_window === 'number'){
          bj = Math.max(0, Math.min(9, o.seq_window % 10));
        } else if (typeof o.window === 'number'){
          bj = Math.max(0, Math.min(9, o.window % 10));
        }
      }
      if (bi >= 0 && bj >= 0){ updateBest(bi, bj); }
    }
    if (ev === 'scale'){
      // Optional: visualize vmin/vmax trend as a quick line; if ix provided use it
      if ('ix' in o) {
        addPoint(o.ix, o.value || (o.ix % 10));
      } else if ('vmin' in o && 'vmax' in o) {
        addPoint((Date.now()/1000)|0, (o.vmin + o.vmax) / 2);
      }
    }
  }

  // Prefer Socket.IO if available, otherwise fall back to SSE
  if (typeof io !== 'undefined'){
    const socket = io();
    socket.on('connect',()=>{ row(Date.now(),'socket','connected'); });
    socket.on('event',(o)=> handle(o));
    socket.on('disconnect',()=>{ row(Date.now(),'socket','disconnected'); });
    // controls
    presetSel.addEventListener('change',()=>{ socket.emit('control', {action:'set_preset', preset:presetSel.value}); });
    noart.addEventListener('change',()=>{ socket.emit('control', {action:'set_no_artifacts', value:noart.checked}); });
  } else {
    row(Date.now(),'info','Socket.IO client unavailable, using SSE fallback');
    const es = new EventSource('/events');
    es.onmessage = (evt)=>{ try{ handle(JSON.parse(evt.data)); }catch(e){ handle({type:'log', raw: evt.data}); } };
    es.onerror = ()=>{ row(Date.now(),'error','EventSource error'); };
    // controls not wired for SSE in MVP (server restarts producer on first load)
  }

  // simple line chart
  const tsCtx=document.getElementById('ts').getContext('2d');
  const chart=new Chart(tsCtx,{ type:'line', data:{ labels:[], datasets:[{label:'metric', data:[], borderColor:'#60a5fa', tension:0.25}]}, options:{ animation:false, responsive:true, scales:{ x:{display:false}, y:{beginAtZero:true}} }});
  function addPoint(x,y){ chart.data.labels.push(String(x)); chart.data.datasets[0].data.push(y); if(chart.data.labels.length>120){ chart.data.labels.shift(); chart.data.datasets[0].data.shift(); } chart.update('none'); }
})();
</script>
</body>
</html>
"""

producer_lock = threading.Lock()
producer: Producer | None = None

@app.route('/')
def index():
    return Response(INDEX_HTML, mimetype='text/html')

import sqlite3
import json as _json
import os as _os

def _db_conn():
    path = _os.environ.get('MEMDB', '')
    if not path:
        return None
    try:
        return sqlite3.connect(path, timeout=10)
    except Exception:
        return None

@app.route('/embeddings/run/<int:run_id>')
def embeddings_run(run_id: int):
    conn = _db_conn()
    if conn is None:
        return Response(_json.dumps({"error":"no db"}), mimetype='application/json', status=500)
    cur = conn.cursor()
    cur.execute("SELECT step, content_id, state_type, vec_json, meta_json FROM embeddings WHERE run_id=? ORDER BY step DESC LIMIT 500", (run_id,))
    rows = cur.fetchall()
    conn.close()
    out = []
    for step, cid, stype, vj, mj in rows:
        try:
            v = _json.loads(vj) if vj else {"vec": []}
            m = _json.loads(mj) if mj else {}
        except Exception:
            v = {"vec": []}
            m = {}
        out.append({"step": step, "content_id": cid, "state_type": stype, "vec": v.get("vec", []), "meta": m})
    return Response(_json.dumps(out), mimetype='application/json')

# SSE endpoint
@app.route('/events')
def events():
    def gen():
        q = broker.add_client()
        try:
            yield 'retry: 1000\n\n'
            while True:
                try:
                    data = q.get(timeout=10.0)
                except queue.Empty:
                    yield ': keep-alive\n\n'
                    continue
                yield f'data: {data}\n\n'
        finally:
            broker.remove_client(q)
    headers = {'Cache-Control':'no-cache', 'Connection':'keep-alive'}
    return Response(gen(), mimetype='text/event-stream', headers=headers)

@socketio.on('connect')
def on_connect():
    emit('event', {'type':'socket', 'message':'connected'})

@socketio.on('disconnect')
def on_disconnect():
    pass

@socketio.on('control')
def on_control(data):
    global producer
    with producer_lock:
        if not producer:
            return
        action = data.get('action')
        if action == 'set_preset':
            producer.preset = data.get('preset','accuracy')
            producer.start()
        elif action == 'set_no_artifacts':
            producer.no_artifacts = bool(data.get('value'))
            producer.start()


def main():
    ap = argparse.ArgumentParser(description='NeuroForge Phase 2 live dashboard (Flask + Socket.IO)')
    ap.add_argument('--host', type=str, default='127.0.0.1', help='Bind address (default: 127.0.0.1)')
    ap.add_argument('--port', type=int, default=8020)
    ap.add_argument('--brand-color', type=str, default=None)
    ap.add_argument('--preset', type=str, choices=['accuracy','strength','max','avg'], default='accuracy')
    ap.add_argument('--no-artifacts', action='store_true')
    args, extras = ap.parse_known_args()

    global producer
    producer = Producer(socketio, brand_color=args.brand_color, preset=args.preset, extras=extras, no_artifacts=args.no_artifacts)
    producer.start()

    print(f"[phase_c_flask] Socket.IO dashboard at http://{args.host}:{args.port}/", flush=True)
    socketio.run(app, host=args.host, port=args.port)

if __name__ == '__main__':
    main()
