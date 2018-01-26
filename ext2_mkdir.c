// ext2_mkdir: This program takes two command line arguments. 
//	The first is the name of an ext2 formatted virtual disk. 
//	The second is an absolute path on your ext2 formatted disk. 
//	The program should work like mkdir, creating the final directory on the specified path on the disk. 
//	If any component on the path to the location where the final directory is to be created does not exist
// or if the specified directory already exists, then your program should return the appropriate error (ENOENT or EEXIST).
// Note:

//     Please read the specifications to make sure you're implementing everything correctly (e.g., 
// 	   directory entries should be aligned to 4B, entry names are not null-terminated, etc.).
//     When you allocate a new inode or data block, you *must use the next one available*
//		 from the corresponding bitmap (excluding reserved inodes, of course). Failure to do so will result in deductions,
//		 so please be careful about this requirement.
//     Be careful to consider trailing slashes in paths. These will show up during testing so it's your
//		 responsibility to make your code as robust as possible by capturing corner cases.

// 								f1/f2/f3/....../f7/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"
#include "ext2_mkdir.h"
#include "ext2_utils.h"


unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_group_desc *bg;
unsigned char * inode_table;
struct ext2_inode * rootInode;

int main(int argc, char **argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <absolute path name>\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[1], O_RDWR);

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

    ext2_mkdir(argv[2]);
}


int ext2_mkdir(char * path){


	//get the root dir inode
	int index = (2-1) % sb->s_inodes_per_group;
	struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
	int headInodeIndex = 2;

	//get dir entry of first dir in root inode
	struct ext2_dir_entry *  dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);

	const char s[2] = "/";
	char* nextDir = strtok (path,s);

	while(nextDir != NULL){

		dir_entry = find_dir_in_dir(dir_entry,inode,nextDir);

		int found = strncmp((char *) ((char *)dir_entry + sizeof(struct ext2_dir_entry)),nextDir,strlen(nextDir));

		//no match found
		if(found != 0){

			// gonna overwrite nextDir so save a copy of it in case its valid.
			int dirSize = strlen(nextDir);
			char nameCopy[dirSize+1]; // +1 for safety TODO: maybe remove +1 ?
			memset(nameCopy, '\0',sizeof(nameCopy));
			strcpy(nameCopy,nextDir);

			nextDir = strtok(NULL,s);

			//this was the last dir name so it wasn't supposed to exist. this is good
			if(nextDir == NULL){
				int status = mk_dir(dir_entry,nameCopy,headInodeIndex);
				return status;
			}
			//this wasn't the last dir name, this is an error 
			else{
				return ENOENT;
			}


		}
		//else we foound it keep going. find_dir_in_dir updates value of dir_entry ezpz

		nextDir = strtok(NULL,s);
		
		if(nextDir == NULL){
			return ENOENT;
		}
		else{
			if(!(dir_entry->file_type & EXT2_FT_DIR)){
				return ENOENT;
			}
			index = (dir_entry->inode-1) % sb->s_inodes_per_group;
			inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
			dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);
			headInodeIndex = dir_entry->inode;
		}


	}

	return 1;

}

int mk_dir(struct ext2_dir_entry *  dir_entry, char * path,int headInodeIndex){

	//get frst free inode index and first free block_index
	int inode_index = get_inode_index();
	int block_index = get_block_index();	
	


	struct ext2_inode * inode = setup_inode_directory(inode_index,block_index);


	int leftOver = dir_entry->name_len;
	dir_entry->rec_len = sizeof(struct ext2_dir_entry) + leftOver + (4-((sizeof(struct ext2_dir_entry) + leftOver) % 4));

	//get inode of dir_entry
	int index = (headInodeIndex-1) % sb->s_inodes_per_group;
	struct ext2_inode * dir_entry_inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

	//setup the new directory dir_entry
	struct ext2_dir_entry * newDir = (struct ext2_dir_entry *)(((char *) dir_entry) + dir_entry->rec_len);
	newDir->inode = inode_index; 
	newDir->name_len = strlen(path);
	leftOver = ( ((unsigned char *)(newDir)) - (unsigned char *)(disk + 1024*dir_entry_inode->i_block[0])  ); 
	newDir->rec_len = 1024-leftOver;  
	memcpy( (char *) ((char *)newDir + sizeof(struct ext2_dir_entry)) ,path,  strlen(path) );
	newDir->file_type = EXT2_FT_DIR; 

	//setup . directory dir_entry
	struct ext2_dir_entry * dot = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);
	leftOver = dir_entry->name_len;
	int rec_len = sizeof(struct ext2_dir_entry) + leftOver + (4-((sizeof(struct ext2_dir_entry) + leftOver) % 4));
	char dotName[1] = ".";
	setup_dir_entry(dot,inode_index,1,rec_len,EXT2_FT_DIR,dotName);

	//setup .. directory dir_entry
	struct ext2_dir_entry * dotdot = (struct ext2_dir_entry *)(((char *) dot) + dot->rec_len);
	leftOver = ( ((unsigned char *)(dotdot)) - (unsigned char *)(disk + 1024*inode->i_block[0])  );
	rec_len = 1024-leftOver;
	char dotdotName[2] = "..";
	setup_dir_entry(dotdot,headInodeIndex,2,rec_len,EXT2_FT_DIR,dotdotName);

	//Update links count of head directory Inode
	dir_entry_inode->i_links_count += 1;


	update_block_bitmap(block_index,1);
	update_inode_bitmap(inode_index,1);
	//UPDATE METADATA
	sb->s_free_inodes_count -= 1;
	sb->s_free_blocks_count -= 1;
	bg->bg_free_blocks_count -= 1;
	bg->bg_free_inodes_count -= 1;
	bg->bg_used_dirs_count += 1;

	return 1;
}
