#!/usr/bin/env python3
import argparse
import json
import os
import queue
import signal
import sys
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, HTTPServer
from subprocess import Popen, PIPE
from typing import List

# -----------------------------
# Simple SSE broker (stdlib)
# -----------------------------
class SSEBroker:
    def __init__(self):
        self._clients: List[queue.Queue] = []
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


# -----------------------------
# Minimal HTML page (inline)
# -----------------------------
INDEX_HTML = """
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>NeuroForge Live Dashboard (MVP)</title>
  <style>
    body { font-family: system-ui, Segoe UI, Roboto, Helvetica, Arial, sans-serif; margin: 0; background:#0b0c10; color:#f2f4f8; }
    header { padding: 12px 16px; background:#111318; border-bottom: 1px solid #23262d; }
    h1 { margin: 0; font-size: 18px; }
    .wrap { display:flex; gap:16px; padding: 16px; }
    .panel { background:#141720; border:1px solid #23262d; border-radius:8px; padding:12px; flex:1; min-width: 320px; }
    .panel h2 { margin:0 0 8px 0; font-size:14px; color:#c3c7cf; }
    table { width:100%; border-collapse: collapse; font-size: 12px; }
    th, td { border-bottom:1px dashed #2a2f3a; padding:6px 8px; vertical-align: top; }
    th { text-align:left; color:#aeb4bf; }
    #heat { width: 420px; height: 420px; background:#0f1117; border:1px solid #23262d; border-radius:6px; }
    .meta { color:#aeb4bf; font-size:12px; }
    .ok { color:#a3e635; }
    .warn { color:#fbbf24; }
  </style>
</head>
<body>
  <header>
    <h1>NeuroForge Live Dashboard (Phase C – MVP)</h1>
    <div class="meta">SSE over stdlib http.server • JSON-only stream • Placeholder heatmap</div>
  </header>
  <div class="wrap">
    <div class="panel" style="max-width:460px;">
      <h2>Placeholder Heatmap</h2>
      <canvas id="heat" width="420" height="420"></canvas>
      <div class="meta" id="heat_meta">Waiting for events…</div>
    </div>
    <div class="panel">
      <h2>Live JSON Events</h2>
      <table>
        <thead>
          <tr><th style="width:120px;">Time</th><th style="width:120px;">Type</th><th>Payload</th></tr>
        </thead>
        <tbody id="log"></tbody>
      </table>
    </div>
  </div>
<script>
(function(){
  const logBody = document.getElementById('log');
  const canvas = document.getElementById('heat');
  const ctx = canvas.getContext('2d');
  const meta = document.getElementById('heat_meta');

  const N = 10; // grid size placeholder
  let best = {i: -1, j: -1};

  function drawGrid(){
    const w = canvas.width, h = canvas.height;
    const cellW = Math.floor(w / N), cellH = Math.floor(h / N);
    ctx.clearRect(0,0,w,h);
    // base gradient placeholder (brand-like ramp)
    for (let i=0;i<N;i++){
      for (let j=0;j<N;j++){
        const t = (i + j) / (2*(N-1));
        const r = Math.floor(15 + 30*t);
        const g = Math.floor(55 + 120*t);
        const b = Math.floor(90 + 150*t);
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.fillRect(j*cellW, i*cellH, cellW-1, cellH-1);
      }
    }
    // best cell outline
    if (best.i >= 0 && best.j >= 0){
      ctx.strokeStyle = '#f2f4f8';
      ctx.lineWidth = 3;
      ctx.strokeRect(best.j*cellW+1.5, best.i*cellH+1.5, cellW-3, cellH-3);
    }
  }

  drawGrid();

  function row(time, type, payload){
    const tr = document.createElement('tr');
    const td1 = document.createElement('td'); td1.textContent = new Date(time).toLocaleTimeString();
    const td2 = document.createElement('td'); td2.textContent = type || '';
    const td3 = document.createElement('td'); td3.textContent = payload.length > 240 ? payload.slice(0,240) + '…' : payload;
    tr.appendChild(td1); tr.appendChild(td2); tr.appendChild(td3);
    logBody.prepend(tr);
  }

  const es = new EventSource('/events');
  es.onmessage = (evt) => {
    let o = null;
    try { o = JSON.parse(evt.data); } catch(e) { o = { raw: evt.data } }
    const t = (o && o.type) ? o.type : 'event';
    row(Date.now(), t, evt.data);
    if (t === 'panel_best'){
      // expected fields: capacity_ix, best_i, best_j
      if ('best_i' in o && 'best_j' in o){
        best.i = o.best_i; best.j = o.best_j;
        drawGrid();
        meta.textContent = `Best cell @ [${best.i}, ${best.j}]`;
      }
    }
  };
  es.onerror = () => {
    row(Date.now(), 'error', 'EventSource connection error');
  };
})();
</script>
</body>
</html>
"""


