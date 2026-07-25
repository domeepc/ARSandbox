#!/bin/sh
# Launches the packaged sandbox. Thin wrapper so `arsandbox` is on PATH
# without needing users to know the /opt/arsandbox install layout.
export SANDBOX_DIR=/opt/arsandbox
exec /opt/arsandbox/bin/run-sandbox.sh "$@"
