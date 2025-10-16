@echo off
echo Building RAG Code Assistant WebView...

echo Building webview...
cd ..\webview
mkdir build_rel
cd build_rel
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

cd ..

echo Build complete!
