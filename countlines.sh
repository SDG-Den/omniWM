#!/usr/bin/env bash

files=$(find "$(pwd)/devnotes" -type f -name "*.md")
counter=0

for file in $files; do
    lines=$(wc -l $file | cut -d' ' -f1)
    filepretty=$(echo "$file" | sed "s|$(pwd)||g")
    echo "$filepretty = $lines lines of markdown"
    counter=$(( $counter + $lines ))
done
echo ""
echo "total lines: $counter"
