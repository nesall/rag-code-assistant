@echo off
echo Building RAG Code Assistant WebView...

echo Building webview...
cd ..\webview
mkdir build
cd build
cmake ..
cmake --build . --config Release

cd ..

echo Build complete!
