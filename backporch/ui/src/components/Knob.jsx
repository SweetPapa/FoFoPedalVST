import React, { useEffect, useRef, useState, useCallback } from 'react';
import { getSliderState } from '../juceBridge.js';

// Series-B knob. Drag vertically; Shift = fine; double-click = default.
export default function Knob({ paramId, label, accent = '#ffb454', defaultNorm = 0.5, size = 120 }) {
  const stateRef = useRef(null);
  const [norm, setNorm] = useState(defaultNorm);
  const dragRef = useRef({ active: false, startY: 0, startVal: 0 });

  useEffect(() => {
    const s = getSliderState(paramId, defaultNorm);
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
    const sensitivity = e.shiftKey ? 700 : 220;
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
    stateRef.current.setNormalisedValue(defaultNorm);
    setNorm(defaultNorm);
    stateRef.current.sliderDragEnded?.();
  }, [defaultNorm]);

  const minAngle = -135, maxAngle = 135;
  const angle = minAngle + (maxAngle - minAngle) * norm;
  const cx = size / 2, cy = size / 2, r = size / 2 - 12;
  const toXY = (a) => {
    const rad = ((a - 90) * Math.PI) / 180;
    return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
  };
  const [sx, sy] = toXY(minAngle);
  const [ex, ey] = toXY(angle);
  const largeArc = angle - minAngle > 180 ? 1 : 0;
  const arcPath = `M ${sx} ${sy} A ${r} ${r} 0 ${largeArc} 1 ${ex} ${ey}`;
  const [bex, bey] = toXY(maxAngle);
  const bgPath = `M ${sx} ${sy} A ${r} ${r} 0 1 1 ${bex} ${bey}`;

  return (
    <div className="flex flex-col items-center select-none">
      <svg
        width={size} height={size} viewBox={`0 0 ${size} ${size}`}
        onPointerDown={onPointerDown} onPointerMove={onPointerMove}
        onPointerUp={endDrag} onPointerCancel={endDrag} onDoubleClick={onDoubleClick}
        className="cursor-ns-resize touch-none"
      >
        <circle cx={cx} cy={cy} r={r - 6} fill="rgba(255,255,255,0.04)"
                stroke="rgba(255,255,255,0.10)" strokeWidth="1.5" />
        <path d={bgPath} fill="none" stroke="rgba(255,255,255,0.12)" strokeWidth="5" strokeLinecap="round" />
        <path d={arcPath} fill="none" stroke={accent} strokeWidth="5" strokeLinecap="round" />
        <line
          x1={cx} y1={cy}
          x2={cx + (r - 16) * Math.cos(((angle - 90) * Math.PI) / 180)}
          y2={cy + (r - 16) * Math.sin(((angle - 90) * Math.PI) / 180)}
          stroke={accent} strokeWidth="4" strokeLinecap="round"
        />
        <circle cx={cx} cy={cy} r="4.5" fill={accent} />
      </svg>
      <div className="mt-2 text-[11px] uppercase tracking-[0.35em] opacity-90" style={{ color: accent }}>
        {label}
      </div>
      <div className="text-lg tabular-nums opacity-80">{Math.round(norm * 100)}</div>
    </div>
  );
}
