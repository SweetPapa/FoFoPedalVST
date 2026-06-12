import React, { useEffect, useRef, useState, useCallback } from 'react';
import { getSliderState } from '../juceBridge.js';

// Vertical-drag knob bound to a JUCE WebSliderRelay parameter.
// Drag up/down to change value; Shift = fine. Double-click resets to default.
// `color` is the SVG stroke for the active arc + indicator (rainbow panel).
export default function Knob({ paramId, label, size = 110, color = '#ff8a3d' }) {
  const stateRef = useRef(null);
  const [norm, setNorm] = useState(0.5);
  const dragRef = useRef({ active: false, startY: 0, startVal: 0 });

  useEffect(() => {
    const s = getSliderState(paramId, 0.5);
    stateRef.current = s;
    const sync = () => setNorm(s.getNormalisedValue());
    sync();
    s.valueChangedEvent.addListener(sync);
    return () => s.valueChangedEvent.removeListener?.(sync);
  }, [paramId]);

  const onPointerDown = useCallback((e) => {
    e.target.setPointerCapture(e.pointerId);
    dragRef.current = { active: true, startY: e.clientY, startVal: stateRef.current.getNormalisedValue() };
    stateRef.current.sliderDragStarted?.();
  }, []);

  const onPointerMove = useCallback((e) => {
    if (!dragRef.current.active) return;
    const dy = dragRef.current.startY - e.clientY;
    const sensitivity = e.shiftKey ? 600 : 200;
    const next = Math.max(0, Math.min(1, dragRef.current.startVal + dy / sensitivity));
    stateRef.current.setNormalisedValue(next);
    setNorm(next);
  }, []);

  const endDrag = useCallback((e) => {
    if (!dragRef.current.active) return;
    dragRef.current.active = false;
    stateRef.current.sliderDragEnded?.();
    try { e.target.releasePointerCapture(e.pointerId); } catch {}
  }, []);

  const onDoubleClick = useCallback(() => {
    stateRef.current.sliderDragStarted?.();
    stateRef.current.setNormalisedValue(0.5);
    setNorm(0.5);
    stateRef.current.sliderDragEnded?.();
  }, []);

  const minAngle = -135;
  const maxAngle = 135;
  const angle = minAngle + (maxAngle - minAngle) * norm;
  const cx = size / 2;
  const cy = size / 2;
  const r = size / 2 - 10;
  const toXY = (a) => {
    const rad = ((a - 90) * Math.PI) / 180;
    return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
  };
  const [sx, sy] = toXY(minAngle);
  const [ex, ey] = toXY(angle);
  const largeArc = angle - minAngle > 180 ? 1 : 0;
  const arcPath = `M ${sx} ${sy} A ${r} ${r} 0 ${largeArc} 1 ${ex} ${ey}`;

  const [bsx, bsy] = toXY(minAngle);
  const [bex, bey] = toXY(maxAngle);
  const bgPath = `M ${bsx} ${bsy} A ${r} ${r} 0 1 1 ${bex} ${bey}`;

  // Glow above 50% — gives the panel a sense of liveness when you're actually
  // using the knob.
  const glow = Math.max(0, (norm - 0.5) * 2);
  const filterId = `glow-${paramId}`;

  return (
    <div className="flex flex-col items-center select-none group">
      <svg
        width={size}
        height={size}
        viewBox={`0 0 ${size} ${size}`}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
        onDoubleClick={onDoubleClick}
        className="cursor-ns-resize touch-none transition-transform duration-150 group-hover:scale-[1.03]"
      >
        <defs>
          <filter id={filterId} x="-30%" y="-30%" width="160%" height="160%">
            <feGaussianBlur stdDeviation={2 + glow * 4} result="b" />
            <feMerge>
              <feMergeNode in="b" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
          <radialGradient id={`face-${paramId}`} cx="50%" cy="40%" r="70%">
            <stop offset="0%" stopColor="#2b313c" />
            <stop offset="100%" stopColor="#1a1d24" />
          </radialGradient>
        </defs>

        {/* Knob body */}
        <circle cx={cx} cy={cy} r={r - 4} fill={`url(#face-${paramId})`} stroke="#2c313b" strokeWidth="2" />

        {/* Track (full sweep, dim) */}
        <path d={bgPath} fill="none" stroke="#2c313b" strokeWidth="5" strokeLinecap="round" />

        {/* Value arc — glows when value is high */}
        <path
          d={arcPath}
          fill="none"
          stroke={color}
          strokeWidth="5"
          strokeLinecap="round"
          filter={glow > 0.05 ? `url(#${filterId})` : undefined}
          opacity={0.55 + 0.45 * norm}
        />

        {/* Indicator line */}
        <line
          x1={cx}
          y1={cy}
          x2={cx + (r - 14) * Math.cos(((angle - 90) * Math.PI) / 180)}
          y2={cy + (r - 14) * Math.sin(((angle - 90) * Math.PI) / 180)}
          stroke={color}
          strokeWidth="3"
          strokeLinecap="round"
        />

        {/* Center dot */}
        <circle cx={cx} cy={cy} r="2.5" fill={color} opacity="0.8" />
      </svg>
      <div className="mt-1.5 text-[10px] uppercase tracking-[0.25em]" style={{ color }}>
        {label}
      </div>
      <div className="text-sm tabular-nums text-vroom-ink">{Math.round(norm * 100)}</div>
    </div>
  );
}
