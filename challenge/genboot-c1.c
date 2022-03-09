/*
 *This .c file can expand the origin mbr.bin in any possible size to 512 B
 *with magic munber 0x550xaa at the end of .bin file to meet the requirement
 *of mbr.
 *Meanwhile, if we do some changes to the fseek part, we can handle the .elf 
 *directly, combining objcpy and genboot.pl together. We just need to find the 
 * start address of .text section in the program header, and than move the file
 * seek to the address, and copy the whole .text section according to the size
 * in Elf32_phdr. The following operations are exactly the same. 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char buf[1000] = { '\0' };

int main(int argc, char* argv[], char ** env){
	if(argc < 1){
		fprintf(stderr,"ERROR:The file name is missing!\n");
		exit(-1);
	}
	FILE *fp;
	//fprintf(stdout,"argv[1]:%s\n",argv[1]);
	fp = fopen(argv[1],"r");
	if(!fp){
		fprintf(stderr,"ERROR:The file doesn't exit!\n");
		exit(-1);
	}
	fseek(fp, 0, SEEK_END);
	int filesize;
	filesize = ftell(fp);
	if(filesize > 510){
		fprintf(stderr,"ERROR: boot block too large: %d bytes (max 510)\n",filesize);
		exit(-1);
	}
	fprintf(stdout,"OK: boot block is %d bytes (max 510)\n",filesize);
	fseek(fp,0,SEEK_SET);
	fread(buf,filesize,sizeof(char),fp);
	for(int i = filesize ;i<510;i++)buf[i] = 0;
	buf[510] = 0x55;
	buf[511] = 0xAA;
	fclose(fp);
	fp = fopen(argv[1],"w");
	fwrite(buf,1,512,fp);
	fclose(fp);
	return 0;
}
