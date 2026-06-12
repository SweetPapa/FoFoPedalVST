/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        vroom: {
          bg: '#14161b',
          panel: '#22262e',
          edge: '#2c313b',
          ink: '#e6e6e6',
          dim: '#9aa1ab',
          accent: '#ff8a3d',
          accentDim: '#7a431c'
        },
        // Per-vibe accent colors for preset tiles.
        vibe: {
          smooth: '#ffd06b',  // honey
          crunch: '#ff7a3c',  // orange
          lead:   '#ffd040',  // gold
          fuzz:   '#c84cff',  // magenta
          fat:    '#d97a4c',  // brown-orange
          custom: '#6b8cff'   // user
        },
        // Per-knob accent colors so the panel reads like a rainbow.
        knob: {
          input:     '#4ac8f0',  // cool blue
          drive:     '#ff5050',  // red — the headline knob
          character: '#a55aff',  // violet
          body:      '#ffaa3c',  // amber
          tone:      '#4cdef0',  // cyan
          sag:       '#8c5aff',  // purple
          blend:     '#ff5aa3',  // pink
          level:     '#5aff8c'   // green
        }
      },
      fontFamily: {
        display: ['"Inter"', 'system-ui', 'sans-serif']
      },
      keyframes: {
        breathe: {
          '0%, 100%': { opacity: '0.4' },
          '50%':      { opacity: '0.7' }
        }
      },
      animation: {
        breathe: 'breathe 4s ease-in-out infinite'
      }
    }
  },
  plugins: []
};
