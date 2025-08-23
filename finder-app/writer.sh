#! /bin/bash

set -eu -o pipefail

readonly SCRIPTNAME=$(basename $0)
readonly HELP="${SCRIPTNAME} <FILESDIR> <SEARCH_STR>"

# 1) Accepts the following arguments: the first argument is a full path to a
# file (including filename) on the filesystem, referred to below as writefile;
# the second argument is a text string which will be written within this file,
# referred to below as writestr

# 2) Exits with value 1 error and print statements if any of the arguments
# above were not specified

readonly WRITEFILE="${1?No write file given: ${HELP}}"
readonly WRITESTR="${2?No write string given: ${HELP}}"

# 3) Creates a new file with name and path writefile with content writestr, over
# writing any existing file and creating the path if it doesn’t exist. Exits
# with value 1 and error print statement if the file could not be created.

readonly WRITEDIR="$(dirname "${WRITEFILE}")"
mkdir -p "${WRITEDIR}"
echo "${WRITESTR}" > "${WRITEFILE}"
exit 0
