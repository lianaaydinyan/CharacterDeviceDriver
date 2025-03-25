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
    struct file *out_file;
    char *local_buf;
    char *hex_line;
    int n;

    out_file = filp_open("/tmp/output", O_WRONLY | O_CREAT | O_APPEND, 0777);
    if (IS_ERR(out_file)) {
        printk(KERN_ERR "loop: Failed to open output file, error: %ld\n", PTR_ERR(out_file));
        return PTR_ERR(out_file);
    }

    // Allocate memory for the buffer dynamically (to handle large writes)
    local_buf = kmalloc(u_len, GFP_KERNEL);
    if (!local_buf) {
        printk(KERN_ERR "loop: Failed to allocate memory\n");
        total = -ENOMEM;
        goto out_close;
    }

    // Copy data from user-space in one go
    if (copy_from_user(local_buf, buffer, u_len)) {
        printk(KERN_ERR "loop: copy_from_user failed\n");
        total = -EFAULT;
        goto out_free;
    }

    // Allocate memory for hex_line output
    hex_line = kmalloc(3 * u_len + 20, GFP_KERNEL);  // Each byte needs 3 chars (XX ), plus offset info
    if (!hex_line) {
        printk(KERN_ERR "loop: Failed to allocate memory for hex output\n");
        total = -ENOMEM;
        goto out_free;
    }

    // Convert to hexdump format
    n = snprintf(hex_line, 20, "%07zx: ", (size_t)*offset);
    for (size_t j = 0; j < u_len; j++)
        n += snprintf(hex_line + n, 3, "%02x ", (unsigned char)local_buf[j]);
    snprintf(hex_line + n, 2, "\n");

    // Write the hex dump to file
    if (kernel_write(out_file, hex_line, strlen(hex_line), &out_file->f_pos) < 0) {
        printk(KERN_ERR "loop: kernel_write failed at file offset %lld\n", out_file->f_pos);
        total = -EIO;
        goto out_free_hex;
    }

    total = u_len; // Successfully processed all bytes
    *offset += u_len;

out_free_hex:
    kfree(hex_line);
out_free:
    kfree(local_buf);
out_close:
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