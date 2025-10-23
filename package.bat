@echo off
setlocal

set NAME=phenixcode_v1
set EMBEDDER=backend/embedder-cpp
set WEBVIEW=clients/webview

cd %EMBEDDER%
call build_rel.bat
echo FINISHED %EMBEDDER%

cd ../../%WEBVIEW%
call build_rel.bat
cd ../..

rm -rf %NAME%
mkdir %NAME%

cp %EMBEDDER%/dist/* %NAME% -rf
cp %WEBVIEW%/dist/* %NAME% -rf

echo %NAME%.zip...
powershell -NoProfile -Command "Compress-Archive -Path '%NAME%' -DestinationPath '%NAME%.zip' -Force"

rm -rf %NAME%/
echo Package '%NAME%' ready.

endlocal
