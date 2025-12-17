import os
import sys
import json
import sqlite3
from typing import List, Dict, Any, Optional, Tuple

try:
    from dash import Dash, dcc, html
    import plotly.express as px
    import plotly.graph_objects as go
except Exception:
    Dash = None
    dcc = None
    html = None
    px = None
    go = None

DB_SCHEMA: Dict[str, Any] = {
    "runs": ["id", "started_ms", "metadata_json"],
    "experiences": [
        "id", "run_id", "ts_ms", "step", "tag", "input_json", "output_json", "significant"
    ],
    "episodes": ["id", "run_id", "name", "start_ms", "end_ms"],
    "episode_experiences": ["episode_id", "experience_id"],
    "episode_stats": ["episode_id", "steps", "success", "episode_return"],
    "reward_log": ["id", "run_id", "ts_ms", "step", "reward", "source", "context_json"],
    "learning_stats": [
        "id", "run_id", "ts_ms", "step", "processing_hz", "total_updates", "hebbian_updates",
        "stdp_updates", "reward_updates", "avg_weight_change", "consolidation_rate", "active_synapses",
        "potentiated_synapses", "depressed_synapses", "avg_energy", "metabolic_hazard"
    ],
    "hippocampal_snapshots": [
        "id", "run_id", "ts_ms", "step", "priority", "significance", "snapshot_data"
    ],
    "motivation_state": ["id", "run_id", "ts_ms", "motivation", "coherence", "notes"],
}


def connect(db_path: str) -> sqlite3.Connection:
    return sqlite3.connect(db_path)


def latest_run_id(con: sqlite3.Connection) -> Optional[int]:
    cur = con.cursor()
    row = cur.execute("SELECT id FROM runs ORDER BY id DESC LIMIT 1").fetchone()
    return row[0] if row else None


def list_runs(con: sqlite3.Connection) -> List[Tuple[int, int]]:
    cur = con.cursor()
    return cur.execute("SELECT id, started_ms FROM runs ORDER BY id DESC").fetchall()


def fetch_experiences(con: sqlite3.Connection, run_id: int, tag: Optional[str] = None) -> List[Dict[str, Any]]:
    cur = con.cursor()
    if tag:
        rows = cur.execute(
            "SELECT id, ts_ms, step, tag, input_json, output_json, significant FROM experiences WHERE run_id=? AND tag=? ORDER BY ts_ms",
            (run_id, tag),
        ).fetchall()
    else:
        rows = cur.execute(
            "SELECT id, ts_ms, step, tag, input_json, output_json, significant FROM experiences WHERE run_id=? ORDER BY ts_ms",
            (run_id,),
        ).fetchall()
    out: List[Dict[str, Any]] = []
    for r in rows:
        d: Dict[str, Any] = {
            "id": r[0], "ts_ms": r[1], "step": r[2], "tag": r[3], "significant": r[6]
        }
        try:
            d["input"] = json.loads(r[4]) if r[4] else {}
        except Exception:
            d["input"] = {}
        try:
            d["output"] = json.loads(r[5]) if r[5] else {}
        except Exception:
            d["output"] = {}
        out.append(d)
    return out


