#include <stdio.h>
#include "filesystem.h"

struct Master master;
char arr[512];
copy(char *p,char *p1, int sz){
for (int i=0;i<sz;i++){
  p[i]=p1[i];
  }
}

int zero(){
	for (int i=0;i<512;i++)
		arr[i]=0;
}

main(int argc,char *argv[]){

   if ( argc < 2)
      return printf("need some file names as arguments\n");
   if (argc > 10)
      return printf("cant handle so many files\n");

   FILE *fp = fopen("sdimage","w");
   zero();

   // write a blank sector initially
   fwrite(arr,1,512,fp);
   master.no=argc-1; // number of files in the file system
   int fileno=0;
   int sector=1;
   int fsize=0;
   int fsector=0;
   int n;
   for (int i=1;i<argc;i++){
      FILE * fp1;
      if (!(fp1=fopen(argv[i],"r")))
         return printf("cannot open %s\n",argv[i]); 
      strncpy(master.files[fileno].name,argv[i],9);
      fsize=0;
      fsector=0;// no of sectors in the file
      for (int j=0;j<MAX_SECTORS;j++){
         zero();
         if (!(n=fread(arr,1,512,fp1)))
            break;
         fwrite(arr,1,512,fp);
         fsize+=n;      // n may be less than 512
         master.files[fileno].sectors[fsector]=sector;
         sector++;
         fsector++;
         }
      master.files[fileno].size=fsize;
      fileno++;
//char * data2 = "Good night";
//master.files[0].size=strlen(data2);
//master.files[0].sectors[0]=2;
      }

   fseek(fp,0l,0);// go back to the first sector
   zero();
   copy(arr,(char *)&master,sizeof(struct Master));
   fwrite(arr,1,512,fp);

   fclose(fp);
}

