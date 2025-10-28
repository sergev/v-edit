#!/bin/bash
#
# Automated refactoring script to replace deprecated Workspace getter/setter methods
# with direct struct member access.
#
# Usage: ./refactor_workspace_getters_setters.sh
#

set -e

echo "Starting Workspace getter/setter refactoring..."
echo "This script will update all .cpp files in the current directory and tests/"
echo ""

# List of files to process
FILES=$(find . -maxdepth 1 -name "*.cpp" -o -path "./tests/*.cpp")

# Backup files first
echo "Creating backups..."
for file in $FILES; do
    cp "$file" "$file.bak"
done

echo "Applying replacements..."

# Process each file
for file in $FILES; do
    echo "Processing: $file"
    
    # Use sed with extended regex (-E) for replacements
    # Note: These are applied in order, so more specific patterns come first
    
    sed -E -i '' \
        -e 's/->nlines\(\)/->file_state.nlines/g' \
        -e 's/->set_nlines\(([^)]+)\)/->file_state.nlines = \1/g' \
        -e 's/->topline\(\)/->view.topline/g' \
        -e 's/->set_topline\(([^)]+)\)/->view.topline = \1/g' \
        -e 's/->basecol\(\)/->view.basecol/g' \
        -e 's/->set_basecol\(([^)]+)\)/->view.basecol = \1/g' \
        -e 's/->line\(\)/->position.line/g' \
        -e 's/->set_line\(([^)]+)\)/->position.line = \1/g' \
        -e 's/->segmline\(\)/->position.segmline/g' \
        -e 's/->set_segmline\(([^)]+)\)/->position.segmline = \1/g' \
        -e 's/->cursorcol\(\)/->view.cursorcol/g' \
        -e 's/->set_cursorcol\(([^)]+)\)/->view.cursorcol = \1/g' \
        -e 's/->cursorrow\(\)/->view.cursorrow/g' \
        -e 's/->set_cursorrow\(([^)]+)\)/->view.cursorrow = \1/g' \
        -e 's/->writable\(\)/->file_state.writable/g' \
        -e 's/->set_writable\(([^)]+)\)/->file_state.writable = \1/g' \
        -e 's/->modified\(\)/->file_state.modified/g' \
        -e 's/->set_modified\(([^)]+)\)/->file_state.modified = \1/g' \
        -e 's/->backup_done\(\)/->file_state.backup_done/g' \
        -e 's/->set_backup_done\(([^)]+)\)/->file_state.backup_done = \1/g' \
        "$file"
done

echo ""
echo "Refactoring complete!"
echo ""
echo "Next steps:"
echo "1. Review the changes: git diff"
echo "2. Build the project: cmake -S . -B build && make -C build"
echo "3. Run tests: ./build/tests/v_edit_tests"
echo "4. If everything works, remove backups: rm *.cpp.bak tests/*.cpp.bak"
echo "5. If there are issues, restore: for f in *.cpp.bak; do mv \"\$f\" \"\${f%.bak}\"; done"
echo ""
