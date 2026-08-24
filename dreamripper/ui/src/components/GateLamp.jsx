import React from 'react';

// The gate lamp. A threshold is the one control on a high-gain amp that cannot
// be set by ear alone — you are listening for the absence of something — so the
// plugin sends the gate's actual gain up every frame and this shows it.
//
// Lit = signal is getting through. Dark = the gate is holding it shut. Set the
// threshold so the lamp goes out between riffs and never flickers during one.
export default function GateLamp({ gain = 1, accent = '#ff2d55' }) {
  const open = Math.max(0, Math.min(1, gain));
  const glow = 0.15 + 0.85 * open;

  return (
    <div className="flex items-center gap-2 select-none" title="Gate — lit means signal is passing">
      <span className="text-[9px] uppercase tracking-[0.3em] text-white/45 w-6 shrink-0">Gt</span>
      <div className="relative w-4 h-4 shrink-0">
        <div
          className="absolute inset-0 rounded-full border"
          style={{
            borderColor: 'rgba(255,255,255,0.18)',
            background: `radial-gradient(circle at 35% 30%, ${accent}${Math.round(glow * 255).toString(16).padStart(2, '0')}, #1a1113 72%)`,
            boxShadow: open > 0.15 ? `0 0 ${4 + 8 * open}px ${accent}66` : 'none',
            transition: 'box-shadow 60ms linear'
          }}
        />
      </div>
    </div>
  );
}
