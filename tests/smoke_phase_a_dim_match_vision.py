import subprocess
import tempfile
import os
import sys
import json


def resolve_exe_path() -> str:
    env_path = os.environ.get("NEUROFORGE_EXE")
    if env_path and os.path.exists(env_path):
        return env_path
    cwd = os.getcwd()
    candidates = [
        os.path.join(cwd, "Debug", "neuroforge.exe"),
        os.path.join(cwd, "Release", "neuroforge.exe"),
        os.path.join(cwd, "neuroforge.exe"),
    ]
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
    candidates.extend([
        os.path.join(repo_root, "build", "Debug", "neuroforge.exe"),
        os.path.join(repo_root, "build", "Release", "neuroforge.exe"),
        os.path.join(repo_root, "build", "neuroforge.exe"),
    ])
    for p in candidates:
        if os.path.exists(p):
            return p
    raise FileNotFoundError("Could not locate neuroforge executable. Checked: " + "; ".join(candidates))


def main():
    # Teacher file length exactly matches grid^2 (196 for 14x14)
    with tempfile.NamedTemporaryFile(delete=False, suffix=".txt", mode="w") as f:
        f.write(" ".join(["0"] * 196))
        teacher_file = f.name
    try:
        exe = resolve_exe_path()
        cmd = [
            exe,
            "--enable-language",
            "--phase-a=on",
            f"--teacher-embed={teacher_file}",
            "--mirror-mode=vision",
            "--vision-grid=14",
            "--log-json",
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, check=False)
        stdout = result.stdout or ""

        # Parse JSON events from stdout
        decided = None
        conflict = None
        for line in stdout.splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                evt = json.loads(line)
            except Exception:
                continue
            if isinstance(evt, dict):
                if evt.get("t") == "phase_a_embed_conflict":
                    conflict = evt
                elif evt.get("t") == "phase_a_embed_decided":
                    decided = evt

        # Expect no conflict event and final decision set to 196 from teacher vector length
        assert decided is not None, f"Missing phase_a_embed_decided event. stdout:\n{stdout}"
        assert conflict is None, f"Unexpected conflict event emitted: {conflict}"
        assert decided.get("decided_dim") == 196, f"Expected decided_dim=196, got: {decided}"
        assert decided.get("source") == "teacher vector length", f"Expected source 'teacher vector length', got: {decided}"

        print("PhaseA_DimMatch_Vision test passed")
    finally:
        try:
            os.remove(teacher_file)
        except OSError:
            pass


if __name__ == "__main__":
    sys.exit(main())