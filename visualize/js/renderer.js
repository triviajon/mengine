import {
  nodeSize,
  attachPoint,
  quadraticControlPoint,
  svgElement,
} from "./geometry.js";
import { getNodeState, NODE_STYLES, EDGE_KINDS } from "./state.js";
import { CODE_LINES, FILL_CODE_LINES, FLAT_GC_CODE_LINES, CODE_HIGHLIGHT_CLASSES } from "./code-lines.js";

const PAD = 20;
function computeViewBox(nodes) {
  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  for (const node of Object.values(nodes)) {
    const [w, h] = nodeSize(node);
    minX = Math.min(minX, node.x - w / 2);
    minY = Math.min(minY, node.y - h / 2);
    maxX = Math.max(maxX, node.x + w / 2);
    maxY = Math.max(maxY, node.y + h / 2);
  }
  return `${minX - PAD} ${minY - PAD} ${maxX - minX + PAD * 2} ${maxY - minY + PAD * 2}`;
}

function collectEdges(scenario, step) {
  const edges = [];
  const rewritten = new Set(step?.rewritten || []);
  const isFill = step?.mode === "fill_hole";

  for (const [id, node] of Object.entries(scenario.nodes)) {
    if (node.type) edges.push({ from: id, to: node.type, kind: "type" });
    if (node.ctx) edges.push({ from: id, to: node.ctx, kind: "ctx" });
    for (const child of node.children || []) {
      if (isFill && child.rewritesTo) {
        // This slot will be rewritten from child.to → child.rewritesTo.
        // Before rewrite: show old edge (solid) + new edge (dashed, pending).
        // After rewrite: show new edge (solid) + old edge (very dim).
        const parentRewritten = rewritten.has(id);
        edges.push({
          from: id,
          to: child.to,
          kind: parentRewritten ? "old_edge" : "child",
          rel: child.rel,
        });
        edges.push({
          from: id,
          to: child.rewritesTo,
          kind: parentRewritten ? "child" : "new_edge",
          rel: child.rel,
        });
      } else {
        edges.push({ from: id, to: child.to, kind: "child", rel: child.rel });
      }
    }
  }
  for (const u of scenario.nonowns || [])
    edges.push({ from: u.from, to: u.to, kind: "nonown", label: u.label });
  for (const u of scenario.uplinks || [])
    edges.push({ from: u.from, to: u.to, kind: "uplink", label: u.label });
  return edges;
}

function drawRootArrow(gBg, scenario) {
  const root = scenario.nodes[scenario.root];
  const [w] = nodeSize(root);
  const rx = root.x - w / 2;
  const ry = root.y;

  gBg.appendChild(
    svgElement("path", {
      d: `M${rx - 36},${ry}L${rx - 4},${ry}`,
      stroke: "#1c2128",
      "stroke-width": "1",
      fill: "none",
      "marker-end": "url(#ar-ch)",
    }),
  );

  const t = svgElement("text", {
    x: rx - 38,
    y: ry,
    fill: "#282e38",
    "font-size": "8",
    "font-family": "monospace",
    "text-anchor": "end",
    "dominant-baseline": "middle",
  });
  t.textContent = "rt→ctx";
  gBg.appendChild(t);
}

function drawEdges(gBg, gAct, edges, nodes, activeEdge, step) {
  const rewritten = new Set(step?.rewritten || []);
  const isFill = step?.mode === "fill_hole";

  for (const e of edges) {
    const n1 = nodes[e.from];
    const n2 = nodes[e.to];
    if (!n1 || !n2) continue;

    const ek = EDGE_KINDS[e.kind] || EDGE_KINDS.child;

    // In fill_hole mode, old_edge fades to near-invisible after rewrite;
    // new_edge is pending (very dim) before rewrite, normal after.
    let baseOpacity = 0.55;
    if (isFill) {
      if (e.kind === "old_edge") baseOpacity = rewritten.has(e.from) ? 0.08 : 0.55;
      if (e.kind === "new_edge") baseOpacity = rewritten.has(e.from) ? 0.55 : 0.12;
    }

    const [ax, ay] = attachPoint(n1, n2.x, n2.y);
    const [bx, by] = attachPoint(n2, n1.x, n1.y);
    const [qx, qy] = quadraticControlPoint(ax, ay, bx, by, ek.curvature);
    const isActive =
      activeEdge?.from === e.from &&
      activeEdge?.to === e.to &&
      activeEdge?.kind === e.kind;

    const path = svgElement("path", {
      d: `M${ax},${ay} Q${qx},${qy} ${bx},${by}`,
      stroke: isActive ? ek.activeColor : ek.dimColor,
      "stroke-width": isActive ? 1.8 : 1,
      opacity: isActive ? 0.9 : baseOpacity,
      fill: "none",
      "marker-end": `url(#${ek.markerActive})`,
    });
    if (ek.dash) path.setAttribute("stroke-dasharray", ek.dash);
    gBg.appendChild(path);

    const label = e.rel || e.label;
    if (label && (isActive || e.kind === "nonown")) {
      const mx = (ax + 2 * qx + bx) / 4;
      const my = (ay + 2 * qy + by) / 4;
      const t = svgElement("text", {
        x: mx,
        y: my - 5,
        fill: isActive ? ek.activeColor : ek.dimColor,
        "fill-opacity": isActive ? 1 : 0.4,
        "font-size": "7",
        "font-family": "monospace",
        "text-anchor": "middle",
      });
      t.textContent = label;
      gBg.appendChild(t);
    }
  }

  if (activeEdge) {
    const n1 = nodes[activeEdge.from];
    const n2 = nodes[activeEdge.to];
    if (n1 && n2) {
      const ek = EDGE_KINDS[activeEdge.kind] || EDGE_KINDS.child;
      const [ax, ay] = attachPoint(n1, n2.x, n2.y);
      const [bx, by] = attachPoint(n2, n1.x, n1.y);
      const [qx, qy] = quadraticControlPoint(ax, ay, bx, by, ek.curvature);
      const path = svgElement("path", {
        d: `M${ax},${ay} Q${qx},${qy} ${bx},${by}`,
        stroke: ek.activeColor,
        "stroke-width": 2.5,
        fill: "none",
        "marker-end": `url(#${ek.markerActive})`,
        filter: "url(#glow-edge)",
      });
      if (ek.dash) path.setAttribute("stroke-dasharray", ek.dash);
      gAct.appendChild(path);
    }
  }
}

