import FitScale from './components/FitScale.jsx';
import ResizeHandle from './components/ResizeHandle.jsx';
import React, { useEffect, useState } from 'react';
import BigKnob from './components/BigKnob.jsx';
import Meter from './components/Meter.jsx';
import SegmentedToggle from './components/SegmentedToggle.jsx';
import ToggleDot from './components/ToggleDot.jsx';
import CharacterPicker from './components/CharacterPicker.jsx';
import { isJuceHost } from './juceBridge.js';

const CHARACTER_NAMES = [
  'Front Porch', 'Cassette Sunday', 'Cathedral Larynx', 'Dub Lounge',
  'Shoebox Shoegaze', 'Nylon Velvet', 'Garage Vox', 'Pillow Bass',
  'Synth Bath', 'Tin-Can Telephone', 'Slap and Float', 'Vapor Trail',
];

function useAudioLevels() {
  const [levels, setLevels] = useState({ in: 0, out: 0 });
  useEffect(() => {
    const backend = window.__JUCE__?.backend;
    if (!backend) {
      // Dev: synthesize a slow breathing meter for visual feedback.
      let t = 0;
      const id = setInterval(() => {
        t += 0.05;
        setLevels({
          in:  0.18 + 0.14 * Math.abs(Math.sin(t * 1.3)),
          out: 0.24 + 0.22 * Math.abs(Math.sin(t * 1.1 + 0.7)),
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

function formatMs(n) {
  // Map normalised back to ms-range via a skewed inverse — same NormalisableRange
  // as the param. Display string is the slider's stored ms value when available,
  // but BigKnob currently feeds the raw normalised in; show a friendly string.
  const minMs = 1, maxMs = 2000, skew = 0.4;
  const ms = minMs + (maxMs - minMs) * Math.pow(n, 1 / skew);
  return ms < 1000 ? `${Math.round(ms)} ms` : `${(ms / 1000).toFixed(2)} s`;
}

export default function App() {
  const levels = useAudioLevels();

  return (
    <>
    <FitScale designWidth={980} designHeight={800} background="#f3ecdd">
    <div className="relative w-full h-full text-paper-ink overflow-hidden paper-grain bg-paper">
      {/* Foreground */}
      <div className="relative flex flex-col h-full">

        {/* Header — logo + voicing + meters */}
        <header className="px-6 pt-4 pb-3 border-b border-paper-line/80 flex items-center gap-5">
          <div className="flex flex-col">
            <h1 className="font-display text-2xl font-semibold tracking-[0.05em] text-paper-ink leading-none">
              FOFOPEDAL
            </h1>
            <span className="text-[10px] uppercase tracking-[0.35em] text-paper-dim mt-0.5">
              Sweet Papa Technologies · Series A
            </span>
          </div>

          <div className="flex-1 flex justify-center">
            <SegmentedToggle
              paramId="voicing"
              options={['Vox', 'Gtr', 'Bass', 'Acoustic']}
              size="md"
            />
          </div>

          <div className="flex flex-col gap-1.5">
            <Meter label="In"  level={levels.in} />
            <Meter label="Out" level={levels.out} />
          </div>
        </header>

        {/* Character picker */}
        <section className="px-6 py-3 border-b border-paper-line/80 flex justify-center">
          <CharacterPicker paramId="characterPreset" names={CHARACTER_NAMES} />
        </section>

        {/* Six big knobs */}
        <section className="flex-1 px-6 py-6 flex flex-col">
          <div className="flex items-start justify-between gap-3">
            {/* CHARACTER */}
            <div className="flex flex-col items-center gap-3 w-[14%]">
              <BigKnob paramId="character" label="Character" accent="terracotta" defaultNormalised={0.25} />
            </div>

            {/* DRIVE */}
            <div className="flex flex-col items-center gap-3 w-[16%]">
              <BigKnob paramId="drive" label="Drive" accent="terracotta" defaultNormalised={0.20} />
              <SegmentedToggle
                paramId="driveType"
                options={['Tube', 'Tape', 'Iron']}
                size="sm"
              />
            </div>

            {/* SHAPE */}
            <div className="flex flex-col items-center gap-3 w-[18%]">
              <BigKnob paramId="shape" label="Shape" accent="mustard" defaultNormalised={0.5} />
              <div className="flex flex-col gap-1.5">
                <SegmentedToggle
                  paramId="modType"
                  options={['Chorus', 'Phaser', 'Trem/Vib']}
                  size="sm"
                />
                <SegmentedToggle
                  paramId="pitchType"
                  options={['Detune', 'Harmony', 'Freeze']}
                  size="sm"
                />
              </div>
            </div>

            {/* TIME */}
            <div className="flex flex-col items-center gap-3 w-[16%]">
              <BigKnob
                paramId="timeMs"
                label="Time"
                accent="sage"
                defaultNormalised={0.45}
                formatValue={formatMs}
              />
              <SegmentedToggle
                paramId="delayType"
                options={['Digital', 'BBD', 'Tape']}
                size="sm"
              />
            </div>

            {/* SPACE */}
            <div className="flex flex-col items-center gap-3 w-[18%]">
              <BigKnob paramId="space" label="Space" accent="sage" defaultNormalised={0.30} />
              <SegmentedToggle
                paramId="spaceType"
                options={['Plate', 'Hall', 'Room', 'Shimmer']}
                size="sm"
              />
            </div>

            {/* MIX */}
            <div className="flex flex-col items-center gap-3 w-[14%]">
              <BigKnob paramId="mix" label="Mix" accent="ink" defaultNormalised={0.30} />
            </div>
          </div>
        </section>

        {/* Footer — routing + block bypass */}
        <footer className="px-6 py-3 border-t border-paper-line/80 flex flex-col gap-2">
          <div className="flex items-center gap-2 flex-wrap">
            <span className="text-[10px] uppercase tracking-[0.3em] text-paper-dim w-16">
              routing
            </span>
            <ToggleDot paramId="swapModPitch"   label="Mod ↔ Pitch" />
            <ToggleDot paramId="swapDelaySpace" label="Delay ↔ Space" />
            <ToggleDot paramId="hostSync"       label="Host Sync" />
            <ToggleDot paramId="glueDefeated"   label="Glue On" useInverted />
            <ToggleDot paramId="delayPingPong"  label="Ping-Pong" />
          </div>

          <div className="flex items-center gap-2 flex-wrap">
            <span className="text-[10px] uppercase tracking-[0.3em] text-paper-dim w-16">
              blocks
            </span>
            <ToggleDot paramId="characterDefeated" label="Character" useInverted />
            <ToggleDot paramId="driveBypassed"     label="Drive"     useInverted />
            <ToggleDot paramId="modBypassed"       label="Mod"       useInverted />
            <ToggleDot paramId="pitchBypassed"     label="Pitch"     useInverted />
            <ToggleDot paramId="delayBypassed"     label="Delay"     useInverted />
            <ToggleDot paramId="spaceBypassed"     label="Space"     useInverted />
          </div>

          <div className="text-[9px] text-paper-dim/80 mt-1">
            {isJuceHost ? 'plugin' : 'preview'} · v0.1 · made for FoFo
          </div>
        </footer>

      </div>
    </div>
    </FitScale>
    <ResizeHandle minWidth={640} minHeight={522} />
    </>
  );
}
