#ifndef _Char_Driver_
 #define _Char_Driver_
 
# include <linux/kernel.h>
# include <linux/init.h>
# include <linux/module.h>
# include <linux/kdev_t.h>
# include <linux/fs.h>
# include <linux/cdev.h>
# include <linux/device.h>
# include <linux/slab.h>
# include <linux/uaccess.h>

# define DEVICE_NAME "liana" // I hade already loop divess 
// # define OUTPUT_FILE_PATH "/tmp/output"
# define BUFFER_SIZE 65536 // for 1G
# define ROW_SPACE_HEX 47
# define MESSAGE_SIZE 1024

#define WRITE_BUFFER_SIZE 4096
struct file             *output_file;
dev_t                   dev_fd = 0;
uint8_t *               kernel_buffer; 
static int              major_number;
static struct class*    loop_class = NULL;
static struct device*   loop_device = NULL;

static int              loop_open(struct inode* inodep, struct file* filep);
static int              loop_release(struct inode* inodep, struct file* filep);
// static ssize_t          loop_write(struct file* filep, const char __user* buffer, size_t len, loff_t* offset);
static ssize_t          loop_write(struct file *pfile, const char __user *buffer, size_t u_len, loff_t *offset);
static ssize_t          loop_read(struct file * filep, char __user * buffer, size_t len, loff_t* offset);

static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = loop_read,
        .write          = loop_write,
        .open           = loop_open,
        .release        = loop_release,
};

struct my_device_d
{
    struct cdev cdev;
    char buffer[BUFFER_SIZE];
};

#define DEVICE_BUFFER_SIZE (1024 * 1024 * 100) // 100MB buffer

static char *device_buffer;
static size_t buffer_size = 0;

#endif