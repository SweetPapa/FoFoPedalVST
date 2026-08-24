// Vendored JUCE 8 WebView bridge — same vendored copy as VROOM/DAYDREAM. In
// dev (no host) the bridge falls back to mock state so the UI still renders.
import * as Juce from './juce/index.js';

function isSliderLive(name) {
  const sliders = window.__JUCE__?.initialisationData?.__juce__sliders;
  return Array.isArray(sliders) && sliders.includes(name);
}
function isToggleLive(name) {
  const toggles = window.__JUCE__?.initialisationData?.__juce__toggles;
  return Array.isArray(toggles) && toggles.includes(name);
}
function isComboLive(name) {
  const combos = window.__JUCE__?.initialisationData?.__juce__comboBoxes;
  return Array.isArray(combos) && combos.includes(name);
}

function isFunctionLive(name) {
  const fns = window.__JUCE__?.initialisationData?.__juce__functions;
  return Array.isArray(fns) && fns.includes(name);
}

const hasJuceGlobal = typeof window !== 'undefined' && !!window.__JUCE__;

function mockSlider(initial = 0.35) {
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
function mockToggle(initial = false) {
  let value = initial;
  const listeners = new Set();
  return {
    getValue: () => value,
    setValue: (v) => { value = !!v; listeners.forEach((cb) => cb()); },
    valueChangedEvent: { addListener: (cb) => listeners.add(cb), removeListener: (cb) => listeners.delete(cb) },
    propertiesChangedEvent: { addListener: () => {}, removeListener: () => {} }
  };
}
function mockCombo(initial = 0, choices = ['A','B','C']) {
  let value = initial;
  const listeners = new Set();
  return {
    getChoiceIndex: () => value,
    setChoiceIndex: (v) => { value = v|0; listeners.forEach((cb) => cb()); },
    properties: { choices },
    valueChangedEvent: { addListener: (cb) => listeners.add(cb), removeListener: (cb) => listeners.delete(cb) },
    propertiesChangedEvent: { addListener: () => {}, removeListener: () => {} }
  };
}

export function getSliderState(name, fallbackInitial = 0.35) {
  if (isSliderLive(name)) return Juce.getSliderState(name);
  return mockSlider(fallbackInitial);
}
export function getToggleState(name, fallbackInitial = false) {
  if (isToggleLive(name)) return Juce.getToggleState(name);
  return mockToggle(fallbackInitial);
}
export function getComboState(name, fallbackChoices = ['A','B','C']) {
  if (isComboLive(name)) return Juce.getComboBoxState(name);
  return mockCombo(0, fallbackChoices);
}

export const isJuceHost = hasJuceGlobal;

// ── Presets ──────────────────────────────────────────────────────────────────
// The bank lives in C++ (Source/presets/PresetBank.cpp) and reaches the UI over
// the native bridge. This mirror exists only so the browser dev preview can
// step through the same names; in a host, every value below is replaced by
// what the plugin sends.
const MOCK_PRESETS = [
  { name: 'Lead Vocal Double', blurb: 'the house sound — one more of you, slightly late' },
  { name: 'Subtle Thicken', blurb: 'not a double, just a wider version of the take you have' },
  { name: 'Mono-Safe Wide', blurb: 'width that survives the club, the phone and the mono fold' },
  { name: 'Backing Vocals', blurb: 'loose enough to sound like other people singing along' },
  { name: 'Wide Chorus Stack', blurb: 'the big one — hard left and right, obviously an effect' },
  { name: 'String Section', blurb: "turns two tracked lines into a section that isn't in time" },
  { name: 'Choir', blurb: 'many voices, badly rehearsed, which is what makes it a choir' },
  { name: 'Synth Unison', blurb: 'detuned oscillator stack — machines drift less than people' }
];

function mockPresetState(index) {
  const p = MOCK_PRESETS[index] ?? MOCK_PRESETS[0];
  return { name: p.name, blurb: p.blurb, index, count: MOCK_PRESETS.length, modified: false };
}

let mockPresetIndex = 0;
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
