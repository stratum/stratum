#!/bin/bash
set -ex

# Files in this branch that are different from master.
FILES=$(git diff-tree --no-commit-id --name-status -r HEAD origin/master \
          | grep -v "^D" \
          | cut -f 2 \
          | grep -F -e ".cc" -e ".h" || :
      )

# List of files that are already formatted.
read -r -d '\0' KNOWN_FILES << EOF
stratum/tools/gnmi/gnmi_cli.cc
\0
EOF

for file in "${FILES}" "${KNOWN_FILES}"; do
    [ -f "$file" ] || continue  # handle empty match
    clang-format --style=file -i "${file}"
done

# Report which files need to be fixed.
git update-index --refresh
git diff
