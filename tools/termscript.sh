#!/usr/bin/env bash

RIOTBASE="$(git rev-parse --show-toplevel)/RIOT/"
PYTERMSESSION="$(date +%Y-%m-%d_%H.%M.%S)"
PYTERMLOGDIR="/tmp/pyterm-$USER"
BAUD="115200"

$RIOTBASE/dist/tools/pyterm/pyterm -p "$1" -b "$BAUD" -ln "$PYTERMLOGDIR" -rn "$PYTERMSESSION" -np
