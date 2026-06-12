import React, { useEffect, useRef, useState, useCallback } from 'react';
import { getSliderState } from '../juceBridge.js';

// Centerpiece knob. Drag vertically; Shift for fine; double-click resets to 0.
// The knob's halo and indicator tint shift along the dial:
//   low  = cool teal (clean / barely-there)
//   mid  = purple (drifty, dreamy)
//   high = warm magenta (full wash / lo-fi)
export default function BigKnob({ paramId, label, size = 240 }) {
  const stateRef = useRef(null);
  const [norm, setNorm] = useState(0.35);
  const dragRef = useRef({ active: false, startY: 0, startVal: 0 });

  useEffect(() => {
    const s = getSliderState(paramId, 0.35);
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
    const sensitivity = e.shiftKey ? 700 : 240;
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
    stateRef.current.setNormalisedValue(0);
    setNorm(0);
    stateRef.current.sliderDragEnded?.();
  }, []);

  // Dial geometry.
  const minAngle = -135;
  const maxAngle = 135;
  const angle = minAngle + (maxAngle - minAngle) * norm;
  const cx = size / 2;
  const cy = size / 2;
  const r = size / 2 - 18;
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

  // Color tint shifts across the dial.
  const blend = (a, b, t) => a + (b - a) * t;
  const lerpColor = (c1, c2, t) => `rgb(${blend(c1[0], c2[0], t).toFixed(0)},${blend(c1[1], c2[1], t).toFixed(0)},${blend(c1[2], c2[2], t).toFixed(0)})`;
  const cool = [76, 222, 240];   // #4cdef0 teal
  const mid  = [192, 132, 255];  // #c084ff purple
  const hot  = [255, 138, 211];  // #ff8ad3 magenta
  const color = norm < 0.5
    ? lerpColor(cool, mid, norm / 0.5)
    : lerpColor(mid, hot, (norm - 0.5) / 0.5);

  const haloSize = size * (1.15 + norm * 0.5);

  return (
    <div className="relative flex flex-col items-center select-none">
      {/* Halo glow — grows and intensifies with the dial */}
      <div
        className="absolute rounded-full pointer-events-none"
        style={{
          width: haloSize,
          height: haloSize,
          left: (size - haloSize) / 2,
          top: (size - haloSize) / 2,
          background: `radial-gradient(circle, ${color}${Math.round(40 + norm * 70).toString(16).padStart(2, '0')} 0%, transparent 70%)`,
          filter: `blur(${20 + norm * 25}px)`,
          transition: 'background 200ms ease-out'
        }}
      />

      <svg
        width={size}
        height={size}
        viewBox={`0 0 ${size} ${size}`}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
        onDoubleClick={onDoubleClick}
        className="relative cursor-ns-resize touch-none drop-shadow-lg"
      >
        <defs>
          <radialGradient id={`face-${paramId}`} cx="50%" cy="35%" r="75%">
            <stop offset="0%"  stopColor="#3a2b65" />
            <stop offset="60%" stopColor="#1f1340" />
            <stop offset="100%" stopColor="#0f0822" />
          </radialGradient>
          <filter id={`g-${paramId}`} x="-30%" y="-30%" width="160%" height="160%">
            <feGaussianBlur stdDeviation={2 + norm * 6} result="b" />
            <feMerge>
              <feMergeNode in="b" />
              <feMergeNode in="SourceGraphic" />
            </feMerge>
          </filter>
        </defs>

        <circle cx={cx} cy={cy} r={r + 6} fill="none" stroke="#2c1e4d" strokeWidth="1" />
        <circle cx={cx} cy={cy} r={r - 4} fill={`url(#face-${paramId})`} stroke="#2c1e4d" strokeWidth="2" />

        <path d={bgPath} fill="none" stroke="#2c1e4d" strokeWidth="7" strokeLinecap="round" />
        <path
          d={arcPath}
          fill="none"
          stroke={color}
          strokeWidth="7"
          strokeLinecap="round"
          filter={`url(#g-${paramId})`}
        />

        {/* Indicator */}
        <line
          x1={cx}
          y1={cy}
          x2={cx + (r - 22) * Math.cos(((angle - 90) * Math.PI) / 180)}
          y2={cy + (r - 22) * Math.sin(((angle - 90) * Math.PI) / 180)}
          stroke={color}
          strokeWidth="5"
          strokeLinecap="round"
        />
        <circle cx={cx} cy={cy} r="6" fill={color} opacity="0.9" />
      </svg>

      <div className="mt-3 text-[11px] uppercase tracking-[0.45em]" style={{ color }}>
        {label}
      </div>
      <div className="text-2xl tabular-nums mt-0.5" style={{ color }}>
        {Math.round(norm * 100)}
      </div>
    </div>
  );
}