function drawNodes(gN, nodes, step) {
  for (const [id, node] of Object.entries(nodes)) {
    const [w, h] = nodeSize(node);
    const skipGlobal = step?.op === "sg" && step?.active === id;
    const state = node.isGlobal
      ? skipGlobal
        ? "global_sg"
        : "global"
      : getNodeState(id, step);
    const style = NODE_STYLES[state] || NODE_STYLES.none;
    const isActive =
      step?.active === id && !["te", "ce", "ke"].includes(step?.op || "");
    const isLeaked = step?.leaked?.includes(id);

    const g = svgElement("g", {
      transform: `translate(${node.x},${node.y})`,
    });
    if (isActive || isLeaked) g.setAttribute("filter", "url(#glow-node)");

    const rx = node.isGlobal ? 11 : 4;
    g.appendChild(
      svgElement("rect", {
        x: -w / 2,
        y: -h / 2,
        width: w,
        height: h,
        rx,
        fill: style.fill,
        stroke: style.stroke,
        "stroke-width": isActive ? 1.8 : 1,
      }),
    );

    const nameY = node.isGlobal ? 0 : -4;
    const nameEl = svgElement("text", {
      "text-anchor": "middle",
      "dominant-baseline": "middle",
      fill: style.text,
      "font-size": node.isGlobal ? "8.5" : "10",
      "font-family": "monospace",
      "font-weight": "600",
      y: nameY,
    });
    nameEl.textContent = node.label;
    g.appendChild(nameEl);

    if (!node.isGlobal) {
      const tagEl = svgElement("text", {
        "text-anchor": "middle",
        "dominant-baseline": "middle",
        fill: "#252c38",
        "font-size": "7",
        "font-family": "monospace",
        y: 7,
      });
      tagEl.textContent = node.tag;
      g.appendChild(tagEl);
    }

    // has_evar badge (fill_hole mode only): small circle top-right of node.
    // Orange = has_evar true, dark = false.
    if (!node.isGlobal && step?.mode === "fill_hole") {
      const [w, h] = nodeSize(node);
      const hasEvar = step.hasEvar?.[id] ?? node.initialHasEvar ?? false;
      const dot = svgElement("circle", {
        cx: w / 2 - 5,
        cy: -h / 2 + 5,
        r: "3.5",
        fill: hasEvar ? "#d29922" : "#21262d",
        stroke: hasEvar ? "#e3b341" : "#30363d",
        "stroke-width": "0.8",
      });
      g.appendChild(dot);
    }

    gN.appendChild(g);
  }
}

export function drawGraph(scenario, step) {
  const gBg = document.getElementById("g-edges-bg");
  const gAct = document.getElementById("g-edge-act");
  const gN = document.getElementById("g-nodes");
  gBg.innerHTML = "";
  gAct.innerHTML = "";
  gN.innerHTML = "";
  document.getElementById("g").setAttribute("viewBox", computeViewBox(scenario.nodes));

  const activeEdge =
    step?.src && step?.active && step?.ekind
      ? { from: step.src, to: step.active, kind: step.ekind }
      : null;

  const edges = collectEdges(scenario, step);
  drawRootArrow(gBg, scenario);
  drawEdges(gBg, gAct, edges, scenario.nodes, activeEdge, step);
  drawNodes(gN, scenario.nodes, step);
}

function pill(text, cls) {
  const s = document.createElement("span");
  s.className = `pill ${cls}`;
  s.textContent = text;
  return s;
}

