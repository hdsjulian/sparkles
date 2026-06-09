#!/usr/bin/env python3
"""
Deprecated — use update.sh instead:
    bash ~/sparkles/update.sh
"""
import subprocess, os, sys

print("deploy.py is deprecated. Running update.sh instead...")
script = os.path.join(os.path.dirname(__file__), "update.sh")
result = subprocess.run(["bash", script])
sys.exit(result.returncode)
