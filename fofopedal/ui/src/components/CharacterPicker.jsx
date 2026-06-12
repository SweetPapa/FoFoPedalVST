import React, { useEffect, useMemo, useState } from 'react';
import { getComboState } from '../juceBridge.js';

// Center display: shows the current character name large in soft serif, with
// prev/next arrows. Below is a row of 12 paper-coloured dots marking position.
// Tapping a dot jumps to that character.
export default function CharacterPicker({ paramId, names }) {
  const combo = useMemo(() => getComboState(paramId, names), [paramId, names.join('|')]);
  const [idx, setIdx] = useState(() => combo.getChoiceIndex());

  useEffect(() => {
    const onChange = () => setIdx(combo.getChoiceIndex());
    combo.valueChangedEvent.addListener(onChange);
    onChange();
    return () => combo.valueChangedEvent.removeListener(onChange);
  }, [combo]);

  const jump = (delta) => {
    const next = (idx + delta + names.length) % names.length;
    combo.setChoiceIndex(next);
  };

  return (
    <div className="flex flex-col items-center">
      <div className="flex items-center gap-5">
        <button
          onClick={() => jump(-1)}
          className="text-paper-dim hover:text-terracotta text-2xl select-none w-8"
          aria-label="Previous character"
        >‹</button>

        <div className="text-center min-w-[20rem]">
          <div className="text-[9px] uppercase tracking-[0.45em] text-paper-dim mb-1">
            character
          </div>
          <div className="font-display text-3xl font-medium text-paper-ink leading-tight ink-underline inline-block px-1.5">
            « {names[idx] || '—'} »
          </div>
        </div>

        <button
          onClick={() => jump(+1)}
          className="text-paper-dim hover:text-terracotta text-2xl select-none w-8"
          aria-label="Next character"
        >›</button>
      </div>

      <div className="mt-3 flex items-center gap-1.5">
        {names.map((name, i) => (
          <button
            key={name}
            title={name}
            onClick={() => combo.setChoiceIndex(i)}
            className={`w-2 h-2 rounded-full transition-all
              ${i === idx ? 'bg-terracotta scale-150' : 'bg-paper-line hover:bg-sage-soft'}`}
          />
        ))}
      </div>
    </div>
  );
}
