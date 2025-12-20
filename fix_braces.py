#!/usr/bin/env python3
"""
Script to automatically add braces around single-statement control flow blocks.
Fixes clang-tidy readability-braces-around-statements warnings.
"""

import re
import sys
from pathlib import Path

def find_matching_paren(s, start_pos):
    """Find the position of the matching closing parenthesis."""
    depth = 0
    for i in range(start_pos, len(s)):
        if s[i] == '(':
            depth += 1
        elif s[i] == ')':
            depth -= 1
            if depth == 0:
                return i
    return -1

def fix_braces_in_file(filepath):
    """Add braces around single-statement if/else/while/for blocks."""
    with open(filepath, 'r') as f:
        content = f.read()

    original_content = content
    lines = content.split('\n')
    modified_lines = []
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.lstrip()
        indent = line[:len(line) - len(stripped)]

        # Pattern 1: if/while/for (condition) statement;
        keyword_match = re.match(r'^(if|while|for)\s*\(', stripped)
        elif_match = re.match(r'^else\s+if\s*\(', stripped)
        else_match = re.match(r'^else\s+([^{].*?;)\s*$', stripped)

        if keyword_match:
            keyword = keyword_match.group(1)
            paren_start = stripped.index('(')
            paren_end = find_matching_paren(stripped, paren_start)

            if paren_end != -1:
                condition = stripped[paren_start+1:paren_end]
                after_paren = stripped[paren_end+1:].lstrip()

                # Check if there's a statement on the same line (not starting with {)
                if after_paren and not after_paren.startswith('{') and not after_paren.startswith('//') and after_paren.endswith(';'):
                    modified_lines.append(indent + f'{keyword} ({condition}) {{')
                    modified_lines.append(indent + '    ' + after_paren)
                    modified_lines.append(indent + '}')
                    i += 1
                else:
                    modified_lines.append(line)
                    i += 1
            else:
                modified_lines.append(line)
                i += 1

        elif elif_match:
            paren_start = stripped.index('(')
            paren_end = find_matching_paren(stripped, paren_start)

            if paren_end != -1:
                condition = stripped[paren_start+1:paren_end]
                after_paren = stripped[paren_end+1:].lstrip()

                # Check if there's a statement on the same line
                if after_paren and not after_paren.startswith('{') and not after_paren.startswith('//') and after_paren.endswith(';'):
                    modified_lines.append(indent + f'else if ({condition}) {{')
                    modified_lines.append(indent + '    ' + after_paren)
                    modified_lines.append(indent + '}')
                    i += 1
                else:
                    modified_lines.append(line)
                    i += 1
            else:
                modified_lines.append(line)
                i += 1

        elif else_match:
            statement = else_match.group(1).rstrip()
            modified_lines.append(indent + 'else {')
            modified_lines.append(indent + '    ' + statement)
            modified_lines.append(indent + '}')
            i += 1

        else:
            modified_lines.append(line)
            i += 1

    new_content = '\n'.join(modified_lines)

    # Only write if content changed
    if new_content != original_content:
        with open(filepath, 'w') as f:
            f.write(new_content)
        return True
    return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python fix_braces.py <file1> <file2> ...")
        sys.exit(1)

    files_modified = 0
    for filepath in sys.argv[1:]:
        path = Path(filepath)
        if path.exists() and path.suffix == '.c':
            if fix_braces_in_file(filepath):
                print(f"Fixed: {filepath}")
                files_modified += 1
        else:
            print(f"Skipped: {filepath}")

    print(f"\nTotal files modified: {files_modified}")

if __name__ == "__main__":
    main()
