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
