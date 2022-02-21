#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "b_tree.h"
#include "jdisk.h"

Tree_Node *create_node(void *b_tree, unsigned int lba);
void print_node_keys(void *b_tree, Tree_Node *node);
int add_key(void *b_tree, Tree_Node *node, void *key, unsigned int index, int lba);
void write_node(void *b_tree, Tree_Node *node);

void add_free_list(void *b_tree, Tree_Node *node);
void free_nodes(void *b_tree);

void *b_tree_create(char *filename, long size, int key_size)
{
	int i, zero;
	B_Tree *tree;
	Tree_Node *root;

	tree = (B_Tree *)malloc(sizeof(B_Tree));
	tree->key_size = key_size;
	tree->root_lba = 1;
	tree->first_free_block = 2;
	tree->disk = jdisk_create(filename, size);
	tree->size = size;
	tree->num_lbas = size / JDISK_SECTOR_SIZE;
	tree->keys_per_block = (JDISK_SECTOR_SIZE - 6) / (key_size + 4);
	tree->lbas_per_block = tree->keys_per_block + 1;
	tree->flush = 0;

	tree->free_list = (Tree_Node **)malloc(tree->num_lbas * sizeof(Tree_Node *));
	
	for(i = 0; i < tree->num_lbas; i++)
		tree->free_list[i] = NULL;
	
	root = create_node(tree, 1);
	root->bytes[0] = 0;
	root->bytes[1] = 0;
	zero = 0;
	memcpy(root->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4), &zero, 4);
	memcpy(root->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4) + 4, &zero, 4);
	jdisk_write(tree->disk, 0, tree);
	jdisk_write(tree->disk, 1, root->bytes);

	free(root->keys);
	free(root->lbas);
	free(root);

	return tree;
}

void *b_tree_attach(char *filename)
{
	int i;
	B_Tree *tree;
	unsigned char sector_0[JDISK_SECTOR_SIZE];

	tree = (B_Tree *)malloc(sizeof(B_Tree));

	tree->disk = jdisk_attach(filename);
	jdisk_read(tree->disk, 0, sector_0);

	memcpy(&(tree->key_size), sector_0, 4);
	memcpy(&(tree->root_lba), sector_0 + 4, 4);
	memcpy(&(tree->first_free_block), sector_0 + 8, 8);

	tree->size = jdisk_size(tree->disk);
	tree->num_lbas = tree->size / JDISK_SECTOR_SIZE;
	tree->keys_per_block = (JDISK_SECTOR_SIZE - 6) / (tree->key_size + 4);
	tree->lbas_per_block = tree->keys_per_block + 1;

	tree->free_list = (Tree_Node **)malloc(tree->num_lbas * sizeof(Tree_Node *));

	for(i = 0; i < tree->num_lbas; i++)
		tree->free_list[i] = NULL;

	tree->flush = 0;
	
	return tree;
}

