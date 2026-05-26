import React, { useEffect, useRef, useState, useCallback } from 'react';
import { getSliderState } from '../juceBridge.js';

// Vertical-drag knob bound to a JUCE WebSliderRelay parameter.
// Drag up/down to change value; double-click to reset to default (0.5 norm).
export default function Knob({ paramId, label, size = 120 }) {
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

  // Arc maths: knob sweeps from -135° to +135°.
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

  return (
    <div className="flex flex-col items-center select-none">
      <svg
        width={size}
        height={size}
        viewBox={`0 0 ${size} ${size}`}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
        onDoubleClick={onDoubleClick}
        className="cursor-ns-resize touch-none"
      >
        <circle cx={cx} cy={cy} r={r - 4} fill="#22262e" stroke="#2c313b" strokeWidth="2" />
        <path d={bgPath} fill="none" stroke="#2c313b" strokeWidth="5" strokeLinecap="round" />
        <path d={arcPath} fill="none" stroke="#ff8a3d" strokeWidth="5" strokeLinecap="round" />
        <line
          x1={cx}
          y1={cy}
          x2={cx + (r - 14) * Math.cos(((angle - 90) * Math.PI) / 180)}
          y2={cy + (r - 14) * Math.sin(((angle - 90) * Math.PI) / 180)}
          stroke="#e6e6e6"
          strokeWidth="3"
          strokeLinecap="round"
        />
      </svg>
      <div className="mt-2 text-xs uppercase tracking-[0.25em] text-vroom-dim">{label}</div>
      <div className="text-sm tabular-nums text-vroom-ink">{Math.round(norm * 100)}</div>
    </div>
  );
}
