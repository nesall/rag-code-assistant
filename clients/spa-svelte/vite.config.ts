import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import tailwindcss from "@tailwindcss/vite";

// https://vite.dev/config/
export default defineConfig({
  plugins: [tailwindcss(), svelte()],
  server: {
    proxy: {
      '/api': {
        target: 'http://localhost:8081',
        // rewrite: path => path.replace(/^\/api/, '')
      }
    }
  },
  base: './',
  build: {
    outDir: 'dist',
    assetsInlineLimit: 100000000, // Inline everything
    cssCodeSplit: false,
    rollupOptions: {
      output: {
        inlineDynamicImports: true, // Single JS file
        manualChunks: undefined,
      }
    }
  },
})
