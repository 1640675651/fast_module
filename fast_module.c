#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include<pthread.h>

#include <asm/io.h>
#include <linux/slab.h>

//#include <asm/kvm_host.h>
#include <kvm/shmem_guest.h>
struct sekvm_shmem_data_struct
{
struct vm_area_struct *vma;
int shmem_size;
struct list_head * mylist;
bool is_active_map;
};



DEFINE_MUTEX(sekvm_shmem_mutex);
static int sekvm_shmem_open(struct inode *inode, struct file *file)
{
	struct sekvm_shmem_data_struct *sekvm_shmem_data = kmalloc(sizeof(struct sekvm_shmem_data_struct),);
	if(mutex_trylock((&sekvm_shmem_mutex)) {
		printk(KERN_ERR "sekvm_shmem is already mapped. Aborting mmap\n");
		file->private_data->is_active_map = false;
		return -EBUSY;
	}
	file->private_data = sekvm_shmem_data;
	file->private_data->is_active_map = true;

	printk(KERN_ERR "sekvm_shmem file opened.\n");
	return 0;
}

static int sekvm_shmem_close(struct inode *inode, struct file *file)
{
	struct mmu_notifier_range range;
	struct mmu_gather tlb;
	if (file->private_data->is_active_map){
		mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, file->private_data->vma, file->private_data->vma->vm_mm,
					file->private_data->vma->vm_start, file->private_data->vma->vm_start + file->private_data->shmem_size);
		tlb_gather_mmu(&tlb, vma->vm_mm, start, range.end);
		unmap_single_vma(&tlb,
			file->private_data->vma,
			file->private_data->vma->vm_start,
			file->private_data->vma->vm_start + file->private_data->size,
			NULL)
		kfree(file->private_data);
		sekvm_shmem_mutex.unlock();
	}
	printk(KERN_ERR "sekvm_shmem file closed.\n");
	return 0;
}

static int sekvm_shmem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	filp->private_data->vma = vma;
	filp->private_data->size = size;
	u64 base_phys = get_shmem_base();
	int ret;
	printk(KERN_ERR "[SeKVM_KM] sekvm_shmem_mmap size = %lu\n", size);
	printk(KERN_ERR "[SeKVM_KM] sekvm_shmem_mmap base = %llu\n", base_phys);
	if(size == 0)
	{
		printk(KERN_ERR "[SeKVM_KM] No shared memory is registered. Aborting mmap\n");
		return -1;
	}
	ret = remap_pfn_range(vma, vma->vm_start, __phys_to_pfn(base_phys), size, vma->vm_page_prot);
	if(ret < 0)
	{
		printk(KERN_ERR "[SeKVM_KM] mmap failed\n");
		return -1;
	}
	printk(KERN_ERR "[SeKVM_KM] sekvm_shmem file mmaped.\n");
	return 0;
}

static struct file_operations fops =
{
	.owner		= THIS_MODULE,
	.open		= sekvm_shmem_open,
	.release	= sekvm_shmem_close,
	.mmap		= sekvm_shmem_mmap,
};

dev_t dev = 0;
static struct class *dev_class;
static struct cdev sekvm_shmem_cdev;


static int __init lkm_example_init(void)
{
	u64 registered_size = get_registered_size();
	printk(KERN_INFO "[SeKVM_KM] registered size = %llu\n", registered_size);
	// We register memory in kernel init so nothing to be done here 
	//printk(KERN_INFO "[SeKVM_Guest] HVC_GET_SHMEM_SIZE = %llu\n", shmem_size);
	//void* base = alloc_shmem_guest(shmem_size);
	//printk(KERN_INFO "[SeKVM_Guest] Read the first byte of the shared memory = %d\n", *(int*)base);

	/* Allocating Major number */
	if ((alloc_chrdev_region(&dev, 0, 1, "sekvm_shmem")) < 0) {
		printk(KERN_INFO "sekvm_shmem_test: Cannot allocate major number.\n");
		return -1;
	}
	printk(KERN_INFO "sekvm_shmem_test: Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));

	/* Creating cdev structure */
	cdev_init(&sekvm_shmem_cdev, &fops);

	/* Adding character device to the system */
	if ((cdev_add(&sekvm_shmem_cdev, dev, 1)) < 0) {
		printk(KERN_INFO "sekvm_shmem_test: Cannot add the device to the system.\n");
		goto r_class;
	}

	/* Creating struct class */
	if ((dev_class = class_create(THIS_MODULE, "sekvm_shmem")) == NULL) {
		printk(KERN_INFO "sekvm_shmem_test: Cannot create the struct class.\n");
		goto r_class;
	}

	/* Creating device */
	if ((device_create(dev_class, NULL, dev, NULL, "sekvm_shmem_device")) == NULL) {
		printk(KERN_INFO "sekvm_shmem_test: Cannot create the Device 1\n");
		goto r_device;
	}

	printk(KERN_INFO "sekvm_shmem_test: installed.\n");
	return 0;

r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev,1);
	return -1;
}

static void __exit lkm_example_exit(void)
{
	printk("fast_module: Bye!");
}

module_init(lkm_example_init);
module_exit(lkm_example_exit);
MODULE_LICENSE("GPL");
