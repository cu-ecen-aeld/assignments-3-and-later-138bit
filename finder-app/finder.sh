#! /bin/sh

# To test use following commands:
# $ mkdir -p a/b/c
# $ echo "derp" > a/b.txt
# $ echo -e "derp\nblaat" > a/b/c/c.txt
# $ ./finder.sh a derp ; ./finder.sh a blaat

set -eu #-o pipefail

readonly SCRIPTNAME=$(basename $0)
readonly HELP="${SCRIPTNAME} <FILESDIR> <SEARCH_STR>"

# 1) Accepts the following runtime arguments: the first argument is a path to a 
# directory on the filesystem, referred to below as filesdir; the second
# argument is a text string which will be searched within these files, referred # to below as searchstr

# 2) Exits with return value 1 error and print statements if any of the
# parameters above were not specified

readonly FILESDIR="${1?No file directory given: ${HELP}}"
readonly SEARCHSTR="${2?No search string given: ${HELP}}"

die() {
	local _msg="${1:-Unregistered error. Exiting..}"
	echo "${SCRIPTNAME}: ${_msg}"
	exit 1
}

# 3) Exits with return value 1 error and print statements if filesdir does not
# represent a directory on the filesystem
[ -d "${FILESDIR}" ] || die "${FILESDIR} not a directory."

# $) Prints a message "The number of files are X and the number of matching
# lines are Y" where X is the number of files in the directory and all
# subdirectories and Y is the number of matching lines found in respective
# files, where a matching line refers to a line which contains searchstr (and
# may also contain # additional content).

# Used to use `wc -l`, but due to assignment can't use that no more.. Ugly hack around it.
cnt () {
	echo $#
}

readonly files_total="$(cnt $(find ${FILESDIR} -type f))"
readonly results_total="$(cnt $(grep -R "${SEARCHSTR}" "${FILESDIR}"))"

# Long text does not fit in 80 characters -_-'
echo "The number of files are ${files_total} and the number of matching lines are ${results_total}"
exit 0
