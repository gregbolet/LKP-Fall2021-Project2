
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/hashtable.h>
#include <linux/types.h>
#include <linux/rbtree.h>
#include <linux/radix-tree.h>

/* This is where we will write our characters to
 * Don't want to deal with VLAs or heavy string manipulation
 * We assume that the input doesn't surpass 256 characters 
 */
#define PRINT_BUFF_SIZE 256
static char print_buff[PRINT_BUFF_SIZE];
static int print_buff_head = 0;

static char *int_str;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gregory Bolet");
MODULE_DESCRIPTION("LKP Project 2");

module_param(int_str, charp, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(int_str, "A comma-separated list of integers");

/////////////////// LINKED LIST BEGIN ///////////////////////////
static LIST_HEAD(mylist);

struct entry {
	int val;
	struct list_head list;
};
/////////////////// LINKED LIST END ///////////////////////////

/////////////////// HASH TABLE BEGIN ///////////////////////////
/*
 * Let's make our hash table have 2^10 = 1024 bins
 * */
#define MY_HASH_TABLE_BINS 10
static DEFINE_HASHTABLE(myhtable, MY_HASH_TABLE_BINS);


/* Hashtable entry struct */
struct hentry {
	int val;
	struct hlist_node hash;
};
/////////////////// HASH TABLE END ///////////////////////////

/////////////////// RBTree BEGIN ///////////////////////////

// Define an RBTree
static struct rb_root myrbtree = RB_ROOT;


// Define an rbtree entry
struct rbentry {
	int val;
	struct rb_node rbnode;
};

/////////////////// RBTree END ///////////////////////////

/////////////////// RADIX TREE BEGIN///////////////////////////

static RADIX_TREE(myradtree, GFP_KERNEL);

struct radentry{
	int val;
};

/////////////////// RADIX TREE END ///////////////////////////

/////////////////// XARRAY BEGIN///////////////////////////

static DEFINE_XARRAY(myxarray);

struct xarrentry{
	int val;
};

/////////////////// XARRAY END ///////////////////////////

static int store_value_xarray(int val){

	// Allocate the new struct
	struct xarrentry* new_node = kmalloc(sizeof(struct xarrentry), GFP_KERNEL);
	// Handle error of failed kmalloc
	if(!new_node){ return -ENOMEM; }
	new_node->val = val;			

	xa_store(&myxarray, val, new_node, GFP_KERNEL);

	return 0;
}

static int store_value_radtree(int val){

	// Allocate the new struct
	struct radentry* new_node = kmalloc(sizeof(struct radentry), GFP_KERNEL);
	// Handle error of failed kmalloc
	if(!new_node){ return -ENOMEM; }
	new_node->val = val;			

	radix_tree_preload(GFP_KERNEL);

	// We make our key the value passed in
	// And we make the value the new_node
	radix_tree_insert(&myradtree, val, new_node);

	radix_tree_preload_end();

	return 0;
}

// Made this function by following the directions from 
// https://www.kernel.org/doc/Documentation/rbtree.txt
static int store_value_rbtree(int val){

	struct rb_node **new;
	struct rb_node *parent;
	struct rb_root *root;

	// Allocate the new struct
	struct rbentry* new_node = kmalloc(sizeof(struct rbentry), GFP_KERNEL);
	// Handle error of failed kmalloc
	if(!new_node){ return -ENOMEM; }
	new_node->val = val;			

	// Get the root of our tree
	root = &myrbtree;
	// We will insert the new node at this location
	new = &(root->rb_node);
	// Keep track of the parent
	parent = NULL;

	// Figure out where to put the new node
	while(*new ){
		struct rbentry* this = container_of(*new, struct rbentry, rbnode);
		parent = *new;
		// If it's already in the tree, don't add it
		if(this->val == val){
			kfree(new_node);
			// Let the user know it's an already-exists error
			return EEXIST;
		}

		// If it's less than the current value
		// go left
		else if(this->val < val){
			new = &((*new)->rb_left);
		}
		// If it's greater than the current value
		// go right
		else{
			new = &((*new)->rb_right);
		}
	}

	// Add the new node and trigger a rebalance
	rb_link_node(&new_node->rbnode, parent, new);
	rb_insert_color(&new_node->rbnode, root);
	return 0;
}

static int store_value_hash_table(int val){
	
	// Allocate the new struct
	struct hentry* new_node = kmalloc(sizeof(struct hentry), GFP_KERNEL);

	// Handle error of failed kmalloc
	if(!new_node){ return -ENOMEM; }
	
	new_node->val = val;			
	hash_add(myhtable, &new_node->hash, (long)val);

	return 0;
}


static int store_value_linked_list(int val)
{
	// Allocate the new struct
	struct entry* new_node = kmalloc(sizeof(struct entry), GFP_KERNEL);

	// Handle error of failed kmalloc
	if(!new_node){ return -ENOMEM; }

	// Add the new value
	new_node->val = val;

	// Add the entry before the specified head
	// thus making it the new tail element
	list_add_tail(&(new_node->list), &mylist);
	return 0;
}

static int store_value(int val)
{
	int retval;
	retval = store_value_linked_list(val);
	if(retval){ return retval; }

	retval = store_value_hash_table(val);
	if(retval){ return retval; }

	retval = store_value_rbtree(val);
	if(retval){ return retval; }

	retval = store_value_radtree(val);
	if(retval){ return retval; }

	retval = store_value_xarray(val);
	if(retval){ return retval; }

	return 0;
}

static void add_val_to_print_buff(int val){
	
	int printed;
	printed = sprintf(print_buff+print_buff_head, "%d, ", val);
	print_buff_head += printed;

	return;
}

static void clear_print_buff(void){
	print_buff[0] = '\0';
	print_buff_head = 0;
}

static void test_linked_list(void)
{
	// Pointer used to hold the current item 
	// while iterating
	struct entry *current_elem;
	clear_print_buff();

	strcpy(print_buff, "Linked List: \0");
	print_buff_head = 13;

	// Iterate over the list
	list_for_each_entry(current_elem, &mylist, list) {
		//printk(KERN_INFO "Item: '%d'\n", current_elem->val);
		
		add_val_to_print_buff(current_elem->val);
	}

	// Let's get rid of the last comma and space
	if(print_buff_head >= 2){
		print_buff[print_buff_head-2] = '\0';
	}

}

static void test_hash_table(void)
{
	// Pointer used to hold the current item 
	// while iterating
	struct hentry *current_elem;
	unsigned bkt;
	clear_print_buff();

	strcpy(print_buff, "Hash Table: \0");
	print_buff_head = 12;

	// Iterate over the table
	hash_for_each(myhtable, bkt, current_elem, hash) {
		add_val_to_print_buff(current_elem->val);
	}

	// Let's get rid of the last comma and space
	if(print_buff_head >= 2){
		print_buff[print_buff_head-2] = '\0';
	}

}

static void test_rbtree(void){
	
	struct rb_node* node;

	clear_print_buff();
	strcpy(print_buff, "Red-Black Tree: \0");
	print_buff_head = 16;

	for(node = rb_first(&myrbtree); node; node=rb_next(node)){
		struct rbentry* curr = rb_entry(node, struct rbentry, rbnode);

		add_val_to_print_buff(curr->val);
	}	

	// Let's get rid of the last comma and space
	if(print_buff_head >= 2){
		print_buff[print_buff_head-2] = '\0';
	}

}

static void test_radtree(void){
	struct radix_tree_iter iter;
	void **slot;

	clear_print_buff();
	strcpy(print_buff, "Radix Tree: \0");
	print_buff_head = 12;
	
	radix_tree_for_each_slot(slot, &myradtree, &iter, 0){
		struct radentry* curr = *slot;
		if(curr){
			add_val_to_print_buff(curr->val);
		}
	}

	// Let's get rid of the last comma and space
	if(print_buff_head >= 2){
		print_buff[print_buff_head-2] = '\0';
	}

}

static void test_xarray(void){

	unsigned long index;
	struct xarrentry* entry;

	clear_print_buff();
	strcpy(print_buff, "XArray: \0");
	print_buff_head = 8;


	xa_for_each(&myxarray, index, entry){
		if(entry){
			add_val_to_print_buff(entry->val);
		}
	}

	
	// Let's get rid of the last comma and space
	if(print_buff_head >= 2){
		print_buff[print_buff_head-2] = '\0';
	}
}


static void destroy_linked_list_and_free(void)
{
	// Pointer used to hold the current item 
	// while iterating
	struct entry *current_elem, *next;
	
	// Iterate and remove elements from the list
	list_for_each_entry_safe(current_elem, next, &mylist, list){
		list_del(&current_elem->list);
		kfree(current_elem);
	}
}

static void destroy_hash_table_and_free(void){

	struct hentry *current_elem;
	unsigned bkt;

	// Iterate over the table
	hash_for_each(myhtable, bkt, current_elem, hash) {
		hash_del(&current_elem->hash);
		kfree(current_elem);
	}
}

static void destroy_rbtree_and_free(void){
	
	struct rb_node* node;
	struct rb_node* next;

	// Get the next item in the tree so we have a
	// starting point for the next iteration
	for(node = rb_first(&myrbtree); node; node = next){
		struct rbentry* curr = rb_entry(node, struct rbentry, rbnode);
		next = rb_next(node);	
		if(curr){
			rb_erase(&curr->rbnode, &myrbtree);
			kfree(curr);
		}	
	}	
}

static void destroy_radtree_and_free(void){

	struct radix_tree_iter iter;
	void **slot;

	radix_tree_for_each_slot(slot, &myradtree, &iter, 0){
		struct radentry* curr = *slot;
		if(curr){
			radix_tree_delete(&myradtree, curr->val);
			kfree(curr);
		}
	}
}

static void destroy_xarray_and_free(void){

	unsigned long index;
	struct xarrentry* entry;

	xa_for_each(&myxarray, index, entry){
		if(entry){
			xa_erase(&myxarray, entry->val);
			kfree(entry);
		}
	}
	
}

static int parse_params(void)
{
	int val, err = 0;
	char *p, *orig, *params;

	params = kstrdup(int_str, GFP_KERNEL);
	if (!params)
		return -ENOMEM;
	orig = params;

	while ((p = strsep(&params, ",")) != NULL) {
		if (!*p)
			continue;

		err = kstrtoint(p, 0, &val);
		if (err)
			break;

		err = store_value(val);
		if (err)
			break;
	}

	kfree(orig);
	return err;
}

static void run_tests(void)
{
	test_linked_list();
	printk(KERN_INFO "%s\n", print_buff);
	test_hash_table();
	printk(KERN_INFO "%s\n", print_buff);
	test_rbtree();
	printk(KERN_INFO "%s\n", print_buff);
	test_radtree();
	printk(KERN_INFO "%s\n", print_buff);
	test_xarray();
	printk(KERN_INFO "%s\n", print_buff);
}

static void cleanup(void)
{
	destroy_linked_list_and_free();
	destroy_hash_table_and_free();
	destroy_rbtree_and_free();
	destroy_radtree_and_free();
	destroy_xarray_and_free();
}

static int proj_proc_show(struct seq_file *m, void *v) {
	test_linked_list();
	seq_printf(m, "%s\n", print_buff);
	test_hash_table();
	seq_printf(m, "%s\n", print_buff);
	test_rbtree();
	seq_printf(m, "%s\n", print_buff);
	test_radtree();
	seq_printf(m, "%s\n", print_buff);
	test_xarray();
	seq_printf(m, "%s\n", print_buff);
	return 0;
}

static int proj_proc_open(struct inode *inode, struct  file *file) {
	  return single_open(file, proj_proc_show, NULL);
}

static const struct proc_ops proj_proc_fops = {
	  .proc_open = proj_proc_open,
	  .proc_read = seq_read,
	  .proc_lseek = seq_lseek,
	  .proc_release = single_release,
};

static int __init proj2_init(void)
{
	int err = 0;

	if (!int_str) {
		printk(KERN_INFO "Missing \'int_str\' parameter, exiting\n");
		return -1;
	}

	err = parse_params();
	if (err){
		cleanup();
	}

	run_tests();

	// Create our /proc/proj2 file
	proc_create("proj2", 0, NULL, &proj_proc_fops);
	return err;
}

static void __exit proj2_exit(void)
{
	// Remove the /proc/proj2 entry
	cleanup();
	remove_proc_entry("proj2", NULL);
	return;
}



module_init(proj2_init);
module_exit(proj2_exit);
