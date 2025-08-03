#!/bin/bash

# Directory where compile_commands.json is
BUILD_DIR="."

# Enable this if you want automatic fixes:
AUTO_FIX=1

# Additional args to pass (e.g., include dirs)
EXTRA_ARGS="-Iinclude"

echo "Running clang-tidy on all .c files..."

find src -name '*.c' | while read -r file; do
    echo ">> Checking $file"
    if [ "$AUTO_FIX" -eq 1 ]; then
        clang-tidy -checks='modernize-use-override' "$file" -p "$BUILD_DIR" -fix-errors -- -std=c11 $EXTRA_ARGS
    else
        clang-tidy "$file" -p "$BUILD_DIR" -- -std=c11 $EXTRA_ARGS
    fi
done

echo "Done."

