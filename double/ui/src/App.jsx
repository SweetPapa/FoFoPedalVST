import FitScale from './components/FitScale.jsx';
import ResizeHandle from './components/ResizeHandle.jsx';
import React, { useEffect, useState } from 'react';
import Knob from './components/Knob.jsx';
import ModeSwitch from './components/ModeSwitch.jsx';
import Meter from './components/Meter.jsx';
import PresetStepper from './components/PresetStepper.jsx';
import { isJuceHost } from './juceBridge.js';

const ACCENT = '#ffb454';

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
    <FitScale designWidth={680} designHeight={430} background="#14181f">
    <div className="relative w-full h-full flex flex-col bg-dbl-bg text-dbl-ink overflow-hidden">
      {/* faint tape-stripe texture */}
      <div className="absolute inset-0 pointer-events-none opacity-[0.05]"
           style={{ background: 'repeating-linear-gradient(90deg, transparent 0 38px, #ffb454 38px 40px)' }} />

      <header className="relative px-6 pt-4 pb-3 flex items-center gap-5 border-b border-white/10">
        <div className="flex flex-col shrink-0">
          <h1 className="text-2xl font-extrabold tracking-[0.35em] leading-none" style={{ color: ACCENT }}>DOUBLE</h1>
          <span className="text-[10px] uppercase tracking-[0.3em] opacity-50 mt-1.5 whitespace-nowrap">Sweet Papa Tech</span>
        </div>
        <div className="flex-1 flex justify-center min-w-0">
          <PresetStepper accent={ACCENT} />
        </div>
        <div className="flex flex-col gap-1.5 shrink-0">
          <Meter label="In" level={levels.in} />
          <Meter label="Out" level={levels.out} />
        </div>
      </header>

      <main className="relative flex-1 flex flex-col items-center justify-center gap-6">
        <div className="flex items-end gap-10">
          <Knob paramId="thick" label="Thick" accent={ACCENT} defaultNorm={0.5} />
          <Knob paramId="wide"  label="Wide"  accent={ACCENT} defaultNorm={0.7} />
          <Knob paramId="human" label="Human" accent={ACCENT} defaultNorm={0.5} />
          <Knob paramId="mix"   label="Mix"   accent={ACCENT} defaultNorm={0.6} />
        </div>
        <ModeSwitch paramId="mode" choices={['Vox', 'Strings', 'Synth']} accent={ACCENT} />
      </main>

      <footer className="relative px-6 py-3 border-t border-white/10 text-[10px] opacity-40 flex justify-between">
        <span>every take you didn't record</span>
        <span>4 drifting voices · mono-safe · dry stays sacred · {isJuceHost ? 'plugin' : 'preview'} · series b</span>
      </footer>
    </div>
    </FitScale>
    <ResizeHandle minWidth={476} minHeight={301} />
    </>
  );
}
