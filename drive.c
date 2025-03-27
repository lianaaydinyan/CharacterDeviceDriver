#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/syscalls.h>

#define DEVICE_NAME "liana"
#define OUTPUT_FILE "/tmp/output"
#define BUFFER_SIZE 4096

static dev_t dev_number;
static struct cdev loop_cdev;
static struct class *loop_class;
static struct mutex loop_mutex;

static struct file *output_file = NULL;

static struct file *file_open(const char *path, int flags, int rights) {
    struct file *filp = filp_open(path, flags, rights);
    if (IS_ERR(filp)) {
        return NULL;
    }
    return filp;
}

static ssize_t loop_write(struct file *file, const char __user *buf, size_t len, loff_t *ppos) {
    unsigned char *kbuf;
    char hex_buf[49]
    unsigned int i, pos;
    size_t written;

    if (!output_file) {
        output_file = file_open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (!output_file) {
            pr_err("Failed to open output file\n");
            return -EFAULT;
        }
    }
    loff_t offset = 4LL * 1024 * 1024 * 1024; // 4GB offset
    loff_t new_pos = vfs_llseek(file, offset, SEEK_SET);
    if (new_pos < 0) {
        pr_err("Seek failed: %lld\n", new_pos);
    } else {
        pr_info("New file position: %lld\n", new_pos);
    }
    kbuf = kvmalloc(len, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buf, len)) {
        kfree(kbuf);
        return -EFAULT;
    }

    for (i = 0; i < len; i += 16) {
    vfs_llseek(file, 0, SEEK_SET);
    memset(hex_buf, 0, sizeof(hex_buf)); 
    vfs_llseek(0);
    for (int j = 0; j < 16 && i + j < len; j++) {
        if (pos >= sizeof(hex_buf) - 3) 
            break;
        pos += snprintf(hex_buf + pos, sizeof(hex_buf) - pos, "%02X ", (unsigned char)kbuf[i + j]);
    }

    if (pos < sizeof(hex_buf) - 2) {  
        hex_buf[pos++] = '\n';
        hex_buf[pos] = '\0';  
    }

    written = kernel_write(output_file, hex_buf, strlen(hex_buf), &output_file->f_pos);
    if (written < 0) {
        pr_err("Error writing to file\n");
        kvfree(kbuf);
        return written;
    }
}

    kvfree(kbuf);
    return len;
}

static int loop_open(struct inode *inode, struct file *file) {
    mutex_lock(&loop_mutex);
    return 0;
}

static int loop_release(struct inode *inode, struct file *file) {
    if (output_file) {
        filp_close(output_file, NULL);
        output_file = NULL;
    }
    mutex_unlock(&loop_mutex);
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = loop_write,
    .open = loop_open,
    .release = loop_release,
};

static int __init loop_init(void) {
    if (alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME) < 0) return -1;
    cdev_init(&loop_cdev, &fops);
    if (cdev_add(&loop_cdev, dev_number, 1) < 0) return -1;
    loop_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(loop_class)) return -1;
    device_create(loop_class, NULL, dev_number, NULL, DEVICE_NAME);
    mutex_init(&loop_mutex);
    pr_info("Loop device driver initialized\n");
    return 0;
}

static void __exit loop_exit(void) {
    device_destroy(loop_class, dev_number);
    class_destroy(loop_class);
    cdev_del(&loop_cdev);
    unregister_chrdev_region(dev_number, 1);
    pr_info("Loop device driver removed\n");
}

module_init(loop_init);
module_exit(loop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Liana Aydinyan");
MODULE_DESCRIPTION("Character Device Loop Driver");