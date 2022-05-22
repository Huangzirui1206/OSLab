#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#define MAX_BUF_SIZE 128
#define MAX_WORDS_NUM 24


void splitsentence(char *src,const char *d,char **buf,int *num)
{
	if ((src == NULL) || (d == NULL)){
		return;
	}
	*buf = strtok(src, d);
	while(*buf != NULL){
		(*num)++;
		buf++;
		*buf = strtok(NULL, d);
	}
}

void turn(char **buf,int num)
{
	for (int i = num; i >= 0; i--){
		printf("%s ", buf[i]);
	}
    printf("\n");
}

int main(int argc, char** argv){
    printf("Redirect system() with pipe.\n");

    /*Open pipe*/
    int filedes[2];
    char buf[MAX_BUF_SIZE];
    pipe(filedes);

    /*Redirect*/
    int old = dup(1);
    dup2(filedes[1], 1);

    /*Pay attention: unsafe use of system*/
    system(argv[1]);
    
    /*read system() STD_OUT to buf*/
    int sz = read(filedes[0], (void*)buf, 128);

    /*recover STD_OUT*/
    dup2(old, 1);

    /*Handle the output*/
    if(sz == MAX_BUF_SIZE){
        buf[--sz] = '\0';
        printf("The output is more than %d byte.\n", MAX_BUF_SIZE);
    }
    else{
        buf[sz] = '\0';
    }
    for(int i = 0;i<sz;i++){
        if(buf[i]=='\n'){
            buf[i] = ' ';
        }
    }
    printf("Successfully realize the redirection of system.\nThe output is %d bytes long.\nThe output should be:\n%s\n",sz,  buf);
    printf("Reverse the output by words:\n");
    char *reverse[MAX_WORDS_NUM];
	const char *d = "\n ";
	int num=-1;
    splitsentence(buf, d, reverse, &num);
	turn(reverse, num);
    return 0;
}