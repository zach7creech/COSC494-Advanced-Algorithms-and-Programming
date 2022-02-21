/* Author: Zachery Creech
 * COSC494 Fall 2021
 * Lab 3: b_tree.c
 * This program implements the functions defined in b_tree.h for manipulating a b_tree
 * data structure on a jdisk. It also implements numerous helper functions used in these
 * functions. Jdisk and its associated functions are defined in jdisk.h and implemented
 * in jdisk.c by Dr. James Plank (jplank@utk.edu).
 * 11/1/2021 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "b_tree.h"
#include "jdisk.h"

typedef struct tnode
{
	unsigned char bytes[JDISK_SECTOR_SIZE+256];
	unsigned char nkeys;
	unsigned char flush;
	unsigned char internal;
	unsigned int lba;
	unsigned char **keys;
	unsigned int *lbas;
	struct tnode *parent;
	struct tnode *new_sibling;
	int parent_index;
} Tree_Node;

typedef struct
{
	int key_size;
	unsigned int root_lba;
	unsigned long first_free_block;

	void *disk;
	unsigned long size;
	unsigned long num_lbas;
	int keys_per_block;
	int lbas_per_block;
	Tree_Node **free_list;

	Tree_Node *tmp_e;
	int tmp_e_index;

	int flush;
} B_Tree;

int add_key(void *b_tree, Tree_Node *node, void *key, unsigned int index, int lba);
unsigned int my_find(void *b_tree, void *key, int insert);
void write_node(void *b_tree, Tree_Node *node);
void flush_and_free_nodes(void *b_tree);
Tree_Node *create_node(void *b_tree, unsigned int lba, int new);

void add_free_list(void *b_tree, Tree_Node *node);
void free_nodes(void *b_tree);
void print_node_keys(void *b_tree, Tree_Node *node);

//create and return a handle for a new b_tree from the given jdisk of size bytes from filename, with key size of key_size bytes
void *b_tree_create(char *filename, long size, int key_size)
{
	int i, zero;
	B_Tree *tree;
	Tree_Node *root;

	//allocate the b_tree structure and set all of its members
	
	tree = (B_Tree *)malloc(sizeof(B_Tree));
	tree->key_size = key_size;
	//empty tree's root is at sector 1
	tree->root_lba = 1;
	//empty tree's first sector available for data is sector 2
	tree->first_free_block = 2;
	tree->disk = jdisk_create(filename, size);
	tree->size = size;
	//total number of sectors in the b_tree
	tree->num_lbas = size / JDISK_SECTOR_SIZE;
	//each sector can hold at most this many keys
	tree->keys_per_block = (JDISK_SECTOR_SIZE - 6) / (key_size + 4);
	//each sector holds one more LBA than max keys
	tree->lbas_per_block = tree->keys_per_block + 1;
	tree->flush = 0;

	//create and initialize the free_list
	tree->free_list = (Tree_Node **)malloc(tree->num_lbas * sizeof(Tree_Node *));
	for(i = 0; i < tree->num_lbas; i++)
		tree->free_list[i] = NULL;
	
	//create the root
	root = create_node(tree, 1, 1);
	//new root is external and contains no keys
	root->bytes[0] = 0;
	root->bytes[1] = 0;
	//set first two LBAs to zero so greatest key in the b_tree's "greater LBA" pointer points to sector 0
	zero = 0;
	memcpy(root->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4), &zero, 4);
	memcpy(root->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4) + 4, &zero, 4);
	//write sector zero and the root node to the b_tree
	jdisk_write(tree->disk, 0, tree);
	jdisk_write(tree->disk, 1, root->bytes);

	tree->flush = 0;
	
	free(root->keys);
	free(root->lbas);
	free(root);

	return tree;
}

//attach to an existing b_tree from jdisk filename and return a handle to it
void *b_tree_attach(char *filename)
{
	int i;
	B_Tree *tree;
	unsigned char sector_0[JDISK_SECTOR_SIZE];

	//allocate the b_tree data structure
	tree = (B_Tree *)malloc(sizeof(B_Tree));

	//attach to the jdisk and read sector 0
	tree->disk = jdisk_attach(filename);
	jdisk_read(tree->disk, 0, sector_0);

	//read the key_size, root_lba, and first_free_block from sector 0
	memcpy(&(tree->key_size), sector_0, 4);
	memcpy(&(tree->root_lba), sector_0 + 4, 4);
	memcpy(&(tree->first_free_block), sector_0 + 8, sizeof(unsigned long));

	//set the b_tree's members based on data from the jdisk
	tree->size = jdisk_size(tree->disk);
	tree->num_lbas = tree->size / JDISK_SECTOR_SIZE;
	tree->keys_per_block = (JDISK_SECTOR_SIZE - 6) / (tree->key_size + 4);
	tree->lbas_per_block = tree->keys_per_block + 1;

	//create and initialize the free_list
	tree->free_list = (Tree_Node **)malloc(tree->num_lbas * sizeof(Tree_Node *));

	for(i = 0; i < tree->num_lbas; i++)
		tree->free_list[i] = NULL;

	tree->flush = 0;
	
	return tree;
}

//insert a new key into the b_tree, or, if it is already in the tree, update the key's value with record
//handles splitting a node when a key is inserted in a full node
unsigned int b_tree_insert(void *b_tree, void *key, void *record)
{
	int i, return_lba, nkeys;
	B_Tree *tree;
	Tree_Node *old_child, *new_child, *parent;
	
	tree = (B_Tree *)b_tree;

	//if b_tree is full, can't insert
	if(tree->num_lbas <= tree->first_free_block)
		return 0;

	//calling find will return the correct location of the key in the b_tree, either where it actually is or where it should go
	return_lba = my_find(b_tree, key, 1);
	
	//found the key in the tree, overwrite the value and return the LBA written
	if(return_lba != 0)
	{
		jdisk_write(tree->disk, return_lba, record);	
		return return_lba;
	}
	//otherwise the key needs to be inserted
	else
	{
		//add the key into the found node whether it is full already or not; there is room for one extra
		nkeys = add_key(b_tree, tree->tmp_e, key, tree->tmp_e_index, tree->first_free_block);
		//write the new key's value to the b_tree, update the tree's first_free_block and set sector 0 to flush
		jdisk_write(tree->disk, tree->first_free_block, record);
		tree->first_free_block++;
		tree->flush = 1;
		//old_child is the node that might need to be split on each loop, tree->tmp_e is the node where the key was just inserted
		old_child = tree->tmp_e;
		
		//split an overfull node and send the middle key up to the node's parent, then split the parent if it becomes overfull
		//do this forever until a parent is added to and it doesn't need to be split
		while(1)
		{	
			//node doesn't need to be split, so flush and free all modified nodes and return the LBA the key's value was written
			if(nkeys <= tree->keys_per_block)
			{
				write_node(b_tree, old_child);
				return_lba = tree->tmp_e->lbas[tree->tmp_e_index];
				flush_and_free_nodes(b_tree);
				return return_lba;
			}
			//otherwise the node needs to be split
			else
			{
				//create a new child node for half of the original, overfull node's keys to go into
				new_child = create_node(b_tree, tree->first_free_block, 1);
				tree->first_free_block++;

				//set if the node is internal or external, it will always be internal other than the very first split
				if(old_child->lba == tree->tmp_e->lba)
					new_child->internal = 0;
				else
					new_child->internal = 1;

				//arbitrarily, the number of keys left in the original node will be one greater than in the new sibling node when keys_per_block is odd
				//when it's even, both nodes from the split will have the same number of keys, half of keys_per_block
				new_child->nkeys = tree->keys_per_block / 2;
				old_child->nkeys = tree->keys_per_block - new_child->nkeys;
			
				//copy over the last half of the keys from the original node to the new sibling node
				for(i = 0; i < new_child->nkeys; i++)
				{
					memcpy(new_child->keys[i], old_child->keys[old_child->nkeys + 1 + i], tree->key_size);
					new_child->lbas[i] = old_child->lbas[old_child->nkeys + 1 + i];
				}
				//the index of the last LBA to be copied differs by one depending on the parity of keys_per_block
				if(tree->keys_per_block % 2 == 0)
					new_child->lbas[new_child->nkeys] = old_child->lbas[2 * new_child->nkeys + 1];
				else
					new_child->lbas[new_child->nkeys] = old_child->lbas[2 * new_child->nkeys + 2];

				//flush nodes simply follows parent pointers from where the key is inserted to the last modified parent
				//this allows the new sibling to be written at the same time the original node is overwritten
				old_child->new_sibling = new_child;

				//write the bytes array with updated information for each node and set them to be flushed
				write_node(b_tree, old_child);
				write_node(b_tree, new_child);
				
				parent = old_child->parent;
				
				//this means a new root needs to be created
				if(parent == NULL)
				{
					parent = create_node(b_tree, tree->first_free_block, 1);
					parent->internal = 1;
					parent->nkeys = 0;
					//this parent will point to the original node, the sector before the split
					parent->lbas[0] = old_child->lba;
					old_child->parent_index = 0;
					//root has changed so update sector 0
					tree->root_lba = tree->first_free_block;
					tree->first_free_block++;
					tree->flush = 1;
					//the original node needs to know that its parent is no longer NULL because it isn't the root anymore
					old_child->parent = parent;
				}
				
				parent->flush = 1;
				//add the middle key from the original node that is now split to the parent
				nkeys = add_key(b_tree, parent, old_child->keys[old_child->nkeys], old_child->parent_index, parent->lbas[old_child->parent_index]);
				//make sure the newly inserted parent key's "greater" pointer is set to the new sibling node
				parent->lbas[old_child->parent_index + 1] = new_child->lba;
				
				//repeat checking for a split all the way up the tree until a node can be inserted into and not split
				old_child = parent;
			}

		}
	}
}

//add a key to a node, shifting keys forward in the node as necessary
int add_key(void *b_tree, Tree_Node *node, void *key, unsigned int index, int lba)
{
		int i, keybytes_to_shift;
		B_Tree *tree;

		tree = (B_Tree *)b_tree;

		//if the key isn't being inserted at the end of the node, the other keys need to be shifted
		if(index != tree->keys_per_block)
		{
			//only the key being replaced at index and greater need to be moved forward
			keybytes_to_shift = tree->key_size * (node->nkeys - index);
			
			memcpy(node->keys[index + 1], node->keys[index], keybytes_to_shift);
	
			//move the LBAs forward
			for(i = node->nkeys; i >= index; i--)
			{
				node->lbas[i + 1] = node->lbas[i];
				//nkeys is unsigned, so this for loop will blow up if i-- when i = 0
				if(i == 0)
					break;
			}
		}
		//otherwise simply move the rightmost "greater" pointer over once so the new key will point to the next greatest key in the b_tree
		else
			node->lbas[index + 1] = node->lbas[index];

		//copy the new key and LBA into the node
		memcpy(node->keys[index], key, tree->key_size);
		node->lbas[index] = lba;
		node->nkeys++;
		
		return node->nkeys;
}

//this is just b_tree_find but for dealing with memory management
//i.e., b_tree_find calls this function and frees nodes, but b_tree_insert also uses find to create a list of nodes from
//root to where the key should be, then it uses that list to do splits, so the nodes can't be freed when b_tree_find returns
unsigned int my_find(void *b_tree, void *key, int insert)
{
	int i, keycmp, find_val, finish, return_lba;
	B_Tree *tree;
	Tree_Node *node, *child;

	//create the root
	tree = (B_Tree *)b_tree;
	node = create_node(b_tree, tree->root_lba, 0);
	node->parent = NULL;
	node->parent_index = 0;

	//the b_tree is completely empty, so the key just needs to be put in the root
	if(node->nkeys == 0)
	{
		if(!insert)
			add_free_list(b_tree, node);
		tree->tmp_e = node;
		tree->tmp_e_index = 0;
		return 0;
	}

	find_val = 0;
	finish = 0;

	//look for the key until an external node is hit. If the key isn't there, it isn't in the tree
	while(1)
	{	
		if(finish)
			break;

		//go through all keys in the current node
		for(i = 0; i < node->nkeys; i++)
		{
			//this means the key has been found in an internal node, so find the key's immediate predecessor key to find its value
			if(find_val)
			{
				//at the last key in an immediate predecessor node
				if(i == node->nkeys - 1)
				{
					//if still at an internal node, go to the next greater node
					if(node->internal)
					{
						if(!insert)
							add_free_list(b_tree, node);
						node = create_node(b_tree, node->lbas[i + 1], 0);
						break;
					}
					//if at an external node, the rightmost LBA is the key's value
					else
					{
						return_lba = node->lbas[i + 1];
						finish = 1;
						break;
					}
				}
				else
					continue;
			}
			
			keycmp = memcmp(key, node->keys[i], tree->key_size);

			//key is less than this node's key at this index
			if(keycmp < 0)
			{
				//if at an external node, the key should be here but it isn't. Set tree->tmp_e and return
				if(!node->internal)
				{
					tree->tmp_e = node;
					tree->tmp_e_index = i;
					return_lba = 0;
					finish = 1;
					break;
				}

				//otherwise follow this key's "lesser" pointer
				if(!insert)
					add_free_list(b_tree, node);
				child = create_node(b_tree, node->lbas[i], 0);
				child->parent = node;
				child->parent_index = i;
				node = child;
				break;
			}
			//key is greater than this node's key at this index
			else if(keycmp > 0)
			{
				//if at the last key in the node, either move on to the next "greater" node or the key isn't in the tree
				if(i == node->nkeys - 1)
				{
					//if at an external node, the key isn't in the tree
					if(!node->internal)
					{
						tree->tmp_e = node;
						tree->tmp_e_index = i + 1;
						return_lba = 0;
						finish = 1;
						break;
					}
					
					//follow the next greatest node
					if(!insert)
						add_free_list(b_tree, node);
					child = create_node(b_tree, node->lbas[i + 1], 0);
					child->parent = node;
					child->parent_index = i + 1;
					node = child;
					break;
				}
			}
			//otherwise the key is found
			else
			{
				//if at an external node, simply grab the LBA and return
				if(!node->internal)
				{
					return_lba = node->lbas[i];
					finish = 1;
					break;
				}
				//otherwise find the LBA in the key's immediate predecessor (follow the "lesser" pointer)
				else
				{
					find_val = 1;
					if(!insert)
						add_free_list(b_tree, node);
					node = create_node(b_tree, node->lbas[i], 0);
					break;
				}
			}
		}
	}

	//if this wasn't called from insert, free all the nodes
	if(!insert)
	{
		add_free_list(b_tree, node);
		free_nodes(b_tree);
	}
	
	return return_lba;
}

//calls find with memory management and returns the key's associated LBA, or 0 if it isn't in the tree
unsigned int b_tree_find(void *b_tree, void *key)
{
	return my_find(b_tree, key, 0);
}

//helper function to update the node's bytes array to be written to the jdisk when the node is flushed
void write_node(void *b_tree, Tree_Node *node)
{
	int i;
	B_Tree *tree;

	tree = (B_Tree *)b_tree;
	
	//copy the node's LBAs into bytes
	for(i = 0; i < tree->lbas_per_block; i++)
		memcpy(node->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4) + (i * 4), &(node->lbas[i]), 4);	

	//set the node to be internal or external and update nkeys
	node->bytes[0] = node->internal;
	node->bytes[1] = node->nkeys;
			
	node->flush = 1;
}

//writes all modified nodes to the jdisk and free all nodes in free_list
void flush_and_free_nodes(void *b_tree)
{
	B_Tree *tree;
	Tree_Node *node;
	
	tree = (B_Tree *)tree;
	node = tree->tmp_e;

	//follow the parents of the modified key until reaching the root or a node that doesn't need to be written
	while(node != NULL && node->flush == 1)
	{
		jdisk_write(tree->disk, node->lba, node->bytes);
		//the node will have a sibling if it was split, and it needs to be written too
		if(node->new_sibling != NULL)
		{
			jdisk_write(tree->disk, node->new_sibling->lba, node->new_sibling->bytes);
			add_free_list(b_tree, node->new_sibling);
		}

		add_free_list(b_tree, node);
		node = node->parent;
	}

	//flush sector 0 if the first_free_block was modified
	if(tree->flush == 1)
		jdisk_write(tree->disk, 0, tree);

	free_nodes(b_tree);
}

//create a node and assign it LBA value lba. If the node already has data on the jdisk, read it
Tree_Node *create_node(void *b_tree, unsigned int lba, int new)
{
	int i, free_node;
	B_Tree *tree;
	Tree_Node *node;

	tree = (B_Tree *)b_tree;

	free_node = 0;

	//see if there's a node on the free_list that can be reused
	for(i = 0; i < tree->num_lbas; i++)
	{
		if(tree->free_list[i] != NULL)
		{
			node = tree->free_list[i];
			tree->free_list[i] = NULL;
			free_node = 1;
			break;
		}
	}
	
	//if there isn't one on the free_list, create a new one and set keys to point to the correct locations in bytes
	if(!free_node)
	{
		node = (Tree_Node *)malloc(sizeof(Tree_Node));
		node->keys = (unsigned char **)malloc((tree->keys_per_block + 1) * sizeof(unsigned char *));
		node->lbas = (unsigned int *)malloc((tree->lbas_per_block + 1) * sizeof(unsigned int));
	
		for(i = 0; i < tree->keys_per_block + 1; i++)
			node->keys[i] = node->bytes + 2 + (tree->key_size * i);
	}

	node->lba = lba;

	//if this node already exists on the jdisk, read it and set the values
	if(!new)
	{
		jdisk_read(tree->disk, node->lba, node->bytes);

		memcpy(&(node->internal), node->bytes, 1);
		memcpy(&(node->nkeys), node->bytes + 1, 1);
	
		for(i = 0; i < tree->lbas_per_block; i++)
			memcpy(&(node->lbas[i]), node->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4) + (i * 4), 4);
	}

	node->new_sibling = NULL;
	
	return node;
}

//add a node to the first free spot on the free_list
void add_free_list(void *b_tree, Tree_Node *node)
{
	int i;
	B_Tree *tree;

	tree = (B_Tree *)b_tree;

	for(i = 0; i < tree->num_lbas; i++)
	{
		if(tree->free_list[i] == NULL)
		{
			tree->free_list[i] = node;
			break;
		}
	}
}

//free all nodes on the free_list
void free_nodes(void *b_tree)
{
	int i;
	B_Tree *tree;

	tree = (B_Tree *)b_tree;

	for(i = 0; i < tree->num_lbas; i++)
	{
		if(tree->free_list[i] != NULL)
		{
			free(tree->free_list[i]->keys);
			free(tree->free_list[i]->lbas);
			free(tree->free_list[i]);
			tree->free_list[i] = NULL;
		}
	}
}

//return a handle to the jdisk associated with this b_tree
void *b_tree_disk(void *b_tree)
{
	return ((B_Tree *)b_tree)->disk;
}

//returns the key_size of this b_tree
int b_tree_key_size(void *b_tree)
{
	return ((B_Tree *)b_tree)->key_size;
}

//calls recursive function print_node_keys to print the tree
void b_tree_print_tree(void *b_tree)
{
	int i;
	B_Tree *tree;
	Tree_Node *root;
	
	tree = (B_Tree *)b_tree;

	printf("key_size = %d\nroot_lba = %d\nkeys_per_block = %d\n", tree->key_size, tree->root_lba, tree->keys_per_block);

	root = create_node(b_tree, tree->root_lba, 0);

	print_node_keys(b_tree, root);
	
	free_nodes(b_tree);
}

//traverse the b_tree recursively and print all information
void print_node_keys(void *b_tree, Tree_Node *node)
{
	int i;
	unsigned char buf[JDISK_SECTOR_SIZE];
	Tree_Node *child;
	
	printf("\nNode LBA: %d\n", node->lba);
	
	for(i = 0; i < node->nkeys; i++)
	{
		printf("Key: %s LBA: %d\n", node->keys[i], node->lbas[i]);
		if(!node->internal)
		{
			jdisk_read(((B_Tree *)b_tree)->disk, node->lbas[i], buf);
			printf("Value is %s\n", buf);
		}
		if(i == node->nkeys - 1)
		{
			printf("Key: %s LBA: %d\n", node->keys[i], node->lbas[i + 1]);
			if(!node->internal)
			{
				jdisk_read(((B_Tree *)b_tree)->disk, node->lbas[i + 1], buf);
				printf("Value is %s\n", buf);
			}
		}
	}

	if(node->internal)
	{
		for(i = 0; i < node->nkeys + 1; i++)
		{
			child = create_node(b_tree, node->lbas[i], 0);
			print_node_keys(b_tree, child);
		}
	}
	else
		printf("LBAs are data.\n");

	add_free_list(b_tree, node);
}
