#!/bin/sh

while read mangled expected_demangled
do
  if echo "${mangled}" | grep -q -E '^#|^$'; then
    continue
  fi
  actual_damangled="$(./d-demangle "${mangled}")"
  if [ "${actual_damangled}" = "${expected_demangled}" ]; then
    echo "GOOD ${mangled}" "${actual_damangled}"
  else
    echo "BAD  ${mangled}" "${actual_damangled}"
    echo "EXP  ${mangled}" "${expected_demangled}"
  fi
done < tests.txt
