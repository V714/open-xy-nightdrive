"""Keep checked-in generated C++ readable and reproducible."""

from pathlib import Path
import shutil
import subprocess


def write_header(path: Path, source: str) -> None:
    formatter = shutil.which("clang-format")
    if formatter is None:
        raise SystemExit("Install clang-format 23 to regenerate Nightdrive headers.")
    formatted = subprocess.run(
        [formatter, "--assume-filename", str(path)],
        input=source,
        text=True,
        capture_output=True,
        check=True,
    ).stdout
    path.write_text(formatted)
