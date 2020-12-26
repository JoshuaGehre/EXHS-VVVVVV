#!/bin/bash

set -e -u

clang-format-10 -style=file -i $(find src/ -type f -regextype posix-egrep -regex ".*\.(h|cpp)$")
