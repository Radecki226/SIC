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
#include        <unistd.h>
#include        <limits.h>
#include        <math.h>


#define PORT 4444
#define MAXLINE 1024
#define SA      struct sockaddr

#define RUNNING_TIME 1 //co ile wysylamy pakiety[s]
#define TIMEOUT 800000 //[us]
#define P 60 //ilosc pakietow wyslanych
#include <gsl/gsl_fit.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sort.h>

#include	<syslog.h>
#define	MAXFD	64


#include <sys/utsname.h>    /* uname() */

//argumenty po kolei: adres mcast, port (5300), scaler opcjonalnie


long long int current_time() {
	struct timeval t;
	long long int current_time_us;
	gettimeofday(&t, NULL);
	current_time_us = t.tv_sec * 1000000 + t.tv_usec;
	return current_time_us;
}

int
sic_cli(int sockfd, const SA *pservaddr, socklen_t servlen, long double scaler)
{
	int		n, i;
	char		sendline[MAXLINE], recvline[MAXLINE + 1];
	socklen_t	len;
	struct sockaddr	*preply_addr;
	char		str[INET6_ADDRSTRLEN+1];
	struct sockaddr_in*	 cliaddrv4;
	struct timeval delay;
	long long int t1, t2, t3, t4;
	double  K, F;
	double cov00, cov01, cov11, sumasq, m, c;
	float errRTT = 1; //0.2; //roznica w RTT przy ktorej zakladamy zmiane route
	int MAX_TO = P/10; //maksymalna liczba utraconych pakietow
	double Wm[P]; //tablica z obliczonymi phi(t)
	double WRTT[P]; //tablica z obliczonymi RTT
	double RTTl, RTTf, m_RTT;
	int n_to;
	double epoch; //aktualny czas w sekundach
	double Wepoch[P]; //tablica aktualnych czasow


	if( (preply_addr = malloc(servlen)) == NULL ){
		syslog(LOG_ERR, "malloc error");
		exit(1);
	}
	bzero( sendline, sizeof(sendline));
	delay.tv_sec = 0;  //opoznienie na gniezdzie
	delay.tv_usec = TIMEOUT; //0.8sec
	len = sizeof(delay);
	if( setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &delay, len) == -1 ){
		syslog(LOG_ERR,"SO_RCVTIMEO setsockopt error");
		return -1;
	}
	len = servlen;
	Starting_sync_cycle:
	for(i=0; i < P ; i++ ){

        t1=current_time()*scaler;
        snprintf(sendline, sizeof(sendline), "%lld\n", t1);
		if( sendto(sockfd, sendline, strlen(sendline), 0, pservaddr, servlen) <0 ){
			syslog(LOG_ERR, "sendto error");
			free(preply_addr);
			exit(1);
		}

		if( (n = recvfrom(sockfd, recvline, MAXLINE, 0, preply_addr, &len) ) < 0 ){
			syslog(LOG_NOTICE, "timeout");
			n_to+=1;
			if(n_to==MAX_TO)
			{
			    syslog(LOG_ERR, "Maximum timeouts reached");
			    free(preply_addr);
                exit(1);
			}
			else
            {
                i-=1;
                continue;
            }
        }
        t4=current_time()*scaler;

        char * curLine = recvline;
        int line=0;
        while(curLine)
        {
            char * nextLine = strchr(curLine, '\n');
            if (nextLine) *nextLine = '\0';  // temporarily terminate the current line
            {
                //printf("cur str %s\n", curLine);
                if(line==0)
                    t1=strtoll(curLine, NULL, 0);
                if(line==1)
                    t2=strtoll(curLine, NULL, 0);
                if(line==2)
                    t3=strtoll(curLine, NULL, 0);
            }
            if (nextLine) *nextLine = '\n';  // then restore newline-char, just to be tidy
                curLine = nextLine ? (nextLine+1) : NULL;
            line+=1;
        }
        epoch=t1/1000000; //aktualny czas w sekundach
        Wepoch[i]=epoch;

        Wm[i]=t1-t2+(t2-t1+t4-t3)/2;
        //printf("%lf\n", Wm[i]);
        WRTT[i]=t4-t1;
        sleep(RUNNING_TIME); //pakiety wysylane co sekunde
	}

	RTTl=gsl_stats_min(WRTT, 1, P/2);
	RTTf=gsl_stats_min(WRTT+P/2, 1, P/2);
	m_RTT=gsl_stats_min(WRTT, 1, P);
    //sprawdzamy czy nie zmienila sie sciezka do serwera
	if(fabs(RTTl-RTTf)>errRTT*m_RTT)
    {
        syslog(LOG_NOTICE, "Route changed. Starting to synchronize again.");
        goto Starting_sync_cycle;
    }
    //obliczenie wspolczynnikow z regresji liniowej
    gsl_fit_linear(Wepoch, 1, Wm, 1, P, &c, &m,&cov00, &cov01, &cov11, &sumasq);
    K=c;
    F=m;

    //zapisywanie wspolczynnikow do pliku
    FILE *f = fopen("sic_ext.txt", "w");
    if (f == NULL)
    {
        syslog(LOG_ERR, "Error opening file!");
        exit(1);
    }

    fprintf(f, "%lf\n%lf\n", K, F);
    fclose(f);

	free(preply_addr);
	return 0;
}


