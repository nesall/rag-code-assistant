
cd backend/embedder-cpp

call build_rel.bat

echo "FINISHED embedder-cpp"

cd ../../clients/webview

call build_rel.bat

cd ../..

rm phenixcode_v1 -rf

mkdir phenixcode_v1 

cp backend/embedder-cpp/dist/* phenixcode_v1 -rf
cp clients/webview/dist/* phenixcode_v1 -rf


echo phenixcode_v1.zip...
powershell -NoProfile -Command "Compress-Archive -Path 'phenixcode_v1\*' -DestinationPath 'phenixcode_v1.zip' -Force"

rm phenixcode_v1/ -rf

