import FitScale from './components/FitScale.jsx';
import ResizeHandle from './components/ResizeHandle.jsx';
import React, { useEffect, useState } from 'react';
import Knob from './components/Knob.jsx';
import ModeSwitch from './components/ModeSwitch.jsx';
import Meter from './components/Meter.jsx';
import { isJuceHost } from './juceBridge.js';

const ACCENT = '#6be8c2';

function useAudioLevels() {
  const [levels, setLevels] = useState({ in: 0, out: 0 });
  useEffect(() => {
    const backend = window.__JUCE__?.backend;
    if (!backend) {
      let t = 0;
      const id = setInterval(() => {
        t += 0.06;
        setLevels({ in: 0.25 + 0.15 * Math.abs(Math.sin(t)), out: 0.35 + 0.2 * Math.abs(Math.sin(t * 1.2)) });
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
    <>
    <FitScale designWidth={680} designHeight={430} background="#0f1a18">
    <div className="relative w-full h-full flex flex-col bg-sway-bg text-sway-ink overflow-hidden">
      {/* faint tape-stripe texture */}
      <div className="absolute inset-0 pointer-events-none opacity-[0.05]"
           style={{ background: 'radial-gradient(ellipse 70% 60% at 20% 0%, #6be8c2 0%, transparent 55%)' }} />

      <header className="relative px-6 pt-4 pb-3 flex items-center justify-between border-b border-white/10">
        <div className="flex items-baseline gap-3">
          <h1 className="text-2xl font-extrabold tracking-[0.35em]" style={{ color: ACCENT }}>SWAY</h1>
          <span className="text-[10px] uppercase tracking-[0.3em] opacity-50">Sweet Papa Tech</span>
        </div>
        <div className="flex items-center gap-4">
          <Meter label="In" level={levels.in} />
          <Meter label="Out" level={levels.out} />
        </div>
        <span className="text-[10px] uppercase tracking-widest opacity-40">
          {isJuceHost ? 'plugin' : 'preview'} · series b
        </span>
      </header>

      <main className="relative flex-1 flex flex-col items-center justify-center gap-6">
        <div className="flex items-end gap-10">
          <Knob paramId="move" label="Move" accent={ACCENT} defaultNorm={0.45} />
          <Knob paramId="rate" label="Rate" accent={ACCENT} defaultNorm={0.35} />
          <Knob paramId="color" label="Color" accent={ACCENT} defaultNorm={0.5} />
          <Knob paramId="mix" label="Mix" accent={ACCENT} defaultNorm={1.0} />
        </div>
        <ModeSwitch paramId="mode" choices={['Tape', 'Ensemble', 'Pump']} accent={ACCENT} />
      </main>

      <footer className="relative px-6 py-3 border-t border-white/10 text-[10px] opacity-40 flex justify-between">
        <span>makes static tracks move like a band</span>
        <span>drifted lfos · nothing sits still</span>
      </footer>
    </div>
    </FitScale>
    <ResizeHandle minWidth={476} minHeight={301} />
    </>
  );
}
