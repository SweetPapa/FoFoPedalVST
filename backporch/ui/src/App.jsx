import FitScale from './components/FitScale.jsx';
import ResizeHandle from './components/ResizeHandle.jsx';
import React, { useEffect, useState } from 'react';
import Knob from './components/Knob.jsx';
import ModeSwitch from './components/ModeSwitch.jsx';
import Meter from './components/Meter.jsx';
import PresetStepper from './components/PresetStepper.jsx';
import { isJuceHost } from './juceBridge.js';

const ACCENT = '#ff9b6b';

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
    <FitScale designWidth={680} designHeight={430} background="#1c1410">
    <div className="relative w-full h-full flex flex-col bg-bkpr-bg text-bkpr-ink overflow-hidden">
      {/* faint tape-stripe texture */}
      <div className="absolute inset-0 pointer-events-none opacity-[0.05]"
           style={{ background: 'radial-gradient(ellipse 80% 50% at 50% 110%, #ff9b6b 0%, transparent 60%)' }} />

      <header className="relative px-6 pt-4 pb-3 flex items-center gap-5 border-b border-white/10">
        <div className="flex flex-col shrink-0">
          <h1 className="text-2xl font-extrabold tracking-[0.35em] leading-none" style={{ color: ACCENT }}>BACKPORCH</h1>
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
          <Knob paramId="space" label="Space" accent={ACCENT} defaultNorm={0.45} />
          <Knob paramId="tone" label="Tone" accent={ACCENT} defaultNorm={0.45} />
          <Knob paramId="duck" label="Duck" accent={ACCENT} defaultNorm={0.35} />
          <Knob paramId="mix" label="Mix" accent={ACCENT} defaultNorm={0.4} />
        </div>
        <ModeSwitch paramId="mode" choices={['Slap', 'Room', 'Plate']} accent={ACCENT} />
      </main>

      <footer className="relative px-6 py-3 border-t border-white/10 text-[10px] opacity-40 flex justify-between">
        <span>sounds produced, not wet</span>
        <span>hpf send · pre-delay · ducked tail · {isJuceHost ? 'plugin' : 'preview'} · series b</span>
      </footer>
    </div>
    </FitScale>
    <ResizeHandle minWidth={476} minHeight={301} />
    </>
  );
}
