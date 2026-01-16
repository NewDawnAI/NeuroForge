import subprocess
import os
import time
import sys

def run_test(name, cmd, checks):
    print(f"Running Test: {name}")
    print(f"Command: {' '.join(cmd)}")

    try:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, stderr = process.communicate(timeout=30)

        if process.returncode != 0:
            print(f"FAILED: Process exited with code {process.returncode}")
            print("STDERR:", stderr)
            return False

        print("Process completed successfully.")

        all_passed = True
        for check in checks:
            if not check(stdout, stderr):
                all_passed = False
                print(f"Check failed: {check.__doc__}")

        if all_passed:
            print("PASSED")
            return True
        else:
            print("FAILED")
            return False

    except subprocess.TimeoutExpired:
        process.kill()
        print("FAILED: Timeout")
        return False
    except Exception as e:
        print(f"FAILED: Exception {e}")
        return False

def check_file_exists(filepath):
    def check(stdout, stderr):
        exists = os.path.exists(filepath) and os.path.getsize(filepath) > 0
        if not exists:
            print(f"File {filepath} does not exist or is empty.")
        return exists
    check.__doc__ = f"File {filepath} exists and is not empty"
    return check

def check_output_contains(text):
    def check(stdout, stderr):
        found = text in stdout or text in stderr
        if not found:
            print(f"Output does not contain '{text}'")
        return found
    check.__doc__ = f"Output contains '{text}'"
    return check

def main():
    executable = "./build/neuroforge"
    if not os.path.exists(executable):
        executable = "./build/Release/neuroforge.exe" # Windows fallback?
        if not os.path.exists(executable):
            print("Executable not found.")
            sys.exit(1)

    executable = os.path.abspath(executable)
    print(f"Executable: {executable}")

    # Test M6: Hippocampal Snapshots
    m6_output_csv = "m6_synapses.csv"
    if os.path.exists(m6_output_csv):
        os.remove(m6_output_csv)

    m6_cmd = [
        executable,
        "--steps=1000",
        "--step-ms=1", # fast
        "--hippocampal-snapshots=on",
        "--consolidation-interval-m6=100", # fast consolidation
        f"--snapshot-live={m6_output_csv}",
        "--snapshot-interval=200",
        "--enable-learning" # needed for synapses?
    ]

    m6_passed = run_test(
        "M6 Memory Internalization",
        m6_cmd,
        [
            check_output_contains("Hippocampal snapshots: ENABLED"),
            check_file_exists(m6_output_csv)
        ]
    )

    # Test M7: Autonomous Operation
    m7_cmd = [
        executable,
        "--steps=50",
        "--step-ms=1",
        "--autonomous-mode=on",
        "--substrate-mode=native",
        "--autonomy-metrics=on",
        "--task-generation-interval=100",
        "--enable-learning", # Usually required for autonomy
        "--curiosity-threshold=0.1", # Low threshold to trigger tasks
    ]

    m7_passed = run_test(
        "M7 Autonomous Operation",
        m7_cmd,
        [
            check_output_contains("Autonomous mode: ENABLED"),
            check_output_contains("Autonomous loop started"),
            check_output_contains("substrate task generation enabled")
        ]
    )

    if m6_passed and m7_passed:
        print("\nALL VALIDATION TESTS PASSED")
        sys.exit(0)
    else:
        print("\nSOME VALIDATION TESTS FAILED")
        sys.exit(1)

if __name__ == "__main__":
    main()
