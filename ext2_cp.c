/*ext2_cp

This program takes three command line arguments. 
	The first is the name of an ext2 formatted virtual disk. 
	The second is the path to a file on your native operating system, 
struct ext2_super_block *sb	The third is an absolute path on your ext2 formatted disk. 

The program should work like cp, copying the file on your native file system 
onto the specified location on the disk. If the specified file or target location does not exist, 
then your program should return the appropriate error (ENOENT). */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "ext2_utils.h"
#include "ext2_cp.h"
#include "ext2.h"

unsigned char *disk;
unsigned char *inode_table;
char *dest_file_name;
char *parentPath;
struct ext2_super_block *sb;
struct ext2_group_desc *bg;
struct ext2_inode *parent_inode;
struct ext2_inode *new_inode;
struct ext2_dir_entry *parent_dir;
struct ext2_dir_entry *last_dir;
struct ext2_dir_entry *new_dir;
FILE *fd;
long int file_size;
int inode_index; 
int block_count;
int block_remains;
void *block;

int main(int argc, char **argv) {
	//check for correct args
	if (argc != 4){
		printf("Usage: ext2_cp <disk> <source_file_path> <dest_virtual_path>\n");
		return 0;
	}

	//open map out disk image
	int virtualfd = open(argv[1], O_RDWR);

	disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, virtualfd, 0);
	if(disk == MAP_FAILED) {
	    perror("mmap");
	    exit(1);
	}

	sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	bg = (struct ext2_group_desc *)(disk + (2 * EXT2_BLOCK_SIZE));
	inode_table = (unsigned char*)(disk + (bg->bg_inode_table * EXT2_BLOCK_SIZE));

	// Get parent inode on disk for copied file
	parent_inode = get_inode(get_parent_path(argv[3]));

	// Opens file on physical system
	fd = fopen(argv[2], "r");
	// Check if file exists
	if(fd == NULL){
		printf("File doesn't exist\n");
		fclose(fd);
		exit(ENOENT);
	}
	//Check if the file already exists on disk
	new_inode = get_inode(argv[2]);
	if(new_inode != NULL){
		printf("File already exists at destination path");
		fclose(fd);
		exit(EEXIST);
	}

	//Check if directory to place new file in exists
	if(parent_inode == NULL){
		printf("Target directory doesn't exist\n");
		fclose(fd);
		exit(ENOENT);
	}

	// get size of file
	fseek(fd, 0, SEEK_END);
	file_size = ftell(fd) - 1;

	// Calculate how many blocks are needed
	block_count = file_size / EXT2_BLOCK_SIZE;
	if(file_size % EXT2_BLOCK_SIZE){
		block_count++;
	}

	//need extra block
	if(block_count > 12){
		block_count++;
	}

	// Check for space on disk
	if(block_count > sb->s_free_blocks_count){
		printf("No space on drive.\n");
		exit(ENOSPC);
	}

	// Reset file pointer 
	fseek(fd, 0, SEEK_SET);

        ext2_cp(argv[2],argv[3]);
}

int ext2_cp (char* src_path, char* dest_path) {
	
	// Assign new inode and update inode bitmap
	inode_index = get_inode_index();
	update_inode_bitmap(inode_index,1);
	new_inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * ((inode_index - 1) % sb->s_inodes_per_group));
        new_inode->i_mode = EXT2_S_IFREG;
	new_inode->i_size = file_size;
	new_inode->i_links_count = 1;
	new_inode->i_blocks = 2 * block_count;
	
	int i;
	for(i = 0; i < 15; i++){
		new_inode->i_block[i] = 0;
	}

	// Allocate neccessary blocks
	for(i = 0; i < 13 && i < block_count; i++){
		new_inode->i_block[i] = get_block_index();
		update_block_bitmap(new_inode->i_block[i], 1);
	}
	// if i == 13 then we need to use a single indirect block.
	if(i == 13){
		unsigned int *indir_block = (unsigned int *)(disk + (new_inode->i_block[12] * EXT2_BLOCK_SIZE));
		int j;
		for(j = 0; i < block_count; i++, j++){
			indir_block[j] = get_block_index();
			update_block_bitmap(indir_block[j], 1);
		}
	}

	// Copy info from the file to the direct blocks

	for(i = 0; i < block_count && i < 12; i++){
		block = (void *)(disk + (new_inode->i_block[i] * EXT2_BLOCK_SIZE));
		fread(block, sizeof(char), EXT2_BLOCK_SIZE / sizeof(char), fd);
	}

	//copy into indirect blocks.
	if(block_count > 12){
		unsigned int *indir_block = (unsigned int *)(disk + (new_inode->i_block[i++] * EXT2_BLOCK_SIZE));
		int j;
		for(j = 0; i < block_count; i++, j++){
			block = (void *)(disk + (indir_block[j] * EXT2_BLOCK_SIZE));
			fread(block, sizeof(char), EXT2_BLOCK_SIZE / sizeof(char), fd);
		}
	}

        // find the last dir in the parent dir.	
	struct ext2_dir_entry *last_dir = find_last_dir(parent_inode);

        //lastDir no longer the last dir.. remove its padding
        remove_padding(last_dir);


        new_dir = (struct ext2_dir_entry *)(((char *) last_dir) + last_dir->rec_len);
        // getting the destination file name. 
	parentPath = get_parent_path(dest_path);
        dest_file_name = trim_parent_path(dest_path,parentPath);

	int leftOver = ( ((unsigned char *)(new_dir)) - (unsigned char *)(disk + 1024*parent_inode->i_block[0])); //last Dir takes up rest of block 	
	setup_dir_entry(new_dir, inode_index, strlen(dest_file_name), EXT2_BLOCK_SIZE - leftOver, EXT2_FT_REG_FILE, dest_file_name);
	
	//UPDATE META DATA
	sb->s_free_inodes_count -= 1;
        sb->s_free_blocks_count -= 1;
        bg->bg_free_blocks_count -= 1;
        bg->bg_free_inodes_count -= 1;
        return 0;


}
