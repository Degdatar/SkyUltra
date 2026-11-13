let depth = 0;
export function getSkyInputScopeDepth(): number { return depth; }
/** Notify an embedding host when a popup owns Escape. Returns an idempotent release. */
export function enterSkyInputScope(): () => void {
  let released = false;
  const publish = (): void => { if (typeof window !== "undefined") window.dispatchEvent(new CustomEvent("skyultra:input-scope", { detail: { depth } })); };
  depth++; publish();
  return () => { if (released) return; released = true; depth = Math.max(0,depth-1); publish(); };
}