def fetch_reward_log(con: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    cur = con.cursor()
    rows = cur.execute(
        "SELECT id, ts_ms, step, reward, source, context_json FROM reward_log WHERE run_id=? ORDER BY ts_ms",
        (run_id,),
    ).fetchall()
    out: List[Dict[str, Any]] = []
    for r in rows:
        d: Dict[str, Any] = {"id": r[0], "ts_ms": r[1], "step": r[2], "reward": r[3], "source": r[4]}
        try:
            d["context"] = json.loads(r[5]) if r[5] else {}
        except Exception:
            d["context"] = {}
        out.append(d)
    return out


def fetch_learning_stats(con: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    cur = con.cursor()
    rows = cur.execute(
        "SELECT ts_ms, step, processing_hz, total_updates, hebbian_updates, stdp_updates, reward_updates, avg_weight_change, consolidation_rate, active_synapses, potentiated_synapses, depressed_synapses, avg_energy, metabolic_hazard FROM learning_stats WHERE run_id=? ORDER BY ts_ms",
        (run_id,),
    ).fetchall()
    out: List[Dict[str, Any]] = []
    for r in rows:
        out.append({
            "ts_ms": r[0], "step": r[1], "processing_hz": r[2], "total_updates": r[3], "hebbian_updates": r[4],
            "stdp_updates": r[5], "reward_updates": r[6], "avg_weight_change": r[7], "consolidation_rate": r[8],
            "active_synapses": r[9], "potentiated_synapses": r[10], "depressed_synapses": r[11], "avg_energy": r[12],
            "metabolic_hazard": r[13]
        })
    return out


def fetch_motivation_state(con: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    cur = con.cursor()
    rows = cur.execute(
        "SELECT ts_ms, motivation, coherence FROM motivation_state WHERE run_id=? ORDER BY ts_ms",
        (run_id,),
    ).fetchall()
    return [{"ts_ms": r[0], "motivation": r[1], "coherence": r[2]} for r in rows]


def fetch_hippocampal_snapshots(con: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    cur = con.cursor()
    rows = cur.execute(
        "SELECT ts_ms, step, priority, significance FROM hippocampal_snapshots WHERE run_id=? ORDER BY ts_ms",
        (run_id,),
    ).fetchall()
    return [{"ts_ms": r[0], "step": r[1], "priority": r[2], "significance": r[3]} for r in rows]


def build_ingestion_fig(items: List[Dict[str, Any]]) -> Any:
    if px is None:
        return {}
    if not items:
        return px.scatter(x=[], y=[])
    xs = [it["step"] for it in items]
    ys = list(range(1, len(items) + 1))
    fig = px.line(x=xs, y=ys, labels={"x": "step", "y": "cumulative_ingestion"}, title="Triplet Ingestion")
    return fig


def build_reward_fig(rewards: List[Dict[str, Any]]) -> Tuple[Any, Any]:
    if px is None:
        return {}, {}
    if not rewards:
        return px.line(x=[], y=[], title="Reward Over Time"), px.pie(values=[], names=[], title="Reward Sources")
    fig_ts = px.line(
        x=[r["step"] for r in rewards], y=[r["reward"] for r in rewards], labels={"x": "step", "y": "reward"}, title="Reward Over Time"
    )
    src_counts: Dict[str, int] = {}
    for r in rewards:
        src = r.get("source") or "unknown"
        src_counts[src] = src_counts.get(src, 0) + 1
    fig_src = px.pie(values=list(src_counts.values()), names=list(src_counts.keys()), title="Reward Sources")
    return fig_ts, fig_src


def build_coherence_fig(mot: List[Dict[str, Any]]) -> Any:
    if px is None:
        return {}
    if not mot:
        return px.line(x=[], y=[], title="Coherence")
    fig = px.line(x=[m["ts_ms"] for m in mot], y=[m["coherence"] for m in mot], labels={"x": "ts_ms", "y": "coherence"}, title="Coherence Timeline")
    return fig


def build_assembly_fig(stats: List[Dict[str, Any]]) -> Any:
    if px is None:
        return {}
    if not stats:
        return px.line(x=[], y=[], title="Assembly Count (proxy)")
    fig = px.line(x=[s["ts_ms"] for s in stats], y=[s["active_synapses"] for s in stats], labels={"x": "ts_ms", "y": "active_synapses"}, title="Assembly Count (Active Synapses)")
    return fig


def build_hazard_fig(stats: List[Dict[str, Any]]) -> Any:
    if px is None:
        return {}
    if not stats:
        return px.line(x=[], y=[], title="Hazard Dynamics")
    fig = px.line(x=[s["ts_ms"] for s in stats], y=[s.get("metabolic_hazard", 0.0) for s in stats], labels={"x": "ts_ms", "y": "metabolic_hazard"}, title="Hazard Dynamics")
    return fig


def build_hippocampal_fig(snaps: List[Dict[str, Any]]) -> Any:
    if px is None:
        return {}
    if not snaps:
        return px.scatter(x=[], y=[], title="Hippocampal Snapshot Priority")
    fig = px.scatter(
        x=[s["ts_ms"] for s in snaps], y=[s["priority"] for s in snaps], size=[max(0.1, s["significance"]) for s in snaps],
        labels={"x": "ts_ms", "y": "priority"}, title="Hippocampal Snapshot Priority"
    )
    return fig


def build_similarity_fig(items: List[Dict[str, Any]], top_n: int = 50) -> Any:
    if go is None:
        return {}
    stems: Dict[str, Dict[str, int]] = {}
    for it in items:
        inp = it.get("input", {})
        stem = None
        tchr = inp.get("teacher_id")
        if isinstance(tchr, str) and tchr.startswith("triplet_"):
            stem = tchr[len("triplet_"):]
        if not stem:
            img = inp.get("image") or ""
            aud = inp.get("audio") or ""
            cap = inp.get("caption") or ""
            for p in (img, aud, cap):
                if p and isinstance(p, str):
                    base = os.path.basename(p)
                    name = os.path.splitext(base)[0]
                    stem = name
                    break
        if not stem:
            continue
        m = stems.get(stem)
        if not m:
            m = {"image": 0, "audio": 0, "caption": 0}
            stems[stem] = m
        if inp.get("image"):
            m["image"] = 1
        if inp.get("audio"):
            m["audio"] = 1
        if inp.get("caption"):
            m["caption"] = 1
    stem_items = sorted(stems.items(), key=lambda kv: sum(kv[1].values()), reverse=True)[:top_n]
    if not stem_items:
        return go.Figure(data=go.Heatmap(z=[[0, 0, 0]], x=["image", "audio", "caption"], y=["no_data"]))
    y_labels = [s for s, _ in stem_items]
    z = [[m["image"], m["audio"], m["caption"]] for _, m in stem_items]
    fig = go.Figure(data=go.Heatmap(z=z, x=["image", "audio", "caption"], y=y_labels, colorscale="Blues"))
    fig.update_layout(title="Cross-Modal Similarity (Stem Presence)")
    return fig


def build_app(db_path: str) -> Optional[Dash]:
    if Dash is None:
        return None
    con = connect(db_path)
    runs = list_runs(con)
    rid = latest_run_id(con)
    items = fetch_experiences(con, rid or -1, tag="triplet_ingestion") if rid else []
    rewards = fetch_reward_log(con, rid or -1) if rid else []
    stats = fetch_learning_stats(con, rid or -1) if rid else []
    mot = fetch_motivation_state(con, rid or -1) if rid else []
    snaps = fetch_hippocampal_snapshots(con, rid or -1) if rid else []
    con.close()

    app = Dash(__name__)
    app.layout = html.Div([
        html.H2("NeuroForge MemoryDB Dashboard"),
        dcc.Dropdown(
            id="run-id",
            options=[{"label": f"Run {r[0]}", "value": r[0]} for r in runs],
            value=rid,
            clearable=False,
        ),
        dcc.Graph(id="ingestion-fig", figure=build_ingestion_fig(items)),
        dcc.Graph(id="reward-fig", figure=build_reward_fig(rewards)[0]),
        dcc.Graph(id="reward-source-fig", figure=build_reward_fig(rewards)[1]),
        dcc.Graph(id="coherence-fig", figure=build_coherence_fig(mot)),
        dcc.Graph(id="assembly-fig", figure=build_assembly_fig(stats)),
        dcc.Graph(id="hazard-fig", figure=build_hazard_fig(stats)),
        dcc.Graph(id="hippocampal-fig", figure=build_hippocampal_fig(snaps)),
        dcc.Graph(id="similarity-fig", figure=build_similarity_fig(items)),
        html.Div(id="db-path", style={"display": "none"}, children=db_path),
    ])

    @app.callback(
        [
            dcc.Output("ingestion-fig", "figure"),
            dcc.Output("reward-fig", "figure"),
            dcc.Output("reward-source-fig", "figure"),
            dcc.Output("coherence-fig", "figure"),
            dcc.Output("assembly-fig", "figure"),
            dcc.Output("hazard-fig", "figure"),
            dcc.Output("hippocampal-fig", "figure"),
            dcc.Output("similarity-fig", "figure"),
        ],
        [dcc.Input("run-id", "value")],
    )
    def update_figures(run_id: Optional[int]):
        dbp = app.layout.children[-1].children  # html.Div db-path
        con = connect(dbp)
        items2 = fetch_experiences(con, run_id or -1, tag="triplet_ingestion") if run_id else []
        rewards2 = fetch_reward_log(con, run_id or -1) if run_id else []
        stats2 = fetch_learning_stats(con, run_id or -1) if run_id else []
        mot2 = fetch_motivation_state(con, run_id or -1) if run_id else []
        snaps2 = fetch_hippocampal_snapshots(con, run_id or -1) if run_id else []
        con.close()
        return (
            build_ingestion_fig(items2),
            build_reward_fig(rewards2)[0],
            build_reward_fig(rewards2)[1],
            build_coherence_fig(mot2),
            build_assembly_fig(stats2),
            build_hazard_fig(stats2),
            build_hippocampal_fig(snaps2),
            build_similarity_fig(items2),
        )

    return app


def run_test(db_path: str, run_id: Optional[int] = None) -> None:
    con = connect(db_path)
    rid = run_id or latest_run_id(con)
    items = fetch_experiences(con, rid or -1, tag="triplet_ingestion") if rid else []
    rewards = fetch_reward_log(con, rid or -1) if rid else []
    stats = fetch_learning_stats(con, rid or -1) if rid else []
    mot = fetch_motivation_state(con, rid or -1) if rid else []
    snaps = fetch_hippocampal_snapshots(con, rid or -1) if rid else []
    con.close()
    print("DB", db_path)
    print("RUN_ID", rid)
    print("INGESTION_COUNT", len(items))
    print("REWARD_COUNT", len(rewards))
    print("LEARNING_STATS_COUNT", len(stats))
    print("MOTIVATION_STATE_COUNT", len(mot))
    print("HIPPOCAMPAL_SNAPSHOTS_COUNT", len(snaps))


def main(argv: List[str]) -> None:
    db_default = os.path.join(os.path.dirname(os.path.dirname(__file__)), "phasec_mem.db")
    db_path = db_default
    run_id: Optional[int] = None
    port = int(os.environ.get("NF_DASH_PORT", "8050"))
    for arg in argv[1:]:
        if arg.startswith("--db="):
            db_path = arg.split("=", 1)[1]
        elif arg.startswith("--run="):
            try:
                run_id = int(arg.split("=", 1)[1])
            except Exception:
                pass
        elif arg == "--test":
            run_test(db_path, run_id)
            return
    if Dash is None:
        print("Dash/Plotly not available. Install with: pip install dash plotly")
        run_test(db_path, run_id)
        return
    app = build_app(db_path)
    if app is None:
        print("Failed to initialize dashboard.")
        return
    app.run_server(host="127.0.0.1", port=port, debug=False)


if __name__ == "__main__":
    main(sys.argv)

