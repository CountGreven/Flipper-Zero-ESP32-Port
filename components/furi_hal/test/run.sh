#!/usr/bin/env bash
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
cc -I"$here/.." "$here/msc_cdrom_test.c" "$here/../msc_cdrom.c" -o /tmp/msc_cdrom_test
/tmp/msc_cdrom_test
