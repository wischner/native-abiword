#!/bin/sh
# Isolate settings and exercise persisted UI preferences in fresh processes.
set -eu
test_config=$(mktemp -d "${TMPDIR:-/tmp}/fyodor-ui.XXXXXXXX")
trap 'rm -rf -- "$test_config"' EXIT HUP INT TERM
export FYODOR_TEST_CONFIG="$test_config"
"$@" --preferences-write
"$@" --preferences-read
"$@"
