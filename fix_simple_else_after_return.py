#!/usr/bin/env python3
"""
Simple script to fix basic else-after-return patterns.
Handles the pattern:
    if (...) {
        return X;
    } else {
        return Y;
    }

Converts to:
    if (...) {
        return X;
    }
    return Y;
"""

import re
import sys
from pathlib import Path

def fix_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    original = content

    # Pattern 1: Simple else block with single return
    # Match: "} else {\n    return ...;\n}"
    pattern1 = re.compile(
        r'(\s*)(}\s*else\s*\{\s*\n)'  # } else {
        r'(\s+)(return [^;]+;)\s*\n'   # return statement
        r'(\s*}\s*\n)',                # closing }
        re.MULTILINE
    )

    def replace1(match):
        indent1 = match.group(1)
        stmt_indent = match.group(3)
        return_stmt = match.group(4)

        # Dedent the return statement
        # Remove one level of indentation (4 spaces)
        new_indent = stmt_indent[:-4] if len(stmt_indent) >= 4 else stmt_indent
        return f'{indent1}}}\n{new_indent}{return_stmt}\n'

    content = pattern1.sub(replace1, content)

    # Pattern 2: else if after return
    # Convert "} else if" to "}\n if" when the previous block returns
    pattern2 = re.compile(
        r'(\s*)(}\s*else\s+if\s*\()',
        re.MULTILINE
    )

    # We need to check if the previous block ends with return
    # This is tricky with regex, so let's do it line by line
    lines = content.split('\n')
    modified_lines = []
    i = 0

    while i < len(lines):
        line = lines[i]

        # Check for "} else if" pattern
        if re.match(r'^\s*}\s*else\s+if\s*\(', line):
            # Look back to check if previous block ends with return
            # Search for the last non-empty line before this closing brace
            j = i - 1
            while j >= 0:
                prev_line = lines[j].strip()
                if prev_line and prev_line != '}' and prev_line != '{':
                    break
                j -= 1

            if j >= 0 and ('return' in lines[j] or 'break' in lines[j] or 'continue' in lines[j]):
                # Replace "} else if" with "}\nif"
                indent = line[:len(line) - len(line.lstrip())]
                rest = line.lstrip()[1:]  # Remove the '}'
                rest = rest.lstrip()  # Remove whitespace
                rest = rest[4:]  # Remove 'else'
                rest = rest.lstrip()  # Remove whitespace before 'if'
                modified_lines.append(indent + '}')
                modified_lines.append(indent + rest)
                i += 1
                continue

        modified_lines.append(line)
        i += 1

    content = '\n'.join(modified_lines)

    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python fix_simple_else_after_return.py <file1> <file2> ...")
        sys.exit(1)

    files_modified = 0
    for filepath in sys.argv[1:]:
        path = Path(filepath)
        if path.exists() and path.suffix in ['.c', '.h']:
            if fix_file(filepath):
                print(f"Fixed: {filepath}")
                files_modified += 1

    print(f"\nTotal files modified: {files_modified}")

if __name__ == "__main__":
    main()
