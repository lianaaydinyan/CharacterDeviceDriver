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
static ssize_t loop_write(struct file* filep, const char __user* buffer, size_t len, loff_t* offset)
{
    char *kernel_buffer;
    ssize_t ret = 0;
    loff_t pos = *offset;
    size_t chunk_len;
    char hex_buffer[256];  

    if (len == 0)
        return 0;  

    if (!output_file)
    {
        output_file = filp_open("/tmp/output", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (IS_ERR(output_file))
        {
            printk(KERN_ERR "loop: Failed to open output file\n");
            return PTR_ERR(output_file);
        }
    }

    // Use vmalloc for large buffers instead of kmalloc
    kernel_buffer = vmalloc(PAGE_SIZE);
    if (!kernel_buffer)
        return -ENOMEM;

    while (len > 0)
    {
        chunk_len = (len > PAGE_SIZE) ? PAGE_SIZE : len;

        if (copy_from_user(kernel_buffer, buffer, chunk_len))
        {
            ret = -EFAULT;
            goto out;
        }

        for (size_t i = 0; i < chunk_len; i += 16)
        {
            int j, line_len = (chunk_len - i >= 16) ? 16 : chunk_len - i;
            int offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%07llx ", (unsigned long long)(pos + i));
            
            for (j = 0; j < line_len; j += 2)
            {
                if (j + 1 < line_len)
                    offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "%02x%02x ", kernel_buffer[i + j + 1], kernel_buffer[i + j]);
                else
                    offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "00%02x ", kernel_buffer[i + j]);
            }

            while (offset_chars < 56)
                hex_buffer[offset_chars++] = ' ';

            hex_buffer[offset_chars++] = '\n';
            hex_buffer[offset_chars] = '\0';

            ssize_t written = kernel_write(output_file, hex_buffer, offset_chars, &pos);
            if (written < 0)
            {
                ret = written;
                goto out;
            }
        }

        pos += chunk_len;
        buffer += chunk_len;
        len -= chunk_len;
    }

    *offset = pos;  // Update offset
    ret = pos - *offset;

out:
    vfree(kernel_buffer); // Free the large buffer properly
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