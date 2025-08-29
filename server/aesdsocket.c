#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>	/* exit         */
#include <syslog.h>	/* openlog, syslog, closelog */
#include <sys/socket.h>	/* socket	*/
#include <arpa/inet.h>	/* inet_ntop	*/
#include <signal.h>	/* sigaction */
#include <pthread.h>
#include <time.h>

#define PROG_NAME	"aesdsocket"

// Global data
FILE *log_fp;
pthread_mutex_t mutex;

void global_clean() {
	if (log_fp > 0) fclose(log_fp);

	/* I don't have to check this, because if pthread_mutex_init
	 * fails the program exists */
	pthread_mutex_destroy(&mutex);
}

void global_setup() {
	log_fp = fopen("/var/tmp/aesdsocketdata", "w+");
	if (!log_fp) {
		perror("fopen failed");
		exit(1);
	}
	if (pthread_mutex_init(&mutex, NULL)) {
		perror("pthread_mutex_init failed");
		goto err;
	}
	return;
err:
	global_clean();
	exit(1);

}

// Server
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

	{ /* The tests starts this service too fast. Hence we iterativly attempt to bind
	   * until the kernel frees the port. */
		int i = 1, j = 1024;
		for (; i <= j; i++) {
			if (bind(s->sock, (struct sockaddr *)&s->info, sizeof(s->info))) {
				usleep(i * 100*1000);
			} else {
				break;
			}
			if (i == j) {
				perror("bind failed");
				goto err;
			}
		}
	}

	if (listen(s->sock, 20)) {
		perror("listen failed");
		goto err;
	}

	return;
err:
	server_close(s);
	exit(1);
}

// Client
typedef struct {
	int sock;
	struct sockaddr_in addr;
	char ipaddr[INET_ADDRSTRLEN];
	FILE *fp;
	socklen_t addrlen;
} client_t;

void client_setup(client_t *c) {
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
	if (fseek(log_fp, 0, SEEK_END)) {
		perror("fseek END failed");
		goto err;
	}

	// Get's IP address 
	if ( ! inet_ntop(AF_INET, &c->addr.sin_addr, c->ipaddr, INET_ADDRSTRLEN)) {
		perror("inet_ntop failed");
		goto err;
	}
	

	if (pthread_mutex_lock(&mutex)) {
		perror("pthread_mutex_lock failed");
		goto err;
	}
	syslog(LOG_INFO, "Accepted connection from %s\n", c->ipaddr);


	// Read from socket and write to file
	while ((readsocklen = recv(c->sock, buf, BUFLEN, MSG_DONTWAIT)) > 1) {
		buf[readsocklen] = '\0';
		syslog(LOG_INFO, "%s", buf);
		if (fputs(buf, log_fp) < 0) {
			perror("fputs failed");
			goto err;
		}
	}

	if (fseek(log_fp, 0, SEEK_SET)) {
		perror("fseek SET failed");
		goto err;
	}
	fflush(log_fp);

	// Read from the file and write to the socket
	while ((readfilelen = fread(buf, sizeof(char), BUFLEN, log_fp))) {
		sendsocklen = send(c->sock, buf, readfilelen, 0);

		if (readfilelen != sendsocklen) {
			perror("send failed");
			goto err;
		}
		memset(buf, 0, BUFLEN);
	}

err:
	if (pthread_mutex_unlock(&mutex)) {
		perror("pthread_mutex_unlock failed");
	}
	shutdown(c->sock, SHUT_RDWR);
	close(c->sock);
	syslog(LOG_INFO, "Closed connection from %s", c->ipaddr);
}

// sigaction
static int program_active = 1;
static void sig_handler(int signal) {
	// https://man7.org/linux/man-pages/man7/signal-safety.7.html
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

// Thread
typedef struct task_t_ {
	pthread_t tid;
	void **value_ptr;
	struct task_t_ *next;
	client_t clnt;
} task_t;

static task_t *head;

static void * thread_run(void *arg) {
	client_t *clnt = (client_t *)arg;
	client_logic(clnt);
	//pthread_detach(pthread_self());
	pthread_exit(NULL);
}

void thread_clean() {
	static task_t *next;
	int x = 0;

	while(head) {
		printf("cleanup %d\n", x++);
		next = head->next;
		pthread_join(head->tid, NULL);
		free(head);
		head = NULL;
		head = next;
	}
}

static void * timer_log(void *arg) {
	time_t wall_time;
	char * timestamp;
	char msg[512];

	while(program_active) {
		// sleep 10 seconds
		sleep(10);
		
		if (pthread_mutex_lock(&mutex)) {
			perror("pthread_mutex_lock error");
			// do something?
		}

		fflush(log_fp);
		if (fseek(log_fp, 0, SEEK_END)) {
			perror("fseek END failed");
		}

		// write timestamp to file
		wall_time = time(NULL);
		timestamp = asctime(localtime(&wall_time));

		memset(msg, 0, 512);
		sprintf(msg, "timestamp:%s", timestamp);
		fwrite(msg, sizeof(char), strlen(msg), log_fp);
		fflush(log_fp);

		if (pthread_mutex_unlock(&mutex)) {
			perror("pthread_mutex_unlock error");
			// do something?
		}
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	server_t serv;
	struct sigaction actn;
	int ret = 0;
	pthread_t log_tid;

	if (argc > 1) {
		if (strncmp(argv[1], "-d", 2)) {
			fprintf(stderr, "Option %s not supported\n", argv[1]);
			exit(1);
		}
		if (daemon(0, 0)) {
			perror("daemon failed");
			exit(1);
		}
	}

	openlog(PROG_NAME, LOG_PERROR|LOG_PID, LOG_USER);
	global_setup();
	server_setup(&serv);

	if (pthread_create(&log_tid, NULL, timer_log, NULL)) {
		perror("pthread_create LOG failed");
		goto out;
	}

	signal_setup(&actn);
	head = NULL;
	
	while (program_active) {
		task_t *next = malloc(sizeof(task_t));
		next->next = head;
		head = next;

		memset(next, 0, sizeof(task_t));
		client_setup(&next->clnt);
		next->clnt.sock = accept(serv.sock, (struct sockaddr *)&next->clnt.addr, &next->clnt.addrlen);

		if (next->clnt.sock < 0) {
			ret = errno != EINTR;
			if (ret) perror("accept failed");
			goto out;
		}

		if (pthread_create(&next->tid, NULL, thread_run, &next->clnt)) {
			ret = errno != EINTR;
			if (ret) perror("pthread_create CLNT failed");
			goto out;
		}
	}
out:
	pthread_join(log_tid, NULL);

	global_clean();
	server_close(&serv);
	thread_clean();

	closelog();
	return ret;
}
