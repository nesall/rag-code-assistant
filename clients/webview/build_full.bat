@echo off
echo Building RAG Code Assistant WebView...

echo Building SPA client...
cd ..\spa-svelte
rm dist -rf
REM call npm ci
call npm run build

echo Building webview...
cd ..\webview
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Releas
cmake --build . --config Release
cd ..
echo Build complete!
