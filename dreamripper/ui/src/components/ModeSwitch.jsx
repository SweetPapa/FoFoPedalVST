import React, { useEffect, useRef, useState } from 'react';
import { getComboState } from '../juceBridge.js';

// Four-way amplifier selector. Each mode is a different amp rather than a tone
// preset, so the switch is the loudest control on the panel and the selected
// segment is filled rather than merely outlined — there is never a moment
// where you have to squint to see which amplifier you are playing through.
export default function ModeSwitch({ paramId, choices, accent = '#ff2d55' }) {
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
    <div
      className="inline-flex overflow-hidden border border-white/12 bg-black/40"
      style={{ clipPath: 'polygon(9px 0, 100% 0, 100% calc(100% - 9px), calc(100% - 9px) 100%, 0 100%, 0 9px)' }}
    >
      {choices.map((c, i) => (
        <button
          key={c}
          onClick={() => { stateRef.current.setChoiceIndex(i); setIndex(i); }}
          className="px-5 py-2 text-[11px] uppercase tracking-[0.26em] transition-colors border-r border-white/8 last:border-r-0"
          style={{
            background: i === index ? accent : 'transparent',
            color: i === index ? '#120b0d' : 'rgba(255,255,255,0.55)',
            fontWeight: i === index ? 800 : 500
          }}
        >
          {c}
        </button>
      ))}
    </div>
  );
}