unsigned int b_tree_insert(void *b_tree, void *key, void *record)
{
	int i, found_lba, nkeys;
	B_Tree *tree;
	Tree_Node *old_child, *new_child, *parent;
	
	tree = (B_Tree *)b_tree;

	if(tree->num_lbas <= tree->first_free_block)
		return 0;

	found_lba = b_tree_find(b_tree, key);
	
	if(found_lba != 0)
	{
		jdisk_write(tree->disk, found_lba, record);	
		return found_lba;
	}
	else
	{
		nkeys = add_key(b_tree, tree->tmp_e, key, tree->tmp_e_index, tree->first_free_block);
		jdisk_write(tree->disk, tree->first_free_block, record);
		tree->first_free_block++;
		old_child = tree->tmp_e;
		
		while(1)
		{	
			if(nkeys <= tree->keys_per_block)
			{
				write_node(b_tree, old_child);
				return tree->tmp_e->lbas[tree->tmp_e_index];
			}
			else
			{
				new_child = create_node(b_tree, tree->first_free_block);
				tree->first_free_block++;

				if(old_child->lba == tree->tmp_e->lba)
					new_child->internal = 0;
				else
					new_child->internal = 1;

				new_child->nkeys = tree->keys_per_block / 2;
				old_child->nkeys = tree->keys_per_block - new_child->nkeys;
			
				for(i = 0; i < new_child->nkeys; i++)
				{
					memcpy(new_child->keys[i], old_child->keys[old_child->nkeys + 1 + i], tree->key_size);
					new_child->lbas[i] = old_child->lbas[old_child->nkeys + 1 + i];
				}
				if(tree->keys_per_block % 2 == 0)
					new_child->lbas[new_child->nkeys] = old_child->lbas[2 * new_child->nkeys + 1];
				else
					new_child->lbas[new_child->nkeys] = old_child->lbas[2 * new_child->nkeys + 2];
				//printf("copied keys and lbas\n");

				write_node(b_tree, old_child);
				write_node(b_tree, new_child);
				
				//printf("Old child LBA: %d\n", old_child->lba);
				//for(i = 0; i < old_child->nkeys; i++)
				//	printf("Key: %s LBA: %d\n", old_child->keys[i], old_child->lbas[i]);
				//printf("Key: %s LBA: %d\n", old_child->keys[old_child->nkeys - 1], old_child->lbas[old_child->nkeys]);
				
				//printf("New child LBA: %d\n", new_child->lba);
				//for(i = 0; i < new_child->nkeys; i++)
				//	printf("Key: %s LBA: %d\n", new_child->keys[i], new_child->lbas[i]);
				//printf("Key: %s LBA: %d\n", new_child->keys[new_child->nkeys - 1], new_child->lbas[new_child->nkeys]);
				//printf("wrote nodes\n");
				
				parent = old_child->parent;
				
				if(parent == NULL)
				{
					parent = create_node(b_tree, tree->first_free_block);
					parent->internal = 1;
					parent->nkeys = 0;
					parent->lbas[0] = old_child->lba;
					old_child->parent_index = 0;
					tree->root_lba = tree->first_free_block;
					tree->first_free_block++;
				}
				
				nkeys = add_key(b_tree, parent, old_child->keys[old_child->nkeys], old_child->parent_index, parent->lbas[old_child->parent_index]);
				parent->lbas[old_child->parent_index + 1] = new_child->lba;
				
				old_child = parent;

				//printf("LBA %d\n", old_child->lba);
				//for(i = 0; i < old_child->nkeys; i++)
				//	printf("Key %s LBA %d\n", old_child->keys[i], old_child->lbas[i]);
				//printf("Key %s LBA %d\n", old_child->keys[old_child->nkeys - 1], old_child->lbas[old_child->nkeys]);
			}

		}
	}
}

int add_key(void *b_tree, Tree_Node *node, void *key, unsigned int index, int lba)
{
		int i, keybytes_to_shift;
		B_Tree *tree;

		tree = (B_Tree *)b_tree;

		if(index != tree->keys_per_block)
		{
			keybytes_to_shift = tree->key_size * (node->nkeys - index);
			
			memcpy(node->keys[index + 1], node->keys[index], keybytes_to_shift);
	
			for(i = node->nkeys; i >= index; i--)
			{
				node->lbas[i + 1] = node->lbas[i];
				if(i == 0)
					break;
			}
		}
		else
			node->lbas[index + 1] = node->lbas[index];

		memcpy(node->keys[index], key, tree->key_size);
		node->lbas[index] = lba;
		node->nkeys++;
		
		//printf("LBA: %d\n", node->lba);
		//for(i = 0; i < node->nkeys; i++)
		//	printf("Key %s LBA %d\n", node->keys[i], node->lbas[i]);
		//printf("Key %s LBA %d\n", node->keys[node->nkeys - 1], node->lbas[node->nkeys]);
		
		return node->nkeys;
}

