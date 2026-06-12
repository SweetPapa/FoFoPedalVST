import React, { useEffect, useMemo, useState } from 'react';
import { getToggleState } from '../juceBridge.js';

// Tiny pill toggle — used for per-block bypass and the routing swap buttons.
// Inverted polarity option: useInverted=true means "showing on = stored false"
// (e.g., when the underlying APVTS param is `*Bypassed` but the UI label is
// "ON").
export default function ToggleDot({ paramId, label, useInverted = false, fallbackInitial = false }) {
  const t = useMemo(() => getToggleState(paramId, fallbackInitial), [paramId]);
  const [v, setV] = useState(() => t.getValue());
  useEffect(() => {
    const onChange = () => setV(t.getValue());
    t.valueChangedEvent.addListener(onChange);
    onChange();
    return () => t.valueChangedEvent.removeListener(onChange);
  }, [t]);

  const visibleOn = useInverted ? !v : v;

  return (
    <button
      onClick={() => t.setValue(!v)}
      className={`inline-flex items-center gap-1.5 text-[10px] font-bold tracking-[0.2em] uppercase
        px-2.5 py-1 rounded-full border transition-colors
        ${visibleOn
          ? 'bg-sage/20 border-sage text-sage-deep'
          : 'bg-paper-deep/40 border-paper-line text-paper-dim'}`}
    >
      <span className={`inline-block w-1.5 h-1.5 rounded-full ${visibleOn ? 'bg-sage' : 'bg-paper-line'}`} />
      {label}
    </button>
  );
}
