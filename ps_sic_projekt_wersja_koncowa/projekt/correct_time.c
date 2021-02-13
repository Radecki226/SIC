#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <sys/types.h>
#include        <sys/time.h>
#include        <time.h>


long long int current_time() {
	struct timeval t;
	long long int current_time_us;
	gettimeofday(&t, NULL);
	current_time_us = t.tv_sec * 1000000 + t.tv_usec;
	return current_time_us;
}


//wywolany bez argumentu zmienia czas maszyny
//jezeli podany jest argument to jest to scaler zegara i poprawny czas maszyny jest jedynie printowany

int main(int argc, char **argv)
{
    struct timeval t;
    FILE * f;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    long long int  phi, usec, usec_cor;
    time_t ticks;
    double K, F;
    long double scaler=1;
    double sec;

    if (argc==2)
        scaler=strtold(argv[1], NULL);
    if (argc>2){
        fprintf(stderr, "usage: a.out <scaler(optional)> \n");
        return 1;
    }

    f = fopen("sic.txt", "r");

    if (f == NULL)
    {
        printf("Error opening file!\n");
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
    printf("K=%lf\nF=%lf\n", K, F);
    if(argc!=2)
    {
        ticks=time(NULL);
        printf("Old time was: %.24s\r\n", ctime(&ticks));
    }
    usec=current_time()*scaler;
    sec=usec/1000000;
    phi=K+F*sec;
    usec_cor=usec-phi;
    if(argc==2)
    {
        printf("Old time was: %lld\nCorrected time is: %lld\n", usec, usec_cor);
        return 0;
    }
    t.tv_sec=usec_cor/1000000;
    t.tv_usec=usec_cor%1000000;
    if(settimeofday(&t, NULL) < 0)
        printf("Error using settimeofday\n");
    ticks=time(NULL);
    printf("New time is: %.24s\r\n", ctime(&ticks));
}
