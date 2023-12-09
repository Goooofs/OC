#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/cpu.h>
#include <linux/version.h>

#define procfs_name "tsu"
static struct proc_dir_entry *our_proc_file = NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static ssize_t procfile_read(struct file *file, char __user *buffer, size_t count, loff_t *offset)
#else
static int procfile_read(struct file *file, char *buffer, size_t length, loff_t *offset)
#endif
{
    int ret = 0;
    char s[40] = "2 part 3 laboratory\n";
    size_t len = strlen(s);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
    if (*offset >= len) {
        return 0;
    }

    if (count > len - *offset) {
        count = len - *offset;
    }

    ret = copy_to_user(buffer, s + *offset, count);
    if (ret == 0) {
        *offset += count;
        ret = count;
    }
#else
    if (copy_to_user(buffer, s, strlen(s)) != 0) {
        ret = -EFAULT;
    } else {
        ret = strlen(s);
    }
#endif

    pr_info("procfile_read %s successfully\n", procfs_name);
    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

static int __init tsulab_init(void)
{
    our_proc_file = proc_create(procfs_name, 0644, NULL, &proc_file_fops);

    if (our_proc_file == NULL) {
        pr_err("Failed to create proc file\n");
        return -ENOMEM;
    }

    pr_info("Welcome to Tomsk State University\n");
    return 0;
}

static void __exit tsulab_exit(void)
{
    proc_remove(our_proc_file);
    pr_info("Tomsk State University forever!\n");
}

module_init(tsulab_init);
module_exit(tsulab_exit);

MODULE_LICENSE("GPL");