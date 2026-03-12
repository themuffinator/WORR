#!/usr/bin/env python3
"""tools/project/import_roadmap.py

Import a WORR roadmap CSV into a GitHub Projects v2 board.

Creates:
  - GitHub labels for roadmap, type, epic, and priority groupings
  - GitHub Issues for each task row
  - A GitHub Projects v2 board with custom fields
  - Project items linked to the issues, with all field values populated

Usage (requires gh CLI authenticated with 'project' scope):
    python3 tools/project/import_roadmap.py \\
        --csv .tmp/worr-github-project-import-2026-02-27.csv \\
        --repo themuffinator/WORR \\
        --title "WORR Roadmap 2026"

Environment:
    GH_TOKEN  GitHub token with 'project' and 'repo' (issues:write) scopes.
              The default GITHUB_TOKEN in Actions does NOT have 'project' scope;
              pass a PAT via secrets.PROJECT_TOKEN instead.
"""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

# ---------------------------------------------------------------------------
# Label definitions
# ---------------------------------------------------------------------------

# (name, hex-colour-without-#, description)
LABEL_DEFS: list[tuple[str, str, str]] = [
    ("roadmap",      "0075ca", "Roadmap-tracked task"),
    ("feature",      "0e8a16", "Feature roadmap task"),
    ("development",  "e4e669", "Development roadmap task"),
    ("priority:P0",  "b60205", "Critical priority"),
    ("priority:P1",  "d93f0b", "High priority"),
    ("priority:P2",  "fbca04", "Medium priority"),
    ("priority:P3",  "c5def5", "Low priority"),
    # Epic labels are generated dynamically from the CSV, see ensure_labels()
]

EPIC_COLOURS = {
    "FR": "1d76db",  # blue  – Feature epics
    "DV": "e99695",  # pink  – Development epics
}
EPIC_COLOUR_FALLBACK = "cccccc"

# ---------------------------------------------------------------------------
# gh CLI helpers
# ---------------------------------------------------------------------------


def _gh(*args: str, stdin: str | None = None, check: bool = True) -> subprocess.CompletedProcess[str]:
    cmd = ["gh", *args]
    result = subprocess.run(cmd, capture_output=True, text=True, input=stdin)
    if check and result.returncode != 0:
        print(f"ERROR running: {' '.join(cmd)}", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)
    return result


def gh_json(*args: str, stdin: str | None = None) -> Any:
    r = _gh(*args, stdin=stdin)
    return json.loads(r.stdout) if r.stdout.strip() else None


def graphql(query: str, variables: dict[str, Any] | None = None) -> dict[str, Any]:
    """Execute a GraphQL query/mutation via ``gh api graphql``."""
    payload: dict[str, Any] = {"query": query}
    if variables:
        payload["variables"] = variables
    r = _gh("api", "graphql", "--input", "-", stdin=json.dumps(payload))
    data = json.loads(r.stdout)
    if "errors" in data:
        print(f"GraphQL errors: {json.dumps(data['errors'], indent=2)}", file=sys.stderr)
        sys.exit(1)
    return data.get("data", {})


# ---------------------------------------------------------------------------
# Owner ID resolution
# ---------------------------------------------------------------------------