long long int
check_time_cli(int sockfd, const SA *pservaddr, socklen_t servlen, long double scaler)
{
	int		n, i;
	char		sendline[MAXLINE], recvline[MAXLINE + 1];
	socklen_t	len;
	struct sockaddr	*preply_addr;
	char		str[INET6_ADDRSTRLEN+1];
	struct sockaddr_in*	 cliaddrv4;
	struct timeval delay;
	long long int t1, t2, t3, t4, RTT, phi1, phi2, phit;
    double K, F;
    FILE * f;
    char * line = NULL;
    ssize_t read;
    size_t length = 0;

	f = fopen("sic_ext.txt", "r");

    if (f == NULL)
    {
        syslog(LOG_ERR, "Error opening file!");
        exit(1);
    }
    int iter=0;
    while ((read = getline(&line, &length, f)) != -1) {
        if(iter==0)
            K=strtod(line, NULL);
        if(iter==1)
            F=strtod(line, NULL);
        iter+=1;
    }
    fclose(f);
    syslog(LOG_INFO, "K=%lf     F=%lf", K, F);



	if( (preply_addr = malloc(servlen)) == NULL ){
		syslog(LOG_ERR, "malloc error");
		exit(1);
	}

	bzero( sendline, sizeof(sendline));
	delay.tv_sec = 3;  //opoznienie na gniezdzie
	delay.tv_usec = 0;
	len = sizeof(delay);
	if( setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &delay, len) == -1 ){
		syslog(LOG_ERR,"SO_RCVTIMEO setsockopt error");
		return -1;
	}

	len = servlen;

    t1=current_time()*scaler;
    phit=K+F*t1/1000000;
    t1-=phit;
    snprintf(sendline, sizeof(sendline), "%lld\n", t1);
    if( sendto(sockfd, sendline, strlen(sendline), 0, pservaddr, servlen) <0 ){
        syslog(LOG_ERR, "sendto error");
        free(preply_addr);
        exit(1);
    }

    if( (n = recvfrom(sockfd, recvline, MAXLINE, 0, preply_addr, &len) ) < 0 ){
        syslog(LOG_ERR, "recvfrom error");
        free(preply_addr);
        exit(1);}
    t4=current_time()*scaler;
    phit=K+F*t4/1000000;
    t4-=phit;



    char * curLine = recvline;
    int line_nb=0;
    while(curLine)
    {
        char * nextLine = strchr(curLine, '\n');
        if (nextLine) *nextLine = '\0';  // temporarily terminate the current line
        {
            //printf("cur str %s\n", curLine);
            if(line_nb==0)
                t1=strtoll(curLine, NULL, 0);
            if(line_nb==1)
                t2=strtoll(curLine, NULL, 0);
            if(line_nb==2)
                t3=strtoll(curLine, NULL, 0);
        }
        if (nextLine) *nextLine = '\n';  // then restore newline-char, just to be tidy
            curLine = nextLine ? (nextLine+1) : NULL;
        line_nb+=1;
    }

    RTT = ( t2 - t1 ) + ( t4 - t3 );
    phi1 = t1 - ( t2 - RTT/2 );
    phi2 = t4 - ( t3 + RTT/2 ) ,
    syslog(LOG_INFO, "t1 = %lld     t2 = %lld       t3 = %lld       t4 = %lld", t1, t2, t3, t4);
    syslog(LOG_INFO, "RTT = %lld        phi1 = %lld       phi2 = %lld", RTT, phi1, phi2);

	free(preply_addr);
	return (phi1+phi2)/2;
}


