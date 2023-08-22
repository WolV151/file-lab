
#define MAX_SECTORS 10
struct sdc_File{
	char name[10];
	char sectors[MAX_SECTORS];
        int size;
};

struct Master{
       int no;
       struct sdc_File files[10];
};
struct Master master;




