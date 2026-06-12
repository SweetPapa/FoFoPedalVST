import FitScale from './components/FitScale.jsx';
import ResizeHandle from './components/ResizeHandle.jsx';
import React, { useEffect, useState } from 'react';
import Knob from './components/Knob.jsx';
import Meter from './components/Meter.jsx';
import CabSection from './components/CabSection.jsx';
import ModeSelector from './components/ModeSelector.jsx';
import VoiceSelector from './components/VoiceSelector.jsx';
import PresetBar from './components/PresetBar.jsx';
import { isJuceHost } from './juceBridge.js';

// One accent family, two tiers — hierarchy over rainbow. The four hero
// knobs are THE pedal; the support row is deliberately smaller and dimmer.
const HERO_COLOR    = '#ff8a3d';
const SUPPORT_COLOR = '#9b8a78';

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
    <FitScale designWidth={980} designHeight={780} background="#14161b">
    <div className="relative w-full h-full flex flex-col text-vroom-ink overflow-hidden">
      {/* Layered background: radial vignette + faint rainbow blob */}
      <div className="absolute inset-0 bg-vroom-bg" />
      <div className="absolute inset-0 pointer-events-none opacity-[0.18]"
           style={{
             background:
               'radial-gradient(ellipse 80% 60% at 20% 0%, #ff5050 0%, transparent 60%),' +
               'radial-gradient(ellipse 70% 60% at 80% 0%, #a55aff 0%, transparent 55%),' +
               'radial-gradient(ellipse 90% 50% at 50% 110%, #4cdef0 0%, transparent 60%)'
           }} />
      <div className="absolute inset-0 pointer-events-none opacity-30 animate-breathe"
           style={{
             background: `radial-gradient(circle at 50% 50%, ${
               levels.out > 0.5 ? '#ff5050' : levels.out > 0.2 ? '#ffaa3c' : '#4ac8f0'
             }11 0%, transparent 60%)`
           }} />

      {/* Foreground */}
      <div className="relative flex flex-col h-full">
        <header className="px-6 pt-4 pb-3 flex items-center justify-between gap-4 border-b border-vroom-edge backdrop-blur-sm">
          <div className="flex items-baseline gap-3">
            <h1 className="text-3xl font-extrabold tracking-[0.25em]"
                style={{
                  background: 'linear-gradient(90deg, #ff5050, #ffaa3c, #ffd040, #4cdef0, #a55aff, #ff5aa3)',
                  WebkitBackgroundClip: 'text',
                  WebkitTextFillColor: 'transparent'
                }}>
              VROOM
            </h1>
            <span className="text-[10px] uppercase tracking-[0.3em] text-vroom-dim">
              Sweet Papa Tech
            </span>
          </div>
          <div className="flex items-center gap-4">
            <ModeSelector />
            <VoiceSelector />
          </div>
          <span className="text-[10px] uppercase tracking-widest text-vroom-dim">
            {isJuceHost ? 'plugin' : 'dev preview'} · v0.1
          </span>
        </header>

        <PresetBar />

        <div className="px-6 py-3 flex gap-6 border-b border-vroom-edge">
          <Meter label="In"  level={levels.in} />
          <Meter label="Out" level={levels.out} />
        </div>

        <main className="flex-1 flex flex-col items-center justify-center gap-5 p-4">
          {/* The pedal: four hero knobs */}
          <div className="flex items-end gap-9">
            <Knob paramId="drive"     label="Drive"     color={HERO_COLOR} size={150} />
            <Knob paramId="character" label="Character" color={HERO_COLOR} size={150} />
            <Knob paramId="tone"      label="Tone"      color={HERO_COLOR} size={150} />
            <Knob paramId="level"     label="Level"     color={HERO_COLOR} size={150} />
          </div>
          {/* Trim & feel — quieter row */}
          <div className="flex items-end gap-7 opacity-75">
            <Knob paramId="input" label="Input" color={SUPPORT_COLOR} size={78} />
            <Knob paramId="body"  label="Body"  color={SUPPORT_COLOR} size={78} />
            <Knob paramId="sag"   label="Sag"   color={SUPPORT_COLOR} size={78} />
            <Knob paramId="blend" label="Blend" color={SUPPORT_COLOR} size={78} />
          </div>
        </main>

        <CabSection />

        <footer className="px-6 py-2 border-t border-vroom-edge text-[10px] text-vroom-dim flex justify-between">
          <span>4 voices · source-aware · 16 factory presets</span>
          <span>made for FoFo · v0.1.0</span>
        </footer>
      </div>
    </div>
    </FitScale>
    <ResizeHandle minWidth={560} minHeight={475} />
    </>
  );
}
