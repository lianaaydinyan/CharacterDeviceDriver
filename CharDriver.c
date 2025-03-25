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
#define CHUNK 16 

static ssize_t loop_write(struct file *pfile, const char __user *buffer, size_t u_len, loff_t *offset)
{
    ssize_t total = 0;
    loff_t pos = 0;
    struct file *out_file;
    char local_buf[CHUNK];
    char hex_line[128];
    size_t i, chunk;
    int n;

    /* Open output file in append mode */
    out_file = filp_open("/tmp/output", O_WRONLY | O_CREAT | O_APPEND, 0777);
    if (IS_ERR(out_file)) {
        printk(KERN_ERR "loop: Failed to open output file\n");
        return PTR_ERR(out_file);
    }

    for (i = 0; i < u_len; i += CHUNK) {
        chunk = min(u_len - i, (size_t)CHUNK);
        
        /* Copy a chunk from user space into a small local buffer */
        if (copy_from_user(local_buf, buffer + i, chunk) != 0) {
            printk(KERN_ERR "loop: copy_from_user failed at offset %zu\n", i);
            total = -EFAULT;
            goto out;
        }
        
        /* Create a hex dump line. Format: "0000000: 41 42 43 ...\n" */
        n = snprintf(hex_line, sizeof(hex_line), "%07zx: ", i);
        if (n < 0 || n >= sizeof(hex_line)) {
            printk(KERN_ERR "loop: snprintf error for offset formatting\n");
            total = -EFAULT;
            goto out;
        }
        
        for (size_t j = 0; j < chunk; j++) {
            n += snprintf(hex_line + n, sizeof(hex_line) - n, "%02x ", (unsigned char)local_buf[j]);
            if (n < 0 || n >= sizeof(hex_line)) {
                printk(KERN_ERR "loop: snprintf error in hex conversion\n");
                total = -EFAULT;
                goto out;
            }
        }
        n += snprintf(hex_line + n, sizeof(hex_line) - n, "\n");
        
        /* Write the hex line to the output file */
        if (kernel_write(out_file, hex_line, n, &pos) < 0) {
            printk(KERN_ERR "loop: kernel_write failed at pos %lld\n", pos);
            total = -EIO;
            goto out;
        }
        total += chunk;
    }

out:
    filp_close(out_file, NULL);
    return total;
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