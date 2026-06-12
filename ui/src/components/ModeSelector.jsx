import React, { useEffect, useRef, useState } from 'react';
import { isJuceHost } from '../juceBridge.js';
import * as Juce from '../juce/index.js';

const FALLBACK_CHOICES = ['Electric', 'Acoustic', 'Bass'];

function getComboBoxState(name, fallbackChoices) {
  if (isJuceHost && window.__JUCE__?.initialisationData?.__juce__comboBoxes?.includes(name)) {
    return Juce.getComboBoxState(name);
  }
  let idx = 0;
  const listeners = new Set();
  return {
    getChoiceIndex: () => idx,
    setChoiceIndex: (i) => { idx = i; listeners.forEach((cb) => cb()); },
    properties: { choices: fallbackChoices ?? [] },
    valueChangedEvent: { addListener: (cb) => listeners.add(cb), removeListener: (cb) => listeners.delete(cb) },
    propertiesChangedEvent: { addListener: () => {}, removeListener: () => {} }
  };
}

export default function ModeSelector() {
  const stateRef = useRef(null);
  const [choices, setChoices] = useState(FALLBACK_CHOICES);
  const [index, setIndex] = useState(0);

  useEffect(() => {
    const s = getComboBoxState('sourceMode', FALLBACK_CHOICES);
    stateRef.current = s;
    const sync = () => {
      setIndex(s.getChoiceIndex());
      if (s.properties?.choices?.length) setChoices(s.properties.choices);
    };
    sync();
    s.valueChangedEvent.addListener(sync);
    s.propertiesChangedEvent.addListener?.(sync);
    return () => {
      s.valueChangedEvent.removeListener?.(sync);
      s.propertiesChangedEvent.removeListener?.(sync);
    };
  }, []);

  const pick = (i) => {
    setIndex(i);
    stateRef.current?.setChoiceIndex(i);
  };

  return (
    <div className="flex items-center gap-2">
      <span className="text-[10px] uppercase tracking-[0.3em] text-vroom-dim">Source</span>
      <div className="flex items-center bg-vroom-panel border border-vroom-edge rounded-md p-1">
        {choices.map((c, i) => (
          <button
            key={c}
            type="button"
            onClick={() => pick(i)}
            className={`px-3 py-1 text-[11px] uppercase tracking-[0.18em] rounded-sm transition-colors ${
              i === index
                ? 'bg-vroom-accent text-black'
                : 'text-vroom-dim hover:text-vroom-ink'
            }`}
          >
            {c}
          </button>
        ))}
      </div>
    </div>
  );
}
