"""Allow ``python -m rover`` invocation."""

import sys

from rover.cli import main

if __name__ == "__main__":
    sys.exit(main())
