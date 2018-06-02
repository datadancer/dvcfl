#include <stdio.h>  
#include <stdlib.h>  
#include <fcntl.h> 
#include <linux/ioctl.h>
#include <dirent.h> 
#include <string.h>

#define DVCFL_DEVICE "/dev/dvcfl"  

struct fd_ptr {
	int fd;
	void *ptr;
};

struct fname_ptr{
	char fname[512];
	unsigned long ptr;
};
static struct fname_ptr devs[1000];
static int dev_cnt = 0;

int handle_dir(int fd, char *s_dir){
    DIR *d;
    struct dirent *dir;
    char fname[512];
    struct fd_ptr fdp;

    d = opendir(s_dir);
    if (d) {
	while( (dir = readdir(d)) != NULL ) {
	    if(dir->d_type == DT_DIR) {
		if(strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
		char * newdir = malloc(strlen(s_dir) + strlen(dir->d_name)+1);
		newdir[0] = '\0';
		strcat(newdir, s_dir); strcat(newdir, "/"); strcat(newdir, dir->d_name);
		handle_dir(fd, newdir);
		free(newdir);
		continue;
	    }
	    fdp.fd = -1; fdp.ptr = NULL;
	    fname[0] = '\0';
	    strcat(fname, s_dir); strcat(fname, "/");
	    strcat(fname, dir->d_name);
        printf("Try to open %s\n", fname);
        //if(strncmp(fname, "/dev/snd/", 8) == 0) continue;
        //if(strncmp(fname, "/dev/tty", 8) == 0) continue;
    	fdp.fd = open(fname, O_RDONLY);
    	if(fdp.fd < 0) { printf("Open %s error.\n", fname); continue;}
    	if (ioctl(fd, 0x40086602, &fdp) == -1) {perror("ioctl");}
    	else {
		    if(fdp.ptr != NULL) {
		        strcpy(devs[dev_cnt].fname, fname); devs[dev_cnt].ptr = (unsigned long)fdp.ptr;
		        printf("%3d: %lx %s\n",dev_cnt, devs[dev_cnt].ptr, devs[dev_cnt].fname);
		        dev_cnt++;
		    }else{
		        printf("no unlocked_ioctl found, fdp.ptr=%p.\n", fdp.ptr); 
            }
	    }
    	close(fdp.fd);
	}
	closedir(d);
    }
}

int main(int argc, char** argv)  
{  
    int fd = -1;
    
    fd = open(DVCFL_DEVICE, O_RDWR);  
    if(fd == -1) {  
        printf("Failed to open device %s.\n", DVCFL_DEVICE);  
        return -1;  
    }  
    handle_dir(fd, "/dev");
    unsigned long addr; char tc; char func_name[256]; int i;
    FILE * kallsyms = fopen("kallsyms", "r");
    FILE * syms_devs = fopen("syms_devs", "w");
    if(kallsyms == NULL || syms_devs==NULL ) perror("Open kallsyms Error");
    else{
	while(fscanf(kallsyms, "%lx %c %s\n", &addr, &tc, func_name) == 3){
	    //if(tc == 'T' || tc == 't') printf("%lx %c %s\n", addr, tc, func_name);
	    for(i=0;i<dev_cnt;i++) {
                if(addr == devs[i].ptr) { 
                        fprintf(stdout, "%s %s\n", devs[i].fname, func_name); 
                        fprintf(syms_devs, "%s %s\n", devs[i].fname, func_name); break;
                }
        }
	}
    }
    fclose(kallsyms);
    fclose(syms_devs);
    
    close(fd);  
    return 0;  
}  
