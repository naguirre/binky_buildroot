#!/bin/bash

set -e

echo "Adding 'dtparam=audio=on' to config.txt."
echo "dtparam=audio=on" >> "${BINARIES_DIR}/rpi-firmware/config.txt"
