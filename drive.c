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
    char hex_buf[49];
    unsigned int i, pos;
    unsigned int written;

    if (!output_file) {
        output_file = file_open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0777);
        if (!output_file) {
            pr_err("Failed to open output file\n");
            return -EFAULT;
        }
    }

    kbuf = kvmalloc(len, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buf, len)) {
        kfree(kbuf);
        return -EFAULT;
    }
    ////////////////////////////// please loo k at this block and integrate it to my code /////
    unsigned int i = 0;
    uint8_t line_len = (len - i >= 16) ? 16 : len - i;
    int offset_chars = 0;
    uint8_t j = 0;
    while (i < len)
    {
        line_len = (len - i >= 16) ? 16 : len - i;
        offset_chars = snprintf(hex_buf, sizeof(hex_buf), "%07x,", (unsigned int)i);
        for (j = 0; j < line_len; j += 2)
        {
            if (j + 1 < line_len)
            {
                offset_chars += snprintf(hex_buf + offset_chars, sizeof(hex_buf) - offset_chars, "%02x%02x", kernel_buffer[i + j + 1], kernel_buffer[i + j]);
                if (j + 2 < line_len)
                    offset_chars += snprintf(hex_buf + offset_chars, sizeof(hex_buf) - offset_chars, " ");
            }
            else
                offset_chars += snprintf(hex_buf + offset_chars, sizeof(hex_buf) - offset_chars, "00%02x", kernel_buffer[i + j]);
        }
        // Fill spaces until the required column width is reached as hexdump does
        while (offset_chars < ROW_SPACE_HEX)
            hex_buf[offset_chars++] = ' ';
        hex_buf[offset_chars] = '\n';
        hex_buf[offset_chars + 1] = '\0';
        ret = kernel_write(output_file, hex_buf, strlen(hex_buf), &ppos);
        if (ret < 0)
        {
            printk(KERN_ERR "loop: Failed to write in file\n");
            goto out;
        }
        i += 16;
    }
    snprintf(hex_buf, sizeof(hex_buf), "%07lx\n", (unsigned int)len);
    kernel_write(output_file, hex_buf, strlen(hex_buf), &ppos);
    ret = len;
    //////////////////////////////
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