unsigned int b_tree_find(void *b_tree, void *key)
{
	int i, keycmp, find_val, finish, return_lba;
	B_Tree *tree;
	Tree_Node *node, *child;

	tree = (B_Tree *)b_tree;
	node = create_node(b_tree, tree->root_lba);
	node->parent = NULL;
	node->parent_index = 0;

	if(node->nkeys == 0)
	{
		//add_free_list(b_tree, node);
		tree->tmp_e = node;
		tree->tmp_e_index = 0;
		return 0;
	}

	find_val = 0;
	finish = 0;

	while(1)
	{	
		if(finish)
			break;

		for(i = 0; i < node->nkeys; i++)
		{
			if(find_val)
			{
				if(i == node->nkeys - 1)
				{
					if(node->internal)
					{
						//add_free_list(b_tree, node);
						node = create_node(b_tree, node->lbas[i + 1]);
						break;
					}
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

			if(keycmp < 0)
			{
				if(!node->internal)
				{
					tree->tmp_e = node;
					tree->tmp_e_index = i;
					return_lba = 0;
					finish = 1;
					break;
				}

				//add_free_list(b_tree, node);
				child = create_node(b_tree, node->lbas[i]);
				child->parent = node;
				child->parent_index = i;
				node = child;
				break;
			}
			else if(keycmp > 0)
			{
				if(i == node->nkeys - 1)
				{
					if(!node->internal)
					{
						tree->tmp_e = node;
						tree->tmp_e_index = i + 1;
						return_lba = 0;
						finish = 1;
						break;
					}

					//add_free_list(b_tree, node);
					child = create_node(b_tree, node->lbas[i + 1]);
					child->parent = node;
					child->parent_index = i + 1;
					node = child;
					break;
				}
			}
			else
			{
				if(!node->internal)
				{
					return_lba = node->lbas[i];
					finish = 1;
					break;
				}
				else
				{
					find_val = 1;
					//add_free_list(b_tree, node);
					node = create_node(b_tree, node->lbas[i]);
					break;
				}
			}
		}
	}

	/*if(return_lba == 0)
	{
		node = tree->tmp_e->parent;
		printf("Key would have been found at %d in node LBA %d\n", tree->tmp_e_index, tree->tmp_e->lba);
		while(node != NULL)
		{
			printf("Parent is node LBA %d\n", node->lba);
			node = node->parent;
		}
	}*/
	
	//add_free_list(b_tree, node);
	//free_nodes(b_tree);

	return return_lba;
}

void *b_tree_disk(void *b_tree)
{
	return ((B_Tree *)b_tree)->disk;
}

int b_tree_key_size(void *b_tree)
{
	return ((B_Tree *)b_tree)->key_size;
}

void b_tree_print_tree(void *b_tree)
{
	int i;
	B_Tree *tree;
	Tree_Node *root;
	
	tree = (B_Tree *)b_tree;

	printf("key_size = %d\nroot_lba = %d\nkeys_per_block = %d\n", tree->key_size, tree->root_lba, tree->keys_per_block);

	root = create_node(b_tree, tree->root_lba);

	print_node_keys(b_tree, root);
	
	free_nodes(b_tree);
}

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
			child = create_node(b_tree, node->lbas[i]);
			print_node_keys(b_tree, child);
		}
	}
	else
		printf("LBAs are data.\n");

	add_free_list(b_tree, node);
}

Tree_Node *create_node(void *b_tree, unsigned int lba)
{
	int i, free_node;
	B_Tree *tree;
	Tree_Node *node;

	tree = (B_Tree *)b_tree;

	free_node = 0;

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
	
	if(!free_node)
	{
		node = (Tree_Node *)malloc(sizeof(Tree_Node));
		node->keys = (unsigned char **)malloc((tree->keys_per_block + 1) * sizeof(unsigned char *));
		node->lbas = (unsigned int *)malloc((tree->lbas_per_block + 1) * sizeof(unsigned int));
	
		for(i = 0; i < tree->keys_per_block + 1; i++)
			node->keys[i] = node->bytes + 2 + (tree->key_size * i);
	}

	node->lba = lba;

	jdisk_read(tree->disk, node->lba, node->bytes);

	memcpy(&(node->internal), node->bytes, 1);
	memcpy(&(node->nkeys), node->bytes + 1, 1);
	
	for(i = 0; i < tree->lbas_per_block; i++)
	{	
		memcpy(&(node->lbas[i]), node->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4) + (i * 4), 4);
		//printf("LBA %d: %d\n", i, node->lbas[i]);
	}
	
	//printf("nkeys = %d\n", node->nkeys);
	
	return node;
}

void write_node(void *b_tree, Tree_Node *node)
{
	int i;
	B_Tree *tree;

	tree = (B_Tree *)b_tree;
	
	for(i = 0; i < tree->lbas_per_block; i++)
		memcpy(node->bytes + JDISK_SECTOR_SIZE - (tree->lbas_per_block * 4) + (i * 4), &(node->lbas[i]), 4);	

	node->bytes[0] = node->internal;
	node->bytes[1] = node->nkeys;
			
	jdisk_write(tree->disk, node->lba, node->bytes);
	jdisk_write(tree->disk, 0, tree);
}

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
