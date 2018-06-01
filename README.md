# dvcfl
A tool for getting the driver device file and coressponding ioctl function name.
For dynamic kernel driver analysis, it is important to get all the device files and their corresponding ioctl functions. 
This repo contains a kernel module and a user-space program interact with the kernel module to get ioctl functions.
This is module would leak kernel infomation to user-space, it is important that remove the module if you didn't need it.

Wish it be help.
Thanks for visiting.
