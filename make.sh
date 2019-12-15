#!/bin/bash
gcc "$1" -o "${1/%.c/.bin}" -ggdb -O0 -lpthread -lsqlite3 -std=c99