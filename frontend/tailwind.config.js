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
          bg: '#0b0f19',
          panel: '#111827',
          card: '#1f2937',
          border: '#374151',
          hover: '#283548',
          muted: '#9ca3af',
          text: '#f3f4f6',
        },
        bid: {
          DEFAULT: '#16a34a',
          hover: '#15803d',
          dim: 'rgba(22, 163, 74, 0.1)',
          bar: 'rgba(22, 163, 74, 0.18)',
          text: '#22c55e',
        },
        ask: {
          DEFAULT: '#dc2626',
          hover: '#b91c1c',
          dim: 'rgba(220, 38, 38, 0.1)',
          bar: 'rgba(220, 38, 38, 0.18)',
          text: '#ef4444',
        },
      },
      fontFamily: {
        sans: ['Inter', 'system-ui', '-apple-system', 'Segoe UI', 'Roboto', 'sans-serif'],
        mono: ['ui-monospace', 'SFMono-Regular', 'Menlo', 'Monaco', 'Consolas', 'monospace'],
      },
    },
  },
  plugins: [],
};
