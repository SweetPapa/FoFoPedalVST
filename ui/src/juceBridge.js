// Thin wrapper around the JUCE 8 WebView frontend bridge.
//
// In production (loaded inside the plugin's WebBrowserComponent), the JUCE
// runtime injects a `window.__JUCE__` global and the `juce-framework-frontend`
// package exposes typed helpers over it. In a browser dev server there is no
// JUCE runtime, so we fall back to a mock store that lets the UI render and
// react to local interactions for layout work.

// Vendored copy of JUCE 8's frontend JS bridge (modules/juce_gui_extra/native/javascript).
// JUCE injects window.__JUCE__ at runtime when withNativeIntegrationEnabled() is set;
// the imported module wraps that with typed slider/toggle/combobox state helpers.
import * as Juce from './juce/index.js';

// check_native_interop.js (imported transitively) installs a stub `window.__JUCE__`
// in dev so the module doesn't throw. The reliable signal that we're inside the
// real plugin is that the host registered our slider name in initialisationData.
function isJuceLive(name) {
  const sliders = window.__JUCE__?.initialisationData?.__juce__sliders;
  return Array.isArray(sliders) && sliders.includes(name);
}

const hasJuceGlobal = typeof window !== 'undefined' && !!window.__JUCE__;

function createMockSliderState(initial = 0.5) {
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

export function getSliderState(name, fallbackInitial = 0.5) {
  if (isJuceLive(name)) {
    return Juce.getSliderState(name);
  }
  return createMockSliderState(fallbackInitial);
}

export const isJuceHost = hasJuceGlobal;
