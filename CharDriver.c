#include "header.h"

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
#define CHUNK_SIZE (16 * 1024) // 16KB chunk size

static ssize_t loop_write(struct file* filep, const char __user* buffer, size_t len, loff_t* offset)
{
    char* kernel_buffer;
    unsigned int ret = 0;
    loff_t pos = 0;

    // Allocate a small chunk buffer
    kernel_buffer = kmalloc(CHUNK_SIZE, GFP_KERNEL);
    if (!kernel_buffer) {
        printk(KERN_ERR "loop: Failed to allocate memory\n");
        return -ENOMEM;
    }

    // Open output file if not already opened
    if (!output_file) {
        output_file = filp_open("/tmp/output", O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0777);
        if (IS_ERR(output_file)) {
            printk(KERN_ERR "loop: Failed to open file\n");
            ret = PTR_ERR(output_file);
            output_file = NULL;
            goto out;
        }
    }

    // Copy and process data in chunks
    size_t copied = 0;
    unsigned int i;
    while (copied < len) {
        size_t chunk_size = min(len - copied, CHUNK_SIZE);

        if (copy_from_user(kernel_buffer, buffer + copied, chunk_size)) {
            printk(KERN_ERR "loop: Failed to copy data from user\n");
            ret = -EFAULT;
            goto out;
        }

        // Prepare hex formatting
        i = 0;
        while (i < chunk_size) {
            char hex_buffer[80];
            int offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%07lx,", (unsigned long)(copied + i));

            for (int j = 0; j < min(16, chunk_size - i); j += 2) {
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars,
                    "%02x%02x ", kernel_buffer[i + j + 1], kernel_buffer[i + j]);
            }

            hex_buffer[offset_chars++] = '\n';
            hex_buffer[offset_chars] = '\0';

            // Write to file
            ret = kernel_write(output_file, hex_buffer, strlen(hex_buffer), &output_file->f_pos);
            if (ret < 0) {
                printk(KERN_ERR "loop: Failed to write to file\n");
                goto out;
            }
            i += 16;
        }
        copied += chunk_size;
    }

    ret = len;
out:
    kfree(kernel_buffer);
    return ret;
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