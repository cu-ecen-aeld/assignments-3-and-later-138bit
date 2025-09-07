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
#include "aesd_ioctl.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("138bit"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

#define PROG_NAME "aesd-char-driver"

#define DEBUG
#ifdef DEBUG
#define dbg(fmt, ...) \
	printk(KERN_ERR "%s: %s: " fmt " \n", PROG_NAME, __func__, ##__VA_ARGS__)
#else
#define dbg(fmt, ...)
#endif


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
	dbg("read %zu bytes with offset %lld",count,*f_pos);

	// mutex_lock_interruptible?
	if (mutex_lock_interruptible(&aesd_device.lock)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	entry = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_device.buffer, *f_pos, &e_pos);
	if (entry == NULL) {
		//retval = -EIO;
		retval = 0;
		goto out;
	}

#if 0
	{ // print out buffer.
		uint8_t index;
		struct aesd_buffer_entry *entry;
		AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
			if (entry->buffptr) {
				dbg("%d = %s", index, entry->buffptr);
			}
		}
	}
#endif

	size = entry->size - e_pos;
	size = (count > size) ? size : count;

	//size = (e_pos > count) ? count : e_pos;

	//dbg("got '%s' with size %zu (cnt: %zu, e_pos: %zu)\n", entry->buffptr, size, count, e_pos);
	if (copy_to_user(buf, entry->buffptr + e_pos, size)) {
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
	dbg("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */

	// If nothing is written, return directly
	if (count == 0) return 0;

	if (mutex_lock_interruptible(&aesd_device.lock)) {
		retval = -ERESTARTSYS;
		goto out;
	}

	// Either create or increase buffer.
	if (aesd_device.work.buffptr == NULL) {
		aesd_device.work.buffptr = kmalloc(sizeof(char) * count + 1, GFP_KERNEL);
		if (! aesd_device.work.buffptr) {
			goto out;
		}
		memset(aesd_device.work.buffptr, '\0', count + 1);
		len = 0;
		aesd_device.work.size = count + 1;
	} else {
		char *old = aesd_device.work.buffptr;
		size_t total = count + aesd_device.work.size;

		aesd_device.work.buffptr = kmalloc(sizeof(char) * total, GFP_KERNEL);
		if (! aesd_device.work.buffptr) {
			kfree(old);
			goto out;
		}
		memset(aesd_device.work.buffptr, '\0', total);

		strncpy(aesd_device.work.buffptr, old, aesd_device.work.size);

		// because we're messing around with a '\0' on the end of the string.
		// strlen would give this number!
		len = aesd_device.work.size - 1;
		aesd_device.work.size = total;
		kfree(old);
	}

	if (copy_from_user(aesd_device.work.buffptr + len, buf, count)) {
		printk(KERN_ERR "Could not copy from user\n");

		// kfree or not to kfree?
		retval = -EFAULT;
		goto out;
	}
	//dbg("Got string '%s' (len:%zu, count:%zu, size:%zu)\n", buf, len, count, aesd_device.work.size);

	// Remove two because we add 1, and indexes start at 0.
	if (aesd_device.work.buffptr[aesd_device.work.size - 2] == '\n') {
		//dbg("putting string '%s' into circular buffer\n", aesd_device.work.buffptr);
		// remove null terminator:
		aesd_device.work.size -= 1;
		// This function re-uses the pointer, ..
		aesd_circular_buffer_add_entry(&aesd_device.buffer, 
			(const struct aesd_buffer_entry *)&aesd_device.work);
		// .. therefore I don't have to free it.
		aesd_device.work.buffptr = NULL;
		aesd_device.work.size = 0;
	} else {
		//dbg("string '%s' not ready yet..\n", aesd_device.work.buffptr);
	}

	retval = count;
	*f_pos += retval;
out:
	mutex_unlock(&aesd_device.lock);
	return retval;
}

loff_t aesd_lseek(struct file *file, loff_t offset, int whence) {
	uint8_t index;
	struct aesd_buffer_entry *entry;
	loff_t ret = 0;

	if (mutex_lock_interruptible(&aesd_device.lock)) {
		ret = -ERESTARTSYS;
		goto out;
	}

	// I presume I can do this because unused have size 0, and once the buffer is full
	// an "old" entry gets overwritten.
	AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
		ret += entry->size;
	}

	if ((ret = fixed_size_llseek(file, offset, whence, ret)) < 0) {
		goto out;
	}

out:
	mutex_unlock(&aesd_device.lock);
	return ret;
}

