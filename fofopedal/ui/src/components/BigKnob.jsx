import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { getSliderState } from '../juceBridge.js';

// A warm "sketched" knob: paper-coloured face, soft sage ring, terracotta
// indicator stroke. Pulses an amber glow when the value crosses 50%.
// Drag vertically; shift-drag for fine; double-click resets to default.
export default function BigKnob({
  paramId,
  label,
  subtitle = null,
  size = 110,
  accent = 'terracotta',
  fallbackInitial = 0.3,
  defaultNormalised = null,
  formatValue = null,
}) {
  const slider = useMemo(() => getSliderState(paramId, fallbackInitial), [paramId]);
  const [n, setN] = useState(() => slider.getNormalisedValue());
  const ref = useRef(null);
  const drag = useRef({ y: 0, n: 0, fine: false, dragging: false });

  useEffect(() => {
    const onChange = () => setN(slider.getNormalisedValue());
    slider.valueChangedEvent.addListener(onChange);
    onChange();
    return () => slider.valueChangedEvent.removeListener(onChange);
  }, [slider]);

  const startDrag = useCallback((e) => {
    e.preventDefault();
    drag.current = {
      y: e.clientY ?? e.touches?.[0]?.clientY ?? 0,
      n: slider.getNormalisedValue(),
      fine: e.shiftKey,
      dragging: true,
    };
    slider.sliderDragStarted();
    const move = (ev) => {
      if (!drag.current.dragging) return;
      const y = ev.clientY ?? ev.touches?.[0]?.clientY ?? 0;
      const dy = drag.current.y - y;
      const scale = drag.current.fine ? 0.0015 : 0.006;
      const next = Math.max(0, Math.min(1, drag.current.n + dy * scale));
      slider.setNormalisedValue(next);
    };
    const end = () => {
      drag.current.dragging = false;
      slider.sliderDragEnded();
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', end);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', end);
  }, [slider]);

  const onDoubleClick = useCallback(() => {
    const def = defaultNormalised ?? 0.3;
    slider.sliderDragStarted();
    slider.setNormalisedValue(def);
    slider.sliderDragEnded();
  }, [slider, defaultNormalised]);

  // Indicator angle: -135° at 0, +135° at 1.
  const angle = -135 + n * 270;
  const ringPct = Math.max(0.02, n);

  // Accent → stroke + glow colours.
  const palette = {
    terracotta: { stroke: '#b56245', soft: '#d49477', glow: '#e6a85c' },
    mustard:    { stroke: '#c89a3a', soft: '#e0bd6d', glow: '#f0c777' },
    sage:       { stroke: '#7d9277', soft: '#a8bfa1', glow: '#a8bfa1' },
    ink:        { stroke: '#2a1f15', soft: '#7a6b56', glow: '#e6a85c' },
  }[accent] || { stroke: '#b56245', soft: '#d49477', glow: '#e6a85c' };

  // Pencil-sketched arc: SVG arc path computed from current value.
  const s = size;
  const cx = s / 2, cy = s / 2;
  const r = (s / 2) - 8;
  const startA = -135;
  const endA = startA + 270 * ringPct;
  const toXY = (ang) => {
    const rad = (ang - 90) * Math.PI / 180;
    return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
  };
  const [x1, y1] = toXY(startA);
  const [x2, y2] = toXY(endA);
  const largeArc = (endA - startA) > 180 ? 1 : 0;
  const arcPath = `M ${x1} ${y1} A ${r} ${r} 0 ${largeArc} 1 ${x2} ${y2}`;

  return (
    <div className="flex flex-col items-center select-none">
      <div
        ref={ref}
        onPointerDown={startDrag}
        onDoubleClick={onDoubleClick}
        style={{ width: size, height: size }}
        className="relative cursor-ns-resize"
      >
        {/* glow */}
        <div
          className="absolute inset-2 rounded-full pointer-events-none transition-opacity"
          style={{
            background: `radial-gradient(closest-side, ${palette.glow}55 0%, transparent 70%)`,
            opacity: Math.max(0, n - 0.5) * 1.4
          }}
        />

        <svg viewBox={`0 0 ${s} ${s}`} width={s} height={s} className="relative">
          {/* Background ring (paper line) */}
          <circle cx={cx} cy={cy} r={r} fill="#fbf6e8" stroke="#d8cdb3" strokeWidth="1.4" />
          {/* Track */}
          <path
            d={`M ${toXY(-135)[0]} ${toXY(-135)[1]} A ${r} ${r} 0 1 1 ${toXY(135)[0]} ${toXY(135)[1]}`}
            fill="none"
            stroke="#e8dcc1"
            strokeWidth="3"
            strokeLinecap="round"
          />
          {/* Active arc */}
          <path
            d={arcPath}
            fill="none"
            stroke={palette.stroke}
            strokeWidth="3.4"
            strokeLinecap="round"
            opacity="0.95"
          />
          {/* Indicator line */}
          <g transform={`rotate(${angle} ${cx} ${cy})`}>
            <line x1={cx} y1={cy - 6} x2={cx} y2={cy - (r - 8)}
                  stroke={palette.stroke} strokeWidth="2.2" strokeLinecap="round" />
          </g>
          {/* Center dot */}
          <circle cx={cx} cy={cy} r={2.2} fill={palette.stroke} />
        </svg>
      </div>

      <div className="mt-2 text-[11px] font-semibold tracking-[0.22em] uppercase text-paper-ink">
        {label}
      </div>
      {subtitle && (
        <div className="text-[10px] tracking-wide text-paper-dim mt-0.5 lowercase">
          {subtitle}
        </div>
      )}
      <div className="text-[10px] tabular-nums text-paper-dim mt-0.5 font-mono">
        {formatValue ? formatValue(n) : Math.round(n * 100)}
      </div>
    </div>
  );
}
