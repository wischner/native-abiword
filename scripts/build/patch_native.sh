#!/bin/sh
# Apply the reviewed compatibility patch atomically, never overwrite conflicts.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
native_root=$1
patch="$root/scripts/build/native-dialogs.patch"
# Native integrated these fixes with its own implementation and documentation.
# The old patch cannot apply (or reverse) there; leave upstream sources intact.
upstream_fix=75cdcd488ac5baa2ab6bc426f4f9d498a11a2909
if git -C "$native_root" merge-base --is-ancestor "$upstream_fix" HEAD 2>/dev/null; then
    exit 0
fi
if git -C "$native_root" apply --reverse --check "$patch" 2>/dev/null; then
    exit 0
fi
if ! git -C "$native_root" apply --check "$patch"; then
    echo 'Native dialog compatibility patch conflicts; review the dependency before building.' >&2
    exit 1
fi
git -C "$native_root" apply "$patch"
