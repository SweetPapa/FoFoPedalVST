import React from 'react';

function linearToNorm(level) {
  if (!level || level <= 0) return 0;
  const dB = 20 * Math.log10(level);
  if (dB <= -60) return 0;
  if (dB >= 0) return 1;
  return (dB + 60) / 60;
}

function colorForLevel(level) {
  if (!level || level <= 0) return '#7a8a94';
  const dB = 20 * Math.log10(level);
  if (dB >= -3) return '#ff2d55';
  if (dB >= -12) return '#ffb545';
  return '#8fa4b0';
}

export default function Meter({ label, level }) {
  const norm = linearToNorm(level);
  const color = colorForLevel(level);
  return (
    <div className="flex items-center gap-2 min-w-0">
      <span className="text-[9px] uppercase tracking-[0.3em] text-white/45 w-6 shrink-0">{label}</span>
      <div className="relative h-1.5 w-20 shrink-0 bg-white/10 overflow-hidden">
        <div className="absolute inset-y-0 left-0 transition-[width] duration-75 ease-out"
             style={{ width: `${norm * 100}%`, backgroundColor: color }} />
      </div>
    </div>
  );
}
