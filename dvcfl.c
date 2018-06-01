#include <linux/init.h>  
#include <linux/module.h>  
#include <linux/types.h>  
#include <linux/fs.h>  
#include <linux/proc_fs.h>  
#include <linux/device.h>  

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/fcntl.h>

#include <linux/poll.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/file.h>
 

#include "dvcfl.h"  

/*定义主设备和从设备号变量*/  
static int dvcfl_major = 0;  
static int dvcfl_minor = 0;  

/*设备类别和设备变量*/  
static struct class* dvcfl_class = NULL;  
static struct dvcfl_test_dev* dvcfl_dev = NULL;  

/*传统的设备文件操作方法*/  
static int dvcfl_open(struct inode* inode, struct file* filp);  
static int dvcfl_release(struct inode* inode, struct file* filp);  
static ssize_t dvcfl_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos);  
static ssize_t dvcfl_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos);  
static long dvcfl_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
/*设备文件操作方法表*/  
static struct file_operations dvcfl_fops = {  
    .owner = THIS_MODULE,  
    .open = dvcfl_open,  
    .release = dvcfl_release,  
    .read = dvcfl_read,  
    .write = dvcfl_write,   
    .unlocked_ioctl = dvcfl_ioctl,
};  

/*访问设置属性方法*/  
static ssize_t dvcfl_val_show(struct device* dev, struct device_attribute* attr,  char* buf);  
static ssize_t dvcfl_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);  

/*定义设备属性*/  
static DEVICE_ATTR(val, S_IRUGO | S_IWUSR, dvcfl_val_show, dvcfl_val_store); 

/*打开设备方法*/  
static int dvcfl_open(struct inode* inode, struct file* filp) {  
    struct dvcfl_test_dev* dev;          

    /*将自定义设备结构体保存在文件指针的私有数据域中，以便访问设备时拿来用*/  
    dev = container_of(inode->i_cdev, struct dvcfl_test_dev, dev);  
    filp->private_data = dev;  

    return 0;
}  

static long dvcfl_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    struct fd_ptr fdp;
    struct fd f;
    if(copy_from_user(&fdp, (struct fd_ptr *) arg, sizeof(struct fd_ptr))) return -1;
    f = fdget(fdp.fd);
    if( !f.file) return -1;
    fdp.ptr = (void *)(f.file->f_op->unlocked_ioctl);
    if(copy_to_user((struct fd_ptr *) arg, &fdp, sizeof(struct fd_ptr))) return -1;
    return 0;
}
/*设备文件释放时调用，空实现*/  
static int dvcfl_release(struct inode* inode, struct file* filp) {  
    return 0;  
}  

/*读内存*/  
static ssize_t dvcfl_read(struct file* filp, char __user *buf, size_t count, loff_t* f_pos) {  
    ssize_t err = 0;  
    struct dvcfl_test_dev* dev = filp->private_data;          

    /*同步访问*/  
    if(down_interruptible(&(dev->sem))) {  
        return -ERESTARTSYS;  
    }  

    if(count < sizeof(dev->val)) {  
        goto out;  
    }          

    /*读字符串*/  
    if(copy_to_user(buf, dev->val, sizeof(dev->val))) {  
        err = -EFAULT;  
        goto out;  
    }  

    err = sizeof(dev->val);  

out:  
    up(&(dev->sem));  
    return err;  
}  

/*写字符串*/  
static ssize_t dvcfl_write(struct file* filp, const char __user *buf, size_t count, loff_t* f_pos) {  
    struct dvcfl_test_dev* dev = filp->private_data;  
    ssize_t err = 0;          

    /*同步访问*/  
    if(down_interruptible(&(dev->sem))) {  
        return -ERESTARTSYS;          
    }          

    if(count != sizeof(dev->val)) {  
        goto out;          
    }          

    /*将用户写进来的字符串保存到驱动的内存中*/  
    if(copy_from_user(dev->val, buf, count)) {  
        err = -EFAULT;  
        goto out;  
    }  

    err = sizeof(dev->val);  

out:  
    up(&(dev->sem));  
    return err;  
}  


/*写字符串到内存*/  
static ssize_t __dvcfl_set_val(struct dvcfl_test_dev* dev, const char* buf, size_t count) {       

    /*同步访问*/          
    if(down_interruptible(&(dev->sem))) {                  
        return -ERESTARTSYS;          
    }          
    printk(KERN_ALERT"__dvcfl_set_val.buf: %s  count:%d\n",buf,(int)count);
    printk(KERN_ALERT"__dvcfl_set_val.dev->val: %s  count:%d\n",dev->val,(int)count);
    strncpy(dev->val,buf, count);
    printk(KERN_ALERT"__dvcfl_set_val.dev->val: %s  count:%d\n",dev->val,(int)count);
    up(&(dev->sem));  

    return count;  
}  

/*读取设备属性val*/  
static ssize_t dvcfl_val_show(struct device* dev, struct device_attribute* attr, char* buf) {  
    struct dvcfl_test_dev* hdev = (struct dvcfl_test_dev*)dev_get_drvdata(dev);          
    printk(KERN_ALERT"dvcfl_val_show.\n");
    printk(KERN_ALERT"%s\n",hdev->val);
    return 0;
}  

