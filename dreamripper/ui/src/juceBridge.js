// Vendored JUCE 8 WebView bridge — the same vendored copy every pedal uses. In
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
  { name: 'Tar Pit', blurb: 'slow, enormous and woolly — the riff that never quite ends' },
  { name: 'Cascade Fuzz', blurb: 'the wall of fuzz: scooped, saturated, and it will not decay' },
  { name: 'Fuzz Lead', blurb: 'mids pushed hard into the gain so single notes sing over the band' },
  { name: 'Bass Ruin', blurb: 'for bass — everything above the fundamental destroyed, the low end kept' },
  { name: 'Flannel', blurb: 'a cranked combo on the edge — back the guitar off and it cleans up' },
  { name: 'Garage Crunch', blurb: 'the rhythm sound of a band in somebody\'s basement' },
  { name: 'Broken Amp', blurb: 'falling apart in the good way: everything sagging, nothing tight' },
  { name: 'Verse Grit', blurb: 'barely dirty, for the quiet half of a loud-quiet-loud song' },
  { name: 'Bay Area', blurb: 'fast, scooped and tight enough that every picked note lands' },
  { name: 'Scooped Chug', blurb: 'palm mutes that hit like a door slamming' },
  { name: 'Lead Cut', blurb: 'mids back in front so the solo sits on top of the wall' },
  { name: 'Frostbite', blurb: 'thin, rasping and cold — tremolo-picked and recorded in a shed' },
  { name: 'Modern Chug', blurb: 'the tight, gated, low-tuned rhythm sound, straight out of the box' },
  { name: 'Drop Tune', blurb: 'for sevens and eights — the lowest string stays a note, not a rumble' },
  { name: 'Machine Gun', blurb: 'gate slammed shut: staccato, silent between the hits' },
  { name: 'Nu Bounce', blurb: 'scooped, bouncy and very late-nineties, with the dry still under it' }
];

function mockPresetState(index) {
  const p = MOCK_PRESETS[index] ?? MOCK_PRESETS[0];
  return { name: p.name, blurb: p.blurb, index, count: MOCK_PRESETS.length, modified: false };
}

let mockPresetIndex = 4;
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
