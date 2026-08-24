import React, { useEffect, useRef, useState, useCallback } from 'react';
import { getSliderState } from '../juceBridge.js';

// DREAMRIPPER's knob. The family gesture is unchanged — drag vertically, hold
// shift for fine, double-click for the default — but this one is a chicken-head
// pointer on a notched skirt, because that is what the front of an amplifier
// looks like and this pedal is an amplifier.
//
// `centred` draws the fill outward from twelve o'clock instead of from the
// left stop, which is the honest picture for SCOOP and LEVEL: those knobs have
// a middle, and a fill that grows from zero would tell you they don't.
export default function Knob({
  paramId,
  label,
  hint,
  accent = '#ff2d55',
  defaultNorm = 0.5,
  size = 112,
  centred = false,
  dim = false
}) {
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
    try { e.target.releasePointerCapture(e.pointerId); } catch { /* already released */ }
  }, []);

  const onDoubleClick = useCallback(() => {
    stateRef.current.sliderDragStarted?.();
    stateRef.current.setNormalisedValue(defaultNorm);
    setNorm(defaultNorm);
    stateRef.current.sliderDragEnded?.();
  }, [defaultNorm]);

  const minAngle = -140, maxAngle = 140;
  const angle = minAngle + (maxAngle - minAngle) * norm;
  const cx = size / 2, cy = size / 2;
  const rTrack = size / 2 - 8;
  const rBody = rTrack - 13;

  const toXY = (a, r) => {
    const rad = ((a - 90) * Math.PI) / 180;
    return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
  };

  const arcPath = (from, to) => {
    const [sx, sy] = toXY(from, rTrack);
    const [ex, ey] = toXY(to, rTrack);
    const large = Math.abs(to - from) > 180 ? 1 : 0;
    const sweep = to >= from ? 1 : 0;
    return `M ${sx} ${sy} A ${rTrack} ${rTrack} 0 ${large} ${sweep} ${ex} ${ey}`;
  };

  const fillFrom = centred ? 0 : minAngle;
  const showFill = Math.abs(angle - fillFrom) > 0.4;

  // Notches around the skirt. They are the only thing that makes a knob
  // readable from across a room, and they cost nothing.
  const ticks = [];
  for (let i = 0; i <= 10; i += 1) {
    const a = minAngle + ((maxAngle - minAngle) * i) / 10;
    const major = i === 0 || i === 5 || i === 10;
    const [x1, y1] = toXY(a, rTrack + 5);
    const [x2, y2] = toXY(a, rTrack + (major ? 10 : 8));
    ticks.push(
      <line key={i} x1={x1} y1={y1} x2={x2} y2={y2}
            stroke={major ? 'rgba(255,255,255,0.35)' : 'rgba(255,255,255,0.14)'}
            strokeWidth={major ? 1.6 : 1} strokeLinecap="round" />
    );
  }

  const [px, py] = toXY(angle, rBody - 5);
  const [bx, by] = toXY(angle, rBody * 0.34);

  return (
    <div className={`flex flex-col items-center select-none ${dim ? 'opacity-75' : ''}`}>
      <svg
        width={size} height={size} viewBox={`0 0 ${size} ${size}`}
        onPointerDown={onPointerDown} onPointerMove={onPointerMove}
        onPointerUp={endDrag} onPointerCancel={endDrag} onDoubleClick={onDoubleClick}
        className="cursor-ns-resize touch-none"
      >
        <defs>
          <radialGradient id={`${paramId}-face`} cx="38%" cy="30%" r="78%">
            <stop offset="0%" stopColor="#3a3134" />
            <stop offset="62%" stopColor="#1b1618" />
            <stop offset="100%" stopColor="#0d0a0b" />
          </radialGradient>
        </defs>

        {ticks}

        <path d={arcPath(minAngle, maxAngle)} fill="none"
              stroke="rgba(255,255,255,0.10)" strokeWidth="4" strokeLinecap="round" />
        {showFill && (
          <path d={arcPath(fillFrom, angle)} fill="none"
                stroke={accent} strokeWidth="4" strokeLinecap="round" />
        )}
        {/* A centred knob sitting exactly at noon draws no arc at all, which
            reads as "broken" rather than as "at the detent". The notch says
            where the middle is whether or not there is anything filled. */}
        {centred && (
          <line x1={cx} y1={cy - rTrack - 3} x2={cx} y2={cy - rTrack + 3}
                stroke={accent} strokeWidth="2" strokeLinecap="round" opacity="0.9" />
        )}

        <circle cx={cx} cy={cy} r={rBody} fill={`url(#${paramId}-face)`}
                stroke="rgba(255,255,255,0.13)" strokeWidth="1" />
        <circle cx={cx} cy={cy} r={rBody - 4} fill="none"
                stroke="rgba(0,0,0,0.55)" strokeWidth="1" />

        <line x1={bx} y1={by} x2={px} y2={py}
              stroke={accent} strokeWidth="3.5" strokeLinecap="round" />
      </svg>

      <div className="mt-2 text-[11px] uppercase tracking-[0.32em] font-semibold" style={{ color: accent }}>
        {label}
      </div>
      <div className="text-[15px] tabular-nums text-white/70 leading-tight">{Math.round(norm * 100)}</div>
      {hint && (
        <div className="text-[8.5px] uppercase tracking-[0.18em] text-white/25 mt-0.5 whitespace-nowrap">
          {hint}
        </div>
      )}
    </div>
  );
}