export function renderSidebar(step, description) {
  document.getElementById("explain-op").textContent = step
    ? step.opLabel || step.op
    : "about this scenario";
  document.getElementById("explain-msg").innerHTML = step
    ? step.msg
    : (description || "Press <b>▷</b> to step, or <b>▶</b> to play.");

  const wlEl = document.getElementById("s-wl");
  const viEl = document.getElementById("s-vi");
  const coEl = document.getElementById("s-co");
  wlEl.innerHTML = "";
  viEl.innerHTML = "";
  coEl.innerHTML = "";

  const em = (t) => `<span class="p-em">${t}</span>`;
  if (!step) {
    wlEl.innerHTML = em("—");
    viEl.innerHTML = em("—");
    coEl.innerHTML = em("—");
    return;
  }

  // ── flat_gc mode sidebar ────────────────────────────────────────────────
  if (step.mode === "flat_gc") {
    const gcList = step.gcList || [];
    const freed = new Set(step.freed || []);
    document.querySelector("#s-wl").closest(".set-row")
      .querySelector(".set-lbl").textContent = "alloc list";
    document.querySelector("#s-vi").closest(".set-row")
      .querySelector(".set-lbl").textContent = "freeing";
    document.querySelector("#s-co").closest(".set-row")
      .querySelector(".set-lbl").textContent = "freed";
    wlEl.innerHTML = "";
    viEl.innerHTML = "";
    coEl.innerHTML = "";
    if (!gcList.length) wlEl.innerHTML = em("—");
    else gcList.forEach((id) => wlEl.appendChild(pill(id, "p-wl")));
    const cur = step.active;
    if (!cur) viEl.innerHTML = em("—");
    else viEl.appendChild(pill(cur, "p-fr"));
    const freedArr = [...freed];
    if (!freedArr.length) coEl.innerHTML = em("—");
    else freedArr.forEach((id) => coEl.appendChild(pill(id, "p-fr")));
    return;
  }

  // ── fill_hole mode sidebar ────────────────────────────────────────────────
  if (step.mode === "fill_hole") {
    const bfsQ = step.bfsQueue || [];
    const changed = step.bfsChanged || [];
    const stopped = step.bfsStopped || [];
    // Reuse the three rows: queue / cleared / stopped
    document.querySelector("#s-wl").closest(".set-row")
      .querySelector(".set-lbl").textContent = "bfs queue";
    document.querySelector("#s-vi").closest(".set-row")
      .querySelector(".set-lbl").textContent = "cleared";
    document.querySelector("#s-co").closest(".set-row")
      .querySelector(".set-lbl").textContent = "stopped";
    if (!bfsQ.length) wlEl.innerHTML = em("—");
    else bfsQ.forEach((id) => wlEl.appendChild(pill(id, "p-wl")));
    if (!changed.length) viEl.innerHTML = em("—");
    else changed.forEach((id) => viEl.appendChild(pill(id, "p-fr")));
    if (!stopped.length) coEl.innerHTML = em("—");
    else stopped.forEach((id) => coEl.appendChild(pill(id, "p-co")));
    return;
  }

  // ── bulk_collect mode sidebar ─────────────────────────────────────────────
  document.querySelector("#s-wl").closest(".set-row")
    .querySelector(".set-lbl").textContent = "worklist";
  document.querySelector("#s-vi").closest(".set-row")
    .querySelector(".set-lbl").textContent = "visited";
  document.querySelector("#s-co").closest(".set-row")
    .querySelector(".set-lbl").textContent = "collected";

  const wl = step.worklist || [];
  const vi = step.visited || [];
  if (!wl.length) wlEl.innerHTML = em("—");
  else wl.forEach((id) => wlEl.appendChild(pill(id, "p-wl")));
  if (!vi.length) viEl.innerHTML = em("—");
  else vi.forEach((id) => viEl.appendChild(pill(id, "p-vi")));

  const freed = new Set(step.freed);
  const co = step.collected || [];
  const lk = step.leaked || [];
  if (!co.length && !lk.length) coEl.innerHTML = em("—");
  else {
    co.forEach((id) =>
      coEl.appendChild(pill(id, freed.has(id) ? "p-fr" : "p-co")),
    );
    lk.forEach((id) => coEl.appendChild(pill(id, "p-lk")));
  }
}

export function renderCode(key, mode) {
  const panel = document.getElementById("code");
  panel.innerHTML = "";
  const lines =
    mode === "fill_hole" ? FILL_CODE_LINES
    : mode === "flat_gc" ? FLAT_GC_CODE_LINES
    : CODE_LINES;
  let activeEl = null;
  for (const [k, text] of lines) {
    const span = document.createElement("span");
    const isActive = k !== "//" && k === key;
    span.className =
      "cl" + (isActive ? ` act ${CODE_HIGHLIGHT_CLASSES[k] || ""}` : "");
    span.textContent = text;
    panel.appendChild(span);
    if (isActive) activeEl = span;
  }
  if (activeEl) {
    activeEl.scrollIntoView({ block: "nearest", behavior: "smooth" });
  }
}
