
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





//TODO: minor error checking in all these functions
// find if a directory is in the directory dir_entry of inode, inode.
struct ext2_dir_entry * find_dir_in_dir(struct ext2_dir_entry *  dir_entry ,struct ext2_inode * inode , char * dir_name){

	
	int cont = 1;

	while(cont){

		/*
ts scope is only this definition or declaration, which is probably not what you want
ext2_cp.c: In function ‘ext2_cp’:
			int strncmp(const char *str1, const char *str2, size_t n)

			Compares at most the first n bytes of str1 and str2.

		*/
		char * name = (char *) ((char *)dir_entry + sizeof(struct ext2_dir_entry));


		// if dir_entry name  == dir_name we have a match
		if(strncmp(name,dir_name,dir_entry->name_len) == 0){
			// found a name match return 0
			return dir_entry;
		}
		int prevRecLen = dir_entry->rec_len;
        dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) + dir_entry->rec_len);
		
        if((unsigned char *)(dir_entry) - (unsigned char *)(disk + 1024*inode->i_block[0]) >= 1024 ){
        	//revert to last valid dir_entry before breaking loop and returning ;
        	dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) - prevRecLen);
            cont=0;
        }

	}

	return dir_entry;
}


// find the last dir object in a inode
struct ext2_dir_entry * find_last_dir(struct ext2_inode * inode){
	
	int cont = 1;

	struct ext2_dir_entry *  dir_entry = (struct ext2_dir_entry * ) (disk + 1024*inode->i_block[0]);

	while(cont){

		/*
			int strncmp(const char *str1, const char *str2, size_t n)

			Compares at most the first n bytes of str1 and str2.

		*/
		//char * name = (char *) ((char *)dir_entry + sizeof(struct ext2_dir_entry));

		int prevRecLen = dir_entry->rec_len;
        dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) + dir_entry->rec_len);
		
        if((unsigned char *)(dir_entry) - (unsigned char *)(disk + 1024*inode->i_block[0]) >= 1024 ){
        	//revert to last valid dir_entry before breaking loop and returning ;
        	dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) - prevRecLen);
            cont=0;
        }

	}

	return dir_entry;
}

struct ext2_dir_entry *  find_del_dir_in_dir(struct ext2_dir_entry *  dir_entry ,struct ext2_inode * inode , char * dir_name){

	int cont = 1;

	while(cont){

		int leftOver = dir_entry->name_len; 
		
		int real_rec_len = sizeof(struct ext2_dir_entry) + leftOver;
		real_rec_len += (4-real_rec_len % 4);

		int stated_rec_len = dir_entry->rec_len;

		printf("real rec length: %d   stated rec length: %d\n",real_rec_len,stated_rec_len);

		//we need a while loop to handle cases where we have multiple deleted entries in a row.
		int total_rec_len = real_rec_len;

		//TODO: make this a reuseable method
		while(total_rec_len != stated_rec_len){

			printf("rec_len mismatch\n");

			struct ext2_dir_entry * deleted_entry = (struct ext2_dir_entry *)(((char *) dir_entry) + total_rec_len);

			char * name = (char *) ((char *)deleted_entry + sizeof(struct ext2_dir_entry));



			printf("name of deleted entry:  name=%.*s\n",deleted_entry->name_len,name);

			// if dir_entry name  == dir_name we have a match
			if(strncmp(name,dir_name,deleted_entry->name_len) == 0){
				// found a name match return 0
				printf("found a name match \n");
				return dir_entry;
			}

			total_rec_len += deleted_entry->rec_len;

		}



		int prevRecLen = dir_entry->rec_len;
        dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) + dir_entry->rec_len);
		
        if((unsigned char *)(dir_entry) - (unsigned char *)(disk + 1024*inode->i_block[0]) >= 1024 ){
        	//revert to last valid dir_entry before breaking loop and returning ;
        	dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) - prevRecLen);
            cont=0;
        }

	}
	printf("no name match found\n");
	return dir_entry;
}

//like find_dir_in_dir but returns dir_entry right before dir in the linked list. Used for ext2_rm
struct ext2_dir_entry *  find_dir_before_dir_in_dir(struct ext2_dir_entry *  dir_entry ,struct ext2_inode * inode , char * dir_name){

	int cont = 1;

	int prevRecLen;

	while(cont){

		/*
			int strncmp(const char *str1, const char *str2, size_t n)

			Compares at most the first n bytes of str1 and str2.

		*/
		char * name = (char *) ((char *)dir_entry + sizeof(struct ext2_dir_entry));


		//printf("name of directory inside directory: %s\n",name);

		// if dir_entry name  == dir_name we have a match
		if(strncmp(name,dir_name,dir_entry->name_len) == 0){
			// found a name match return 0
			dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) - prevRecLen);
			return dir_entry;
		}
		prevRecLen = dir_entry->rec_len;
        dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) + dir_entry->rec_len);
		
        if((unsigned char *)(dir_entry) - (unsigned char *)(disk + 1024*inode->i_block[0]) >= 1024 ){
        	//revert to last valid dir_entry before breaking loop and returning ;
        	dir_entry = (struct ext2_dir_entry *)(((char *) dir_entry) - prevRecLen);
            cont=0;
        }

	}

	return dir_entry;
}

