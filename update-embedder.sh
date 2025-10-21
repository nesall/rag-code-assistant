#!/usr/bin/env bash
# Update backend/embedder-cpp submodule from main branch

set -e
cd "$(dirname "$0")"

echo "Updating submodule backend/embedder-cpp..."

git submodule update --init --recursive backend/embedder-cpp
cd backend/embedder-cpp

git fetch origin
git checkout main
git pull origin main

cd ../..
git add backend/embedder-cpp
git commit -m "Update embedder-cpp submodule" || echo "No changes to commit"
git push

echo "Done."

