export function simulate(scenario) {
  const steps = [];
  const visited = new Set();
  const worklist = [];
  const collected = [];
  const freed = new Set();

  function snapshot(extra) {
    return Object.assign(
      {
        visited: [...visited],
        worklist: [...worklist],
        collected: [...collected],
        freed: [...freed],
      },
      extra,
    );
  }

  function enqueue(id, src, edgeKind) {
    if (!id) return;
    const node = scenario.nodes[id];
    if (node.isGlobal) {
      steps.push(
        snapshot({
          op: "sg",
          active: id,
          src,
          ekind: edgeKind,
          opLabel: "skip global",
          msg: `<b>${id}</b> is a shared global (like a type universe or empty context). It belongs to everyone, so we never free it.`,
          code: "sg",
        }),
      );
      return;
    }
    if (visited.has(id)) {
      steps.push(
        snapshot({
          op: "sv",
          active: id,
          src,
          ekind: edgeKind,
          opLabel: "skip visited",
          msg: `<b>${id}</b> was already queued from an earlier edge — skip to avoid freeing it twice.`,
          code: "sv",
        }),
      );
      return;
    }
    visited.add(id);
    worklist.push(id);
    steps.push(
      snapshot({
        op: "eq",
        active: id,
        src,
        ekind: edgeKind,
        opLabel: "enqueue",
        msg: `<b>${id}</b> is reachable from the root — add to the worklist so we visit and free it.`,
        code: "eq",
      }),
    );
  }

  steps.push(
    snapshot({
      op: "init",
      active: scenario.root,
      src: null,
      ekind: null,
      opLabel: "start",
      msg: `Starting the collect pass. We walk the graph from <b>${scenario.root}</b>, following every edge, to find every node that needs to be freed.`,
      code: "eq",
    }),
  );
  enqueue(scenario.root, null, null);

  while (worklist.length > 0) {
    const id = worklist.shift();
    collected.push(id);
    const node = scenario.nodes[id];

    steps.push(
      snapshot({
        op: "dq",
        active: id,
        src: null,
        ekind: null,
        opLabel: "dequeue",
        msg: `Taking <b>${id}</b> off the worklist — it's confirmed reachable. Follow its edges to discover more nodes.`,
        code: "dq",
      }),
    );

    if (node.type) {
      steps.push(
        snapshot({
          op: "te",
          active: node.type,
          src: id,
          ekind: "type",
          opLabel: "type edge",
          msg: `<b>${id}</b> has a <em>type</em> edge to <b>${node.type}</b>. Check if it needs to be freed too.`,
          code: "te",
        }),
      );
      enqueue(node.type, id, "type");
    }

    if (node.ctx) {
      steps.push(
        snapshot({
          op: "ce",
          active: node.ctx,
          src: id,
          ekind: "ctx",
          opLabel: "context edge",
          msg: `<b>${id}</b> has a <em>context</em> edge to <b>${node.ctx}</b>. The collector follows this pointer to ensure nothing is missed.`,
          code: "ce",
        }),
      );
      enqueue(node.ctx, id, "ctx");
    }

    for (const { to, rel } of node.children || []) {
      steps.push(
        snapshot({
          op: "ke",
          active: to,
          src: id,
          ekind: "child",
          opLabel: "child edge",
          msg: `<b>${id}</b> has a child <b>${to}</b> via the <em>${rel}</em> slot — follow it.`,
          code: "ke",
        }),
      );
      enqueue(to, id, "child");
    }
  }

  steps.push(
    snapshot({
      op: "p2",
      active: null,
      src: null,
      ekind: null,
      opLabel: "phase 2",
      msg: `All reachable nodes found. Now free them — release memory for each one in the collected list.`,
      code: "fr",
    }),
  );

  for (const id of [...collected]) {
    freed.add(id);
    steps.push(
      snapshot({
        op: "fr",
        active: id,
        src: null,
        ekind: null,
        opLabel: "free",
        msg: `Free <b>${id}</b> — release its memory back to the heap.`,
        code: "fr",
      }),
    );
  }

  const leaked = Object.keys(scenario.nodes).filter(
    (id) => !scenario.nodes[id].isGlobal && !freed.has(id),
  );
  steps.push(
    snapshot({
      op: "done",
      active: null,
      src: null,
      ekind: null,
      leaked,
      opLabel: leaked.length ? "leak detected" : "complete",
      msg: leaked.length
        ? `<b style="color:#f85149">✗ Leaked: ${leaked.join(", ")}</b> — these nodes were never reachable from the root, so they were never freed.`
        : `<b style="color:#3fb950">✓ Done.</b> Every heap node was reached and freed — no leaks.`,
      code: null,
    }),
  );

  return steps;
}
