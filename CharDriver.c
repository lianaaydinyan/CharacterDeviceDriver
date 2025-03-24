#include "header.h"

static int loop_open(struct inode *inode, struct file * file)
{
        if (0 == (kernel_buffer = kmalloc(MESSAGE_SIZE, GFP_KERNEL)))
        {
                printk(KERN_INFO "Cannot allocate memory\n");
                return -1;
        }
        printk(KERN_INFO "Device file opened \n");
        return 0;
}

static int loop_release(struct inode * inode, struct file * file)
{
        kfree(kernel_buffer); 
        printk(KERN_INFO "Device file closed\n");
        return 0;
}

static ssize_t loop_read(struct file * filep, char __user * buffer, size_t len, loff_t* offset)
{
        if (copy_to_user(buffer, kernel_buffer, MESSAGE_SIZE)){
                printk(KERN_ERR "Failed to copy data to user space");
                return -EFAULT;
        }
        printk(KERN_INFO "Data Read Done \n");
        return MESSAGE_SIZE;
}

static ssize_t loop_write(struct file* filep, const char __user* buffer, size_t len, loff_t* offset)
{
    char* kernel_buffer;
    struct file* output_file;
    loff_t pos = 0;
    ssize_t ret = len;

    kernel_buffer = kmalloc(len + 1, GFP_KERNEL);
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

    kernel_buffer[len] = 0x00; // Null-terminate for safety

    // Open output file
    output_file = filp_open("/tmp/output", O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (IS_ERR(output_file)) {
        printk(KERN_ERR "loop: Failed to open file\n");
        ret = PTR_ERR(output_file);
        goto out;
    }

    char hex_buffer[80];
    for (size_t i = 0; i < len; i += 16) {
        size_t line_len = (len - i >= 16) ? 16 : len - i;
        size_t offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%07zx ", i); // Fixed offset format

        for (size_t j = 0; j < line_len; j += 2) {
            if (j + 1 < line_len)
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "%02x%02x ", kernel_buffer[i + j], kernel_buffer[i + j + 1]);
            else
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "00%02x ", kernel_buffer[i + j]);
        }

        // Write to file
        kernel_write(output_file, hex_buffer, strlen(hex_buffer), &pos);
    }

    filp_close(output_file, NULL); // Close file after writing

out:
    kfree(kernel_buffer); // Free allocated buffer
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
     // Create the device node in /dev
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