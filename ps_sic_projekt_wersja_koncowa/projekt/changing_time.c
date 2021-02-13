#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>
#include        <sys/types.h>
#include        <sys/time.h>
#include        <time.h>

//zmienia czas o wartosc sekund i mikrosekund podana przez uzytkownika
//pierwszy argument to sekundy a drugi mikrosekundy

int main(int argc, char **argv)
{
    struct timeval t;
    long long int sec_bias;
    int usec_bias;
    time_t ticks;
    if(argc!=3)
    {
        printf("Incorrect number of arguments.\n");
        exit(0);
    }
    sec_bias=atoi(argv[1]);
    usec_bias=atoi(argv[2]);
    if(usec_bias>=1000000 || usec_bias<=-1000000)
    {
        printf("Incorrect value of usec. Its absolute value should be less than million.\n");
        exit(0);
    }
    else
    {
        gettimeofday(&t, NULL);
        t.tv_sec+=sec_bias;
        t.tv_usec+=usec_bias;
        if(t.tv_usec>1000000)
        {
            t.tv_sec+=1;
            t.tv_usec=t.tv_usec%1000000;
        }
        if(t.tv_usec<0)
        {
            t.tv_sec-=1;
            t.tv_usec+=1000000;
        }
        if(settimeofday(&t, NULL) < 0) //ustawia nowy czas, potrzebne uprawnienia roota
            printf("Error using settimeofday\n");
        ticks=time(NULL);
        printf("Current time is: %lld\n", t.tv_sec*1000000+t.tv_usec);
        printf("Current time is: %.24s\r\n", ctime(&ticks));
    }
    return 0;
}
