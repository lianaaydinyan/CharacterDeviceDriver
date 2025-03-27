#include "header.h"

static  char* kernel_buffer;
static loff_t pos;
static int count =0;
static unsigned int total = 0;

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
///static unsigned int i =0;
static ssize_t loop_write(struct file* filep, const char __user* buffer, size_t len, loff_t* offset)
{
    unsigned int ret = 0;
    pos +=  *offset;
   // printk(KERN_INFO "loop_write: Initial offset = %lld\n", pos);
    kernel_buffer = kvmalloc(len + 1, GFP_KERNEL);
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

    unsigned  int padded_len = len;
    if (len % 2 != 0)
    {
        kernel_buffer[len] = 0x00;
        padded_len++;
    }

     // Open output file in write mode (create it if it doesn't exist)
    if (!output_file)
    {
        output_file = filp_open("/tmp/output", O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0777);
        if (IS_ERR(output_file)) 
        {
            printk(KERN_ERR "loop: Failed to open file\n");
            ret = PTR_ERR(output_file);
            output_file = NULL;
            goto out;
        }
    }
    // Prepare hex formatting and write to the output file
    unsigned int i = 0;
    char hex_buffer[80];
    unsigned int line_len = 0;
    unsigned int offset_chars = pos;
    uint8_t j = 0;
    unsigned long int off;
    while (i < padded_len)
    {
        line_len = (padded_len - i > 16) ? 16 : padded_len - i;
        off = i + total;
        offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%07lx ", off);
        for (j = 0; j < line_len; j += 2)
        {
            if (j + 1 < line_len)
            {
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "%02x%02x", kernel_buffer[i + j + 1], kernel_buffer[i + j]);
                if (j + 2 < line_len)
                    offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, " ");
            }
            else
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "00%02x", kernel_buffer[i + j]);
        }
        while (offset_chars < 47)
            hex_buffer[offset_chars++] = ' ';
        hex_buffer[offset_chars] = '\n';
        hex_buffer[offset_chars + 1] = '\0';
        ret = kernel_write(output_file, hex_buffer, strlen(hex_buffer), &pos);
        if (ret < 0)
        {
            printk(KERN_ERR "loop: Failed to write in file\n");
            goto out;
        }
        i += 16;
    }
    snprintf(hex_buffer, sizeof(hex_buffer), "%07x\n", (unsigned int)len + total);
    kernel_write(output_file, hex_buffer, strlen(hex_buffer), &pos);
    *offset = pos;
    printk(KERN_INFO "Last offset %lld\n", pos);
    printk(KERN_INFO "file struct %u\n",filep->f_pos); 
    ret = len;
    count++;
    total += 131072;
    printk(KERN_INFO "how many times %i",count);
out:
    kvfree(kernel_buffer);
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