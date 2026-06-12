import React, { useEffect, useRef, useState } from 'react';
import { isJuceHost } from '../juceBridge.js';
import * as Juce from '../juce/index.js';

function getToggleState(name) {
  if (isJuceHost && window.__JUCE__?.initialisationData?.__juce__toggles?.includes(name)) {
    return Juce.getToggleState(name);
  }
  // Dev preview mock
  let value = true;
  const listeners = new Set();
  return {
    getValue: () => value,
    setValue: (v) => { value = !!v; listeners.forEach((cb) => cb()); },
    valueChangedEvent: { addListener: (cb) => listeners.add(cb), removeListener: (cb) => listeners.delete(cb) }
  };
}

export default function Toggle({ paramId, label }) {
  const stateRef = useRef(null);
  const [on, setOn] = useState(true);

  useEffect(() => {
    const s = getToggleState(paramId);
    stateRef.current = s;
    const sync = () => setOn(!!s.getValue());
    sync();
    s.valueChangedEvent.addListener(sync);
    return () => s.valueChangedEvent.removeListener?.(sync);
  }, [paramId]);

  const handleClick = () => {
    const next = !on;
    setOn(next);
    stateRef.current?.setValue(next);
  };

  return (
    <button
      type="button"
      onClick={handleClick}
      className={`flex items-center gap-2 px-3 py-1.5 rounded-md border text-[11px] uppercase tracking-[0.2em] transition-colors ${
        on
          ? 'bg-vroom-accent text-black border-vroom-accent'
          : 'bg-vroom-panel text-vroom-dim border-vroom-edge hover:border-vroom-dim'
      }`}
    >
      <span
        className={`inline-block w-2 h-2 rounded-full ${on ? 'bg-black' : 'bg-vroom-dim'}`}
      />
      {label}
    </button>
  );
}
