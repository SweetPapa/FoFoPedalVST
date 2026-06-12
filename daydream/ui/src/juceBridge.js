// Vendored JUCE 8 WebView bridge (same as VROOM's). The Juce frontend module
// wraps window.__JUCE__ — in dev (no host) it falls back to a mock store so
// the UI can still render and respond to mouse interactions.
import * as Juce from './juce/index.js';

function isJuceLive(name) {
  const sliders = window.__JUCE__?.initialisationData?.__juce__sliders;
  return Array.isArray(sliders) && sliders.includes(name);
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
