/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        // Warm paper + Moleskine sketch palette. The spec calls for
        // off-white background, muted terracotta/mustard/sage accents,
        // soft amber glow — never RGB-pop.
        paper: {
          DEFAULT: '#f3ecdd', // warm off-white
          deep:    '#ebe2cb',
          ink:     '#2a1f15', // espresso-brown ink, never pure black
          dim:     '#7a6b56',
          line:    '#d8cdb3'
        },
        terracotta: { DEFAULT: '#b56245', soft: '#d49477', deep: '#8c4530' },
        mustard:    { DEFAULT: '#c89a3a', soft: '#e0bd6d', deep: '#9a7325' },
        sage:       { DEFAULT: '#7d9277', soft: '#a8bfa1', deep: '#5b6e55' },
        amber:      { glow: '#e6a85c' }
      },
      fontFamily: {
        // The spec asks for a soft serif on character names; we use a
        // system serif stack (no web-fetch in BinaryData). Recoleta-ish
        // when available, falls back to gracefully on the user's box.
        display: ['"Recoleta"', 'Georgia', '"Iowan Old Style"', 'Cambria', 'serif'],
        mono:    ['"IBM Plex Mono"', 'ui-monospace', 'SFMono-Regular', 'Menlo', 'monospace'],
        body:    ['"Inter"', 'system-ui', '-apple-system', 'sans-serif']
      },
      boxShadow: {
        sketch: '0 1px 0 0 #d8cdb3, 0 2px 4px -2px rgba(42,31,21,0.18)',
        knob:   'inset 0 -2px 4px rgba(42,31,21,0.18), 0 4px 10px -4px rgba(42,31,21,0.25)'
      },
      keyframes: {
        bloom: {
          '0%, 100%': { opacity: '0.25' },
          '50%':      { opacity: '0.55' }
        }
      },
      animation: {
        bloom: 'bloom 2.4s ease-in-out infinite'
      }
    }
  },
  plugins: []
};
