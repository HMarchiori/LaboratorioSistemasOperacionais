#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/hrtimer.h>
#include <linux/errno.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/moduleparam.h>

#define TIMEOUT_SEC 1
#define TIMEOUT_NSEC 0
#define PROCFS_NAME "timerdrv"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Estudante");
MODULE_DESCRIPTION("Timer Module");
MODULE_VERSION("0.0.1");

static struct hrtimer hr_timer;
static struct kobject *timerdrv_kobj;
static struct proc_dir_entry *proc_file;

static atomic_t prints = ATOMIC_INIT(0);

static DEFINE_MUTEX(timer_lock);
static bool timer_ready = false;

static s64 timeout_sec = TIMEOUT_SEC;
static unsigned long timeout_nsec = TIMEOUT_NSEC;

static enum hrtimer_restart timer_callback(struct hrtimer *);

static ssize_t timeout_sec_show(struct kobject *, struct kobj_attribute *, char *);
static ssize_t timeout_sec_store(struct kobject *, struct kobj_attribute *, const char *, size_t);

static struct kobj_attribute timeout_sec_attr = __ATTR(
    timeout_sec,
    0660,
    timeout_sec_show,
    timeout_sec_store
);

static ssize_t procfile_read(struct file *, char __user *, size_t, loff_t *);

static struct proc_ops proc_file_fops = {
    .proc_read = procfile_read
};

static void timer_restart(void)
{
    mutex_lock(&timer_lock);
    if (timer_ready)
        hrtimer_start(&hr_timer, ktime_set(timeout_sec, timeout_nsec), HRTIMER_MODE_REL);
    mutex_unlock(&timer_lock);
}

static int timeout_nsec_set(const char *val, const struct kernel_param *kp)
{
    unsigned long value;
    int error = kstrtoul(val, 10, &value);
    if (error)
        return error;
    if (value >= NSEC_PER_SEC || (value == 0 && timeout_sec == 0))
        return -EINVAL;

    timeout_nsec = value;
    pr_info("timeout_nsec set to %lu\n", timeout_nsec);

    timer_restart();
    return 0;
}

static int timeout_nsec_get(char *buffer, const struct kernel_param *kp)
{
    return scnprintf(buffer, PAGE_SIZE, "%lu\n", timeout_nsec);
}

static const struct kernel_param_ops timeout_nsec_ops = {
    .set = timeout_nsec_set,
    .get = timeout_nsec_get
};

module_param_cb(timeout_nsec, &timeout_nsec_ops, &timeout_nsec, 0644);
MODULE_PARM_DESC(timeout_nsec, "Parte em nanossegundos do intervalo do timer");

static int timerdrv_init(void)
{
    pr_info("Inserting the Timer module\n");

    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = timer_callback;

    timerdrv_kobj = kobject_create_and_add("timerdrv", NULL);
    if (timerdrv_kobj == NULL) {
        pr_alert("Failed to create kobject\n");
        return -ENOMEM;
    }

    int error = sysfs_create_file(timerdrv_kobj, &timeout_sec_attr.attr);
    if (error) {
        pr_alert("Failed to create sysfs /sys/timerdrv/timeout_sec\n");
        kobject_put(timerdrv_kobj);
        return error;
    }

    proc_file = proc_create(PROCFS_NAME, 0444, NULL, &proc_file_fops);
    if (proc_file == NULL) {
        pr_alert("Could not initialize /proc/%s\n", PROCFS_NAME);
        sysfs_remove_file(timerdrv_kobj, &timeout_sec_attr.attr);
        kobject_put(timerdrv_kobj);
        return -ENOMEM;
    }

    mutex_lock(&timer_lock);
    timer_ready = true;
    hrtimer_start(&hr_timer, ktime_set(timeout_sec, timeout_nsec), HRTIMER_MODE_REL);
    mutex_unlock(&timer_lock);

    return 0;
}

static void timerdrv_exit(void)
{
    mutex_lock(&timer_lock);
    timer_ready = false;
    mutex_unlock(&timer_lock);

    hrtimer_cancel(&hr_timer);
    proc_remove(proc_file);
    sysfs_remove_file(timerdrv_kobj, &timeout_sec_attr.attr);
    kobject_put(timerdrv_kobj);
    pr_info("Removed the Timer module\n");
}

static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    pr_info("Hello from timer! (%d)\n", atomic_inc_return(&prints));
    ktime_t ktime = ktime_set(timeout_sec, timeout_nsec);
    hrtimer_forward_now(timer, ktime);

    return HRTIMER_RESTART;
}

static ssize_t timeout_sec_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%lld\n", timeout_sec);
}

static ssize_t timeout_sec_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int value;
    if (sscanf(buf, "%d", &value) != 1 || value < 0 || (value == 0 && timeout_nsec == 0))
        return -EINVAL;

    timeout_sec = value;
    pr_info("timeout_sec set to %lld\n", timeout_sec);

    timer_restart();
    return count;
}

static ssize_t procfile_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    char tmpbuf[64];
    int tmplen = snprintf(tmpbuf, sizeof(tmpbuf), "%d\n", atomic_read(&prints));

    return simple_read_from_buffer(buffer, len, offset, tmpbuf, tmplen);
}

module_init(timerdrv_init);
module_exit(timerdrv_exit);
