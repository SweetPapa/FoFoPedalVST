import React, { useEffect, useState } from 'react';
import Knob from './components/Knob.jsx';
import Meter from './components/Meter.jsx';
import { isJuceHost } from './juceBridge.js';

function useAudioLevels() {
  const [levels, setLevels] = useState({ in: 0, out: 0 });

  useEffect(() => {
    const backend = window.__JUCE__?.backend;
    if (!backend) {
      // Dev preview: animate fake levels so the meter is visible.
      let t = 0;
      const id = setInterval(() => {
        t += 0.06;
        setLevels({
          in:  0.2 + 0.18 * Math.abs(Math.sin(t * 1.3)),
          out: 0.3 + 0.30 * Math.abs(Math.sin(t * 1.1 + 0.7))
        });
      }, 33);
      return () => clearInterval(id);
    }

    const handler = (payload) => setLevels(payload);
    const token = backend.addEventListener('audioLevels', handler);
    return () => backend.removeEventListener(token);
  }, []);

  return levels;
}

export default function App() {
  const levels = useAudioLevels();

  return (
    <div className="w-full h-full flex flex-col bg-vroom-bg text-vroom-ink">
      <header className="px-6 pt-5 pb-3 flex items-baseline justify-between border-b border-vroom-edge">
        <div className="flex items-baseline gap-3">
          <h1 className="text-2xl font-bold tracking-widest">VROOM</h1>
          <span className="text-xs uppercase tracking-[0.25em] text-vroom-dim">
            Sweet Papa Technologies
          </span>
        </div>
        <span className="text-[10px] uppercase tracking-widest text-vroom-dim">
          {isJuceHost ? 'plugin' : 'dev preview'} · phase 3
        </span>
      </header>

      <div className="px-6 py-3 flex gap-6 border-b border-vroom-edge">
        <Meter label="In"  level={levels.in} />
        <Meter label="Out" level={levels.out} />
      </div>

      <main className="flex-1 flex items-center justify-center">
        <div className="grid grid-cols-4 gap-x-10 gap-y-6 p-6">
          <Knob paramId="input"     label="Input" />
          <Knob paramId="drive"     label="Drive" />
          <Knob paramId="character" label="Character" />
          <Knob paramId="body"      label="Body" />
          <Knob paramId="tone"      label="Tone" />
          <Knob paramId="sag"       label="Sag" />
          <Knob paramId="blend"     label="Blend" />
          <Knob paramId="level"     label="Level" />
        </div>
      </main>

      <footer className="px-6 py-3 border-t border-vroom-edge text-[11px] text-vroom-dim flex justify-between">
        <span>Asym soft-clip · oversampled 4× · parallel dry blend</span>
        <span>v0.1.0</span>
      </footer>
    </div>
  );
}
