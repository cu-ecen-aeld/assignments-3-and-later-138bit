#include <stdio.h>
#include <stdlib.h>	/* exit		*/
#include <sys/stat.h>	/* mkdir	*/
#include <errno.h>
#include <string.h>
#include <limits.h>	/* NAME_MAX	*/
#include <libgen.h>	/* dirname	*/

#include <syslog.h>	/* openlog, syslog, closelog */

#define PROG_NAME "writer"

#define DEBUG

// Debug function to print values.
#ifdef DEBUG
#define dbg(fmt, ...) \
	printf("%s: %s: " fmt " \n", PROG_NAME, __func__, ##__VA_ARGS__)
#else
#define dbg(fmt, ...)
#endif

/* 7) Use the syslog capability to log any unexpected errors with LOG_ERR level.
 */
// Prints error, debug data, and dies
#define die() do {				\
	syslog(LOG_ERR, "%s: %s(%d): %s",	\
		PROG_NAME, __func__,		\
		__LINE__, strerror(errno));	\
	exit(1);				\
} while(0);

/* To find out how `mkdir -p` works, I've reviewed the source code and changed
 * to my own needs. Hence I would like to give credit the following page:
 * https://elixir.bootlin.com/busybox/0.41/source/utility.c#L498
 */
void mkdir_parent(char *filedir, int mode) {
	char buf[NAME_MAX];
	char *pch, *tmp;
	int ret;

	strcpy(buf, filedir);
	pch = strchr(buf, '/');

	// Handle absolute paths that do not yet exist.
	if (buf[0] == '/') {
		pch = strchr(pch+1, '/');
	}

	while (pch != NULL) {
		tmp = pch;

		// Create the next directory
		*tmp = '\0';
		dbg("found at %ld -- %s",pch-buf+1, buf);
		ret = mkdir(buf, mode);
		*tmp = '/';

		// Check for errors.
		if (ret && errno != EEXIST) {
			die();
		}

		pch = strchr(pch+1, '/');
	}
}

int main(int argc, char *argv[]) {
	char *writefile, *writestr, *tmp;
	char writedir[NAME_MAX];
	FILE *fp;

	/* 5) Setup syslog logging for your utility using the LOG_USER facility.
	 */
	openlog(PROG_NAME ".log", LOG_PERROR | LOG_PID, LOG_USER);

	/* 1) Accepts the following arguments: the first argument is a full path
	 * to a file (including filename) on the filesystem, referred to below
	 * as writefile; the second argument is a text string which will be
	 * written within this file, referred to below as writestr
	 */

	/* 2) Exits with value 1 error and print statements if any of the
	 * arguments above were not specified
	 */
	if (argc < 3) {
		errno = EINVAL;
		die();
	}

	writefile = argv[1];
	writestr = argv[2];

	/* 3) Creates a new file with name and path writefile with content
	 * writestr, over writing any existing file and creating the path if it
	 * doesn’t exist. Exits with value 1 and error print statement if the
	 * file could not be created.
	 */
	tmp = strdup(writefile);
	sprintf(writedir, "%s/", dirname(tmp));
	free(tmp);

	dbg("Creating directory %s", writedir);
	mkdir_parent(writedir, 0775);

	dbg("Opening %s", writefile);
	fp = fopen(writefile, "w");
	if (! fp) {
		die();
	}

	/* 6) Use the syslog capability to write a message "Writing <string> to
	 * <file>” where <string> is the text string written to file (second
	 * argument) and <file> is the file created by the script. This should
	 * be written with LOG_DEBUG level.
	 */
	syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	fputs(writestr, fp);

	// Clean up
	fclose(fp);
	closelog();
	return 0;
}
