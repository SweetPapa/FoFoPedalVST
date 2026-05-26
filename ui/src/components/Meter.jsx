import React from 'react';

// Converts linear amplitude (1.0 = 0 dBFS) to a 0..1 display value across
// a -60..0 dB scale. Anything below -60 dB shows as zero.
function linearToMeterNorm(level) {
  if (!level || level <= 0) return 0;
  const dB = 20 * Math.log10(level);
  if (dB <= -60) return 0;
  if (dB >= 0) return 1;
  return (dB + 60) / 60;
}

function colorForLevel(level) {
  if (!level || level <= 0) return '#3aff8a';
  const dB = 20 * Math.log10(level);
  if (dB >= -3) return '#ff3b3b';   // clipping zone
  if (dB >= -12) return '#ffb800';  // hot
  return '#3aff8a';                 // safe green
}

export default function Meter({ label, level }) {
  const norm = linearToMeterNorm(level);
  const color = colorForLevel(level);

  return (
    <div className="flex items-center gap-3 min-w-0 flex-1">
      <span className="text-[10px] uppercase tracking-[0.25em] text-vroom-dim w-8 shrink-0">
        {label}
      </span>
      <div className="relative h-3 flex-1 bg-vroom-edge rounded-full overflow-hidden">
        <div
          className="absolute inset-y-0 left-0 rounded-full transition-[width] duration-75 ease-out"
          style={{ width: `${norm * 100}%`, backgroundColor: color }}
        />
        {/* 0 dB tick */}
        <div className="absolute right-0 top-0 bottom-0 w-px bg-white/30" />
        {/* -12 dB tick (80% of bar) */}
        <div className="absolute top-0 bottom-0 w-px bg-white/15" style={{ left: '80%' }} />
      </div>
      <span className="text-[10px] tabular-nums text-vroom-dim w-10 text-right shrink-0">
        {level > 0 ? `${Math.max(-60, 20 * Math.log10(level)).toFixed(0)} dB` : '−∞'}
      </span>
    </div>
  );
}