/**
 * Adjust the file offset (f_pos) parameter of @param flip based on the location specified by
 * @param write_cmd (the zero referenced command to locate)
 * and @param write_cmd_offset (the zero referenced offset into the command)
 * @return 0 if successful, negative if error occured:
 *	- ERESTARTSYS if mutex could not be obtained
 *	- -EINVAL if write command or write_cmd_offset was out of range
 */

static long aesd_adjust_file_offset(struct file *flip, unsigned int write_cmd, unsigned int write_cmd_offset) {
	// write_cmd is the 0..(AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED-1)
	// write_cmd_offset = char in (2 in "grass" is "a")
	long ret = -EINVAL;
	size_t tmp;
	loff_t off = 0;
	struct aesd_buffer_entry *entry;

	if (mutex_lock_interruptible(&aesd_device.lock)) {
		dbg("mutex_lock_interruptible ERROR");
		ret = -ERESTARTSYS;
		goto out;
	}

	// Check if 'write_cmd' is in bound.
	if (aesd_device.buffer.full) {
		tmp = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
	} else {
		tmp = (aesd_device.buffer.in_offs > aesd_device.buffer.out_offs) ?
			aesd_device.buffer.in_offs - aesd_device.buffer.out_offs :
			AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - aesd_device.buffer.out_offs - aesd_device.buffer.in_offs ;
		//// Ensures "current" is added to the count.
		tmp += 1;
	}
	if (write_cmd >= tmp) {
		dbg("write_cmd >= tmp!!");
		goto out;
	}

	// Check if 'write_cmd_offset' is in bound.
	entry = &aesd_device.buffer.entry[write_cmd];
	if (write_cmd_offset > entry->size) {
		dbg("write_cmd_offset > entry->size");
		goto out;
	}

	// values are OK.. Do something here..
	// between beginning of 'circ.arr' (out_offs) and tmp_cmd, calc size (E(strlen) + write_cmd_offset)
	tmp = aesd_device.buffer.out_offs;
	while(tmp != write_cmd) {
		entry = &aesd_device.buffer.entry[tmp];
		dbg("entry %zu has size %zu", tmp, entry->size);
		off += entry->size;
		INCREASE_OFFS(tmp);
	}

	off += write_cmd_offset;
	dbg("Setting f_pos from %lld to %lld", flip->f_pos, off);

	flip->f_pos = off;
	ret = 0;

	// Set f_pos.
	//if (off != flip->f_op->llseek(flip, off, SEEK_SET)) {
	//	dbg("Offset does not align with given input");
	//	ret = -EIO;
	//	goto out;
	//}
	//return 0;
out:
	mutex_unlock(&aesd_device.lock);
	return ret;
}

long aesd_compact_ioctl(struct file * file, unsigned int command, unsigned long arg) {
	long ret = 0;
	switch(command) {
		case AESDCHAR_IOCSEEKTO: {
			// passes buffer from user space containing two 4 byte values
			/*
				struct aesd_seekto {
					// The zero referenced write command to seek into
					uint32_t write_cmd;
					// The zero referenced offset within the write
					uint32_t write_cmd_offset;
				};
				sctruct aesd_seekto seekto;
				seekto.write_cmd = write_cmd;
				seekto.write_cmd_offset = offset;
				int result_ret = ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto)
			*/
			struct aesd_seekto seekto;
			if (copy_from_user(&seekto, (void __user *)arg, sizeof(seekto))) {
				ret = -EFAULT;
				goto out;
			}

			if (aesd_adjust_file_offset(file, seekto.write_cmd, seekto.write_cmd_offset) < 0) {
				dbg("aesd_adjust_file_offset FAILED  -- %u / %u", seekto.write_cmd, seekto.write_cmd_offset);
				ret = -EINVAL;
				goto out;
			} else {
				dbg("aesd_adjust_file_offset OK -- %u / %u", seekto.write_cmd, seekto.write_cmd_offset);
			}

			break;
		}
		default:
			ret = -EINVAL;
			goto out;
	}
out:
	return ret;
}

struct file_operations aesd_fops = {
	.owner = THIS_MODULE,
	.read = aesd_read,
	.write = aesd_write,
	.open = aesd_open,
	.release = aesd_release,
	.llseek = aesd_lseek,
	//.compact_ioctl = 

	// unlockes assumes sizeof(unsigned long) == sizeof(void *)
	.unlocked_ioctl = aesd_compact_ioctl
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
