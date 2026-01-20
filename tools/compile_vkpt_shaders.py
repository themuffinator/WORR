#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


RT_EXTS = {".rgen", ".rchit", ".rahit", ".rmiss", ".rint"}


def find_glslang(explicit_path):
    if explicit_path:
        path = Path(explicit_path)
        if path.exists():
            return path
        return None

    vulkan_sdk = os.environ.get("VULKAN_SDK")
    if vulkan_sdk:
        exe = "glslangValidator.exe" if os.name == "nt" else "glslangValidator"
        for subdir in ("Bin", "bin"):
            candidate = Path(vulkan_sdk) / subdir / exe
            if candidate.exists():
                return candidate

    found = shutil.which("glslangValidator")
    if found:
        return Path(found)

    return None


def collect_shader_sources(shader_dir):
    exts = {".comp", ".vert", ".frag", ".rgen", ".rchit", ".rahit", ".rmiss", ".rint"}
    sources = []
    for path in sorted(shader_dir.iterdir()):
        if path.suffix in exts:
            sources.append(path)
    return sources


def collect_dependencies(shader_dir, fsr_dir):
    deps = []
    for ext in ("*.glsl", "*.h"):
        deps.extend(shader_dir.glob(ext))
    deps.extend(fsr_dir.glob("*.h"))
    return deps


def newest_mtime(paths):
    latest = 0.0
    for path in paths:
        try:
            ts = path.stat().st_mtime
        except FileNotFoundError:
            continue
        if ts > latest:
            latest = ts
    return latest


def compile_shader(glslang, src, out_path, includes, defines, stage, dep_mtime):
    if out_path.exists() and out_path.stat().st_mtime >= dep_mtime:
        return

    out_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(glslang),
        "--target-env", "vulkan1.2",
        "--quiet",
        "-DVKPT_SHADER",
        "-V",
    ]
    if stage:
        cmd += ["-S", stage]
    for include in includes:
        cmd.append("-I" + str(include))
    cmd += defines
    cmd += [str(src), "-o", str(out_path)]

    subprocess.check_call(cmd)


def main():
    parser = argparse.ArgumentParser(description="Compile VKPT shaders to SPIR-V.")
    parser.add_argument("--out-dir", required=True, help="Output directory for shader_vkpt files.")
    parser.add_argument("--glslang", help="Path to glslangValidator.")
    parser.add_argument("--stamp", help="Optional stamp file to write on success.")
    args = parser.parse_args()

    glslang = find_glslang(args.glslang)
    if not glslang:
        print("vkpt shaders: glslangValidator not found. "
              "Install Vulkan SDK or set VULKAN_SDK.",
              file=sys.stderr)
        return 1

    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent
    shader_dir = repo_root / "src" / "rend_vk" / "vkpt" / "shader"
    fsr_dir = repo_root / "src" / "rend_vk" / "vkpt" / "fsr"

    if not shader_dir.is_dir():
        print(f"vkpt shaders: shader directory not found: {shader_dir}", file=sys.stderr)
        return 1

    includes = [shader_dir, fsr_dir]
    out_dir = Path(args.out_dir)

    sources = collect_shader_sources(shader_dir)
    deps = collect_dependencies(shader_dir, fsr_dir)

    dep_mtime = newest_mtime(sources + deps)

    for src in sources:
        ext = src.suffix
        if ext in RT_EXTS:
            out_pipeline = out_dir / f"{src.name}.pipeline.spv"
            compile_shader(glslang, src, out_pipeline, includes, [], None, dep_mtime)

            if ext == ".rgen":
                out_query = out_dir / f"{src.name}.query.spv"
                compile_shader(glslang, src, out_query, includes, ["-DKHR_RAY_QUERY"], "comp", dep_mtime)
        else:
            out_file = out_dir / f"{src.name}.spv"
            compile_shader(glslang, src, out_file, includes, [], None, dep_mtime)

    if args.stamp:
        stamp_path = Path(args.stamp)
        stamp_path.parent.mkdir(parents=True, exist_ok=True)
        stamp_path.write_text("ok\n", encoding="ascii")

    return 0


if __name__ == "__main__":
    sys.exit(main())
