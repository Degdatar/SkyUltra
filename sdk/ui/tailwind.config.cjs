module.exports = {
  content: ["./src/**/*.{ts,tsx}"],
  corePlugins: {
    preflight: false,
  },
  theme: {
    extend: {
      colors: {
        skyultra: {
          background: "var(--sky-bg)",
          surface: "var(--sky-surface)",
          raised: "var(--sky-raised)",
          border: "var(--sky-border)",
          text: "var(--sky-text)",
          muted: "var(--sky-muted)",
          accent: "var(--sky-accent)",
          gold: "var(--sky-gold)",
          danger: "var(--sky-danger)",
          success: "var(--sky-success)",
        },
      },
      borderRadius: {
        sky: "var(--sky-radius)",
      },
      boxShadow: {
        sky: "var(--sky-shadow)",
      },
    },
  },
  plugins: [],
};
