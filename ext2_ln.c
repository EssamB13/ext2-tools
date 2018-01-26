#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"
#include "ext2_utils.h"
#include "ext2_ln.h"

unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_group_desc *bg;
unsigned char * inode_table;
struct ext2_inode *rootInode;
struct ext2_inode *dest_inode;
struct ext2_inode *parent_inode;
struct ext2_dir_entry *parent_dir;
struct ext2_dir_entry *dest_dir;
char *src_path, dest_path;
FILE *fd;
long int file_size;
int block_count, block_remains;
void *block;

int main(int argc, char **argv){
        //check for correct args
    if (argc < 4 || argc > 5){
    	printf("Usage: ext2_lns <-s> <disk> <source_file_path> <dest_virtual_path>\n");
        return 0;
	}

	if(argc == 5 && strcmp(argv[1],"-s") != 0){
    	printf("Usage: ext2_cp <-s> <disk> <source_file_path> <dest_virtual_path>\n");
        return 0;
	}

    //open map out disk image
    int fdIndex = 1;

    if(argc == 5){
 		fdIndex = 2;
    }


    int fd = open(argv[fdIndex], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
    	perror("mmap");
        exit(1);
    }

    sb = (struct ext2_super_block *)(disk + 1024);
    bg = (struct ext2_group_desc *)(disk + 1024*2);
    inode_table = (unsigned char *)(disk + 1024*bg->bg_inode_table);     
    //root index
    int index = (2-1) % sb->s_inodes_per_group;
    rootInode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

	if (argc == 4){
		ext2_ln_hard(argv[2], argv[3]);	
	} 
	else {
		ext2_ln_soft(argv[3], argv[4]);
	}
		

	
}



int ext2_ln_hard (char* src_path, char* dest_path){
		
	char * src_parent = get_parent_path(src_path);

	//get file name without parent dir 
	char * src_file_name = trim_parent_path(src_path,src_parent);

	struct ext2_inode *src_parent_inode = get_inode(src_parent);

	if(src_parent_inode == NULL){
		printf("File or directory doesn't not exist");
		exit(ENOENT);
	}

	struct ext2_dir_entry *src_dir = (struct ext2_dir_entry * ) (disk + 1024*src_parent_inode->i_block[0]);

	// get the dir_entry of the src file inside the src parent directory
	src_dir = find_dir_in_dir(src_dir,src_parent_inode,src_file_name);

	//We find the last dir_entry in the parent path of
	//dest_path to prepare to attach a dir_entry to the end of the linked list
	char * dest_parent_path = get_parent_path(dest_path);

	struct ext2_inode *dest_inode = get_inode(dest_parent_path);

	if(dest_inode == NULL){
		printf("File or directory doesn't not exist");
		exit(ENOENT);
	}

	//get the last directory entry in dest_inode
	struct ext2_dir_entry * lastDir = find_last_dir(dest_inode);

	//lastDir no longer the last dir.. remove its padding
	remove_padding(lastDir);

	struct ext2_dir_entry * newDir = (struct ext2_dir_entry *)(((char *) lastDir) + lastDir->rec_len);

	newDir->inode = src_dir->inode;
	newDir->file_type = src_dir->file_type;
	
	//name of dest_file without its parents names
	char* dest_file_name = trim_parent_path(dest_path,dest_parent_path);
	newDir->name_len = strlen(dest_file_name);
	memcpy( (char *) ((char *)newDir + sizeof(struct ext2_dir_entry)) ,dest_file_name,  strlen(dest_file_name) );


	int index = (src_dir->inode-1) % sb->s_inodes_per_group;
	struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

	//increment link count of inode
	inode->i_links_count++;

	int leftOver = ( ((unsigned char *)(newDir)) - (unsigned char *)(disk + 1024*dest_inode->i_block[0])  ); //last Dir takes up rest of block 
	newDir->rec_len = 1024-leftOver;  //WRONG ? TODO:



	return 0;
}

int ext2_ln_soft (char* src_path, char* dest_path){

	//get parent and file names
	char * src_parent = get_parent_path(src_path);
	char * src_file_name = trim_parent_path(src_path,src_parent);

	if(get_parent_path(dest_path) != NULL){
		return EEXIST;
	}
	
	//get src directories inode and the dir_entry 
	struct ext2_inode *src_dir_inode = get_inode(src_parent);
	struct ext2_dir_entry *src_dir = (struct ext2_dir_entry * ) (disk + 1024*src_dir_inode->i_block[0]);

	src_dir = find_dir_in_dir(src_dir,src_dir_inode,src_file_name);
	
	if( !(src_dir->file_type & EXT2_FT_DIR) && !(src_dir->file_type & EXT2_FT_REG_FILE) ){
		//TODO: erroe value should prob be something else
		return EISDIR;
	}
	//We find the last dir_entry in the parent path of
	//dest_path to prepare to attach a dir_entry to the end of the linked list

	//get destination's parent directory inode
	char * dest_parent = get_parent_path(dest_path);
	struct ext2_inode *dest_dir_inode = get_inode(dest_parent);

	//get the last dir_entry in the parent directory
	struct ext2_dir_entry * lastDir = find_last_dir(dest_dir_inode);
	//lastDir no longer the last dir.. remove its padding
	int padding = remove_padding(lastDir);

	struct ext2_dir_entry * newDir = (struct ext2_dir_entry *)(((char *) lastDir) + lastDir->rec_len);

	int inode_index = get_inode_index();
	int block_index = get_block_index();


	struct ext2_inode *link_inode =  setup_inode_link(inode_index,block_index);

	newDir->inode = inode_index;
	newDir->file_type |= EXT2_FT_SYMLINK;

	//name of dest_file without its parents names
	char* dest_file_name = trim_parent_path(dest_path,dest_parent);
	newDir->name_len = strlen(dest_file_name);
	
	memcpy( (char *) ((char *)newDir + sizeof(struct ext2_dir_entry)) ,dest_file_name,  strlen(dest_file_name) );

	//now set the content of the i_block to the path of src
	unsigned char * i_block = (unsigned char *)(disk + 1024*link_inode->i_block[0]) ;
	
	strcpy((char *)i_block,src_path);

	newDir->rec_len = padding;

	update_inode_bitmap(inode_index,1);
	update_block_bitmap(block_index,1);

	sb->s_free_inodes_count -= 1;
	sb->s_free_blocks_count -= 1;
	bg->bg_free_blocks_count -= 1;
	bg->bg_free_inodes_count -= 1;
	
	return 0;
}