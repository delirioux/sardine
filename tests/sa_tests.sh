#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Sardine's host tests: the generated files up to date (the splash, the FM6 patches), the layer engine strict about
# warnings and single precision and its own tests (tests/sa_layer_test.c), the screen and its layout lint
# (tests/sa_ui_test.sh).
#   sh tests/sa_tests.sh        (exit 0: all passed)
set -eu
mkdir -p build/host
python3 tools/sa_logo.py --check
python3 tools/sa_fm6.py --check
cc -std=c11 -O2 -Wall -Wextra -Wshadow -Wdouble-promotion -Werror -Ifirmware/src -c firmware/src/sa_layer.c \
   -o build/host/sa_layer.o
cc -std=c11 -O2 -Wall -Wextra -Wshadow -Wdouble-promotion -Werror -DTUNA_FREESTANDING=1 -ffreestanding -Ifirmware/src \
   -c firmware/src/sa_layer.c -o build/host/sa_layer_fs.o
cc -std=c11 -O2 -Wall -Ifirmware/src -o build/host/sa_layer_test tests/sa_layer_test.c firmware/src/sa_layer.c -lm
build/host/sa_layer_test
sh tests/sa_ui_test.sh build/sa_ui
echo "sa_tests: all passed"
