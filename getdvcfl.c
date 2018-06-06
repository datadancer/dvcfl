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
struct ksym{
    unsigned long addr;
    char tag;
    char *name;
    char *module;
    struct ksym * next;
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

struct ksym * read_kallsyms(){
    struct ksym *head;
    struct ksym *cur;
    ssize_t read_size;
    size_t len;
    char * line = NULL;
    char * tmp= NULL;
    FILE * kallsyms = fopen("kallsyms", "r");

    head = (struct ksym *)malloc(sizeof(struct ksym));
    cur = head;

    while((read_size = getline(&line, &len, kallsyms)) != -1 ){
        tmp = strtok(line, " \t\n");
        cur->addr = strtoull(tmp, NULL, 16);
        cur->tag = strtok(NULL, " \t\n")[0];

        tmp = strtok(NULL, " \t\n");
        if(tmp != NULL){
            cur->name = (char *)malloc(strlen(tmp)+1);
            strcpy(cur->name, tmp);
        } else {
            cur->name = NULL;
        }

        tmp = strtok(NULL, " \t\n");
        if(tmp != NULL){
            cur->module = (char *)malloc(strlen(tmp)+1);
            strcpy(cur->module, tmp);
        } else {
            cur->module = NULL;
        }

        cur->next = (struct ksym *)malloc(sizeof(struct ksym));
        cur = cur->next;
    }

    cur->next = NULL;
    cur = head;
 #ifdef DEBUG
    while(cur != NULL){
        printf("%lx %c %s %s\n", cur->addr, cur->tag, cur->name, cur->module);
        cur = cur->next;
    }
#endif
    fclose(kallsyms);
    return head;
}

struct ksym * find_ksym_by_addr(struct ksym *head, unsigned long addr){
    while(head != NULL) if(head->addr == addr) return head; else head=head->next;
    return NULL;
}

int main(int argc, char** argv)  
{  
    int fd = -1;
    struct ksym *head = read_kallsyms();
    //struct ksym *head = NULL;
    struct ksym *cur= head;
    
    fd = open(DVCFL_DEVICE, O_RDWR);  
    if(fd == -1) {  
        printf("Failed to open device %s.\n", DVCFL_DEVICE);  
        return -1;  
    }  
    handle_dir(fd, "/dev");
    unsigned long addr; char tc; char func_name[256]; int i;
    FILE * syms_devs = fopen("syms_devs", "w");
    FILE * device_address = fopen("device_address", "w");

	for(i=0;i<dev_cnt;i++) {
        cur = find_ksym_by_addr(head, devs[i].ptr);
        if(cur != NULL){
            if(cur->module == NULL)
            fprintf(device_address, "%s %s\n", devs[i].fname, cur->name); 
            else
            fprintf(device_address, "%s %s %s\n", devs[i].fname, cur->name, cur->module); 
        }
    }

    if(syms_devs==NULL ) perror("Open kallsyms Error");
    else{
    while(cur!=NULL){
	    for(i=0;i<dev_cnt;i++) {
            if(cur->addr == devs[i].ptr) { 
                fprintf(stdout, "%s %s\n", devs[i].fname, cur->name); 
                fprintf(syms_devs, "%s %s\n", devs[i].fname, cur->name); 
                break;
            }
        }
        cur=cur->next;
	}
    }
    cur = head;
    while(cur!=NULL){free(cur->name); free(cur->module); cur=cur->next;}
    fclose(syms_devs);
    fclose(device_address);
    close(fd);  
    return 0;  
}  
