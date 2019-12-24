#!/bin/bash

set -e

echo "Adding 'dtoverlay=hifiberry-dac' to config.txt."
echo "dtoverlay=hifiberry-dac" >> "${BINARIES_DIR}/rpi-firmware/config.txt"
