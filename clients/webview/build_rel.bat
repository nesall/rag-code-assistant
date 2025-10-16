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
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
cd ..

echo Copying release artifacts to dist folder...
rm dist -rf
mkdir dist
xcopy build\Release\* dist\ /E /Y
rm dist\output.log -f
rm dist\diagnostics.log -f
xcopy prefs.json dist\

echo Build complete. Package is in dist folder!
