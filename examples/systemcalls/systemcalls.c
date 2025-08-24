#include "systemcalls.h"

#include <stdio.h>
#include <stdlib.h>	/* system()	*/
#include <sys/types.h>	/* pid_t	*/
#include <unistd.h>	/* fork, execv, exit, 	*/
#include <sys/wait.h>	/* waitpid	*/
#include <fcntl.h>	/* open, close	*/

#define PROG_NAME "assignment3"
#define DEBUG

// Debug function to print values.
#ifdef DEBUG
#define dbg(fmt, ...) \
	printf("%s: %s: " fmt " \n", PROG_NAME, __func__, ##__VA_ARGS__)
#else
#define dbg(fmt, ...)
#endif


/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
	bool ret = false;
	int status = 0;

	if (cmd == NULL) {
		goto out;
	}

	if (system(cmd) != 0) {
		perror("System failed");
		goto out;
	}

	if (! WIFEXITED(status)) {
		perror("WIFEXITED failed");
		goto out;
	}

	ret = WEXITSTATUS(status) == 0;

out:
	return ret;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
	command[i] = va_arg(args, char *);
	dbg("cmd %d: %s", i, command[i]);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

	pid_t pid = fork();
	int status = 0;
	bool ret = false;
	if (pid < 0) {
		// Fork failed
		perror("Failure executing syscall fork");
		goto out;
	} else if (pid == 0) {
		// child if pid = 0
		execv(command[0], command);
		perror("Failure executing syscall execv");
		exit(-1);
	}
	// parent if pid > 0
	if (waitpid(pid, &status, 0) == -1) {
		perror("Waitpid failed");
		goto out;
	}
	if (! WIFEXITED(status)) {
		perror("WIFEXITED failed");
		goto out;
	}

	ret = WEXITSTATUS(status) == 0;

out:
	va_end(args);
	return ret;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
	command[i] = va_arg(args, char *);
	dbg("cmd %d: %s", i, command[i]);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

	int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
	int status = 0;
	bool ret = false;
	if (fd < 0) {
		perror("Failed to open output file");
		goto out;
	}

	pid_t pid = fork();
	if (pid < 0) {
		// Fork failed
		perror("Failure executing syscall fork");
		goto out_close;
	} else if (pid == 0) {
		// child if pid = 0
		if (dup2(fd, 1) < 0) {
			perror("Failed to redirect output");
		} else {
			execv(command[0], command);
			perror("Failure executing syscall execv");
		}
		exit(-1);
	}
	// parent if pid > 0
	if (waitpid(pid, &status, 0) == -1) {
		perror("Waitpid failed");
		goto out_close;
	}
	if (! WIFEXITED(status)) {
		perror("WIFEXITED failed");
		goto out_close;
	}

	ret = WEXITSTATUS(status) == 0;

out_close:
	close(fd);
out:
	va_end(args);
	return ret;
}
