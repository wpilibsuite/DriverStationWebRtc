import bz2
import shutil
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: DecompressBzip2.py <input.bz2> <output>", file=sys.stderr)
        return 2

    with bz2.open(sys.argv[1], "rb") as source, open(sys.argv[2], "wb") as destination:
        shutil.copyfileobj(source, destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
