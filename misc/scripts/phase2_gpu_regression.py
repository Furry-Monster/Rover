"""Run the Phase 2 Vulkan regressions and retain logs, captures and a JSON report."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def cases(output: Path) -> list[tuple[str, list[str]]]:
    scenes = ROOT / "main/builtin_assets/scenes/demos"
    result = [(name, ["--scene", str(scenes / f"{name}.rscene"), "--frames", "8", "--verify-rendering"])
              for name in ("primitives", "hierarchy", "depth", "orthographic", "courtyard")]
    result[0][1].extend(["--capture", str(output / "primitives.ppm")])
    scene = str(scenes / "primitives.rscene")
    result.extend([
        ("frame-graph", ["--demo", "frame-graph", "--frames", "32"]),
        ("triangle", ["--demo", "triangle", "--frames", "8"]),
        ("inflight", ["--scene", scene, "--frames", "120"]),
        ("resize", ["--scene", scene, "--frames", "8", "--verify-rendering", "--resize-test",
                    "--capture", str(output / "resize.ppm")]),
    ])
    for name, options in [("exposure-low", ["--exposure", "0.5"]),
                          ("exposure-high", ["--exposure", "2"]), ("tone-none", ["--tone-map", "none"])]:
        result.append((name, ["--scene", scene, "--frames", "8", "--verify-rendering", *options]))
    return result


def inspect_log(name: str, code: int, log: str) -> list[str]:
    errors = [line for line in log.splitlines() if re.search(r"\[error\]|VUID-|SYNC-HAZARD", line)]
    if code:
        errors.append(f"process exit code: {code}")
    if "validation=true" not in log:
        errors.append("Vulkan validation was not enabled")
    if name == "frame-graph" and "2 versions + pooled texture reuse" not in log:
        errors.append("version/reuse readback verification is missing")
    if name in ("primitives", "hierarchy", "depth", "orthographic", "courtyard", "resize",
                "exposure-low", "exposure-high", "tone-none") and log.count("Scene readback verified:") != 8:
        errors.append("expected 8 verified scene readbacks")
    if name == "resize" and "960x540" not in log:
        errors.append("resized target was not observed")
    if name == "inflight":
        if "Rendered 120 frames" not in log:
            errors.append("in-flight stress run did not complete")
        sizes = re.findall(r"releasing (\d+) textures, (\d+) buffers", log)
        if not sizes or any(int(textures) > 6 or int(buffers) > 2 for textures, buffers in sizes):
            errors.append("in-flight resource pool did not remain within two frame sets")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "bin/debug/rover")
    parser.add_argument("--output-dir", type=Path, default=Path("/tmp/rover-phase2-gpu"))
    args = parser.parse_args()
    executable = args.exe.resolve()
    output = args.output_dir.resolve()
    if not executable.is_file():
        parser.error(f"Executable does not exist: {executable}")
    output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, VK_LAYER_VALIDATE_SYNC="1", VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation")
    report = {"executable": str(executable), "cases": []}
    for name, options in cases(output):
        try:
            run = subprocess.run([str(executable), *options], cwd=ROOT, env=environment,
                                 capture_output=True, text=True, timeout=45)
            code, log = run.returncode, run.stdout + run.stderr
        except subprocess.TimeoutExpired as error:
            code = 124
            log = (error.stdout or b"").decode(errors="replace") + (error.stderr or b"").decode(errors="replace")
        (output / f"{name}.log").write_text(log, encoding="utf-8")
        errors = inspect_log(name, code, log)
        report["cases"].append({"name": name, "exit_code": code, "errors": errors,
                                "passed": not errors, "log": f"{name}.log"})
        print(f"{name}: {'FAIL' if errors else 'PASS'}", flush=True)
        (output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return int(any(not case["passed"] for case in report["cases"]))


if __name__ == "__main__":
    raise SystemExit(main())
