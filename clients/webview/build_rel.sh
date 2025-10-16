#!/bin/bash
echo "Building RAG Code Assistant WebView..."

# Build the SPA client first
echo "Building SPA client..."
cd ../spa-svelte
npm ci
npm run build

# Build the webview
echo "Building webview..."
cd ../webview
mkdir -p build
cd build
cmake ..
make -j$(nproc)

echo "Copying release artifacts to dist folder..."
rm -rf dist
mkdir -p dist
cp -r build_rel/* dist/
rm -f dist/output.log
rm -f dist/diagnostics.log
cp settings.template.json dist/
cp bge_tokenizer.json dist/

echo "Build complete. Package is in dist folder!"