def get_owner_id(owner: str) -> str:
    """Return the GraphQL node ID for an organisation or user login."""
    data = graphql(
        """
        query($login: String!) {
          organization(login: $login) { id }
        }
        """,
        {"login": owner},
    )
    org_id = (data.get("organization") or {}).get("id")
    if org_id:
        return org_id
    # fall back to user
    data = graphql(
        """
        query($login: String!) {
          user(login: $login) { id }
        }
        """,
        {"login": owner},
    )
    user_id = (data.get("user") or {}).get("id")
    if user_id:
        return user_id
    print(f"ERROR: could not resolve owner ID for '{owner}'", file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Label management
# ---------------------------------------------------------------------------


def ensure_labels(repo: str, rows: list[dict[str, str]]) -> None:
    """Create all required labels if they do not already exist."""
    # Collect epic labels from CSV
    epic_labels: list[tuple[str, str, str]] = []
    seen_epics: set[str] = set()
    for row in rows:
        epic = row["Epic"].strip()
        label_name = f"epic:{epic}"
        if label_name not in seen_epics:
            seen_epics.add(label_name)
            prefix = epic[:2]
            colour = EPIC_COLOURS.get(prefix, EPIC_COLOUR_FALLBACK)
            epic_labels.append((label_name, colour, f"Epic {epic}"))

    all_labels = LABEL_DEFS + epic_labels

    # Fetch existing labels once
    existing_raw = _gh("label", "list", "--repo", repo, "--json", "name", "--limit", "200")
    existing = {item["name"] for item in json.loads(existing_raw.stdout)}

    for name, colour, description in all_labels:
        if name in existing:
            continue
        print(f"  Creating label: {name}")
        _gh(
            "label", "create", name,
            "--repo", repo,
            "--color", colour,
            "--description", description,
        )


# ---------------------------------------------------------------------------
# Issue creation
# ---------------------------------------------------------------------------


def create_issues(repo: str, rows: list[dict[str, str]]) -> dict[str, str]:
    """Create GitHub Issues for every row; return mapping Task ID → issue node ID."""
    task_to_node_id: dict[str, str] = {}

    owner, repo_name = repo.split("/", 1)

    for row in rows:
        task_id = row["Task ID"].strip()
        title = row["Title"].strip()
        body = row["Body"].strip()
        labels = [lbl.strip() for lbl in row["Labels"].split(",") if lbl.strip()]

        print(f"  Creating issue: {task_id} – {title[:60]}")

        label_args: list[str] = []
        for lbl in labels:
            label_args += ["--label", lbl]

        result = _gh(
            "issue", "create",
            "--repo", repo,
            "--title", title,
            "--body", body,
            *label_args,
        )
        issue_url = result.stdout.strip()

        # Resolve node ID from URL (e.g. https://github.com/owner/repo/issues/42)
        issue_number = int(issue_url.rstrip("/").rsplit("/", 1)[-1])
        node_data = graphql(
            """
            query($owner: String!, $repo: String!, $number: Int!) {
              repository(owner: $owner, name: $repo) {
                issue(number: $number) { id }
              }
            }
            """,
            {"owner": owner, "repo": repo_name, "number": issue_number},
        )
        node_id = node_data["repository"]["issue"]["id"]
        task_to_node_id[task_id] = node_id

        # Throttle to stay well below secondary-rate limits
        time.sleep(0.5)

    return task_to_node_id


# ---------------------------------------------------------------------------
# Project creation and field setup
# ---------------------------------------------------------------------------


def create_project(owner_id: str, title: str) -> dict[str, Any]:
    """Create a Projects v2 board and return {id, number, url}."""
    data = graphql(
        """
        mutation($ownerId: ID!, $title: String!) {
          createProjectV2(input: { ownerId: $ownerId, title: $title }) {
            projectV2 { id number url }
          }
        }
        """,
        {"ownerId": owner_id, "title": title},
    )
    return data["createProjectV2"]["projectV2"]


def create_single_select_field(
    project_id: str,
    name: str,
    options: list[str],
) -> dict[str, Any]:
    """Create a SINGLE_SELECT field and return {id, options:[{id,name}]}."""
    data = graphql(
        """
        mutation($projectId: ID!, $name: String!, $opts: [ProjectV2SingleSelectFieldOptionInput!]!) {
          createProjectV2Field(input: {
            projectId: $projectId
            dataType: SINGLE_SELECT
            name: $name
            singleSelectOptions: $opts
          }) {
            projectV2Field {
              ... on ProjectV2SingleSelectField {
                id
                options { id name }
              }
            }
          }
        }
        """,
        {
            "projectId": project_id,
            "name": name,
            "opts": [{"name": o, "color": "GRAY", "description": ""} for o in options],
        },
    )
    return data["createProjectV2Field"]["projectV2Field"]


def create_text_field(project_id: str, name: str) -> str:
    """Create a TEXT field and return its node ID."""
    data = graphql(
        """
        mutation($projectId: ID!, $name: String!) {
          createProjectV2Field(input: {
            projectId: $projectId
            dataType: TEXT
            name: $name
          }) {
            projectV2Field {
              ... on ProjectV2Field { id }
            }
          }
        }
        """,
        {"projectId": project_id, "name": name},
    )
    return data["createProjectV2Field"]["projectV2Field"]["id"]


def setup_project_fields(project_id: str) -> dict[str, Any]:
    """Create all custom fields; return a structured field map."""
    print("  Creating field: Status")
    status_field = create_single_select_field(
        project_id, "Status", ["Todo", "In Progress", "Done", "Blocked"]
    )
    print("  Creating field: Roadmap")
    roadmap_field = create_single_select_field(
        project_id, "Roadmap", ["Feature", "Development"]
    )
    print("  Creating field: Type")
    type_field = create_single_select_field(
        project_id, "Type", ["Feature", "Development", "Bug", "Chore"]
    )
    print("  Creating field: Priority")
    priority_field = create_single_select_field(
        project_id, "Priority", ["P0", "P1", "P2", "P3"]
    )
    print("  Creating field: Task ID")
    task_id_field_id = create_text_field(project_id, "Task ID")
    print("  Creating field: Epic")
    epic_field_id = create_text_field(project_id, "Epic")
    print("  Creating field: Dependencies")
    deps_field_id = create_text_field(project_id, "Dependencies")
    print("  Creating field: Area")
    area_field_id = create_text_field(project_id, "Area")

    def _opts(field: dict[str, Any]) -> dict[str, str]:
        return {o["name"]: o["id"] for o in field.get("options", [])}

    return {
        "status": {"id": status_field["id"], "options": _opts(status_field)},
        "roadmap": {"id": roadmap_field["id"], "options": _opts(roadmap_field)},
        "type": {"id": type_field["id"], "options": _opts(type_field)},
        "priority": {"id": priority_field["id"], "options": _opts(priority_field)},
        "task_id": task_id_field_id,
        "epic": epic_field_id,
        "dependencies": deps_field_id,
        "area": area_field_id,
    }


# ---------------------------------------------------------------------------
# Adding items and setting field values
# ---------------------------------------------------------------------------


def add_item_to_project(project_id: str, content_id: str) -> str:
    """Add an issue to the project; return the project item node ID."""
    data = graphql(
        """
        mutation($projectId: ID!, $contentId: ID!) {
          addProjectV2ItemById(input: { projectId: $projectId, contentId: $contentId }) {
            item { id }
          }
        }
        """,
        {"projectId": project_id, "contentId": content_id},
    )
    return data["addProjectV2ItemById"]["item"]["id"]


def set_text_field(project_id: str, item_id: str, field_id: str, value: str) -> None:
    graphql(
        """
        mutation($projectId: ID!, $itemId: ID!, $fieldId: ID!, $value: String!) {
          updateProjectV2ItemFieldValue(input: {
            projectId: $projectId
            itemId: $itemId
            fieldId: $fieldId
            value: { text: $value }
          }) { projectV2Item { id } }
        }
        """,
        {
            "projectId": project_id,
            "itemId": item_id,
            "fieldId": field_id,
            "value": value,
        },
    )


def set_single_select_field(
    project_id: str, item_id: str, field_id: str, option_id: str
) -> None:
    graphql(
        """
        mutation($projectId: ID!, $itemId: ID!, $fieldId: ID!, $optionId: String!) {
          updateProjectV2ItemFieldValue(input: {
            projectId: $projectId
            itemId: $itemId
            fieldId: $fieldId
            value: { singleSelectOptionId: $optionId }
          }) { projectV2Item { id } }
        }
        """,
        {
            "projectId": project_id,
            "itemId": item_id,
            "fieldId": field_id,
            "optionId": option_id,
        },
    )


def populate_item(
    project_id: str,
    item_id: str,
    row: dict[str, str],
    fields: dict[str, Any],
) -> None:
    """Set all custom field values for a single project item."""

    # Status
    status_val = row["Status"].strip()
    status_opt = fields["status"]["options"].get(status_val)
    if status_opt:
        set_single_select_field(project_id, item_id, fields["status"]["id"], status_opt)

    # Roadmap
    roadmap_val = row["Roadmap"].strip()
    roadmap_opt = fields["roadmap"]["options"].get(roadmap_val)
    if roadmap_opt:
        set_single_select_field(project_id, item_id, fields["roadmap"]["id"], roadmap_opt)

    # Type
    type_val = row["Type"].strip()
    type_opt = fields["type"]["options"].get(type_val)
    if type_opt:
        set_single_select_field(project_id, item_id, fields["type"]["id"], type_opt)

    # Priority
    priority_val = row["Priority"].strip()
    priority_opt = fields["priority"]["options"].get(priority_val)
    if priority_opt:
        set_single_select_field(project_id, item_id, fields["priority"]["id"], priority_opt)

    # Text fields
    if row["Task ID"].strip():
        set_text_field(project_id, item_id, fields["task_id"], row["Task ID"].strip())
    if row["Epic"].strip():
        set_text_field(project_id, item_id, fields["epic"], row["Epic"].strip())
    if row["Dependencies"].strip():
        set_text_field(project_id, item_id, fields["dependencies"], row["Dependencies"].strip())
    if row["Area"].strip():
        set_text_field(project_id, item_id, fields["area"], row["Area"].strip())


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--csv", required=True, help="Path to the roadmap import CSV")
    parser.add_argument("--repo", required=True, help="GitHub repo in owner/name format")
    parser.add_argument("--title", default="WORR Roadmap 2026", help="Project board title")
    parser.add_argument("--skip-issues", action="store_true", help="Skip issue creation (use with --issue-map)")
    parser.add_argument(
        "--issue-map",
        metavar="FILE",
        help="JSON file mapping Task ID → issue node ID (produced by a prior run with --dump-issue-map)",
    )
    parser.add_argument(
        "--dump-issue-map",
        metavar="FILE",
        help="Write Task ID → issue node ID map to FILE after creating issues",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    csv_path = Path(args.csv)
    if not csv_path.is_file():
        print(f"ERROR: CSV not found: {csv_path}", file=sys.stderr)
        sys.exit(1)

    owner, _ = args.repo.split("/", 1)

    with csv_path.open(newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))

    print(f"Loaded {len(rows)} tasks from {csv_path}")

    # ---- Labels ----
    print("\n[1/5] Ensuring labels exist…")
    ensure_labels(args.repo, rows)

    # ---- Issues ----
    if args.skip_issues and args.issue_map:
        print("\n[2/5] Loading issue map from file…")
        with open(args.issue_map, encoding="utf-8") as fh:
            task_to_node_id: dict[str, str] = json.load(fh)
    else:
        print("\n[2/5] Creating GitHub Issues…")
        task_to_node_id = create_issues(args.repo, rows)
        if args.dump_issue_map:
            with open(args.dump_issue_map, "w", encoding="utf-8") as fh:
                json.dump(task_to_node_id, fh, indent=2)
            print(f"  Issue map written to {args.dump_issue_map}")

    # ---- Project ----
    print("\n[3/5] Resolving owner ID and creating project…")
    owner_id = get_owner_id(owner)
    project = create_project(owner_id, args.title)
    project_id = project["id"]
    print(f"  Project created: {project['url']} (#{project['number']})")

    # ---- Fields ----
    print("\n[4/5] Setting up project fields…")
    fields = setup_project_fields(project_id)

    # ---- Items ----
    print("\n[5/5] Adding issues to project and populating fields…")
    for row in rows:
        task_id = row["Task ID"].strip()
        node_id = task_to_node_id.get(task_id)
        if not node_id:
            print(f"  WARNING: no issue node ID for {task_id}, skipping", file=sys.stderr)
            continue
        print(f"  Adding {task_id}…")
        item_id = add_item_to_project(project_id, node_id)
        populate_item(project_id, item_id, row, fields)
        time.sleep(0.3)

    print(f"\nDone! Project board: {project['url']}")


if __name__ == "__main__":
    main()
