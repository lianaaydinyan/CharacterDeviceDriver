#include "header.h"

static unsigned char* kernel_buffer;


static int loop_open(struct inode *inode, struct file * file)
{

    printk(KERN_INFO "Device file opened \n");
    return 0;
}

static int loop_release(struct inode * inode, struct file * file)
{
        printk(KERN_INFO "Device file closed\n");
        return 0;
}

static ssize_t loop_read(struct file * filep, char __user * buffer, size_t len, loff_t* offset)
{
    printk(KERN_INFO "Data Read Done \n");
    return MESSAGE_SIZE;
}

define BUFFER_SIZE 4096  // Buffer size for efficient large file handling

static ssize_t loop_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    char *kbuf;
    char *hexbuf;
    size_t i, j, chunk_size;
    int bytes_written = 0;

    if (len == 0) return 0;

    kbuf = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    hexbuf = kmalloc(BUFFER_SIZE * 3 + 1, GFP_KERNEL);  // 1 byte -> 3 characters (XX )

    if (!kbuf || !hexbuf) {
        kfree(kbuf);
        kfree(hexbuf);
        return -ENOMEM;
    }

    while (len > 0) {
        chunk_size = min(len, BUFFER_SIZE);

        if (copy_from_user(kbuf, buffer, chunk_size)) {
            kfree(kbuf);
            kfree(hexbuf);
            return -EFAULT;
        }

        // Convert to HEX format
        for (i = 0, j = 0; i < chunk_size; i++) {
            sprintf(&hexbuf[j], "%02x ", kbuf[i]);
            j += 3;
            if ((i + 1) % 16 == 0) hexbuf[j++] = '\n'; // New line every 16 bytes
        }
        hexbuf[j] = '\0';

        // Write to the file in a single kernel_write() call
        kernel_write(output_file, hexbuf, j, &output_file->f_pos);
        buffer += chunk_size;
        len -= chunk_size;
        bytes_written += chunk_size;
    }

    kfree(kbuf);
    kfree(hexbuf);
    return bytes_written;
}
// module loading
static int __init loop_init(void)
{
    // Register the character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0)
    {
        printk(KERN_ERR "loop: Failed to register a major number\n");
        return major_number;
    }
    // Create the device class
    loop_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(loop_class))
    {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "loop: Failed to register device class\n");
        return PTR_ERR(loop_class);
    }
    loop_device = device_create(loop_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(loop_device))
    {
        class_destroy(loop_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "loop: Failed to create the device\n");
        return PTR_ERR(loop_device);
    }

    printk(KERN_INFO "loop: Device initialized successfully\n");
    return 0;
}

//moduel unloading
static void __exit loop_exit(void)
{
    if (output_file)
        filp_close(output_file, NULL);
    device_destroy(loop_class, MKDEV(major_number, 0));
    class_unregister(loop_class);
    class_destroy(loop_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "loop: Device exited successfully\n");
}

module_init(loop_init);
module_exit(loop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liaydiny");