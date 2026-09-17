#!/usr/bin/env sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
  "$project_dir/tests/core_tests.cpp" -o "$test_dir/core_tests"
"$test_dir/core_tests" "$test_dir"
