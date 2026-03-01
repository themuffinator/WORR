# GitHub Projects Roadmap Import

Date: 2026-02-27

## Overview

This document describes how to create the WORR roadmap GitHub Projects v2 board from the canonical CSV import file.

Source CSV: `.tmp/worr-github-project-import-2026-02-27.csv`  
Import script: `tools/project/import_roadmap.py`  
Workflow: `.github/workflows/import-roadmap.yml`

The import creates:
- GitHub **labels** for roadmap categories, epics, and priorities
- GitHub **Issues** for each of the 89 roadmap tasks (16 epics, 2 roadmaps)
- A GitHub **Projects v2** board with custom fields, populated with all task metadata

## Prerequisites

### GitHub Token Scope

The default `GITHUB_TOKEN` in GitHub Actions does **not** have the `project` scope required for Projects v2. You must create a PAT and store it as a repository secret named `PROJECT_TOKEN`.

**Required scopes for the PAT:**

| Scope | Reason |
|-------|--------|
| `repo` (or `public_repo`) | Create issues, read repository |
| `project` | Create and edit GitHub Projects v2 |

1. Go to **Settings → Developer settings → Personal access tokens → Fine-grained tokens** (or classic tokens with `repo` + `project`).
2. Create a token with the scopes above.
3. In the repository, go to **Settings → Secrets and variables → Actions → New repository secret**.
4. Name it `PROJECT_TOKEN` and paste the token value.

## Running the Import

### Via GitHub Actions (recommended)

1. Navigate to **Actions → Import Roadmap to GitHub Projects**.
2. Click **Run workflow**.
3. Optionally adjust the CSV path and project title.
4. Click **Run workflow**.

The workflow uploads a `roadmap-issue-map.json` artifact that maps each Task ID to its GitHub Issue node ID. Keep this artifact if you need to re-run the project-creation step without recreating issues.

### Via Local CLI

Requires: `gh` CLI authenticated with a token that has `project` and `repo` (issues) scopes.

```bash
export GH_TOKEN=<your-pat>
python3 tools/project/import_roadmap.py \
    --csv .tmp/worr-github-project-import-2026-02-27.csv \
    --repo themuffinator/WORR \
    --title "WORR Roadmap 2026"
```

#### Resume after partial run (issues already created)

If issues were already created but the project step failed, pass the saved issue map:

```bash
python3 tools/project/import_roadmap.py \
    --csv .tmp/worr-github-project-import-2026-02-27.csv \
    --repo themuffinator/WORR \
    --title "WORR Roadmap 2026" \
    --skip-issues \
    --issue-map /path/to/issue-map.json
```

## CSV Format

The import CSV at `.tmp/worr-github-project-import-2026-02-27.csv` (mirrored at `docs-dev/proposals/worr-github-project-import-2026-02-27.csv`) contains the following columns:

| Column | Description |
|--------|-------------|
| `Title` | Issue title, includes Task ID prefix (e.g. `[FR-01-T01] …`) |
| `Body` | Issue body with roadmap metadata summary |
| `Status` | Initial status (`Todo`) |
| `Roadmap` | `Feature` or `Development` |
| `Type` | `Feature` or `Development` |
| `Task ID` | Unique task identifier (e.g. `FR-01-T01`) |
| `Epic` | Parent epic ID (e.g. `FR-01`) |
| `Priority` | `P0` – `P3` |
| `Dependencies` | Comma-separated Task IDs this task depends on |
| `Area` | Source file or directory globs |
| `Labels` | Comma-separated GitHub label names |

### Epics

| Epic ID | Roadmap | Description |
|---------|---------|-------------|
| FR-01 | Feature | Native Vulkan Gameplay Parity |
| FR-02 | Feature | Bot AI & Navigation |
| FR-03 | Feature | cgame / HUD Feature Set |
| FR-04 | Feature | Audio Enhancements |
| FR-05 | Feature | Network & Protocol Features |
| FR-06 | Feature | Weapon Wheel & Input |
| FR-07 | Feature | Server-side Game Modes |
| FR-08 | Feature | Accessibility & Localisation |
| DV-01 | Development | CI / Quality Gates |
| DV-02 | Development | Renderer Refactor |
| DV-03 | Development | Documentation Curation |
| DV-04 | Development | Build System Modernisation |
| DV-05 | Development | Technical Debt Reduction |
| DV-06 | Development | Test Coverage |
| DV-07 | Development | Tooling & Automation |
| DV-08 | Development | Protocol / Network Layer |

## Project Fields Created

| Field | Type | Options |
|-------|------|---------|
| Status | Single-select | Todo, In Progress, Done, Blocked |
| Roadmap | Single-select | Feature, Development |
| Type | Single-select | Feature, Development, Bug, Chore |
| Priority | Single-select | P0, P1, P2, P3 |
| Task ID | Text | — |
| Epic | Text | — |
| Dependencies | Text | — |
| Area | Text | — |

## Setting Up the Roadmap View

After the import completes, the Projects v2 board will have a default Table view. To add a Roadmap view:

1. Open the project in GitHub.
2. Click **+ New view** → **Roadmap**.
3. Set the date field to any iteration or date field (or use the built-in GitHub iteration field if you add one).
4. Group by **Epic** or **Roadmap** as needed.

## Related Documents

- `docs-dev/proposals/swot-feature-development-roadmap-2026-02-27.md` — source SWOT and task definitions
- `docs-dev/proposals/worr-github-project-import-2026-02-27.csv` — copy of the import CSV
- `.tmp/worr-github-project-import-2026-02-27.csv` — canonical import CSV
