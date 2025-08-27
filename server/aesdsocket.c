#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>	/* exit         */
#include <syslog.h>	/* openlog, syslog, closelog */
#include <sys/socket.h>	/* socket	*/
#include <arpa/inet.h>	/* inet_ntop	*/
#include <signal.h>	/* sigaction */

#define PROG_NAME	"aesdsocket"

typedef struct {
	int sock;
	struct sockaddr_in info;
} server_t;

void server_close(server_t *s) {
	if (s->sock > 0) {
		if (shutdown(s->sock, SHUT_RDWR)) {
			perror("shutdown failed");
		}
		if (close(s->sock)) {
			perror("close failed");
		}
	}
}

void server_setup(server_t *s) {
	int enable = 1;

	s->info.sin_family = AF_INET;
	s->info.sin_addr.s_addr = htons(INADDR_ANY);
	s->info.sin_port = htons(9000);

	s->sock = socket(PF_INET, SOCK_STREAM, 0);
	if (s->sock < 0) {
		perror("socket failed");
		goto err;
	}

	if (setsockopt(s->sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))) {
		perror("setsockopt failed");
		goto err;
	}

	if (bind(s->sock, (struct sockaddr *)&s->info, sizeof(s->info))) {
		perror("bind failed");
		goto err;
	}

	if (listen(s->sock, 10)) {
		perror("listen failed");
		goto err;
	}

	return;
err:
	server_close(s);
	exit(1);
}

// Struct to control client.
typedef struct {
	int sock;
	struct sockaddr_in addr;
	char ipaddr[INET_ADDRSTRLEN];
	FILE *fp;
	socklen_t addrlen;
} client_t;

void client_close(client_t *c) {
	fclose(c->fp);
}

void client_setup(client_t *c) {
	c->fp = fopen("/var/tmp/aesdsocketdata", "w+");
	if (!c->fp) {
		perror("fopen failed");
		exit(1);
	}
	c->addrlen = sizeof(c->addr);
	memset(c->ipaddr, 0, INET_ADDRSTRLEN);
}

#define BUFLEN 1023
void client_logic(client_t *c) {
	char buf[BUFLEN + 1];
	ssize_t readsocklen = 0;
	size_t readfilelen = 0;
	ssize_t sendsocklen = 0;

	// Always make sure we're writing to the end of the file
	if (fseek(c->fp, 0, SEEK_END)) {
		perror("fseek END failed");
		goto err;
	}

	// Get's IP address 
	if ( ! inet_ntop(AF_INET, &c->addr.sin_addr, c->ipaddr, INET_ADDRSTRLEN)) {
		perror("inet_ntop failed");
		goto err;
	}
	
	syslog(LOG_INFO, "Accepted connection from %s\n", c->ipaddr);

	// Read from socket and write to file
	while ((readsocklen = recv(c->sock, buf, BUFLEN, MSG_DONTWAIT)) > 1) {

		buf[readsocklen] = '\0';
		if (fputs(buf, c->fp) < 0) {
			perror("fputs failed");
			goto err;
		}
	}

	if (fseek(c->fp, 0, SEEK_SET)) {
		perror("fseek SET failed");
		goto err;
	}

	// Read from the file and write to the socket
	while ((readfilelen = fread(buf, sizeof(char), BUFLEN, c->fp))) {
		sendsocklen = send(c->sock, buf, readfilelen, 0);

		if (readfilelen != sendsocklen) {
			perror("send failed");
			goto err;
		}
		memset(buf, 0, BUFLEN);
	}

err:
	shutdown(c->sock, SHUT_RDWR);
	close(c->sock);
	syslog(LOG_INFO, "Closed connection from %s", c->ipaddr);
}

// sigaction
static int program_active = 1;

static void sig_handler(int signal) {
	switch (signal) {
		case SIGINT:
		case SIGTERM:
			program_active = 0;
		break;
	default:
		// do nothing.
	}
}

void signal_setup(struct sigaction *a) {
	memset(a, 0, sizeof(*a));

	a->sa_handler = sig_handler;

	if (sigaction(SIGINT, a, NULL)) {
		perror("sigaction SIGINT failed");
		exit(1);
	}
	if (sigaction(SIGTERM, a, NULL)) {
		perror("sigaction SIGTERM failed");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	server_t serv;
	client_t clnt;
	struct sigaction actn;
	int ret = 0;

	if (argc > 1) {
		if (strncmp(argv[1], "-d", 2)) {
			fprintf(stderr, "Option %s not supported\n", argv[1]);
			exit(1);
		}
		daemon(0, 0);
	}

	openlog(PROG_NAME, LOG_PERROR|LOG_PID, LOG_USER);
	signal_setup(&actn);
	server_setup(&serv);
	client_setup(&clnt);
	
	while (program_active) {
		clnt.sock = accept(serv.sock, (struct sockaddr *)&clnt.addr, &clnt.addrlen);
		if (clnt.sock < 0) {
			ret = errno != EINTR;
			if (ret) perror("accept failed");
			break;
		}
		client_logic(&clnt);
	}

	client_close(&clnt);
	server_close(&serv);
	closelog();
	return ret;
}
