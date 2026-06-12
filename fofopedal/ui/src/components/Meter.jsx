import React from 'react';

// Hand-drawn-looking peak meter. Off-white well, terracotta fill, no LEDs.
export default function Meter({ label, level }) {
  const clamped = Math.max(0, Math.min(1, level));
  return (
    <div className="flex items-center gap-2">
      <span className="text-[10px] uppercase tracking-[0.25em] text-paper-dim w-6 text-right">
        {label}
      </span>
      <div className="relative w-24 h-2.5 rounded-full bg-paper-deep overflow-hidden border border-paper-line">
        <div
          className="absolute inset-y-0 left-0 transition-[width] duration-75"
          style={{
            width: `${clamped * 100}%`,
            background: 'linear-gradient(90deg, #7d9277 0%, #c89a3a 70%, #b56245 100%)'
          }}
        />
      </div>
    </div>
  );
}
