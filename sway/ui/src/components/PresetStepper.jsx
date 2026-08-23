import React, { useEffect, useState } from 'react';
import { fetchPresetState, stepPreset, onPresetState } from '../juceBridge.js';

// Compact preset stepper for the header: ‹ Name ›.
//
// These pedals are deliberately spare, so this is the whole preset UI — no
// browser, no save dialog. The same bank is in the host's preset menu, and
// the plugin pushes changes back here, so stepping from either side agrees.
export default function PresetStepper({ accent }) {
  const [state, setState] = useState(null);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    let alive = true;
    fetchPresetState().then((s) => { if (alive && s) setState(s); });
    const off = onPresetState((s) => { if (alive && s) setState(s); });
    return () => { alive = false; off(); };
  }, []);

  const step = async (dir) => {
    if (busy) return;
    setBusy(true);
    const next = await stepPreset(dir);
    if (next) setState(next);
    setBusy(false);
  };

  const name = state?.name ?? '—';
  const modified = !!state?.modified;

  return (
    <div className="flex flex-col items-center min-w-0 select-none">
      <div className="flex items-center gap-1.5 min-w-0">
        <button
          onClick={() => step(-1)}
          aria-label="Previous preset"
          className="px-1 text-sm leading-none opacity-40 hover:opacity-90 transition-opacity"
        >
          ‹
        </button>

        <span
          className="text-[11px] uppercase tracking-[0.18em] font-semibold whitespace-nowrap truncate"
          style={{ color: accent }}
          title={state?.blurb || undefined}
        >
          {name}{modified ? ' *' : ''}
        </span>

        <button
          onClick={() => step(1)}
          aria-label="Next preset"
          className="px-1 text-sm leading-none opacity-40 hover:opacity-90 transition-opacity"
        >
          ›
        </button>
      </div>

      {state?.count > 0 && (
        <span className="text-[8px] tracking-[0.25em] opacity-30 mt-0.5 tabular-nums">
          {state.index + 1}/{state.count}
        </span>
      )}
    </div>
  );
}
