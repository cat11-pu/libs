#!/usr/bin/env bash
# Reproduces every piece of evidence for the palrle.h fix.
# Requires gcc (ASan/UBSan) and python3 + Pillow.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=repro
CC="gcc -std=c11 -O1 -g -fsanitize=address,undefined -Wall -Wextra -Wno-builtin-declaration-mismatch"

echo "== 1. build old vs new implementation objects (symbol-renamed) =="
gcc -std=c11 -c -Wall -Wextra -Wno-builtin-declaration-mismatch -Irepro -I. $OUT/ab/old_impl.c -o $OUT/ab/old_impl.o
gcc -std=c11 -c -Wall -Wextra                                 -Irepro -I. $OUT/ab/new_impl.c -o $OUT/ab/new_impl.o

echo "== 2. strict C11 / C++17 compile of fixed header =="
gcc -std=c11 -Wall -Wextra -Wpedantic -Wshadow -c -x c   palrle.h -DPALRLE_IMPLEMENTATION -o /tmp/palrle_c.o
g++ -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -c -x c++ palrle.h -DPALRLE_IMPLEMENTATION -o /tmp/palrle_cpp.o

echo "== 3. functional + boundary + malformed-input suite (13k+ assertions, ASan/UBSan) =="
$CC -Wno-sign-compare $OUT/suite.c -o $OUT/suite
ASAN_OPTIONS=detect_leaks=0 $OUT/suite

echo "== 4. large randomized roundtrip fuzz (mask + no-mask, ASan/UBSan) =="
$CC $OUT/megafuzz.c -o $OUT/megafuzz
ASAN_OPTIONS=detect_leaks=0 $OUT/megafuzz 200000

echo "== 5. A/B: old vs new (encoder overrun accounting + corrupt-blob handling) =="
gcc -std=c11 -O1 -g -fsanitize=address,undefined -Wall -Wextra -Irepro -I. \
    $OUT/ab/compare.c $OUT/ab/old_impl.o $OUT/ab/new_impl.o -o $OUT/ab/compare
echo "  (old decoder is expected to SEGV on the corrupt blob; new decoder returns an error first)"
ASAN_OPTIONS=detect_leaks=0 $OUT/ab/compare || echo "  -> old path faulted as expected (exit $?)"

echo "== 6. regenerate visual figure =="
$CC $OUT/gen_visual.c -o $OUT/gen_visual
$OUT/gen_visual
python3 $OUT/make_figure.py
echo "done: see $OUT/palrle_repro.png"
