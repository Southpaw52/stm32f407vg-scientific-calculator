from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="calculator-tests-") as tmp:
    exe = Path(tmp) / "engine_tests.exe"
    subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-ICore/Inc", "Tests/engine_tests.c", "Core/Src/calculator_engine.c", "-lm", "-o", str(exe)], cwd=root, check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
