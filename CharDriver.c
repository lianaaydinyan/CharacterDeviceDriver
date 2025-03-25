#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/file.h>

#define DEVICE_NAME "loop"
#define CLASS_NAME "loop_class"
#define OUTPUT_FILE "/tmp/output"
#define BUFFER_SIZE_INCREMENT (1024 * 1024)  // 1MB increments for buffer growth
#define HEX_DUMP_LINE_LENGTH 16

static int major_number;
static struct class *loop_class = NULL;
static struct device *loop_device = NULL;
static char *buffer = NULL;
static size_t buffer_size = 0;
static struct mutex loop_mutex;  // Mutex to ensure thread safety for write operations
static struct file *output_file = NULL;  // File descriptor for output file
static loff_t output_file_pos = 0;      // Track position in the output file

// Function to write a line of hex data to the output file
static ssize_t write_hex_line(struct file *file, const unsigned char *data, size_t length)
{
    char hex_buffer[3 * HEX_DUMP_LINE_LENGTH + 1];  // Each byte = 2 chars, 16 bytes per line
    char ascii_buffer[HEX_DUMP_LINE_LENGTH + 1];   // ASCII representation of 16 bytes
    size_t i;

    for (i = 0; i < HEX_DUMP_LINE_LENGTH; i++) {
        if (i < length) {
            // Hexadecimal representation
            snprintf(hex_buffer + (i * 3), 4, "%02x ", data[i]);
            // ASCII representation
            ascii_buffer[i] = (data[i] >= 32 && data[i] <= 126) ? data[i] : '.';
        } else {
            // Padding with spaces for rows with less than 16 bytes
            snprintf(hex_buffer + (i * 3), 4, "   ");
            ascii_buffer[i] = ' ';
        }
    }

    ascii_buffer[HEX_DUMP_LINE_LENGTH] = '\0';
    hex_buffer[3 * HEX_DUMP_LINE_LENGTH] = '|';
    hex_buffer[3 * HEX_DUMP_LINE_LENGTH + 1] = ' ';
    hex_buffer[3 * HEX_DUMP_LINE_LENGTH + 2] = '\0';

    // Combine the hexadecimal line and ASCII representation
    strncat(hex_buffer, ascii_buffer, HEX_DUMP_LINE_LENGTH + 1);
    
    // Write the formatted hex line to the file
    if (kernel_write(file, hex_buffer, strlen(hex_buffer), &file->f_pos) < 0)
        return -EIO;

    return strlen(hex_buffer);
}

static int loop_open(struct inode *inode, struct file *file)
{
    pr_info("loop: device opened\n");
    return 0;
}

static int loop_release(struct inode *inode, struct file *file)
{
    pr_info("loop: device closed\n");
    return 0;
}

static ssize_t loop_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset)
{
    size_t i;
    ssize_t result = 0;

    // Locking to ensure thread safety during write
    mutex_lock(&loop_mutex);

    // Ensure there is enough space in the buffer (expand if necessary)
    if (buffer_size + len > buffer_size + BUFFER_SIZE_INCREMENT) {
        size_t new_size = buffer_size + len > buffer_size + BUFFER_SIZE_INCREMENT ? buffer_size + len : buffer_size + BUFFER_SIZE_INCREMENT;
        char *new_buffer = krealloc(buffer, new_size, GFP_KERNEL);
        if (!new_buffer) {
            pr_err("loop: failed to allocate memory for buffer\n");
            mutex_unlock(&loop_mutex);
            return -ENOMEM;
        }
        buffer = new_buffer;
    }

    // Copy data from user space to kernel space
    if (copy_from_user(buffer + buffer_size, user_buffer, len)) {
        pr_err("loop: failed to copy data from user\n");
        mutex_unlock(&loop_mutex);
        return -EFAULT;
    }

    buffer_size += len;

    // Open the output file for writing hex dump, if it's not already open
    if (!output_file) {
        output_file = filp_open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (IS_ERR(output_file)) {
            pr_err("loop: failed to open output file\n");
            mutex_unlock(&loop_mutex);
            return PTR_ERR(output_file);
        }
        output_file_pos = output_file->f_pos;
    }

    // Write buffer to output file in hex dump format, 16 bytes per line
    for (i = 0; i < buffer_size; i += HEX_DUMP_LINE_LENGTH) {
        size_t remaining_bytes = min(HEX_DUMP_LINE_LENGTH, buffer_size - i);
        result = write_hex_line(output_file, buffer + i, remaining_bytes);
        if (result < 0) {
            pr_err("loop: failed to write to output file\n");
            filp_close(output_file, NULL);
            mutex_unlock(&loop_mutex);
            return result;
        }
    }

    // Ensure that data is immediately written to disk
    if (output_file) {
        filp_flush(output_file, 1);
    }

    pr_info("loop: data written to /tmp/output\n");

    mutex_unlock(&loop_mutex);
    return len;
}

static const struct file_operations fops = {
    .open = loop_open,
    .release = loop_release,
    .write = loop_write,
};

static int __init loop_init(void)
{
    pr_info("loop: initializing driver\n");

    // Register the character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("loop: failed to register a major number\n");
        return major_number;
    }

    // Create device class
    loop_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(loop_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        pr_err("loop: failed to register device class\n");
        return PTR_ERR(loop_class);
    }

    // Create device node
    loop_device = device_create(loop_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(loop_device)) {
        class_destroy(loop_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        pr_err("loop: failed to create the device\n");
        return PTR_ERR(loop_device);
    }

    // Initialize mutex
    mutex_init(&loop_mutex);

    pr_info("loop: device created successfully\n");
    return 0;
}

static void __exit loop_exit(void)
{
    pr_info("loop: unloading driver\n");

    // Clean up and remove device
    device_destroy(loop_class, MKDEV(major_number, 0));
    class_unregister(loop_class);
    class_destroy(loop_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    // Free allocated resources
    if (output_file) {
        filp_close(output_file, NULL);
    }
    if (buffer) {
        kfree(buffer);
    }

    pr_info("loop: driver unloaded\n");
}

module_init(loop_init);
module_exit(loop_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A professional Linux loopback device driver that handles large files and writes hex output");