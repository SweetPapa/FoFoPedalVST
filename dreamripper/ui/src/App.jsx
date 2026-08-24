import React, { useEffect, useState } from 'react';
import FitScale from './components/FitScale.jsx';
import ResizeHandle from './components/ResizeHandle.jsx';
import Knob from './components/Knob.jsx';
import ModeSwitch from './components/ModeSwitch.jsx';
import Meter from './components/Meter.jsx';
import GateLamp from './components/GateLamp.jsx';
import PresetStepper from './components/PresetStepper.jsx';
import { isJuceHost } from './juceBridge.js';

const ACCENT = '#ff2d55';

// Levels and the gate lamp arrive together on one event, thirty times a
// second. Without a host — the browser dev preview — they are faked so the
// panel still animates and the layout can be judged.
function useAudioLevels() {
  const [levels, setLevels] = useState({ in: 0, out: 0, gate: 1 });

  useEffect(() => {
    const backend = window.__JUCE__?.backend;
    if (!backend) {
      let t = 0;
      const id = setInterval(() => {
        t += 0.06;
        const riff = Math.max(0, Math.sin(t * 0.7));
        setLevels({
          in: 0.05 + 0.4 * riff,
          out: 0.1 + 0.6 * riff,
          gate: riff > 0.08 ? 1 : 0.04
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
      <FitScale designWidth={780} designHeight={500} background="#0c0a0b">
        <div className="relative w-full h-full flex flex-col bg-drip-bg text-drip-ink overflow-hidden">
          {/* Brushed-and-scratched panel. Two static gradient layers rather
              than an image, so the whole UI stays one self-contained file. */}
          <div
            className="absolute inset-0 pointer-events-none"
            style={{
              background:
                'radial-gradient(ellipse 80% 55% at 50% -10%, rgba(255,45,85,0.16) 0%, transparent 62%),' +
                'radial-gradient(ellipse 60% 40% at 50% 115%, rgba(255,45,85,0.07) 0%, transparent 70%)'
            }}
          />
          <div
            className="absolute inset-0 pointer-events-none opacity-[0.045]"
            style={{
              backgroundImage:
                'repeating-linear-gradient(102deg, #fff 0px, #fff 1px, transparent 1px, transparent 4px),' +
                'repeating-linear-gradient(78deg, #fff 0px, #fff 1px, transparent 1px, transparent 9px)'
            }}
          />

          {/* ── header ─────────────────────────────────────────────── */}
          <header className="relative px-7 pt-4 pb-3 flex items-center gap-5 border-b border-white/10">
            <div className="flex flex-col shrink-0">
              <h1 className="text-[26px] font-black tracking-[0.16em] leading-none flex items-baseline">
                <span style={{ color: ACCENT }}>DREAM</span>
                <span className="text-white/85">RIPPER</span>
              </h1>
              <span className="text-[9px] uppercase tracking-[0.3em] text-white/35 mt-1.5 whitespace-nowrap">
                Sweet Papa Tech · amp in a box
              </span>
            </div>

            <div className="flex-1 flex justify-center min-w-0">
              <PresetStepper accent={ACCENT} />
            </div>

            <div className="flex flex-col gap-1.5 shrink-0">
              <Meter label="In" level={levels.in} />
              <Meter label="Out" level={levels.out} />
              <GateLamp gain={levels.gate} accent={ACCENT} />
            </div>
          </header>

          {/* ── the amplifier ──────────────────────────────────────── */}
          <main className="relative flex-1 flex flex-col items-center justify-center gap-6 px-7">
            <div className="flex items-start gap-5">
              <Knob paramId="rip"   label="Rip"   hint="gain"    accent={ACCENT} defaultNorm={0.55} />
              <Knob paramId="tight" label="Tight" hint="flub → chug" accent={ACCENT} defaultNorm={0.45} />
              <Knob paramId="scoop" label="Scoop" hint="push ↔ scoop" accent={ACCENT} defaultNorm={0.42} centred />
              <Knob paramId="cab"   label="Cab"   hint="dark → bright" accent={ACCENT} defaultNorm={0.50} />
              <Knob paramId="level" label="Level" hint="±20 dB" accent={ACCENT} defaultNorm={0.50} centred />
            </div>

            <div className="flex items-center justify-between w-full gap-6">
              <ModeSwitch paramId="mode" choices={['Sludge', 'Grunge', 'Metal', 'Djent']} accent={ACCENT} />

              <div className="flex items-start gap-4">
                <Knob paramId="gate" label="Gate" accent={ACCENT} defaultNorm={0.35} size={62} dim />
                <Knob paramId="mix"  label="Mix"  accent={ACCENT} defaultNorm={1.0}  size={62} dim />
              </div>
            </div>
          </main>

          <footer className="relative px-7 py-2.5 border-t border-white/10 text-[9.5px] uppercase tracking-[0.2em] text-white/30 flex justify-between">
            <span>seattle sludge → modern chug</span>
            <span>gated · cascaded · 4× oversampled · {isJuceHost ? 'plugin' : 'preview'}</span>
          </footer>
        </div>
      </FitScale>
      <ResizeHandle minWidth={620} minHeight={400} />
    </>
  );
}
