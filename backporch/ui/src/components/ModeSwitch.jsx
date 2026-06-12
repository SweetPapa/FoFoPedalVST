import React, { useEffect, useRef, useState } from 'react';
import { getComboState } from '../juceBridge.js';

// 3-way segmented mode switch bound to an AudioParameterChoice.
export default function ModeSwitch({ paramId, choices, accent = '#ffb454' }) {
  const stateRef = useRef(null);
  const [index, setIndex] = useState(0);

  useEffect(() => {
    const s = getComboState(paramId, choices);
    stateRef.current = s;
    const sync = () => setIndex(s.getChoiceIndex());
    sync();
    s.valueChangedEvent.addListener(sync);
    return () => s.valueChangedEvent.removeListener?.(sync);
  }, [paramId]);

  return (
    <div className="inline-flex rounded-full border border-white/15 overflow-hidden">
      {choices.map((c, i) => (
        <button
          key={c}
          onClick={() => { stateRef.current.setChoiceIndex(i); setIndex(i); }}
          className="px-4 py-1.5 text-[11px] uppercase tracking-[0.25em] transition-colors"
          style={{
            background: i === index ? accent : 'transparent',
            color: i === index ? '#10100e' : 'rgba(255,255,255,0.65)',
            fontWeight: i === index ? 700 : 400
          }}
        >
          {c}
        </button>
      ))}
    </div>
  );
}
