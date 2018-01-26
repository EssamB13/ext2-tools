
#include "ext2.h"

extern unsigned char *disk;
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *bg;
extern unsigned char * inode_table;
extern struct ext2_inode * rootInode;

int get_inode_index();

int get_inode_at_index(int index);

int get_block_at_index(int index);

int get_block_index();

int count_unused_inodes();

int count_unused_blocks();

int update_inode_bitmap(int index,int value);

int update_block_bitmap(int index,int value);

char * trim_parent_path(char *path, char* parent);

int remove_padding(struct ext2_dir_entry *dir_entry);

struct ext2_dir_entry *  find_dir_in_dir(struct ext2_dir_entry *  dir_entry ,struct ext2_inode * inode , char * dir_name);

struct ext2_dir_entry * find_last_dir(struct ext2_inode * inode);

struct ext2_dir_entry *  find_dir_before_dir_in_dir(struct ext2_dir_entry *  dir_entry ,struct ext2_inode * inode , char * dir_name);

struct ext2_dir_entry *  find_del_dir_in_dir(struct ext2_dir_entry *  dir_entry ,struct ext2_inode * inode , char * dir_name);

struct ext2_inode * setup_inode_directory(int inode_index, int block_index);

struct ext2_inode * setup_inode_directory_file(int inode_index, int block_index);

struct ext2_inode * setup_inode_link(int inode_index, int block_index);


int setup_dir_entry(struct ext2_dir_entry * entry, int inode_index,int name_len,int rec_len,unsigned char file_type, char * name);

struct ext2_inode * get_inode(char *path);

char *get_parent_path(char *path);
