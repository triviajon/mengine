import { SCENARIOS } from "./scenarios.js";
import { simulate } from "./simulator.js";
import { simulateFillHole } from "./fill-simulator.js";
import { simulateFlatGC } from "./flat-gc-simulator.js";
import { drawGraph, renderSidebar, renderCode } from "./renderer.js";

let scenario = SCENARIOS.gc_flat;
let steps = [];
let idx = -1;
let timer = null;

function render() {
  const step = idx >= 0 ? steps[idx] : null;
  drawGraph(scenario, step);
  renderSidebar(step, scenario.description);
  renderCode(step?.code ?? null, step?.mode);

  const pct =
    steps.length > 0 && idx >= 0 ? ((idx + 1) / steps.length) * 100 : 0;
  document.getElementById("prog-fill").style.width = pct + "%";
  document.getElementById("step-ctr").textContent =
    idx >= 0 ? `${idx + 1} / ${steps.length}` : "— / —";
  document.getElementById("b-prv").disabled = idx <= -1;
  document.getElementById("b-nxt").disabled = idx >= steps.length - 1;
}

function load(key) {
  scenario = SCENARIOS[key];
  steps =
    scenario.mode === "fill_hole"
      ? simulateFillHole(scenario)
      : scenario.mode === "flat_gc"
      ? simulateFlatGC(scenario)
      : simulate(scenario);
  idx = -1;
  render();
}

function forward() {
  if (idx < steps.length - 1) {
    idx++;
    render();
  }
}

function backward() {
  if (idx > -1) {
    idx--;
    render();
  }
}

function pause() {
  if (timer) {
    clearTimeout(timer);
    timer = null;
  }
  const btn = document.getElementById("b-ply");
  btn.textContent = "▶";
  btn.classList.remove("on");
}

function reset() {
  pause();
  idx = -1;
  render();
}

function play() {
  if (timer) return;
  const btn = document.getElementById("b-ply");
  btn.textContent = "⏸";
  btn.classList.add("on");

  function tick() {
    if (idx >= steps.length - 1) {
      pause();
      return;
    }
    forward();
    timer = setTimeout(
      tick,
      Math.round(1500 / parseInt(document.getElementById("spd").value)),
    );
  }
  timer = setTimeout(
    tick,
    Math.round(1500 / parseInt(document.getElementById("spd").value)),
  );
}

document.getElementById("b-rst").onclick = reset;
document.getElementById("b-prv").onclick = () => {
  pause();
  backward();
};
document.getElementById("b-nxt").onclick = () => {
  pause();
  forward();
};
document.getElementById("b-ply").onclick = () => (timer ? pause() : play());

document.querySelectorAll(".sc-btn").forEach((btn) => {
  btn.onclick = () => {
    pause();
    document
      .querySelectorAll(".sc-btn")
      .forEach((b) => b.classList.remove("active"));
    btn.classList.add("active");
    load(btn.dataset.s);
  };
});

load("gc_flat");
