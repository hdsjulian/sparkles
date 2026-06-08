#!/bin/bash
# Create required directories
set -e

echo "=== [3/5] Creating Folders ==="

BASE=/home/julian/sparkles

mkdir -p "$BASE/songs"
mkdir -p "$BASE/logs"

# Ensure correct ownership
chown -R julian:julian "$BASE/songs"
chown -R julian:julian "$BASE/logs"

echo "Created:"
echo "  $BASE/songs   — drop .mid files here"
echo "  $BASE/logs    — serial.log and other logs"

echo "=== Folders created ==="
