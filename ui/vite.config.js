import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';

// Inline everything (JS, CSS, assets) into a single index.html so the JUCE
// resource provider only has to serve one file from BinaryData.
export default defineConfig({
  plugins: [react(), viteSingleFile()],
  build: {
    target: 'es2020',
    cssCodeSplit: false,
    assetsInlineLimit: 100000000,
    chunkSizeWarningLimit: 100000000,
    rollupOptions: {
      output: { inlineDynamicImports: true, manualChunks: undefined }
    },
    emptyOutDir: true
  }
});
