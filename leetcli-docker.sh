#!/bin/bash

# Create host directories if they don't exist
mkdir -p "$HOME/.leetcli"
mkdir -p "$(pwd)/problems"

exec docker run --rm -it \
  -v "$HOME/.leetcli:/workspace/.leetcli" \
  -v "$(pwd)/problems:/workspace/problems" \
  d3kanesa/leetcli "$@"
