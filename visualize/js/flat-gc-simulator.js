// Simulator for the flat shutdown GC strategy.
// Every expression is prepended to a global list at allocation time.
// At shutdown, the list is walked linearly: each node's non-expression
// heap data (name strings, branch arrays, uplink list nodes) is freed,
// then the expression itself is freed. No graph traversal needed.

export function simulateFlatGC(scenario) {
  const steps = [];
  const { nodes, allocOrder } = scenario;

  // allocOrder: array of node ids in allocation order (most recently allocated first,
  // since the list is prepend-only). Globals are not in the list.
  const list = allocOrder.filter((id) => !nodes[id]?.isGlobal);
  const freed = new Set();

  function snap(extra) {
    return Object.assign({ mode: "flat_gc", freed: [...freed] }, extra);
  }

  // Initial state
  steps.push(
    snap({
      op: "init",
      active: null,
      opLabel: "program exits",
      msg:
        "The program has finished. Every expression ever allocated is on a " +
        "single linked list in the order it was created. We walk it once, " +
        "freeing each node \u2014 no graph traversal, no reachability analysis needed.",
      code: "fg_init",
      gcList: [...list],
    }),
  );

  // Walk the list
  for (let i = 0; i < list.length; i++) {
    const id = list[i];
    const remaining = list.slice(i + 1);
    freed.add(id);
    steps.push(
      snap({
        op: "fg_free",
        active: id,
        opLabel: "free node",
        msg:
          `Free <b>${id}</b>: release its tag-specific data (name, branch arrays, …) ` +
          `and its uplink list, then <code>free()</code> the struct itself. ` +
          `Move to the next pointer in the list.`,
        code: "fg_free",
        gcList: remaining,
      }),
    );
  }

  steps.push(
    snap({
      op: "fg_done",
      active: null,
      opLabel: "complete",
      msg:
        `<b style="color:#3fb950">\u2713 Done.</b> ` +
        `Every expression freed in one linear pass \u2014 ${list.length} node${list.length !== 1 ? "s" : ""} total.`,
      code: "fg_done",
      gcList: [],
    }),
  );

  return steps;
}
