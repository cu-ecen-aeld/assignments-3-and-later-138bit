/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>	// kmalloc
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("138bit"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp) {
	PDEBUG("open");
	/**
	 * TODO: handle open
	 */

	// flip->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
	return 0;
}

int aesd_release(struct inode *inode, struct file *filp) {
	PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos) {
	ssize_t retval = 0;
	size_t size = 0;
	/**
	 * TODO: handle read
	 */
	size_t e_pos = 0;
	struct aesd_buffer_entry * entry;
	printk(KERN_ERR "read %zu bytes with offset %lld",count,*f_pos);

	mutex_lock(&aesd_device.lock);
	entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &e_pos);
	if (entry == NULL) {
		retval = -EIO;
		goto out;
	}

	{
		uint8_t index;
		struct aesd_buffer_entry *entry;
		AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
			if (entry->buffptr) {
				printk(KERN_ERR "%d = %s", index, entry->buffptr);
			}
		}
	}

	size = (e_pos > count) ? count : e_pos;

	printk(KERN_ERR "got '%s'\n", entry->buffptr);
	if (copy_to_user(buf, entry->buffptr + *f_pos, size)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += size;
	retval = size;
out:
	mutex_unlock(&aesd_device.lock);
	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
				loff_t *f_pos) {
	ssize_t retval = -ENOMEM;
	size_t len = 0;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */

	// If nothing is written, return directly
	if (count == 0) return 0;

	mutex_lock(&aesd_device.lock);

	// Either create or increase buffer.
	if (aesd_device.work.buffptr == NULL) {
		aesd_device.work.buffptr = kmalloc(sizeof(char) * count + 1, GFP_KERNEL);
		if (! aesd_device.work.buffptr) {
			goto out;
		}
		memset(aesd_device.work.buffptr, 0, count);
		len = 0;
		aesd_device.work.size = count;
	} else {
		char *old = aesd_device.work.buffptr;
		aesd_device.work.buffptr = kmalloc(sizeof(char) * (count + 1 + aesd_device.work.size), GFP_KERNEL);
		if (! aesd_device.work.buffptr) {
			kfree(old);
			goto out;
		}
		memset(aesd_device.work.buffptr, 0, count);
		len = aesd_device.work.size;
		aesd_device.work.size += count;
		kfree(old);
	}

	if (copy_from_user(aesd_device.work.buffptr + len, buf, count)) {
		printk(KERN_ERR "Could not copy from user\n");

		// kfree or not to kfree?
		retval = -EFAULT;
		goto out;
	}

	if ((retval = aesd_circular_buffer_add_entry(&aesd_device.buffer,
		(const struct aesd_buffer_entry *)&aesd_device.work))) {
		goto out;
	}
		kfree(aesd_device.work.buffptr);
		aesd_device.work.buffptr = NULL;
		aesd_device.work.size = 0;

	retval = count;
out:
	mutex_unlock(&aesd_device.lock);
	return retval;
}
struct file_operations aesd_fops = {
	.owner = THIS_MODULE,
	.read = aesd_read,
	.write = aesd_write,
	.open = aesd_open,
	.release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev) {
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}

int aesd_init_module(void) {
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));
	//aesd_device.work.buffptr = NULL;

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */
	mutex_init(&aesd_device.lock);
	aesd_circular_buffer_init(&aesd_device.buffer);

	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
		mutex_destroy(&aesd_device.lock);
	}
	return result;

}

void aesd_cleanup_module(void) {
	uint8_t index;
	struct aesd_circular_buffer buffer;
	struct aesd_buffer_entry *entry;

	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);
	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */
	mutex_lock(&aesd_device.lock);

	AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
		if (entry->buffptr) {
			kfree(entry->buffptr);
			entry->buffptr = NULL;
		}
	}

	if (aesd_device.work.buffptr) {
		kfree(aesd_device.work.buffptr);
		aesd_device.work.buffptr = NULL;
	}

	mutex_unlock(&aesd_device.lock);

	mutex_destroy(&aesd_device.lock);

	unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
