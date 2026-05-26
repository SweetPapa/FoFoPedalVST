/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        vroom: {
          bg: '#181a1f',
          panel: '#22262e',
          edge: '#2c313b',
          ink: '#e6e6e6',
          dim: '#9aa1ab',
          accent: '#ff8a3d',
          accentDim: '#7a431c'
        }
      },
      fontFamily: {
        display: ['"Inter"', 'system-ui', 'sans-serif']
      }
    }
  },
  plugins: []
};
