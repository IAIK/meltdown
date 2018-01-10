#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int show_offset(struct seq_file *m, void *v) {
  seq_printf(m, "0x%zx\n", (size_t)phys_to_virt(0));
  return 0;
}

static int proc_open(struct inode *inode, struct file *file) {
  return single_open(file, show_offset, NULL);
}
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = proc_open,
    .release = single_release,
    .read = seq_read,
    .llseek = seq_lseek,
};

static int __init kaslr_init(void) {
  struct proc_dir_entry *entry;
  printk(KERN_INFO "Module start\n");
  entry = proc_create("direct_physical_map", 0777, NULL, &fops);
  if (!entry) {
    return -1;
  } else {
    printk(KERN_INFO "Create proc file was successful\n");
  }
  return 0;
}

static void __exit kaslr_exit(void) {
  remove_proc_entry("direct_physical_map", NULL);
  printk(KERN_INFO "Module end\n");
}

module_init(kaslr_init);
module_exit(kaslr_exit);
MODULE_LICENSE("GPL");
