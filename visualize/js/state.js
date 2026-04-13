export const NODE_STYLES = {
  none:      { fill: "#161b22", stroke: "#2d333b", text: "#6e7681" },
  worklist:  { fill: "#160f00", stroke: "#6a4400", text: "#d29922" },
  visited:   { fill: "#080e1c", stroke: "#1a3060", text: "#388bfd" },
  focus:     { fill: "#0c1c38", stroke: "#388bfd", text: "#a5d6ff" },
  collected: { fill: "#061408", stroke: "#1a4020", text: "#3fb950" },
  freeing:   { fill: "#1c0808", stroke: "#8a1a10", text: "#ffa198" },
  freed:     { fill: "#0d1117", stroke: "#161b22", text: "#3d444d" },
  leaked:    { fill: "#160404", stroke: "#8a1010", text: "#f85149" },
  global:    { fill: "#161b22", stroke: "#2d333b", text: "#6e7681" },
  global_sg: { fill: "#0e0c1a", stroke: "#3a2060", text: "#6e40c9" },
  // fill_hole mode states
  fh_hole:     { fill: "#1c1000", stroke: "#d29922", text: "#e3b341" },
  fh_term:     { fill: "#001c08", stroke: "#3fb950", text: "#7ee787" },
  fh_queue:    { fill: "#080e1c", stroke: "#388bfd", text: "#a5d6ff" },
  fh_checking: { fill: "#0c1020", stroke: "#79c0ff", text: "#cae8ff" },
  fh_changed:  { fill: "#1c0808", stroke: "#f85149", text: "#ffa198" },
  fh_stopped:  { fill: "#0c0818", stroke: "#8957e5", text: "#d2a8ff" },
  fh_default:  { fill: "#161b22", stroke: "#2d333b", text: "#6e7681" },
  // flat_gc mode states
  fg_freeing: { fill: "#1c0808", stroke: "#f85149", text: "#ffa198" },
  fg_freed:   { fill: "#0d1117", stroke: "#161b22", text: "#3d444d" },
  fg_pending: { fill: "#161b22", stroke: "#2d333b", text: "#6e7681" },
};

export const EDGE_KINDS = {
  type: {
    dimColor: "#6a5000",
    activeColor: "#d29922",
    dash: "",
    curvature: 0,
    markerDim: "ar-ty",
    markerActive: "ar-ty-a",
  },
  ctx: {
    dimColor: "#1a5c2a",
    activeColor: "#3fb950",
    dash: "5,4",
    curvature: -18,
    markerDim: "ar-ct",
    markerActive: "ar-ct-a",
  },
  child: {
    dimColor: "#1a3a6e",
    activeColor: "#79c0ff",
    dash: "",
    curvature: 0,
    markerDim: "ar-ch",
    markerActive: "ar-ch-a",
  },
  nonown: {
    dimColor: "#3a1870",
    activeColor: "#8957e5",
    dash: "3,3",
    curvature: -60,
    markerDim: "ar-nr",
    markerActive: "ar-nr-a",
  },
  uplink: {
    dimColor: "#5a1010",
    activeColor: "#f85149",
    dash: "4,3",
    curvature: 55,
    markerDim: "ar-ty",
    markerActive: "ar-ty-a",
  },
  // fill_hole: original edge (pre-rewrite), fades after rewrite
  old_edge: {
    dimColor: "#3d444d",
    activeColor: "#8b949e",
    dash: "",
    curvature: 0,
    markerDim: "ar-ch",
    markerActive: "ar-ch-a",
  },
  // fill_hole: new edge to term (post-rewrite), appears after rewrite
  new_edge: {
    dimColor: "#0a2040",
    activeColor: "#79c0ff",
    dash: "5,3",
    curvature: -18,
    markerDim: "ar-ch",
    markerActive: "ar-ch-a",
  },
};

export function getNodeState(id, step) {
  if (!step) return "none";

  // ── flat_gc mode ──────────────────────────────────────────────────────────
  if (step.mode === "flat_gc") {
    const freedSet = new Set(step.freed || []);
    if (id === step.active) return "fg_freeing";
    if (freedSet.has(id)) return "fg_freed";
    return "fg_pending";
  }

  // ── fill_hole mode ──────────────────────────────────────────────────────
  if (step.mode === "fill_hole") {
    if (id === step.holeNode) return "fh_hole";
    if (id === step.termNode) return "fh_term";
    if (step.op === "fh_prop" && step.active === id) return "fh_changed";
    if (step.op === "fh_stop" && step.active === id) return "fh_stopped";
    if ((step.bfsChanged || []).includes(id)) return "fh_changed";
    if ((step.bfsStopped || []).includes(id)) return "fh_stopped";
    if ((step.bfsQueue || []).includes(id)) return "fh_queue";
    return "fh_default";
  }

  // ── bulk_collect mode ───────────────────────────────────────────────────
  if (step.leaked?.includes(id)) return "leaked";
  const freedSet = new Set(step.freed);
  if (step.op === "fr" && step.active === id) return "freeing";
  if (freedSet.has(id)) return "freed";
  if (step.op === "dq" && step.active === id) return "focus";
  if ((step.collected || []).includes(id)) return "collected";
  if ((step.worklist || []).includes(id)) return "worklist";
  if ((step.visited || []).includes(id)) return "visited";
  return "none";
}
