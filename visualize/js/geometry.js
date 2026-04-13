const NODE_WIDTH = 88;
const NODE_HEIGHT = 30;
const GLOBAL_WIDTH = 60;
const GLOBAL_HEIGHT = 22;

export function nodeSize(node) {
  return node.isGlobal
    ? [GLOBAL_WIDTH, GLOBAL_HEIGHT]
    : [NODE_WIDTH, NODE_HEIGHT];
}

export function attachPoint(node, targetX, targetY) {
  const [w, h] = nodeSize(node);
  const dx = targetX - node.x;
  const dy = targetY - node.y;
  if (!dx && !dy) return [node.x, node.y];
  const scale = Math.min(w / 2 / Math.abs(dx), h / 2 / Math.abs(dy));
  return [node.x + dx * scale, node.y + dy * scale];
}

export function quadraticControlPoint(x1, y1, x2, y2, curvature) {
  const mx = (x1 + x2) / 2;
  const my = (y1 + y2) / 2;
  const dx = x2 - x1;
  const dy = y2 - y1;
  const len = Math.sqrt(dx * dx + dy * dy) || 1;
  return [mx - (dy / len) * curvature, my + (dx / len) * curvature];
}

export function svgElement(tag, attrs) {
  const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, v);
  return el;
}