/*写设备属性val*/  
static ssize_t dvcfl_val_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) {   
    struct dvcfl_test_dev* hdev = (struct dvcfl_test_dev*)dev_get_drvdata(dev);    
    printk(KERN_ALERT"dvcfl_val_store.buf: %s  count:%d\n",buf, (int)count);

    return __dvcfl_set_val(hdev, buf, count);  
}  


/*初始化设备*/  
static int  __dvcfl_setup_dev(struct dvcfl_test_dev* dev) {  
    int err;  
    dev_t devno = MKDEV(dvcfl_major, dvcfl_minor);  

    memset(dev, 0, sizeof(struct dvcfl_test_dev));  

    cdev_init(&(dev->dev), &dvcfl_fops);  
    dev->dev.owner = THIS_MODULE;  
    dev->dev.ops = &dvcfl_fops;          

    /*注册字符设备*/  
    err = cdev_add(&(dev->dev),devno, 1);  
    if(err) {  
        return err;  
    }          

    /*初始化信号量和寄存器val的值*/  
    //init_MUTEX(&(dev->sem));  
    //在 2.6.37 之后的 Linux 内核中, init_mutex 已经被废除了, 新版本使用 sema_init 函数
    sema_init(&(dev->sem), 1);

    dev->val = kmalloc(10,GFP_KERNEL);  
    strncpy(dev->val,"dvcfl",sizeof("dvcfl"));
    return 0;  
}  

/*模块加载方法*/  
static int __init dvcfl_init(void){   
    int err = -1;  
    dev_t dev = 0;  
    struct device* temp = NULL;  

    printk(KERN_ALERT"dvcfl_init.\n");          

    /*动态分配主设备和从设备号*/  
    err = alloc_chrdev_region(&dev, 0, 1, HELLO_DEVICE_NODE_NAME);  
    if(err < 0) {  
        printk(KERN_ALERT"Failed to alloc char dev region.\n");  
        goto fail;  
    }  

    dvcfl_major = MAJOR(dev);  
    dvcfl_minor = MINOR(dev);          

    /*分配helo设备结构体变量*/  
    dvcfl_dev = kmalloc(sizeof(struct dvcfl_test_dev), GFP_KERNEL);  
    if(!dvcfl_dev) {  
        err = -ENOMEM;  
        printk(KERN_ALERT"Failed to alloc dvcfl_dev.\n");  
        goto unregister;  
    }          

    /*初始化设备*/  
    err = __dvcfl_setup_dev(dvcfl_dev);  
    if(err) {  
        printk(KERN_ALERT"Failed to setup dev: %d.\n", err);  
        goto cleanup;  
    }          

    /*在/sys/class/目录下创建设备类别目录dvcfl*/  
    dvcfl_class = class_create(THIS_MODULE, HELLO_DEVICE_CLASS_NAME);  
    if(IS_ERR(dvcfl_class)) {  
        err = PTR_ERR(dvcfl_class);  
        printk(KERN_ALERT"Failed to create dvcfl class.\n");  
        goto destroy_cdev;  
    }          

    /*在/dev/目录和/sys/class/dvcfl目录下分别创建设备文件dvcfl*/  
    temp = device_create(dvcfl_class, NULL, dev, "%s", HELLO_DEVICE_FILE_NAME);  
    if(IS_ERR(temp)) {  
        err = PTR_ERR(temp);  
        printk(KERN_ALERT"Failed to create dvcfl device.");  
        goto destroy_class;  
    }          

    /*在/sys/class/dvcfl/dvcfl目录下创建属性文件val*/  
    err = device_create_file(temp, &dev_attr_val);  
    if(err < 0) {  
        printk(KERN_ALERT"Failed to create attribute val.");                  
        goto destroy_device;  
    }  

    dev_set_drvdata(temp, dvcfl_dev);          

    printk(KERN_ALERT"Succedded to initialize dvcfl device.\n");  
    return 0;  

destroy_device:  
    device_destroy(dvcfl_class, dev);  

destroy_class:  
    class_destroy(dvcfl_class);  

destroy_cdev:  
    cdev_del(&(dvcfl_dev->dev));  

cleanup:  
    kfree(dvcfl_dev);  

unregister:  
    unregister_chrdev_region(MKDEV(dvcfl_major, dvcfl_minor), 1);  

fail:  
    return err;  
}  

/*模块卸载方法*/  
static void __exit dvcfl_exit(void) {  
    dev_t devno = MKDEV(dvcfl_major, dvcfl_minor);  

    printk(KERN_ALERT"dvcfl_exit\n");          

    /*销毁设备类别和设备*/  
    if(dvcfl_class) {  
        device_destroy(dvcfl_class, MKDEV(dvcfl_major, dvcfl_minor));  
        class_destroy(dvcfl_class);  
    }          

    /*删除字符设备和释放设备内存*/  
    if(dvcfl_dev) {  
        cdev_del(&(dvcfl_dev->dev));  
        kfree(dvcfl_dev);  
    }          
    if(dvcfl_dev->val != NULL){
     kfree(dvcfl_dev->val);
    }
    /*释放设备号*/  
    unregister_chrdev_region(devno, 1);  
}  

MODULE_LICENSE("GPL");  
MODULE_DESCRIPTION("Test Driver");  

module_init(dvcfl_init);  
module_exit(dvcfl_exit);  
