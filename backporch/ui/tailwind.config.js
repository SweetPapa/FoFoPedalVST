/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        bkpr: {
          bg:  '#1c1410',
          ink: '#f4ebdf'
        }
      },
      fontFamily: { display: ['"Inter"', 'system-ui', 'sans-serif'] }
    }
  },
  plugins: []
};
