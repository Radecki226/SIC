#include        <sys/types.h>   /* basic system data types */
#include        <sys/socket.h>  /* basic socket definitions */
#include        <sys/time.h>    /* timeval{} for select() */
#include        <time.h>                /* timespec{} for pselect() */
#include        <netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include        <arpa/inet.h>   /* inet(3) functions */
#include        <errno.h>
#include        <fcntl.h>               /* for nonblocking */
#include        <netdb.h>
#include        <signal.h>
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include	<syslog.h>
#include	<unistd.h>

/*
Aplikacja tworzy dwa serwery:
I. Serwer sic pracuj¹cy na porcie 4444 w protocole UDP, serwer wysy³a czasy zgodnie z protoko³em sic
II. Serwer informacyjny wysy³aj¹cy co 5 sekund informacjê o adresie unicastowym na adres multicastowy 224.0.0.1 na port 5300
*/

#define MAXLINE 1024


#define LISTENQ 2

#define	MAXFD	64
#define PORT 4444
#define PORT_M 5300
#define GROUP "224.0.0.1"

unsigned long long int current_time() {
	struct timeval t;
	long long int current_time_us;
	gettimeofday(&t, NULL);
	current_time_us = (((unsigned long long int)t.tv_sec)) * 1000000 + (unsigned long long int)t.tv_usec;
	return current_time_us;
}



int
daemon_init(const char *pname, int facility, uid_t uid, int socket)
{
	int		i, p;
	pid_t	pid;

	if ( (pid = fork()) < 0)
		return (-1);
	else if (pid)
		exit(0);			/* parent terminates */

	/* child 1 continues... */

	if (setsid() < 0)			/* become session leader */
		return (-1);

	signal(SIGHUP, SIG_IGN);
	if ( (pid = fork()) < 0)
		return (-1);
	else if (pid)
		exit(0);			/* child 1 terminates */

	/* child 2 continues... */

	chdir("/tmp");				/* change working directory  or chroot()*/
//	chroot("/tmp");

	/* close off file descriptors */
	for (i = 0; i < MAXFD; i++){
		if(socket != i )
			close(i);
	}

	/* redirect stdin, stdout, and stderr to /dev/null */
	p= open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

	openlog(pname, LOG_PID, facility);

        syslog(LOG_ERR," STDIN =   %i\n", p);
	setuid(uid); /* change user */

	return (0);				/* success */
}
//----------------------
int multicast_server(char *address) {
	struct sockaddr_in addr;
	int addrlen, sock, n;
	char message[128];

	//gniazdo
	sock = socket(AF_INET, SOCK_DGRAM, 0);



	if (sock < 0) {
		syslog(LOG_ERR, "socket error");
		exit(1);
	}

	daemon_init("multicast_server", LOG_USER, 1000, sock);
	syslog(LOG_NOTICE, "Multicast server started by User %d", getuid());

	bzero((char*)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT_M);
	addr.sin_addr.s_addr = inet_addr(GROUP);
	addrlen = sizeof(addr);
	for (;;) {

		//wysylanie


		sprintf(message, address);
		n = sendto(sock, message, sizeof(message), 0,
			(struct sockaddr*)&addr, addrlen);
		if (n < 0) {
			syslog(LOG_ERR, "sendto error");
		}
		//co 5 sekund
		sleep(5);

	}
}

int sic_server(char *address) {
	int					sockfd, n;
	socklen_t			len;
	char				buff[MAXLINE], buff1[MAXLINE], str[INET_ADDRSTRLEN + 1];
	time_t				ticks;
	struct sockaddr_in	servaddr, cliaddr;
	unsigned long long int t2, t3;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "socket error");
		return 1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(address);
	servaddr.sin_port = htons(PORT);

	if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		syslog(LOG_ERR, "bind error");
		return 1;
	}

	daemon_init("sic_server", LOG_USER, 1000, sockfd);
	syslog(LOG_NOTICE, "Sic server started by User %d", getuid());
	syslog(LOG_INFO, "Waiting for clients ... ");

	for (; ; ) {
		len = sizeof(cliaddr);
		if (n = recvfrom(sockfd, (char*)buff, MAXLINE, 0, (struct sockaddr*)&cliaddr, &len) < 0) {
			syslog(LOG_ERR, "recvfrom error");
			continue;
		}
		else {
			t2 = current_time();
			inet_ntop(AF_INET, (struct sockaddr*)&cliaddr.sin_addr, str, sizeof(str));
			syslog(LOG_INFO, "New connection from : %s\n", str);
			t3 = current_time();
			sprintf(buff1, "%llu\n%llu\n", t2, t3);
			strcat(buff, buff1);
		}

		if (n = sendto(sockfd, buff, strlen(buff), 0, (struct sockaddr*)&cliaddr, len) < 0) {
			syslog(LOG_ERR, "sendto error");
		}
		memset(buff, 0, MAXLINE);

	}
}


int main(int argc, char **argv){
	if(argc!=2)
    {
        printf("usage: a.out ipv4_address_of_server\n");
        exit(1);
    }
	if (fork() == 0)
		multicast_server(argv[1]);
	else
		sic_server(argv[1]);
}
