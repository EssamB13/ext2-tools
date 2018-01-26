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
#include "ext2_checker.h"



unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_group_desc *bg;
unsigned char * inode_table;
struct ext2_inode * rootInode;

int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name> \n", argv[0]);
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

	return ext2_checker(argv[1]);
}


int ext2_checker(char * path){

    int fixes = 0;

    // check 1.
    int inode_count = count_unused_inodes();
    int block_count = count_unused_blocks();

    if(inode_count != sb->s_free_inodes_count){
        int difference = abs(inode_count - sb->s_free_inodes_count);
        printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap\n",difference);
        sb->s_free_inodes_count = inode_count;
        fixes++;
        
    }

    if(inode_count != bg->bg_free_inodes_count){
        int difference = abs(inode_count - bg->bg_free_inodes_count);
        printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n",difference);
        bg->bg_free_inodes_count = inode_count;
        fixes++;
    }


    if(block_count != sb->s_free_blocks_count){
        int difference = abs(block_count - sb->s_free_blocks_count);
        printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap\n",difference);
        sb->s_free_blocks_count = block_count;
        fixes++;
    }

    if(block_count != bg->bg_free_blocks_count){
        int difference = abs(block_count - bg->bg_free_blocks_count);
        printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n",difference);
        bg->bg_free_blocks_count = block_count;
        fixes++;
    }      

    //check 1 ends

    //all other checks in check_inodes()

    fixes += check_inodes();


    /*
"N file system inconsistencies repaired!", where N is the number of fixes made, or "No file system inconsistencies detected!". 

    */

    if(fixes){
        printf("%d file system inconsistencies repaired!\n",fixes);        
    }
    else{
        printf("No file system inconsistencies detected!\n");
    }

    return 0;


}

/*
#define    EXT2_S_IFLNK  0xA000   
#define    EXT2_S_IFREG  0x8000    
#define    EXT2_S_IFDIR  0x4000 

#define    EXT2_FT_UNKNOWN  0  
#define    EXT2_FT_REG_FILE 1    
#define    EXT2_FT_DIR      2   
#define    EXT2_FT_SYMLINK  7   

"Fixed: Entry type vs inode mismatch: inode [I]", where I is the inode number for the respective file system object

TODO: And, for non-directory, regular files, you'll only need to provide support for at most a single level of indirection.
*/

// check I mode, return amount of inconsistencies, -1 on error 
int check_inodes(){

    int fixes = 0;

    int i = 2;
    while (i < 32){

        //if bitmap at index  i is 0, continue
        if(!get_inode_at_index(i) && i != 2){
            i++;
            continue;
        }

        int index = (i-1) % sb->s_inodes_per_group;
        //folder inode
        struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
        //go through dir_entries of all directories. A file will be in a directory and get checked so we
        //don't need to check it now
        if(inode->i_mode & EXT2_S_IFDIR){
            struct ext2_dir_entry * dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);
            
            int cont = 1;

            while(cont){

                index = (dir_entry->inode-1) % sb->s_inodes_per_group;
                struct ext2_inode * dir_inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

            
                //check 3 start
                //check if inode is initialized in bitmap
                if (!get_inode_at_index(dir_entry->inode)){
                    update_inode_bitmap(dir_entry->inode,1);
                    fixes++;
                    printf("Fixed: inode [%d] not marked as in-use\n",dir_entry->inode);
                }
                //check 3 end

                //check 4 start
                if(dir_inode->i_dtime != 0){
                    inode->i_dtime = 0;
                    fixes++;
                    printf("Fixed: valid inode marked for deletion: [%d]\n",dir_entry->inode);
                }
                //check 4 end
            
                //check 5 start
                int invalid_blocks = 0;
                for (int z = 0; z < dir_inode->i_blocks/2; z++){

                    if(!get_block_at_index(dir_inode->i_block[z])){

                        update_block_bitmap(dir_inode->i_block[z],1);
                        fixes++;
                        invalid_blocks++;

                    } 

                }
                if(invalid_blocks){
                    printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n",invalid_blocks,dir_entry->inode);                
                }
                //check 5 end


                if((dir_inode->i_mode & EXT2_S_IFDIR) && !(dir_entry->file_type & EXT2_FT_DIR) ){

                    dir_entry->file_type |= EXT2_FT_DIR;

                    fixes++;
                    printf("Fixed: Entry type vs inode mismatch: inode [%d]\n",dir_entry->inode);

                }
                else if((dir_inode->i_mode & EXT2_S_IFREG) && !(dir_entry->file_type & EXT2_FT_REG_FILE) ){
                    
                    dir_entry->file_type |= EXT2_FT_REG_FILE;                

                    fixes++;
                    printf("Fixed: Entry type vs inode mismatch: inode [%d]\n",dir_entry->inode);
                    
                }
                else if((inode->i_mode & EXT2_S_IFLNK) && !(dir_entry->file_type & EXT2_FT_SYMLINK) ){

                    dir_entry->file_type |= EXT2_FT_SYMLINK;                

                    fixes++;
                    printf("Fixed: Entry type vs inode mismatch: inode [%d]\n",dir_entry->inode);            
                                    
                }

                dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) + dir_entry->rec_len);


                if((unsigned char *)(dir_entry) - (unsigned char *)(disk + 1024*inode->i_block[0]) >= 1024 ){
                    cont=0;
                
                }

            }  
        }

        if(i == 2){
            i = 12;
        }    
        else{
            i++;
        }

    }

    return fixes;

}
