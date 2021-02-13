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


#define PORT 4444
#define MAXLINE 1024
#define SA      struct sockaddr

//pierwszy argument to adres, drugi argument to scaler (opcjonalny)

long long int current_time() {
	struct timeval t;
	long long int current_time_us;
	gettimeofday(&t, NULL);
	current_time_us = t.tv_sec * 1000000 + t.tv_usec;
	return current_time_us;
}


int
dt_cli(int sockfd, const SA *pservaddr, socklen_t servlen, long double scaler, long double K, long double F)
{
	int		n, i;
	char		sendline[MAXLINE], recvline[MAXLINE + 1];
	socklen_t	len;
	struct sockaddr	*preply_addr;
	char		str[INET6_ADDRSTRLEN+1];
	struct sockaddr_in*	 cliaddrv4;
	struct timeval delay;
	long long int t1, t2, t3, t4, RTT, phi1, phi2, phit;

	if( (preply_addr = malloc(servlen)) == NULL ){
		perror("malloc error");
		exit(1);
	}

	bzero( sendline, sizeof(sendline));
	delay.tv_sec = 3;  //opoznienie na gniezdzie
	delay.tv_usec = 0;
	len = sizeof(delay);
	if( setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &delay, len) == -1 ){
		fprintf(stderr,"SO_RCVTIMEO setsockopt error : %s\n", strerror(errno));
		return -1;
	}

	len = servlen;

    t1=current_time()*scaler;
    phit=K+F*t1/1000000;
    t1-=phit;
    snprintf(sendline, sizeof(sendline), "%lld\n", t1);
    if( sendto(sockfd, sendline, strlen(sendline), 0, pservaddr, servlen) <0 ){
        perror("sendto error");
        free(preply_addr);
        exit(1);
    }

    if( (n = recvfrom(sockfd, recvline, MAXLINE, 0, preply_addr, &len) ) < 0 ){
        printf("errno = %d\n", errno);
        perror("recvfrom error");
        free(preply_addr);
        exit(1);}
    t4=current_time()*scaler;
    phit=K+F*t4/1000000;
    t4-=phit;



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

    RTT = ( t2 - t1 ) + ( t4 - t3 );
    phi1 = t1 - ( t2 - RTT/2 );
    phi2 = t4 - ( t3 + RTT/2 ) ,
    printf("t1 = %lld\nt2 = %lld\nt3 = %lld\nt4 = %lld\n", t1, t2, t3, t4);
    printf("RTT = %lld\nphi1 = %lld\nphi2 = %lld\n", RTT, phi1, phi2);

	free(preply_addr);
	return 0;
}
int
main(int argc, char **argv)
{
	int					sockfd, n, delay;
	struct sockaddr_in	servaddr;
	char				recvline[MAXLINE + 1];
	double                 scaler=1;
	double K, F;
	FILE * f;
    char * line = NULL;
    ssize_t read;
    size_t length = 0;

	if (argc < 2 || argc >3){
		fprintf(stderr, "usage: a.out <IPaddress> <scaler(optional)> \n");
		return 1;
	}
	if (argc==3)
        scaler=strtod(argv[2], NULL);
    //printf("%llf\n", scaler);

    f = fopen("sic.txt", "r");

    if (f == NULL)
    {
        printf("Error opening file!\n");
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
    printf("K=%lf\nF=%lf\n", K, F);

	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		fprintf(stderr,"socket error : %s\n", strerror(errno));
		return 1;
	}


	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port   = htons(PORT);	/* daytime server */
	if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0){
		fprintf(stderr,"inet_pton error for %s : %s \n", argv[1], strerror(errno));
		return 1;
	}

	dt_cli( sockfd, (SA *) &servaddr, sizeof(servaddr), scaler, K, F);


	exit(0);
}
