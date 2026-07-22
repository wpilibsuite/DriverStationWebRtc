#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <directory>" >&2
  exit 2
fi

if [[ -z "${APPLE_DEVELOPER_ID:-}" ]]; then
  echo "APPLE_DEVELOPER_ID must name the imported Developer ID certificate." >&2
  exit 2
fi

directory="$1"
files_to_sign=()
while IFS= read -r -d '' file; do
  files_to_sign+=("$file")
done < <(find "$directory" -maxdepth 1 -type f -name '*.dylib' -print0)

if [[ ${#files_to_sign[@]} -eq 0 ]]; then
  echo "No dylibs were found in $directory." >&2
  exit 1
fi

for file in "${files_to_sign[@]}"; do
  codesign --force --timestamp --options=runtime --sign "$APPLE_DEVELOPER_ID" "$file"
  codesign --verify --strict --verbose=2 "$file"
done
