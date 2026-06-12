import React, { useEffect, useState, useRef } from 'react';
import { isJuceHost } from '../juceBridge.js';
import * as Juce from '../juce/index.js';

const EMPTY_STATE = {
  current: { name: '', category: '', vibe: '', isFactory: true, modified: false },
  presets: []
};

// Vibe palette mirrors tailwind.config.js (kept inline as styles since the
// runtime needs the literal hex values for SVG/border colors).
const VIBE_COLORS = {
  Smooth: '#ffd06b',
  Crunch: '#ff7a3c',
  Lead:   '#ffd040',
  Fuzz:   '#c84cff',
  Fat:    '#d97a4c',
  Custom: '#6b8cff'
};
const VIBE_ORDER = ['Smooth', 'Crunch', 'Lead', 'Fuzz', 'Fat', 'Custom'];

function callNative(fnName, ...args) {
  if (!isJuceHost) return Promise.resolve(null);
  try {
    const fn = Juce.getNativeFunction(fnName);
    return fn(...args);
  } catch (e) {
    console.warn(`native ${fnName} failed`, e);
    return Promise.resolve(null);
  }
}

export default function PresetBar() {
  const [state, setState] = useState(EMPTY_STATE);
  const [browserOpen, setBrowserOpen] = useState(false);
  const [saveOpen, setSaveOpen] = useState(false);
  const [saveName, setSaveName] = useState('');
  const popoverRef = useRef(null);
  const saveInputRef = useRef(null);

  useEffect(() => {
    const backend = window.__JUCE__?.backend;
    if (!backend) return;
    const handler = (payload) => setState(payload ?? EMPTY_STATE);
    const token = backend.addEventListener('presetState', handler);
    callNative('getPresetState').then((s) => { if (s) setState(s); });
    return () => backend.removeEventListener(token);
  }, []);

  useEffect(() => {
    if (!browserOpen) return;
    const onDoc = (e) => {
      if (popoverRef.current && !popoverRef.current.contains(e.target)) setBrowserOpen(false);
    };
    document.addEventListener('mousedown', onDoc);
    return () => document.removeEventListener('mousedown', onDoc);
  }, [browserOpen]);

  useEffect(() => {
    if (saveOpen && saveInputRef.current) saveInputRef.current.focus();
  }, [saveOpen]);

  const cur = state.current ?? EMPTY_STATE.current;
  const presets = state.presets ?? [];

  // Group by vibe; preserve VIBE_ORDER. Unknown vibes fall into "Custom".
  const grouped = VIBE_ORDER.reduce((acc, v) => { acc[v] = []; return acc; }, {});
  for (const p of presets) {
    const vibe = p.vibe && VIBE_ORDER.includes(p.vibe) ? p.vibe
               : (p.isFactory ? 'Custom' : 'Custom');
    if (!grouped[vibe]) grouped[vibe] = [];
    grouped[vibe].push(p);
  }

  const handlePick = async (preset) => {
    setBrowserOpen(false);
    await callNative('loadPreset', preset.name, !!preset.isFactory);
  };
  const handleStep = (dir) => callNative('stepPreset', dir);
  const openSaveAs = () => {
    setSaveName(cur.name && !cur.isFactory ? cur.name : (cur.name || 'New preset'));
    setSaveOpen(true);
  };
  const handleSaveConfirm = async () => {
    const name = saveName.trim();
    if (!name) return;
    const ok = await callNative('saveUserPreset', name);
    if (ok) setSaveOpen(false);
  };
  const handleDelete = async () => {
    if (cur.isFactory || !cur.name) return;
    if (!window.confirm(`Delete user preset "${cur.name}"?`)) return;
    await callNative('deleteUserPreset', cur.name);
  };
  const canOverwrite = !cur.isFactory && !!cur.name;
  const handleSaveOver = async () => {
    if (!canOverwrite) { openSaveAs(); return; }
    await callNative('saveUserPreset', cur.name);
  };

  const currentColor = VIBE_COLORS[cur.vibe] ?? '#6b8cff';

  return (
    <div className="relative px-6 py-2 flex items-center gap-3 border-b border-vroom-edge bg-vroom-panel/40">
      <span className="text-[10px] uppercase tracking-[0.3em] text-vroom-dim w-14 shrink-0">
        Preset
      </span>

      <button type="button" onClick={() => handleStep(-1)}
              className="px-2 py-1 text-vroom-dim hover:text-vroom-ink rounded-sm" title="Previous">
        ◀
      </button>

      <button
        type="button"
        onClick={() => setBrowserOpen((v) => !v)}
        className="flex-1 max-w-[360px] flex items-center justify-between gap-2 px-3 py-1.5 bg-vroom-bg border rounded-md hover:border-vroom-dim"
        style={{ borderColor: cur.name ? currentColor + '66' : '#2c313b' }}
      >
        <span className="flex items-center gap-2 min-w-0">
          {cur.vibe && (
            <span className="w-2 h-2 rounded-full shrink-0"
                  style={{ backgroundColor: currentColor, boxShadow: `0 0 6px ${currentColor}` }} />
          )}
          <span className="text-sm truncate font-semibold">
            {cur.name || '(no preset)'}
          </span>
          {cur.modified && (
            <span className="w-1.5 h-1.5 rounded-full bg-vroom-accent shrink-0"
                  title="Modified — current state differs from the loaded preset" />
          )}
        </span>
        <span className="text-[10px] uppercase tracking-widest text-vroom-dim shrink-0">
          {cur.vibe || (cur.isFactory ? 'factory' : 'user')} · {cur.category || '—'}
        </span>
      </button>

      <button type="button" onClick={() => handleStep(1)}
              className="px-2 py-1 text-vroom-dim hover:text-vroom-ink rounded-sm" title="Next">
        ▶
      </button>

      <div className="flex items-center gap-1 ml-auto">
        <button
          type="button"
          onClick={handleSaveOver}
          disabled={!canOverwrite}
          className="px-3 py-1.5 rounded-md text-[11px] uppercase tracking-[0.2em] border border-vroom-edge text-vroom-ink hover:border-vroom-dim disabled:opacity-40 disabled:cursor-not-allowed"
          title={canOverwrite ? 'Overwrite this user preset' : 'Factory presets are read-only — use Save As'}
        >
          Save
        </button>
        <button
          type="button"
          onClick={openSaveAs}
          className="px-3 py-1.5 rounded-md text-[11px] uppercase tracking-[0.2em] border border-vroom-edge text-vroom-ink hover:border-vroom-dim"
        >
          Save As…
        </button>
        <button
          type="button"
          onClick={handleDelete}
          disabled={cur.isFactory || !cur.name}
          className="px-3 py-1.5 rounded-md text-[11px] uppercase tracking-[0.2em] border border-vroom-edge text-vroom-ink hover:border-red-400 hover:text-red-400 disabled:opacity-40 disabled:cursor-not-allowed"
        >
          Delete
        </button>
      </div>

      {browserOpen && (
        <div
          ref={popoverRef}
          className="absolute top-full left-24 mt-1 z-10 w-[640px] max-h-[420px] overflow-y-auto bg-vroom-panel border border-vroom-edge rounded-lg shadow-2xl p-3"
        >
          {VIBE_ORDER.map((vibe) => {
            const items = grouped[vibe] ?? [];
            if (items.length === 0) return null;
            const color = VIBE_COLORS[vibe];
            return (
              <div key={vibe} className="mb-3 last:mb-0">
                <div className="flex items-center gap-2 px-1 py-1">
                  <span className="w-2 h-2 rounded-full shrink-0"
                        style={{ backgroundColor: color, boxShadow: `0 0 8px ${color}` }} />
                  <span className="text-[10px] uppercase tracking-[0.3em]"
                        style={{ color }}>
                    {vibe}
                  </span>
                  <span className="text-[10px] text-vroom-dim">·  {items.length}</span>
                </div>
                <div className="grid grid-cols-3 gap-2">
                  {items.map((p) => {
                    const isCurrent = p.name === cur.name && p.isFactory === cur.isFactory;
                    return (
                      <button
                        key={`${vibe}-${p.name}`}
                        type="button"
                        onClick={() => handlePick(p)}
                        className="text-left px-3 py-2 rounded-md border bg-vroom-bg hover:bg-vroom-edge transition-colors"
                        style={{
                          borderColor: isCurrent ? color : '#2c313b',
                          boxShadow: isCurrent ? `0 0 14px ${color}44` : 'none'
                        }}
                      >
                        <div className="text-sm" style={{ color: isCurrent ? color : '#e6e6e6' }}>
                          {p.name}
                        </div>
                        <div className="text-[10px] uppercase tracking-widest text-vroom-dim mt-0.5">
                          {p.isFactory ? 'factory' : 'user'} · {p.category}
                        </div>
                      </button>
                    );
                  })}
                </div>
              </div>
            );
          })}
        </div>
      )}

      {saveOpen && (
        <div className="fixed inset-0 z-50 bg-black/60 flex items-center justify-center"
             onClick={() => setSaveOpen(false)}>
          <div className="bg-vroom-panel border border-vroom-edge rounded-md p-5 w-[340px]"
               onClick={(e) => e.stopPropagation()}>
            <div className="text-[10px] uppercase tracking-[0.3em] text-vroom-dim mb-2">
              Save user preset
            </div>
            <input
              ref={saveInputRef}
              value={saveName}
              onChange={(e) => setSaveName(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') handleSaveConfirm();
                if (e.key === 'Escape') setSaveOpen(false);
              }}
              className="w-full bg-vroom-bg border border-vroom-edge rounded px-3 py-2 text-sm focus:outline-none focus:border-vroom-accent"
              placeholder="Preset name"
            />
            <div className="flex justify-end gap-2 mt-4">
              <button type="button" onClick={() => setSaveOpen(false)}
                      className="px-3 py-1.5 rounded text-[11px] uppercase tracking-[0.2em] text-vroom-dim hover:text-vroom-ink">
                Cancel
              </button>
              <button type="button" onClick={handleSaveConfirm} disabled={!saveName.trim()}
                      className="px-3 py-1.5 rounded text-[11px] uppercase tracking-[0.2em] bg-vroom-accent text-black disabled:opacity-40 disabled:cursor-not-allowed">
                Save
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
