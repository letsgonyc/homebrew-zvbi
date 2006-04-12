#!/bin/sh

. `dirname $0`/simd-emu.sh

if ! find_emulator altivec; then
  exit 77
else
  LD_PRELOAD=$pre $emu ./clear_image altivec
  exit $?
fi
