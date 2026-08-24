import React, { useEffect, useRef, useState } from 'react';
import { isJuceHost } from '../juceBridge.js';
import * as Juce from '../juce/index.js';
import Toggle from './Toggle.jsx';

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

const FALLBACK_CHOICES = ['1x12 Warm', '4x12 Modern', 'Bass 1x15', 'Full-Range / DI'];

export default function CabSection() {
  const stateRef = useRef(null);
  const [choices, setChoices] = useState(FALLBACK_CHOICES);
  const [index, setIndex] = useState(0);
  const [customName, setCustomName] = useState('');
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    const s = getComboBoxState('cabIR', FALLBACK_CHOICES);
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

  const handlePick = (i) => {
    setIndex(i);
    stateRef.current?.setChoiceIndex(i);
    setCustomName(''); // selecting a built-in slot clears the custom IR display
  };

  const handleLoadCustom = async () => {
    if (!window.__JUCE__) return; // no-op in dev preview
    setLoading(true);
    try {
      const fn = Juce.getNativeFunction('openIRFileDialog');
      const result = await fn();
      if (typeof result === 'string' && result.length > 0) {
        setCustomName(result);
      }
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="px-6 py-3 border-t border-vroom-edge flex items-center gap-4 flex-wrap">
      <span className="text-[10px] uppercase tracking-[0.3em] text-vroom-dim shrink-0">
        Cabinet
      </span>

      <Toggle paramId="cabEnable" label="On" />

      <div className="flex items-center gap-1 bg-vroom-panel border border-vroom-edge rounded-md p-1">
        {choices.map((c, i) => (
          <button
            key={c}
            type="button"
            onClick={() => handlePick(i)}
            className={`px-2.5 py-1 text-[11px] rounded-sm transition-colors ${
              i === index && !customName
                ? 'bg-vroom-accent text-black'
                : 'text-vroom-dim hover:text-vroom-ink'
            }`}
          >
            {c}
          </button>
        ))}
      </div>

      <button
        type="button"
        onClick={handleLoadCustom}
        disabled={loading || !isJuceHost}
        className="px-3 py-1.5 rounded-md text-[11px] uppercase tracking-[0.2em] border border-vroom-edge text-vroom-ink hover:border-vroom-dim disabled:opacity-50 disabled:cursor-not-allowed"
        title={isJuceHost ? 'Pick a custom .wav IR file' : 'Available inside the plugin only'}
      >
        {loading ? 'Loading…' : customName ? `IR: ${customName}` : 'Load custom IR…'}
      </button>
    </div>
  );
}
