#include "header.h"

// static ssize_t loop_write(struct file *pfile, const char __user *buffer, size_t u_len, loff_t *offset)
// {
//     char *kernel_buffer;
//     size_t length;
//     ssize_t ret;

//     /* Allocate kernel memory for u_len + 1 bytes */
//     kernel_buffer = kvmalloc(u_len + 1, GFP_KERNEL);
//     if (!kernel_buffer)
//         return -ENOMEM;
    
//     /* Calculate how many bytes we can write (assuming BUFFER_SIZE is defined) */
//     length = BUFFER_SIZE - (*offset);
//     if (length > u_len)
//         length = u_len;
    
//     /* Copy from user; copy_from_user returns nonzero if any bytes could not be copied */
//     if (copy_from_user(kernel_buffer + *offset, buffer, length) != 0) {
//         kvfree(kernel_buffer);
//         return -EFAULT;
//     }
    
//     printk(KERN_INFO "[*] %s: Device has been written\n", DEVICE_NAME);
//     *offset += length;
//     ret = length;
    
//     kvfree(kernel_buffer);
//     return ret;
// }

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
    long ret = 0;
    loff_t pos = 0;

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

    long padded_len = len;
    if (len % 2 != 0)
    {
        kernel_buffer[len] = 0x00;
        padded_len++;
    }

     // Open output file in write mode (create it if it doesn't exist)
    if (!output_file) {
        output_file = filp_open("/tmp/output", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (IS_ERR(output_file)) {
            printk(KERN_ERR "loop: Failed to open file\n");
            ret = PTR_ERR(output_file);
            output_file = NULL;
            goto out;
        }
    }
    // Prepare hex formatting and write to the output file
    long i = 0;
    char hex_buffer[80];
    while (i < padded_len)
    {
        long line_len = (padded_len - i >= 16) ? 16 : padded_len - i;
	
       // long offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%07x ", (long)i);
       //long offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%32x ", (long)i); //mec arjeqa
        long offset_chars = snprintf(hex_buffer, sizeof(hex_buffer), "%32lx ", (long)i); //mec arjeqa
   
       long int j = 0; 	
        while( j < line_len)
        {
            if (j + 1 < line_len)
            {
                //offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "%02x%02x", kernel_buffer[i + j + 1], kernel_buffer[i + j]);
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "%32lx%32lx", (long)kernel_buffer[i + j + 1],(long)kernel_buffer[i + j]);
                if (j + 2 < line_len)
                    offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, " ");
            }
            else
                //offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "00%02x", kernel_buffer[i + j]);
                offset_chars += snprintf(hex_buffer + offset_chars, sizeof(hex_buffer) - offset_chars, "00%32lx", (long)kernel_buffer[i + j]);
       	 j += 2; 
	}
        // Fill spaces until the required column width is reached as hexdump does
        while (offset_chars < ROW_SPACE_HEX)
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
    //snprintf(hex_buffer, sizeof(hex_buffer), "%07x\n", (unsigned int)len);
    snprintf(hex_buffer, sizeof(hex_buffer), "%32lx\n", (long)len);
    kernel_write(output_file, hex_buffer, strlen(hex_buffer), &pos);
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