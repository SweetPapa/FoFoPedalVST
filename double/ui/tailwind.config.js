/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        dbl: {
          bg:  '#14181f',
          ink: '#f2ede4'
        }
      },
      fontFamily: { display: ['"Inter"', 'system-ui', 'sans-serif'] }
    }
  },
  plugins: []
};
