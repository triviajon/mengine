// Simulator for fill_hole + has_evar BFS propagation.
// Produces a step array compatible with the renderer, extended with
// fill_hole-specific fields: mode, hasEvar, bfsQueue, bfsChecked,
// bfsChanged, bfsStopped, holeNode, termNode, rewritten.

export function simulateFillHole(scenario) {
  const { nodes, parentUplinks, hole: holeId, term: termId } = scenario;

  // ── mutable algorithm state ──────────────────────────────────────────────
  let hasEvar = {};
  for (const [id, node] of Object.entries(nodes))
    hasEvar[id] = node.initialHasEvar ?? false;

  // effective children: apply rewrites as they happen so recompute_has_evar
  // sees the updated graph.  Starts as the structural children list.
  let effectiveChildren = {};
  for (const [id, node] of Object.entries(nodes))
    effectiveChildren[id] = (node.children || []).map((c) => c.to);

  let rewritten = [];   // parents whose pointer was rewritten
  let bfsQueue = [];
  let bfsChecked = [];
  let bfsChanged = [];
  let bfsStopped = [];

  const steps = [];

  function snap(extra) {
    return Object.assign(
      {
        mode: "fill_hole",
        hasEvar: { ...hasEvar },
        effectiveChildren: {},      // shallow copy per node
        holeNode: holeId,
        termNode: termId,
        rewritten: [...rewritten],
        bfsQueue: [...bfsQueue],
        bfsChecked: [...bfsChecked],
        bfsChanged: [...bfsChanged],
        bfsStopped: [...bfsStopped],
      },
      extra,
    );
  }

  // ── Step 0: initial state ─────────────────────────────────────────────────
  steps.push(
    snap({
      op: "init",
      active: holeId,
      opLabel: "initial state",
      msg:
        `Filling hole <b>${holeId}</b> with term <b>${termId}</b>. ` +
        `Orange badge = this node contains an unfilled hole somewhere inside it; dark badge = fully concrete.`,
      code: "fh_pre",
    }),
  );

  // ── Step 1: preconditions ─────────────────────────────────────────────────
  steps.push(
    snap({
      op: "fh_pre",
      active: holeId,
      opLabel: "preconditions ok",
      msg:
        `The type of <b>${termId}</b> matches the hole's expected type, and it doesn't circularly refer to the hole itself. Safe to proceed.`,
      code: "fh_pre",
    }),
  );

  // ── Step 2: structural rewrites ───────────────────────────────────────────
  const holeParents = parentUplinks[holeId] || [];
  for (const { parent, rel } of holeParents) {
    // Update effective children: swap hole → term in this parent
    effectiveChildren[parent] = effectiveChildren[parent].map((c) =>
      c === holeId ? termId : c,
    );
    rewritten.push(parent);

    steps.push(
      snap({
        op: "fh_rw",
        active: parent,
        src: holeId,
        ekind: "child",
        opLabel: "rewrite pointer",
        msg:
          `<b>${parent}</b>'s <em>${rel}</em> slot now points to <b>${termId}</b> instead of the hole. ` +
          `The edge in the graph is redrawn below.`,
        code: "fh_rw",
      }),
    );
  }

  // ── Step 3: seed BFS queue ────────────────────────────────────────────────
  bfsQueue = holeParents.map((u) => u.parent);
  const seedList = bfsQueue.join(", ") || "—";
  steps.push(
    snap({
      op: "fh_seed",
      active: null,
      opLabel: "seed BFS",
      msg:
        `The pointer is updated. Now we need to tell every ancestor whether they still contain ` +
        `an unfilled hole. We start from <b>${holeId}</b>'s direct parent${bfsQueue.length !== 1 ? "s" : ""}: ` +
        `<code>[${seedList}]</code> and walk upward.`,
      code: "fh_seed",
    }),
  );

  // ── Step 4: BFS upward ────────────────────────────────────────────────────
  while (bfsQueue.length > 0) {
    const nodeId = bfsQueue.shift();
    const oldVal = hasEvar[nodeId];

    // recompute: true iff any effective child still has has_evar == true
    const children = effectiveChildren[nodeId] || [];
    let newVal = nodes[nodeId]?.tag === "HOLE";   // holes are always holey
    if (!newVal) {
      for (const cid of children) {
        if (hasEvar[cid]) { newVal = true; break; }
      }
    }

    hasEvar[nodeId] = newVal;
    bfsChecked.push(nodeId);

    if (newVal !== oldVal) {
      // Flag changed (true → false: we removed the only hole).
      bfsChanged.push(nodeId);
      const parents = (parentUplinks[nodeId] || []).map((u) => u.parent);
      const parStr = parents.join(", ") || "none";
      steps.push(
        snap({
          op: "fh_prop",
          active: nodeId,
          opLabel: "flag cleared, propagating",
          msg:
            `<b>${nodeId}</b> no longer contains any unfilled holes (badge turns dark). ` +
            `Its ancestors might be affected too — enqueue them: <code>[${parStr}]</code>.`,
          code: "fh_prop",
        }),
      );
      for (const p of parents) bfsQueue.push(p);
    } else {
      // No change — flag stays true because another holey child remains.
      bfsStopped.push(nodeId);
      steps.push(
        snap({
          op: "fh_stop",
          active: nodeId,
          opLabel: "no change — BFS stops",
          msg:
            `<b>${nodeId}</b> still contains at least one other unfilled hole, so its badge stays orange. ` +
            `No change means its ancestors are unaffected — the walk stops here.`,
          code: "fh_stop",
        }),
      );
    }
  }

  // ── Step 5: done ──────────────────────────────────────────────────────────
  steps.push(
    snap({
      op: "fh_done",
      active: null,
      opLabel: "complete",
      msg:
        `Done. Every ancestor that lost its last hole has had its badge cleared. ` +
        `Only the nodes that genuinely changed were visited — no wasted work.`,
      code: "fh_done",
    }),
  );

  return steps;
}
