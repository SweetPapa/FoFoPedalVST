/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      colors: {
        drip: {
          bg:   '#0c0a0b',
          deck: '#131011',
          ink:  '#efe6e8'
        }
      },
      fontFamily: { display: ['"Inter"', 'system-ui', 'sans-serif'] }
    }
  },
  plugins: []
};
