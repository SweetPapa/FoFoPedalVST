import FitScale from './components/FitScale.jsx';
import ResizeHandle from './components/ResizeHandle.jsx';
import React, { useEffect, useState } from 'react';
import BigKnob from './components/BigKnob.jsx';
import PresetStepper from './components/PresetStepper.jsx';
import Meter from './components/Meter.jsx';
import { isJuceHost } from './juceBridge.js';

function useAudioLevels() {
  const [levels, setLevels] = useState({ in: 0, out: 0 });
  useEffect(() => {
    const backend = window.__JUCE__?.backend;
    if (!backend) {
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
    <>
    <FitScale designWidth={520} designHeight={490} background="#10081f">
    <div className="relative w-full h-full flex flex-col text-dream-ink overflow-hidden">
      {/* Base layer */}
      <div className="absolute inset-0 bg-dream-deeper" />

      {/* Aurora — slow drifting gradient blobs */}
      <div className="absolute inset-0 pointer-events-none opacity-90"
           style={{
             background:
               'radial-gradient(ellipse 60% 50% at 25% 30%, #4cdef022 0%, transparent 60%),' +
               'radial-gradient(ellipse 70% 60% at 75% 40%, #c084ff33 0%, transparent 60%),' +
               'radial-gradient(ellipse 80% 60% at 50% 95%, #ff8ad322 0%, transparent 65%)'
           }} />
      <div className="absolute inset-0 pointer-events-none animate-aurora"
           style={{
             background:
               'radial-gradient(circle at 30% 70%, #c084ff22 0%, transparent 50%),' +
               'radial-gradient(circle at 70% 20%, #ff8ad322 0%, transparent 55%)'
           }} />

      {/* Subtle pulse driven by output level */}
      <div className="absolute inset-0 pointer-events-none transition-opacity duration-200"
           style={{
             background: `radial-gradient(circle at 50% 55%, #ff8ad3${Math.round(levels.out * 60).toString(16).padStart(2,'0')} 0%, transparent 50%)`
           }} />

      {/* Foreground */}
      <div className="relative flex flex-col h-full">
        <header className="px-6 pt-4 pb-3 flex items-center gap-5 border-b border-dream-edge/60">
          <div className="flex flex-col shrink-0">
            <h1 className="text-2xl font-extrabold tracking-[0.4em]"
                style={{
                  background: 'linear-gradient(90deg, #4cdef0, #c084ff, #ff8ad3)',
                  WebkitBackgroundClip: 'text',
                  WebkitTextFillColor: 'transparent'
                }}>
              DAYDREAM
            </h1>
            <span className="text-[10px] uppercase tracking-[0.35em] text-dream-dim mt-1.5 whitespace-nowrap">
              Sweet Papa Tech
            </span>
          </div>
          <div className="flex-1 flex flex-col items-end gap-1.5">
            <Meter label="In"  level={levels.in} />
            <Meter label="Out" level={levels.out} />
          </div>
        </header>

        <main className="flex-1 flex flex-col items-center justify-center gap-3">
          <div className="animate-drift">
            <BigKnob paramId="dream" label="Dream" size={252} />
          </div>
          <PresetStepper accent="#c084ff" />
        </main>

        <footer className="px-6 py-3 border-t border-dream-edge/60 text-[10px] text-dream-dim flex justify-between">
          <span>tape sat · pitch drift · chorus · slap · shimmer reverb</span>
          <span>turn it up · made for FoFo · {isJuceHost ? 'plugin' : 'preview'} · v0.1</span>
        </footer>
      </div>
    </div>
    </FitScale>
    <ResizeHandle minWidth={380} minHeight={358} />
    </>
  );
}
