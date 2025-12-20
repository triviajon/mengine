#!/usr/bin/env python3
"""
Script to automatically remove else blocks that come after return statements.
Fixes clang-tidy readability-else-after-return warnings.
"""

import re
import sys
from pathlib import Path

def fix_else_after_return(filepath):
    """Remove else blocks that come after return/break/continue statements."""
    with open(filepath, 'r') as f:
        lines = f.readlines()

    original_content = ''.join(lines)
    modified_lines = []
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.lstrip()
        indent = line[:len(line) - len(stripped)]

        # Check if this is an else block
        if stripped.startswith('} else {'):
            # Look back to find if the previous block ended with return/break/continue
            # Search backwards for the last non-empty line before the closing brace
            prev_idx = i - 1
            while prev_idx >= 0 and lines[prev_idx].strip() == '':
                prev_idx -= 1

            if prev_idx >= 0:
                prev_stripped = lines[prev_idx].strip()
                # Check if previous block ends with return, break, or continue
                if (prev_stripped.startswith('return') or
                    prev_stripped.startswith('break') or
                    prev_stripped.startswith('continue')):

                    # Replace "} else {" with just "}"
                    modified_lines.append(indent + '}\n')

                    # Now we need to find the matching closing brace for the else block
                    # and dedent all content in between
                    else_content = []
                    i += 1
                    depth = 1

                    while i < len(lines) and depth > 0:
                        current_line = lines[i]
                        current_stripped = current_line.lstrip()
                        current_indent = current_line[:len(current_line) - len(current_stripped)]

                        # Count braces to track depth
                        depth += current_stripped.count('{') - current_stripped.count('}')

                        if depth == 0:
                            # This is the closing brace of the else block, skip it
                            i += 1
                            break
                        else:
                            # Dedent the line by removing 4 spaces (or equivalent)
                            if current_indent.startswith(indent + '    '):
                                dedented = indent + current_indent[len(indent)+4:] + current_stripped + '\n'
                            else:
                                dedented = current_line
                            modified_lines.append(dedented)
                            i += 1
                    continue

        modified_lines.append(line)
        i += 1

    new_content = ''.join(modified_lines)

    # Only write if content changed
    if new_content != original_content:
        with open(filepath, 'w') as f:
            f.write(new_content)
        return True
    return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python fix_else_after_return.py <file1> <file2> ...")
        sys.exit(1)

    files_modified = 0
    for filepath in sys.argv[1:]:
        path = Path(filepath)
        if path.exists() and path.suffix == '.c':
            if fix_else_after_return(filepath):
                print(f"Fixed: {filepath}")
                files_modified += 1
        else:
            print(f"Skipped: {filepath}")

    print(f"\nTotal files modified: {files_modified}")

if __name__ == "__main__":
    main()
