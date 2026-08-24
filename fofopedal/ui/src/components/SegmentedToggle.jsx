import React, { useEffect, useMemo, useState } from 'react';
import { getComboState } from '../juceBridge.js';

// Segmented button row backed by a JUCE Combo. Use for the 4-way voicing
// row, the per-block type picker, etc.
export default function SegmentedToggle({
  paramId,
  options,        // array of labels (must match the JUCE choices order)
  size = 'md',    // 'sm' | 'md'
  className = '',
}) {
  const combo = useMemo(() => getComboState(paramId, options), [paramId, options.join('|')]);
  const [idx, setIdx] = useState(() => combo.getChoiceIndex());

  useEffect(() => {
    const onChange = () => setIdx(combo.getChoiceIndex());
    combo.valueChangedEvent.addListener(onChange);
    onChange();
    return () => combo.valueChangedEvent.removeListener(onChange);
  }, [combo]);

  // 'sm' rows sit under the six knobs, where every column competes for the
  // same 980px. Tighter padding and letter-spacing keep a row inside its
  // own column instead of painting over its neighbour.
  const sizeClass = size === 'sm'
    ? 'text-[10px] px-1.5 py-1 tracking-[0.06em]'
    : 'text-xs px-3.5 py-1.5 tracking-[0.15em]';

  return (
    <div className={`inline-flex rounded-full border border-paper-line bg-paper p-0.5 shadow-sketch ${className}`}>
      {options.map((opt, i) => (
        <button
          key={opt}
          onClick={() => combo.setChoiceIndex(i)}
          className={`${sizeClass} whitespace-nowrap rounded-full font-semibold uppercase transition-colors
            ${i === idx
              ? 'bg-terracotta text-paper shadow-inner'
              : 'text-paper-dim hover:text-paper-ink'}`}
        >
          {opt}
        </button>
      ))}
    </div>
  );
}
