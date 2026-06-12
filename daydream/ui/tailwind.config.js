/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        dream: {
          bg:     '#1a0e2e',  // deep aubergine
          deeper: '#10081f',
          edge:   '#2c1e4d',
          ink:    '#f0e9ff',
          dim:    '#9b8cc1',
          glow:   '#c084ff',
          warm:   '#ff8ad3'
        }
      },
      fontFamily: { display: ['"Inter"', 'system-ui', 'sans-serif'] },
      keyframes: {
        aurora: {
          '0%, 100%': { transform: 'translate(0, 0) scale(1)',    opacity: '0.55' },
          '33%':      { transform: 'translate(8%, -4%) scale(1.05)', opacity: '0.7' },
          '66%':      { transform: 'translate(-6%, 6%) scale(0.96)', opacity: '0.5' }
        },
        drift: {
          '0%, 100%': { transform: 'translate(0, 0)' },
          '50%':      { transform: 'translate(0, -6px)' }
        }
      },
      animation: {
        aurora: 'aurora 16s ease-in-out infinite',
        drift:  'drift 6s ease-in-out infinite'
      }
    }
  },
  plugins: []
};
