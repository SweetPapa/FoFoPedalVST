import React, { useCallback, useRef } from 'react';
import * as Juce from '../juce/index.js';

// Drag-to-resize grip. The native WKWebView sits on top of JUCE's own corner
// resizer, so the grip lives in the web UI instead and calls the editor's
// registered `resizeTo` native function.
export default function ResizeHandle({ minWidth = 400, minHeight = 300 }) {
  const dragRef = useRef({ active: false, startX: 0, startY: 0, w: 0, h: 0 });
  const fnRef = useRef(null);

  const getFn = () => {
    if (fnRef.current) return fnRef.current;
    try {
      if (window.__JUCE__) fnRef.current = Juce.getNativeFunction('resizeTo');
    } catch {
      fnRef.current = null;
    }
    return fnRef.current;
  };

  const onPointerDown = useCallback((e) => {
    e.preventDefault();
    e.target.setPointerCapture(e.pointerId);
    dragRef.current = {
      active: true,
      startX: e.screenX, startY: e.screenY,
      w: window.innerWidth, h: window.innerHeight
    };
  }, []);

  const onPointerMove = useCallback((e) => {
    const d = dragRef.current;
    if (!d.active) return;
    const fn = getFn();
    if (!fn) return;
    const w = Math.max(minWidth, Math.round(d.w + (e.screenX - d.startX)));
    const h = Math.max(minHeight, Math.round(d.h + (e.screenY - d.startY)));
    fn(w, h);
  }, [minWidth, minHeight]);

  const endDrag = useCallback((e) => {
    dragRef.current.active = false;
    try { e.target.releasePointerCapture(e.pointerId); } catch { /* already released */ }
  }, []);

  return (
    <div
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={endDrag}
      onPointerCancel={endDrag}
      title="Drag to resize"
      style={{
        position: 'fixed', right: 0, bottom: 0, width: 22, height: 22,
        cursor: 'nwse-resize', zIndex: 1000, touchAction: 'none',
        opacity: 0.45
      }}
    >
      <svg viewBox="0 0 22 22" width="22" height="22">
        <path d="M19 9 L9 19 M19 14 L14 19" stroke="currentColor" strokeWidth="2" strokeLinecap="round" fill="none" />
      </svg>
    </div>
  );
}
