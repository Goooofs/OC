#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>
#include <linux/time.h>

#define PROCFS_NAME "tsu"
static struct proc_dir_entry *our_proc_file = NULL;

static int days_until_deadline(void) {
    struct timespec64 now;
    struct tm tm_now, tm_deadline;
    long seconds_until_deadline;

    // Получаем текущее время
    ktime_get_real_ts64(&now);

    // Заполняем структуру tm для текущего времени
    time64_to_tm(now.tv_sec, 0, &tm_now);

    // Заполняем структуру tm для дедлайна (Новый год)
    tm_deadline = (struct tm){0}; // Обнуляем остальные поля
    tm_deadline.tm_year = tm_now.tm_year + 1; // Год следующего Нового года
    tm_deadline.tm_mon = 0; 
    tm_deadline.tm_mday = 1; 

    // Получаем количество секунд до дедлайна
    seconds_until_deadline = mktime64(tm_deadline.tm_year + 1900, tm_deadline.tm_mon + 1, tm_deadline.tm_mday,
                                      tm_deadline.tm_hour, tm_deadline.tm_min, tm_deadline.tm_sec) - now.tv_sec;

    // Преобразуем секунды в дни
    return seconds_until_deadline / (24 * 60 * 60);
}

static ssize_t procfile_read(struct file *file, char __user *buffer, size_t count, loff_t *offset)
{
    int ret = 0;
    char s[50];
    size_t len;

    int days = days_until_deadline();
    len = snprintf(s, sizeof(s), "Days until Deadline: %d\n", days);

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

    pr_info("procfile_read %s successfully\n", PROCFS_NAME);
    return ret;
}

static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};

static int __init tsulab_init(void)
{
    our_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);

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