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

static void write_hex_dump(struct file *output_file, char *kernel_buffer, size_t padded_len, loff_t *pos)
{
    char hex_buffer[48]; // Reduced size to avoid large stack allocations
    size_t i = 0;
    ssize_t write_ret;

    while (i < padded_len)
    {
        int offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%07x ", (unsigned int)i);
        for (int j = 0; j < 16 && i + j < padded_len; j += 2)
        {
            if (i + j + 1 < padded_len)
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "%02x%02x ", 
                                         kernel_buffer[i + j + 1], kernel_buffer[i + j]);
            else
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "00%02x ", kernel_buffer[i + j]);
        }

        hex_buffer[offset_chars++] = '\n';
        hex_buffer[offset_chars] = '\0';

        write_ret = kernel_write(output_file, hex_buffer, offset_chars, pos);
        if (write_ret < 0)
        {
            printk(KERN_ERR "loop: Failed to write to file\n");
            return;
        }
        i += 16;
    }

    // Final offset line
    snprintf(hex_buffer, sizeof(hex_buffer), "%07x\n", (unsigned int)padded_len);
    kernel_write(output_file, hex_buffer, strlen(hex_buffer), pos);
}


static ssize_t loop_write(struct file *pfile, const char __user *buffer, size_t u_len, loff_t *offset)
{
    struct my_device_d *my_device = (struct my_device_d *)pfile->private_data;
    ssize_t len = BUFFER_SIZE - *offset;
    if (len <= 0)
        return 0;
    if (len >= u_len)
        len = u_len;
    memset(my_device->buffer, '\0', BUFFER_SIZE);
    if (copy_from_user(my_device->buffer + *offset, buffer, len))
        return -EFAULT;
    printk(KERN_INFO "[*] %s: Device has been written\n", DEVICE_NAME);
    *offset += len;
    return len;
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