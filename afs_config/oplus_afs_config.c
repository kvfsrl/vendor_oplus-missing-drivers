/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * oplus_afs_config - port of the F5 (marble, kernel 5.10) module
 * "oplus_afs_config.ko" to peridot (kernel 6.1).
 *
 * The stock peridot tree has no AFS config proc nodes at all, so Oplus/Oxygen
 * userland that probes for /proc/oplus_afs_config/afs_enable gets ENOENT and
 * silently disables its adaptive-free-space path.
 *
 * The 5.10 original was a single self-contained file with no module
 * dependencies that, at init, slurped /system_ext/etc/afsConfig.pb into a
 * buffer and exposed it read-only through /proc/oplus_afs_config/.
 * This is a faithful re-implementation of that interface.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define AFS_PROC_DIR		"oplus_afs_config"
#define AFS_PROC_CONFIG		"afs_config"
#define AFS_PROC_ENABLE		"afs_enable"
#define AFS_CONFIG_PATH		"/system_ext/etc/afsConfig.pb"
#define AFS_CONFIG_MAX_SIZE	(1 << 20)	/* 1 MiB sanity cap */

static struct proc_dir_entry *afs_proc_dir;
static char *afs_config_buf;
static size_t afs_config_len;
static bool afs_enable = true;
static DEFINE_MUTEX(afs_config_lock);

static void afs_config_release(void)
{
	mutex_lock(&afs_config_lock);
	kfree(afs_config_buf);
	afs_config_buf = NULL;
	afs_config_len = 0;
	mutex_unlock(&afs_config_lock);
}

/*
 * Load afsConfig.pb if the vendor image actually ships it. Absence is not an
 * error: the proc nodes still get created so userland can probe afs_enable.
 */
static void load_afs_config(void)
{
	struct file *filp;
	struct kvec iov;
	loff_t pos = 0;
	ssize_t got;

	mutex_lock(&afs_config_lock);

	filp = filp_open(AFS_CONFIG_PATH, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_info("no %s, afs_config stays empty\n", AFS_CONFIG_PATH);
		goto out;
	}

	/* one extra byte lets us detect a file that is too large */
	afs_config_buf = kzalloc(AFS_CONFIG_MAX_SIZE + 1, GFP_KERNEL);
	if (!afs_config_buf) {
		pr_warn("no memory for afs_config\n");
		filp_close(filp, NULL);
		goto out;
	}

	iov.iov_base = afs_config_buf;
	iov.iov_len = AFS_CONFIG_MAX_SIZE;

	got = vfs_read(filp, &iov, 0, &pos);
	filp_close(filp, NULL);

	if (got < 0) {
		pr_warn("read %s failed: %zd\n", AFS_CONFIG_PATH, got);
		kfree(afs_config_buf);
		afs_config_buf = NULL;
		afs_config_len = 0;
		goto out;
	}

	afs_config_len = (size_t)got;
	pr_info("%zu bytes (from %s)\n", afs_config_len, AFS_CONFIG_PATH);

out:
	mutex_unlock(&afs_config_lock);
}

static ssize_t afs_config_read(struct file *file, char __user *buf,
			       size_t len, loff_t *ppos)
{
	ssize_t ret;

	mutex_lock(&afs_config_lock);
	ret = simple_read_from_buffer(buf, len, ppos, afs_config_buf,
				      afs_config_len);
	mutex_unlock(&afs_config_lock);

	return ret;
}

static ssize_t afs_enable_read(struct file *file, char __user *buf,
			       size_t len, loff_t *ppos)
{
	return simple_read_from_buffer(buf, len, ppos, &afs_enable,
				       sizeof(afs_enable));
}

static const struct proc_ops afs_config_fops = {
	.proc_read	= afs_config_read,
	.proc_lseek	= default_llseek,
};

static const struct proc_ops afs_enable_fops = {
	.proc_read	= afs_enable_read,
	.proc_lseek	= default_llseek,
};

static int __init oplus_afs_config_init(void)
{
	struct proc_dir_entry *config, *enable;

	load_afs_config();

	afs_proc_dir = proc_mkdir(AFS_PROC_DIR, NULL);
	if (!afs_proc_dir) {
		pr_err("cannot create /proc/%s\n", AFS_PROC_DIR);
		afs_config_release();
		return -ENOMEM;
	}

	config = proc_create(AFS_PROC_CONFIG, 0666, afs_proc_dir,
			     &afs_config_fops);
	enable = proc_create(AFS_PROC_ENABLE, 0666, afs_proc_dir,
			     &afs_enable_fops);

	if (!config || !enable) {
		pr_err("cannot create /proc/%s/{%s,%s}\n", AFS_PROC_DIR,
		       AFS_PROC_CONFIG, AFS_PROC_ENABLE);
		proc_remove(afs_proc_dir);
		afs_proc_dir = NULL;
		afs_config_release();
		return -ENOMEM;
	}

	pr_info("loaded\n");
	return 0;
}

static void __exit oplus_afs_config_exit(void)
{
	remove_proc_subtree(afs_proc_dir);
	afs_proc_dir = NULL;
	afs_config_release();
	pr_info("unloaded\n");
}

module_init(oplus_afs_config_init);
module_exit(oplus_afs_config_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OPLUS AFS config proc nodes");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
