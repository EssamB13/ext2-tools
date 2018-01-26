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
#include "ext2_restore.h"
#include <errno.h>

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

	ext2_restore(argv[2]);
}

int ext2_restore(char * path){

    //first get root inode, and the first dir_entry in it
    int index = (2-1) % sb->s_inodes_per_group;
    struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
    struct ext2_dir_entry *  dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);

    //break the path into its parts seperated by /
    const char s[2] = "/";
    char* nextDir = strtok (path,s);

    //traverse the path on dir at a time, ensure the directories exist and process the final directory
    while(nextDir != NULL){

        dir_entry = find_dir_in_dir(dir_entry,inode,nextDir);

	int found = strncmp((char *) ((char *)dir_entry + sizeof(struct ext2_dir_entry)),nextDir,strlen(nextDir));

        int dirSize = strlen(nextDir);
        char nameCopy[dirSize+1]; // +1 for safety TODO: maybe remove +1 ?
        memset(nameCopy, '\0',sizeof(nameCopy));
        strcpy(nameCopy,nextDir);

        nextDir = strtok(NULL,s);
		
        //name Copy holds the file we want to restore
		if(nextDir == NULL){
            //no next dir and found the last dir == we found the dir to be deleted
			if(found != 0){
                //struct ext2_dir_entry *  find_dir_before_dir_in_dir(struct ext2_dir_entry *  dir_entry ,struct ext2_inode * inode , char * dir_name);
                struct ext2_dir_entry * first_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);
                struct ext2_dir_entry * susp_entry = find_del_dir_in_dir(first_entry,inode,nameCopy);
                int leftOver = susp_entry->name_len; 
                int real_rec_len = sizeof(struct ext2_dir_entry) + leftOver + (4-((sizeof(struct ext2_dir_entry) + leftOver) % 4));
                int stated_rec_len = susp_entry->rec_len;
                int total_rec_len = real_rec_len;
                struct ext2_dir_entry * deleted_entry;

                while(total_rec_len != stated_rec_len){
                    deleted_entry = (struct ext2_dir_entry *)(((char *) susp_entry) + total_rec_len);

                    found = strncmp((char *) ((char *)deleted_entry + sizeof(struct ext2_dir_entry)),nameCopy,strlen(nameCopy));

                    if(found == 0){
                        break;
                    }

                    total_rec_len += deleted_entry->rec_len;
                }
                

                
                //if found == 0, we confirm the name of the recovered entry
                // now we must ensure the inode and block data is preserved
                if(found == 0 ){
                    

                    int inode_valid = get_inode_at_index(deleted_entry->inode);

                    if(inode_valid){

                        int deleted_index = (deleted_entry->inode-1) % sb->s_inodes_per_group;

                        struct ext2_inode * deleted_inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * deleted_index);

                        int block_valid = 1;

                        for(int z = 0; z < deleted_inode->i_blocks / 2 ; z++){

                            if (!get_block_at_index(deleted_inode->i_block[z])){
                                block_valid = 0;
                                break;
                            }

                        }
                        if(!block_valid){
                            exit(ENOENT);
                        }
                    }
                    else{
                        exit(ENOENT);
                    }


                    //int block_valid = get_block_at_index()
                    update_inode_bitmap(deleted_entry->inode,1);
                    //after additional checks
                    int index = (deleted_entry->inode-1) % sb->s_inodes_per_group;
                    struct ext2_inode * restore_inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
                    
                    restore_inode->i_dtime=0; 
                    
                    for(int z = 0; z < inode->i_blocks / 2 ; z++){
                        update_block_bitmap(restore_inode->i_block[z],1);
                        sb->s_free_blocks_count -= 1;
                        bg->bg_free_blocks_count -= 1;
                    }

                    sb->s_free_inodes_count -= 1;
                    bg->bg_free_inodes_count -= 1;


                    susp_entry->rec_len = total_rec_len;

                    return 0;
                }
                else{
                    continue;
                }

            }
            else{
                perror("File wasn't deleted");
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
