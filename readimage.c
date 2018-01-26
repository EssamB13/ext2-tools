#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;


int get_inode_at_index(int index, unsigned char *inode_bits){
    int inode_index = 1;	
    int found = 0;

    for(int byte = 0; byte < 32/8; byte++){
        for(int bit = 0; bit < 8; bit++){
            if (inode_index == index){					
                return (inode_bits[byte] & (1<<bit)) >> bit;
            }
            inode_index++;
        }
        if (found){
            break;
        }
    }
    return -1;
}

int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);
    struct ext2_group_desc *bg = (struct ext2_group_desc *)(disk + 1024*2);
    printf("Block group: \n");
    printf("    Block bitmap: %d\n", bg->bg_block_bitmap);
    printf("    Block bitmap: %d\n", bg->bg_inode_bitmap);
    printf("    Block bitmap: %d\n", bg->bg_inode_table);
    printf("    Block bitmap: %d\n", bg->bg_free_blocks_count);
    printf("    Block bitmap: %d\n", bg->bg_free_inodes_count);
    printf("    Block bitmap: %d\n", bg->bg_used_dirs_count);
    // unsigned char bytes[4];
    // bytes =(disk + 1024*3);

    unsigned char *block_bits = (unsigned char *)(disk + 1024 *bg->bg_block_bitmap);
    
    unsigned int in_use;
    for(int byte = 0; byte < sb->s_blocks_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
            in_use = (block_bits[byte] & (1<<bit)) >> bit;
            printf("%d", in_use);
            if(bit == 7)
            printf(" ");

        }
    }
    printf("\n");

    unsigned char *inode_bits = (unsigned char *)(disk + 1024 *bg->bg_inode_bitmap);


    for(int byte = 0; byte < sb->s_inodes_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
            in_use = (inode_bits[byte] & (1<<bit)) >> bit;
            printf("%d", in_use);
            if(bit == 7)
            printf(" ");
            
            
        }
    }
    printf("\n");

    ///    
    // unsigned int block_group = (2-1) / sb->s_inodes_per_group;
    int index = (2-1) % sb->s_inodes_per_group;

    //unsigned int containing_block = (index * sb->s_inode_size)/EXT2_BLOCK_SIZE;

    unsigned char * inode_table = (unsigned char *)(disk + 1024*bg->bg_inode_table);

    struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

    unsigned int mode = inode->i_mode;
    char mode_c;

    if(mode & EXT2_S_IFREG){
        mode_c = 'f';
    }
    else if(mode & EXT2_S_IFLNK ){
        mode_c = 'l';
    }
    else if(mode & EXT2_S_IFDIR ){
        mode_c = 'd';
    }

    printf("Inodes:\n");

    printf("[2] type: %c size: %d links: %d blocks: %d \n" ,mode_c, inode->i_size,inode->i_links_count,inode->i_blocks  );

    printf("[2] Blocks: %d\n", inode->i_block[0]);


    for(int i = 12; i < 32; i++){
        //(inode_bits[byte] & (1<<bit)) >> bit;
        
        index = (i-1) % sb->s_inodes_per_group;
        inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
        if(inode->i_size == 0){
            continue;
        }
        unsigned short mode = inode->i_mode;
        char mode_c;

        if(mode & EXT2_S_IFREG){
            mode_c = 'f';
        }
        else if(mode & EXT2_S_IFLNK ){
            mode_c = 'l';
        }
        else if(mode & EXT2_S_IFDIR ){
            mode_c = 'd';
        }
            printf("[%d] type: %c size: %d links: %d blocks: %d \n" ,i,mode_c, inode->i_size,inode->i_links_count,inode->i_blocks  );

        printf("[%d] Blocks: %d\n", i,inode->i_block[0]);
    }

    printf("Directory Blocks:\n");

    
    index = (2-1) % sb->s_inodes_per_group;
    inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
    int i = 0;
    printf("   DIR BLOCK NUM: %d (for inode 2)\n",inode->i_block[0]);
    while(inode->i_block[i] != 0){
        int cont = 1;
        struct ext2_dir_entry *  dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[i]);
        while(cont){
            
            char * name = (char *) ((char *)dir_entry + sizeof(struct ext2_dir_entry));
            char mode_c;

            if(mode & EXT2_S_IFREG){
                mode_c = 'f';
            }
            else if(mode & EXT2_S_IFLNK ){
                mode_c = 'l';
            }
            else if(mode & EXT2_S_IFDIR ){
                mode_c = 'd';
            }

            printf("Inode: %d rec_len: %d name_len: %d type= %c name=%.*s\n", dir_entry->inode,dir_entry->rec_len,dir_entry->name_len, mode_c, dir_entry->name_len,name );
            dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) + dir_entry->rec_len);
            if((unsigned char *)(dir_entry) - (unsigned char *)(disk + 1024*inode->i_block[i]) >= 1024 ){
                cont=0;
            }
        }


        i++;
    }



    for(int i = 12; i < 32; i++){
        index = (i-1) % sb->s_inodes_per_group;
        inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
        if(!(inode->i_mode & EXT2_S_IFDIR) || get_inode_at_index(i, inode_bits) == 0){
            continue;
        }
        printf("   DIR BLOCK NUM: %d (for inode %d)\n",inode->i_block[0], i);
        int z = 0;
        while(inode->i_block[z] != 0){
            int cont = 1;
            struct ext2_dir_entry *  dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[z]);
            while(cont){
                
                char * name = (char *) ((char *)dir_entry + sizeof(struct ext2_dir_entry));
                char mode_c;

                if(mode & EXT2_S_IFREG){
                    mode_c = 'f';
                }
                else if(mode & EXT2_S_IFLNK ){
                    mode_c = 'l';
                }
                else if(mode & EXT2_S_IFDIR ){
                    mode_c = 'd';
                }

                printf("Inode: %d rec_len: %d name_len: %d type= %c name=%.*s\n", dir_entry->inode,dir_entry->rec_len,dir_entry->name_len, mode_c, dir_entry->name_len,name );
                dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) + dir_entry->rec_len);
                if((unsigned char *)(dir_entry) - (unsigned char *)(disk + 1024*inode->i_block[z]) >= 1024 ){
                    cont=0;
                }
            }


            z++;
        }
    }
    


    return 0;
}



