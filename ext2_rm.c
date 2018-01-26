#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "ext2.h"
#include "ext2_mkdir.h"
#include "ext2_utils.h"
#include "ext2_rm.h"
#include "time.h"


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

	ext2_rm(argv[2]);
}


/// How to rm:
// simply set the value of previous entry's rec_len to skip over the file.

// 1. find the file we wanna delete, make sure it exists.
// find_dir_in_dir 
// 2. find_dir_before_dir_in_dir get the linked_list object before the dir we are removing

// 3. add the rec_len of dir to the rec_len of before_dir so that dir will get skipped. Done. EZPZ Lemon squeezy
// holy canoly it can't possibly be that easy? nah boi it is yo Im'a write this tommorow I haven't slept in so long yo.

int ext2_rm(char * path){

    int index = (2-1) % sb->s_inodes_per_group;

    struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

    struct ext2_dir_entry *  dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);

    const char s[2] = "/";
	char* nextDir = strtok (path,s);

    while(nextDir != NULL){

        dir_entry = find_dir_in_dir(dir_entry,inode,nextDir);

	int found = strncmp((char *) ((char *)dir_entry + sizeof(struct ext2_dir_entry)),nextDir,strlen(nextDir));

        int dirSize = strlen(nextDir);
        char nameCopy[dirSize+1]; // +1 for safety TODO: maybe remove +1 ?
        memset(nameCopy, '\0',sizeof(nameCopy));
        strcpy(nameCopy,nextDir);

        nextDir = strtok(NULL,s);
		
		if(nextDir == NULL){
            //no next dir and found the last dir == we found the dir to be deleted
			if(found == 0){
                //struct ext2_dir_entry *  find_dir_before_dir_in_dir(struct ext2_dir_entry *  dir_entry ,struct ext2_inode * inode , char * dir_name);
                struct ext2_dir_entry * first_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);
                struct ext2_dir_entry * prev_entry = find_dir_before_dir_in_dir(first_entry,inode,nameCopy);
                
                prev_entry->rec_len += dir_entry->rec_len;

                
                //Set Inode and block bitmap values too zero to potentially be overwritten
                update_inode_bitmap(dir_entry->inode,0);

                int index = (dir_entry->inode-1) % sb->s_inodes_per_group;

                //inode of the to be removed entry
                struct ext2_inode * rm_inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

                rm_inode->i_dtime = (unsigned int )time(NULL); //TODO: double check this
                
                sb->s_free_inodes_count += 1;

                bg->bg_free_inodes_count += 1;

                // TODO: Do we need to do this below?
                for(int z = 0; z < inode->i_blocks / 2 ; z++){
                    update_block_bitmap(rm_inode->i_block[z],0);
                    sb->s_free_blocks_count += 1;
                    bg->bg_free_blocks_count += 1;
                }

            }
            else{
                perror("Directory structure doesn't exist");
            }
		}
		else{
			index = (dir_entry->inode-1) % sb->s_inodes_per_group;
			inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
			dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);
		}

    }


    return 0;
}