int correct_time(long double scaler)
{
    struct timeval t;
    FILE * f;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    long long int  phi, usec, usec_cor;
    time_t ticks;
    double K, F;
    double sec;

    f = fopen("sic_ext.txt", "r");

    if (f == NULL)
    {
        syslog(LOG_ERR, "Error opening file!");
        exit(1);
    }
    //czytanie wspolczynnikow z pliku
    int iter=0;
    while ((read = getline(&line, &len, f)) != -1) {
        if(iter==0)
            K=strtod(line, NULL);
        if(iter==1)
            F=strtod(line, NULL);
        iter+=1;
    }
    fclose(f);
    syslog(LOG_INFO, "K=%lf     F=%lf", K, F);
    ticks=time(NULL);
    syslog(LOG_INFO, "Old time was: %.24s\r", ctime(&ticks));
    usec=current_time()*scaler;
    sec=usec/1000000;
    phi=K+F*sec;
    usec_cor=usec-phi;
    if(scaler!=1)
    {
        syslog(LOG_INFO, "Old time was: %lld        Corrected time is: %lld", usec, usec_cor);
        return 0;
    }
    t.tv_sec=usec_cor/1000000;
    t.tv_usec=usec_cor%1000000;
    settimeofday(&t, NULL);
    ticks=time(NULL);
    syslog(LOG_INFO, "New time is: %.24s\r", ctime(&ticks));
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

	int
main(int argc, char **argv)
{
	int					sockfd, n, delay;
	struct sockaddr_in	servaddr;
	char				recvline[MAXLINE + 1];
	long double                 scaler=1;
	long double    phi;

	if (argc < 3 || argc > 4){
		fprintf(stderr, "usage: a.out <IPaddress> <scaler(optional)> \n");
		return 1;
	}
	if (argc==4)
        scaler=strtold(argv[3], NULL);
    //printf("%llf\n", scaler);

	int recv_s;
	struct sockaddr_in mcast_group;
	struct ip_mreq mreq;
    int len;
    struct sockaddr_in from;
    struct utsname name;
     memset(&mcast_group, 0, sizeof(mcast_group));
     mcast_group.sin_family = AF_INET;
     mcast_group.sin_port = htons((unsigned short int)strtol(argv[2], NULL, 0));
     mcast_group.sin_addr.s_addr = inet_addr(argv[1]);

     if ( (recv_s=socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
         perror ("recv socket");
         exit(1);
     }

     if (bind(recv_s, (struct sockaddr*)&mcast_group, sizeof(mcast_group)) < 0) {
         perror ("bind");
         exit(1);
     }


	mreq.imr_multiaddr = mcast_group.sin_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	if (setsockopt(recv_s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
	perror ("add_membership setsockopt");
	exit(1);
	}

	len=sizeof(from);
    if ( (n=recvfrom(recv_s, recvline, MAXLINE, 0, (struct sockaddr*)&from, &len)) < 0)
    {
        perror ("recv");
        exit(1);
    }
    recvline[n] = '\0';
    printf("Received message from %s:\n", inet_ntoa(from.sin_addr));
    printf("%s \n", recvline);

	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		fprintf(stderr,"socket error : %s\n", strerror(errno));
		return 1;
	}

    daemon_init("sic_extended_client", LOG_USER, 0, sockfd);
    syslog(LOG_NOTICE, "Sic client started by User %d", getuid());

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port   = htons(PORT);
	if (inet_pton(AF_INET, recvline, &servaddr.sin_addr) <= 0){
		syslog(LOG_ERR, "inet_pton error for %s", recvline);
		return 1;
	}
	syslog(LOG_NOTICE, "Starting sync loop");
	sic_cli( sockfd, (SA *) &servaddr, sizeof(servaddr), scaler); //algorytm sic
	if(scaler==1)
    {
        correct_time(scaler); //pierwsza naprawa czasow
        sic_cli( sockfd, (SA *) &servaddr, sizeof(servaddr), scaler); //zaktualizowanie wspolczynnikow
    }
	for(;;)
    {
        phi=check_time_cli( sockfd, (SA *) &servaddr, sizeof(servaddr), scaler); //sprawdzenie zgodnosci czasow przy uzyciu czasu obliczonego ze wspolczynnikow
        if(fabs(phi)>1000000 /*us*/)
        {
            syslog(LOG_NOTICE, "Desynchronization detected. Starting to synchronize again.");
            sic_cli( sockfd, (SA *) &servaddr, sizeof(servaddr), scaler); //algorytm sic
            if(scaler==1)
            {
                correct_time(scaler); //naprawa czasow
                sic_cli( sockfd, (SA *) &servaddr, sizeof(servaddr), scaler); //zaktualizowanie wspolczynnikow
            }
        }
        sleep(10);
    }

	exit(0);
}