# -----------------------------
# HTTP Handler
# -----------------------------
class LiveHandler(BaseHTTPRequestHandler):
    broker: SSEBroker = None  # to be set externally

    def log_message(self, fmt, *args):
        # keep server output clean
        pass

    def do_GET(self):
        if self.path == '/' or self.path.startswith('/index'):
            self.send_response(HTTPStatus.OK)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Cache-Control', 'no-cache')
            self.end_headers()
            self.wfile.write(INDEX_HTML.encode('utf-8'))
            return
        if self.path.startswith('/events'):
            self.send_response(HTTPStatus.OK)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-cache')
            self.send_header('Connection', 'keep-alive')
            self.end_headers()

            q = self.broker.add_client()
            try:
                # initial retry hint
                self.wfile.write(b'retry: 1000\n\n')
                self.wfile.flush()
                while True:
                    try:
                        data = q.get(timeout=1.0)
                    except queue.Empty:
                        # keep-alive comment
                        self.wfile.write(b': keep-alive\n\n')
                        self.wfile.flush()
                        continue
                    payload = f"data: {data}\n\n".encode('utf-8')
                    self.wfile.write(payload)
                    self.wfile.flush()
            except Exception:
                # client disconnected
                pass
            finally:
                self.broker.remove_client(q)
            return

        self.send_response(HTTPStatus.NOT_FOUND)
        self.end_headers()


# -----------------------------
# Producer subprocess (figure)
# -----------------------------
class Producer:
    def __init__(self, broker: SSEBroker, brand_color: str = None, preset: str = 'accuracy', extras: List[str] | None = None):
        self._broker = broker
        self._proc: Popen | None = None
        self._thread: threading.Thread | None = None
        self._brand_color = brand_color
        self._preset = preset
        self._extras = extras or []

    def start(self):
        cmd = [sys.executable, os.path.join('scripts', 'generate_sweep_small_multiples.py')]
        # inject preset shorthand unless user provided their own metric shorthand
        metric_shorthands = {'-Pa', '-Ps', '-Pm', '-Pv'}
        extras_set = set(self._extras)
        if metric_shorthands.isdisjoint(extras_set):
            preset_map = {
                'accuracy': '-Pa',
                'strength': '-Ps',
                'max': '-Pm',
                'avg': '-Pv',
            }
            cmd.append(preset_map.get(self._preset, '-Pa'))
        # always enable JSON streaming for live dashboard
        cmd.extend(['--phase-c-log-json', '--json-only'])
        # pass brand color if not already provided in extras
        if self._brand_color and ('--brand-color' not in extras_set):
            cmd.extend(['--brand-color', self._brand_color])
        # finally append any pass-through args
        if self._extras:
            cmd.extend(self._extras)
        # spawn
        self._proc = Popen(cmd, stdout=PIPE, stderr=PIPE, bufsize=1, universal_newlines=True)
        self._thread = threading.Thread(target=self._pump, daemon=True)
        self._thread.start()
        threading.Thread(target=self._pump_err, daemon=True).start()

    def _pump(self):
        assert self._proc is not None
        for line in self._proc.stdout:
            line = line.strip()
            if not line:
                continue
            # Expect JSON lines; if not, wrap
            try:
                obj = json.loads(line)
                data = json.dumps(obj)
            except Exception:
                data = json.dumps({"type": "log", "message": line})
            self._broker.broadcast(data)
        # signal done
        self._broker.broadcast(json.dumps({"type": "producer_done", "ts": time.time()}))

    def _pump_err(self):
        if self._proc is None or self._proc.stderr is None:
            return
        for line in self._proc.stderr:
            line = line.strip()
            if not line:
                continue
            self._broker.broadcast(json.dumps({"type": "stderr", "message": line}))

    def stop(self):
        if self._proc and self._proc.poll() is None:
            try:
                if os.name == 'nt':
                    self._proc.terminate()
                else:
                    self._proc.send_signal(signal.SIGINT)
            except Exception:
                pass


# -----------------------------
# Main entrypoint
# -----------------------------
DEFAULT_PORT = 8008

def serve(port: int, brand_color: str | None, preset: str, extras: List[str]):
    broker = SSEBroker()
    LiveHandler.broker = broker

    httpd = HTTPServer(('0.0.0.0', port), LiveHandler)

    prod = Producer(broker, brand_color=brand_color, preset=preset, extras=extras)
    prod.start()

    print(f"[phase_c_run] SSE live dashboard at http://localhost:{port}/", flush=True)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        prod.stop()
        httpd.server_close()


def main():
    ap = argparse.ArgumentParser(description='NeuroForge Phase C live dashboard (MVP: stdlib SSE)')
    ap.add_argument('--live', action='store_true', help='Run live SSE server')
    ap.add_argument('--port', type=int, default=DEFAULT_PORT, help='Port to serve UI (default: 8008)')
    ap.add_argument('--brand-color', type=str, default=None, help='Optional brand color to pass through to figure producer')
    ap.add_argument('--preset', type=str, choices=['accuracy','strength','max','avg'], default='accuracy', help='Deck preset to run (default: accuracy)')
    args, extras = ap.parse_known_args()

    # Strip any standalone "--" separators from pass-through args
    if extras:
        extras = [x for x in extras if x != "--"]

    if not args.live:
        print('Use --live to start the live dashboard server.', file=sys.stderr)
        sys.exit(2)

    serve(args.port, brand_color=args.brand_color, preset=args.preset, extras=extras)


if __name__ == '__main__':
    main()