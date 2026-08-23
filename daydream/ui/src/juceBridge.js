// Vendored JUCE 8 WebView bridge (same as VROOM's). The Juce frontend module
// wraps window.__JUCE__ — in dev (no host) it falls back to a mock store so
// the UI can still render and respond to mouse interactions.
import * as Juce from './juce/index.js';

function isJuceLive(name) {
  const sliders = window.__JUCE__?.initialisationData?.__juce__sliders;
  return Array.isArray(sliders) && sliders.includes(name);
}

function isFunctionLive(name) {
  const fns = window.__JUCE__?.initialisationData?.__juce__functions;
  return Array.isArray(fns) && fns.includes(name);
}

const hasJuceGlobal = typeof window !== 'undefined' && !!window.__JUCE__;

function createMockSliderState(initial = 0.35) {
  let value = initial;
  const listeners = new Set();
  return {
    getNormalisedValue: () => value,
    setNormalisedValue: (v) => {
      value = Math.max(0, Math.min(1, v));
      listeners.forEach((cb) => cb());
    },
    sliderDragStarted: () => {},
    sliderDragEnded: () => {},
    properties: { name: 'mock', start: 0, end: 100, skew: 1 },
    valueChangedEvent: { addListener: (cb) => listeners.add(cb), removeListener: (cb) => listeners.delete(cb) },
    propertiesChangedEvent: { addListener: () => {}, removeListener: () => {} }
  };
}

export function getSliderState(name, fallbackInitial = 0.35) {
  if (isJuceLive(name)) return Juce.getSliderState(name);
  return createMockSliderState(fallbackInitial);
}

export const isJuceHost = hasJuceGlobal;

// ── Presets ──────────────────────────────────────────────────────────────────
// The bank lives in C++ (Source/presets/PresetBank.cpp) and reaches the UI over
// the native bridge. This mirror exists only so the browser dev preview can
// step through the same names; in a host, every value below is replaced by
// what the plugin sends.
const MOCK_PRESETS = [
  { name: 'Barely There', blurb: 'the tape is on, and that is all — glue, not an effect' },
  { name: 'Warm Tape', blurb: 'saturation and a small room; still sounds like the dry track' },
  { name: 'Cassette Room', blurb: 'the top end starts to gauze over and the room gets real' },
  { name: 'Memory', blurb: 'the house sound — wow and flutter arrive, the field opens up' },
  { name: 'Wide Hall', blurb: 'the room has become a hall and the wobble is unmistakable' },
  { name: 'Shimmer', blurb: 'octaves begin climbing out of the tail' },
  { name: 'Endless', blurb: 'the decay stops resolving; the wash swells back in the gaps' },
  { name: 'Full Dream', blurb: 'everything, all of it — for endings and empty bars' }
];

function mockPresetState(index) {
  const p = MOCK_PRESETS[index] ?? MOCK_PRESETS[0];
  return { name: p.name, blurb: p.blurb, index, count: MOCK_PRESETS.length, modified: false };
}

let mockPresetIndex = 3;
const mockPresetListeners = new Set();

// Ask the plugin which preset is loaded. Falls back to the mirror above when
// there is no host.
export async function fetchPresetState() {
  const fn = isFunctionLive('getPresetState') ? Juce.getNativeFunction('getPresetState') : null;
  if (!fn) return mockPresetState(mockPresetIndex);
  try {
    return await fn();
  } catch {
    return mockPresetState(mockPresetIndex);
  }
}

// Move one preset forwards (+1) or backwards (-1), wrapping at both ends.
export async function stepPreset(direction) {
  const fn = isFunctionLive('stepPreset') ? Juce.getNativeFunction('stepPreset') : null;
  if (!fn) {
    const n = MOCK_PRESETS.length;
    mockPresetIndex = ((mockPresetIndex + (direction >= 0 ? 1 : -1)) % n + n) % n;
    const state = mockPresetState(mockPresetIndex);
    mockPresetListeners.forEach((cb) => cb(state));
    return state;
  }
  try {
    return await fn(direction);
  } catch {
    return null;
  }
}

// Subscribe to preset changes, including ones the host makes behind our back.
export function onPresetState(handler) {
  const backend = isFunctionLive('getPresetState') ? window.__JUCE__?.backend : null;
  if (!backend) {
    mockPresetListeners.add(handler);
    return () => mockPresetListeners.delete(handler);
  }
  const token = backend.addEventListener('presetState', handler);
  return () => backend.removeEventListener(token);
}