//returns the first free Inode index
int get_inode_index(){
	unsigned char *inode_bits = (unsigned char *)(disk + 1024 *bg->bg_inode_bitmap);
	int inode_index = 1;	
	int found = 0;
    unsigned int in_use;

    for(int byte = 0; byte < sb->s_inodes_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
            in_use = (inode_bits[byte] & (1<<bit)) >> bit;
            if (in_use == 0){					
				found = 1;
				break;
			}
			inode_index++;
        }
		if (found){
			break;
		}
    }
	return inode_index;
}

int get_inode_at_index(int index){
	unsigned char *inode_bits = (unsigned char *)(disk + 1024 *bg->bg_inode_bitmap);
	int inode_index = 1;	
	int found = 0;

    for(int byte = 0; byte < sb->s_inodes_count/8; byte++){
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


int get_block_at_index(int index){

	unsigned char *block_bits = (unsigned char *)(disk + 1024 *bg->bg_block_bitmap);
	int block_index = 1;
	int found = 0;

    for(int byte = 0; byte < sb->s_blocks_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
			if (block_index == index){
				return (block_bits[byte] & (1<<bit)) >> bit;
        	}
			block_index++;
        }
		if (found){
			break;
		}
	}

	return -1;		

}

//return first free block index
int get_block_index(){

	unsigned char *block_bits = (unsigned char *)(disk + 1024 *bg->bg_block_bitmap);
	int block_index = 1;
	int found = 0;
    unsigned int in_use;

    for(int byte = 0; byte < sb->s_blocks_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
            in_use = (block_bits[byte] & (1<<bit)) >> bit;
			if (in_use == 0){
				found = 1;
				break;
        	}
			block_index++;
        }
		if (found){
			break;
		}
	}

	return block_index;		

}

//Count all inodes with value 0 in inode bitmap
int count_unused_inodes(){
	unsigned char *inode_bits = (unsigned char *)(disk + 1024 *bg->bg_inode_bitmap);	
	int count = 0;

    for(int byte = 0; byte < sb->s_inodes_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
            if(!(inode_bits[byte] &  (1<<bit))){
            	count++;
            }
        }
    }
	return count;
}

//Count all blocks with value 0 in block bitmap
int count_unused_blocks(){

	unsigned char *block_bits = (unsigned char *)(disk + 1024 *bg->bg_block_bitmap);
	int count = 0;

    for(int byte = 0; byte < sb->s_blocks_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
			if(!(block_bits[byte] &  (1<<bit))){
				count++;
			}
		}
	}

	return count;		
}



//update inode bitmap, return -1 on error. 0 on success
int update_inode_bitmap(int index,int value){
	unsigned char *inode_bits = (unsigned char *)(disk + 1024 *bg->bg_inode_bitmap);
	int inode_index = 1;	
	int found = -1;

    for(int byte = 0; byte < sb->s_inodes_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
            if (inode_index == index){	
				if(value){
					inode_bits[byte] |= (1<<bit);
				}	
				else{
					inode_bits[byte] &= ~(1<<bit);
				}			
				
				found = 0;
				break;
			}
			inode_index++;
        }
		if (found == 0){
			break;
		}
    }
	return found;
}

//update block bitmap, return -1 on error. 0 on success
int update_block_bitmap(int index, int value){

	unsigned char *block_bits = (unsigned char *)(disk + 1024 *bg->bg_block_bitmap);
	int block_index = 1;
	int found = -1;

    for(int byte = 0; byte < sb->s_blocks_count/8; byte++){
        for(int bit = 0; bit < 8; bit++){
			if (block_index == index){	
				if(value){
					block_bits[byte] |= (1<<bit);
				}
				else{
					block_bits[byte] &= ~(1<<bit);
				}
				
				found = 0;
				break;
        	}
			block_index++;
        }
		if (found == 0){
			break;
		}
	}

	return block_index;		

}

//setup an inode for a directory
struct ext2_inode * setup_inode_link(int inode_index, int block_index){
	//get inode that we want using index_index
	int index = (inode_index-1) % sb->s_inodes_per_group;
	struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

	//setup inode.
	inode->i_mode |= EXT2_S_IFLNK; // gotta add permissions somehow?
	inode->i_size = 1024; //for directories this always 1024?? TODO:
	inode->i_blocks = 2; //represents 512 byte blocks, i_block has 1024 sized blocks, 
	inode->i_links_count = 1; //2 should be the amount for a new directory? TODO: //  . and .. are links or something....
	// hard links and soft links.
	// Hard links: multiple file names mapped to the same inode.
	// soft link: pointer to a given file, contains the path.
	inode->i_block[0] = block_index;
	inode->i_dtime = 0;
	return inode;
}

//setup an inode for a directory
struct ext2_inode * setup_inode_directory(int inode_index, int block_index){
	//get inode that we want using index_index
	int index = (inode_index-1) % sb->s_inodes_per_group;
	struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

