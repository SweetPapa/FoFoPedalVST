/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        sway: {
          bg:  '#0f1a18',
          ink: '#e8f4ef'
        }
      },
      fontFamily: { display: ['"Inter"', 'system-ui', 'sans-serif'] }
    }
  },
  plugins: []
};
