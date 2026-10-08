/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        terminal: {
          bg: '#080c14',
          panel: '#0d1322',
          card: '#131b2e',
          border: '#1f293d',
          hover: '#1a233a',
          muted: '#64748b',
          text: '#e2e8f0',
        },
        bid: {
          DEFAULT: '#00e676',
          hover: '#00c853',
          dim: 'rgba(0, 230, 118, 0.12)',
          bar: 'rgba(0, 230, 118, 0.22)',
          text: '#00e676',
        },
        ask: {
          DEFAULT: '#ff3b30',
          hover: '#e02820',
          dim: 'rgba(255, 59, 48, 0.12)',
          bar: 'rgba(255, 59, 48, 0.22)',
          text: '#ff3b30',
        },
      },
      fontFamily: {
        mono: ['ui-monospace', 'SFMono-Regular', 'Menlo', 'Monaco', 'Consolas', 'Liberation Mono', 'Courier New', 'monospace'],
      },
      animation: {
        'pulse-fast': 'pulse 1s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'flash-green': 'flashGreen 0.35s ease-out',
        'flash-red': 'flashRed 0.35s ease-out',
      },
      keyframes: {
        flashGreen: {
          '0%': { backgroundColor: 'rgba(0, 230, 118, 0.35)' },
          '100%': { backgroundColor: 'transparent' },
        },
        flashRed: {
          '0%': { backgroundColor: 'rgba(255, 59, 48, 0.35)' },
          '100%': { backgroundColor: 'transparent' },
        },
      },
    },
  },
  plugins: [],
};