	//setup inode.
	inode->i_mode |= EXT2_S_IFDIR; // gotta add permissions somehow?
	inode->i_size = 1024; //for directories this always 1024?? TODO:
	inode->i_blocks = 2; //represents 512 byte blocks, i_block has 1024 sized blocks, 
	inode->i_links_count = 2; //2 should be the amount for a new directory? TODO: //  . and .. are links or something....
	// hard links and soft links.
	// Hard links: multiple file names mapped to the same inode.
	// soft link: pointer to a given file, contains the path.
	inode->i_block[0] = block_index;
	inode->i_dtime = 0;
	return inode;
}

//setup an inode for a file
struct ext2_inode * setup_inode_directory_file(int inode_index, int block_index){
	//get inode that we want using index_index
	int index = (inode_index-1) % sb->s_inodes_per_group;
	struct ext2_inode * inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);

	//setup inode.
	inode->i_mode |= EXT2_S_IFREG; // gotta add permissions somehow?
	inode->i_size = 1024; 
	inode->i_blocks = 2;  
	inode->i_links_count = 1; 
	inode->i_block[0] = block_index;
	inode->i_dtime = 0;
	return inode;
}

int setup_dir_entry(struct ext2_dir_entry * entry, int inode_index,int name_len,int rec_len,unsigned char file_type, char * name){

	entry->inode = inode_index;
	entry->name_len = name_len;	
	entry->rec_len = rec_len;	
	memcpy( (char *) ((char *)entry + sizeof(struct ext2_dir_entry)) ,name,  strlen(name) );
	entry->file_type = file_type;
	return 0;
	
}
//return the inode to given path on disk 
struct ext2_inode *get_inode(char *path){

    // start at first inode in the inode table
    int index = (2-1) % sb->s_inodes_per_group;

    	
	
    struct ext2_inode *curr_inode = (struct ext2_inode*) (inode_table + sb->s_inode_size * index);
	
    //returns root inode if the path is null or "/".
    
    if(path == NULL){
	return curr_inode;
    }
    
    if(!strncmp(path, "/", strlen(path))){
    	return curr_inode; 
    }

    struct ext2_dir_entry *dir_entry; 
	
    unsigned int block_count = 0;
    int block_remains;  

    char *parent_path = malloc(sizeof(char) * (strlen(path) + 1));
    strncpy(parent_path, path, strlen(path));

    char *path_split = strtok(parent_path, "/"); 

    while(block_count < 12 && path_split != NULL && curr_inode->i_block[block_count] &&  (curr_inode->i_mode & EXT2_S_IFDIR)){
	
    	block_remains = EXT2_BLOCK_SIZE;
    	dir_entry = (struct ext2_dir_entry *)(disk + (curr_inode->i_block[block_count] * EXT2_BLOCK_SIZE));
	
	// go through the block until we run out of block space.
    	while(block_remains){

    		if(!strncmp(path_split, dir_entry->name, dir_entry->name_len)){
    			index = (dir_entry->inode - 1) % sb->s_inodes_per_group;
			curr_inode = (struct ext2_inode *)(inode_table + sb->s_inode_size * index);
			block_count = 0;
    			path_split = strtok(NULL, "/");	
    			block_remains = EXT2_BLOCK_SIZE;
			break;
    		}
		block_remains -= dir_entry->rec_len;
		dir_entry = (struct ext2_dir_entry *)((char *)dir_entry + dir_entry->rec_len);
    	}
	// we used up the whole block
        if(block_remains == 0){
            block_count++;
        }
    }
    if(!path_split){
	return curr_inode;
    } else {
	return NULL;
    }
    
        
}
// return the path to the parent directory of path
char *get_parent_path(char *path) {
    
    char *parent_path = malloc(strlen(path));
    strncpy(parent_path, path, strlen(path));
    int i;
    for (i = strlen(path)-1; i >= 0; i--){
        //we found a trailing slash
	if ((i == strlen(path)-1 ) && parent_path[i] == '/'){
			continue;
	}
	if (parent_path[i] == '/') {
            parent_path[i + 1] = '\0';
            break;
    	}
    }

    if(strcmp(parent_path,path) == 0 || strcmp(parent_path,"/") == 0 ){
		return NULL;
    }
    return parent_path;
}

//trim parent from path, returning path without prefix parent and without slashes
char * trim_parent_path(char *path, char* parent){

	// f1/f2/f3/f4/f5
	if(parent == NULL){
		char * path_no_slash = strtok(path,"/");
		return path_no_slash;
	}
	int parent_len = strlen(parent);

	char * path_no_slash = strtok(path+parent_len,"/");

	return path_no_slash;

}

//TODO: error checking
int remove_padding (struct ext2_dir_entry *dir_entry){
		/// must fix rec_len of dir_entry as its currently taking up all remaining space
	int oldRecLen = dir_entry->rec_len;
	int leftOver = dir_entry->name_len;
	dir_entry->rec_len = sizeof(struct ext2_dir_entry) + leftOver + (4-((sizeof(struct ext2_dir_entry) + leftOver) % 4));
	//dir_entry should be good now... should haha.
	return oldRecLen-dir_entry->rec_len;

}
