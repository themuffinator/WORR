# CRLF Line Endings Policy

## Summary
The repository now enforces Windows-style CRLF line endings for all text files
via Git attributes. This aligns working copies on Windows and keeps line ending
behavior consistent across the tree.

## Rationale
- The project targets Windows heavily and is commonly edited with Windows tools.
- A single line-ending policy avoids mixed-endings diffs and tooling confusion.

## Implementation
- `.gitattributes` sets a global rule: `* text=auto eol=crlf`.
- Existing file-specific LF rules were removed to avoid conflicting behavior.

## Notes for existing checkouts
- Git will apply the CRLF policy on checkout or when files are re-normalized.
- If a clean normalization pass is desired later, use:
  - `git add --renormalize .`
  - `git commit -m "Normalize line endings to CRLF"`
