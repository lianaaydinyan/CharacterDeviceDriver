#include "header.h"

// Open device
static int loop_open(struct inode *inode, struct file *file)
{
    kernel_buffer = kmalloc(MESSAGE_SIZE, GFP_KERNEL);
    if (!kernel_buffer)
    {
        printk(KERN_ERR "loop: Cannot allocate memory\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "loop: Device file opened\n");
    return 0;
}

// Close device
static int loop_release(struct inode *inode, struct file *file)
{
    kfree(kernel_buffer);
    printk(KERN_INFO "loop: Device file closed\n");
    return 0;
}

// Read data
static ssize_t loop_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    if (copy_to_user(buffer, kernel_buffer, MESSAGE_SIZE))
    {
        printk(KERN_ERR "loop: Failed to copy data to user space\n");
        return -EFAULT;
    }
    printk(KERN_INFO "loop: Data read done\n");
    return MESSAGE_SIZE;
}

static ssize_t loop_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    ssize_t ret = 0;
    loff_t pos;
    size_t i;
    char hex_buffer[48];

    kernel_buffer = kmalloc(len, GFP_KERNEL);
    if (!kernel_buffer)
    {
        printk(KERN_ERR "loop: Failed to allocate memory\n");
        return -ENOMEM;
    }

    if (copy_from_user(kernel_buffer, buffer, len))
    {
        printk(KERN_ERR "loop: Failed to copy data from user\n");
        ret = -EFAULT;
        goto out;
    }

    if (!output_file)
    {
        output_file = filp_open("/tmp/output", O_WRONLY | O_CREAT | O_APPEND | O_LARGEFILE, 0666);
        if (IS_ERR(output_file))
        {
            printk(KERN_ERR "loop: Failed to open file\n");
            ret = PTR_ERR(output_file);
            output_file = NULL;
            goto out;
        }
    }

    // Write hex dump
    pos = output_file->f_pos;
    for (i = 0; i < len; i += 16)
    {
        int line_len = (len - i >= 16) ? 16 : (len - i);
        int offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%08x  ", (unsigned int)i);

        for (int j = 0; j < 16; j++)
        {
            if (j < line_len)
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "%02x ", kernel_buffer[i + j]);
            else
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "   ");
        }

        offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, " |");

        for (int j = 0; j < line_len; j++)
        {
            char c = kernel_buffer[i + j];
            hex_buffer[offset_chars++] = (c >= 32 && c <= 126) ? c : '.';
        }

        snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "|\n");

        ret = kernel_write(output_file, hex_buffer, strlen(hex_buffer), &pos);
        if (ret < 0)
        {
            printk(KERN_ERR "loop: Failed to write in file\n");
            goto out;
        }
    }

    output_file->f_pos = pos;
    ret = len;

out:
    kfree(kernel_buffer);
    return ret;
}

// Module loading
static int __init loop_init(void)
{
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0)
    {
        printk(KERN_ERR "loop: Failed to register major number\n");
        return major_number;
    }

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

// Module unloading
static void __exit loop_exit(void)
{
    if (output_file)
    {
        vfs_fsync(output_file, 0);
        filp_close(output_file, NULL);
    }
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
