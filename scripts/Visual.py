# Interactive 3D visualization of the NeuroForge connectivity CSV
# - Builds a NetworkX DiGraph from connections_df (already loaded earlier)
# - Selects a manageable subset of important nodes for interactivity
# - Uses a 3D force-directed layout and renders with Plotly
#
# You can download the standalone HTML after it renders.

import pandas as pd
import numpy as np
import networkx as nx
import plotly.graph_objs as go
from plotly.offline import plot as plot_offline

# Ensure the dataframe exists (loaded in a previous step)
file_path = "C:\\Users\\ashis\\Desktop\\NeuroForge\\connections.csv"
connections_df = pd.read_csv(file_path)

# Build graph
G_full = nx.DiGraph()
G_full.add_weighted_edges_from(
    connections_df[['pre_neuron', 'post_neuron', 'weight']].itertuples(index=False, name=None)
)

# ---- Choose a subset for responsiveness ----
# Strategy: take top-N nodes by (in+out) degree, keep edges among them, then prune edges to top weights
N = 500  # adjust if you want more/less detail
degree_dict = dict(G_full.degree())
top_nodes = sorted(degree_dict, key=degree_dict.get, reverse=True)[:N]
G = G_full.subgraph(top_nodes).copy()

# Prune edges by weight percentile to reduce visual clutter
weights = np.array([d['weight'] for _,_,d in G.edges(data=True)])
if len(weights) > 0:
    cutoff = np.percentile(weights, 75)  # keep top 25% by weight
    edges_to_remove = [(u,v) for u,v,d in G.edges(data=True) if d.get('weight',0) < cutoff]
    G.remove_edges_from(edges_to_remove)

# If too few edges remain (e.g., small graph), keep at least some
if G.number_of_edges() < 200:
    # fall back to keeping top 200 edges by weight
    edges_sorted = sorted(G_full.subgraph(top_nodes).edges(data=True), key=lambda e: e[2].get('weight',0), reverse=True)
    G = nx.DiGraph()
    G.add_nodes_from(top_nodes)
    for u,v,d in edges_sorted[:200]:
        G.add_edge(u,v, **d)

# 3D force-directed layout
pos = nx.spring_layout(G, dim=3, seed=42)

# Build Plotly traces
# Edge trace
edge_x = []
edge_y = []
edge_z = []
for u, v in G.edges():
    x0, y0, z0 = pos[u]
    x1, y1, z1 = pos[v]
    edge_x += [x0, x1, None]
    edge_y += [y0, y1, None]
    edge_z += [z0, z1, None]

edge_trace = go.Scatter3d(
    x=edge_x, y=edge_y, z=edge_z,
    mode='lines',
    line=dict(width=1),  # no explicit color
    hoverinfo='none'
)

# Node trace (size scaled by degree)
node_x = []
node_y = []
node_z = []
text = []
sizes = []
for n in G.nodes():
    x, y, z = pos[n]
    node_x.append(x)
    node_y.append(y)
    node_z.append(z)
    deg_in = G.in_degree(n)
    deg_out = G.out_degree(n)
    sizes.append(4 + 2 * np.log1p(deg_in + deg_out))
    text.append(f"Neuron {n}<br>in={deg_in}, out={deg_out}")

node_trace = go.Scatter3d(
    x=node_x, y=node_y, z=node_z,
    mode='markers',
    marker=dict(size=sizes),  # no explicit color
    text=text,
    hoverinfo='text'
)

fig = go.Figure(data=[edge_trace, node_trace])

fig.update_layout(
    title="NeuroForge Connectivity (3D interactive) — Top nodes & strongest edges",
    showlegend=False,
    scene=dict(
        xaxis=dict(showgrid=False, zeroline=False, showticklabels=False),
        yaxis=dict(showgrid=False, zeroline=False, showticklabels=False),
        zaxis=dict(showgrid=False, zeroline=False, showticklabels=False),
    ),
    margin=dict(l=0, r=0, t=40, b=0)
)

# Show inline
fig

# Save as a standalone HTML for download/sharing
outfile = "C:/Users/ashis/Desktop/NeuroForge/neuroforge_3d_graph.html"
plot_offline(fig, filename=outfile, auto_open=False)
outfile
