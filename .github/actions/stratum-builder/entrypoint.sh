#!/bin/bash

continue_on_error="$1"
shift
echo "$@" | bash
exit_code=$?
[[ $exit_code -eq 0 ]] && status="success" || status="failure"
echo "::set-output name=exit_code::$exit_code"
echo "::set-output name=status::$status"
[[ "$continue_on_error" == "true" ]] && exit 0 || exit $exit_code
