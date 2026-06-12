import React, { useEffect, useRef, useState } from 'react';
import { isJuceHost } from '../juceBridge.js';
import * as Juce from '../juce/index.js';

const FALLBACK_CHOICES = ['Smooth', 'Crunch', 'Fuzz', 'Octave'];

// Each voice gets a distinct hue so the user can read the rig at a glance.
const VOICE_COLORS = {
  Smooth: '#ffd06b',
  Crunch: '#ff7a3c',
  Fuzz:   '#c84cff',
  Octave: '#4cdef0'
};

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

export default function VoiceSelector() {
  const stateRef = useRef(null);
  const [choices, setChoices] = useState(FALLBACK_CHOICES);
  const [index, setIndex] = useState(0);

  useEffect(() => {
    const s = getComboBoxState('clipShape', FALLBACK_CHOICES);
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

  const activeColor = VOICE_COLORS[choices[index]] ?? '#ff8a3d';

  return (
    <div className="flex items-center gap-2">
      <span className="text-[10px] uppercase tracking-[0.3em] text-vroom-dim">Voice</span>
      <div className="flex items-center bg-vroom-panel border border-vroom-edge rounded-md p-1">
        {choices.map((c, i) => {
          const isActive = i === index;
          const color = VOICE_COLORS[c] ?? '#ff8a3d';
          return (
            <button
              key={c}
              type="button"
              onClick={() => pick(i)}
              className={`px-2.5 py-1 text-[11px] uppercase tracking-[0.18em] rounded-sm transition-colors`}
              style={isActive ? { backgroundColor: color, color: '#000' } : { color: '#9aa1ab' }}
            >
              {c}
            </button>
          );
        })}
      </div>
      <span className="w-1.5 h-1.5 rounded-full"
            style={{ backgroundColor: activeColor, boxShadow: `0 0 8px ${activeColor}` }} />
    </div>
  );
}
