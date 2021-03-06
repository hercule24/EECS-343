// ext2 definitions from the real driver in the Linux kernel.
#include "ext2fs.h"

// Definitions for ext2cat to compile against.
#include "ext2_access.h"

///////////////////////////////////////////////////////////
//  Accessors for the basic components of ext2.
///////////////////////////////////////////////////////////

// Return a pointer to the primary superblock of a filesystem.
struct ext2_super_block * get_super_block(void * fs) {
    return (struct ext2_super_block *) ((size_t) fs + SUPERBLOCK_OFFSET);
}


// Return the block size for a filesystem.
__u32 get_block_size(void * fs) {
    return 1024 << *((__u32 *) ((size_t) fs + SUPERBLOCK_OFFSET + 24));
}


// Return a pointer to a block given its number.
// get_block(fs, 0) == fs;
void * get_block(void * fs, __u32 block_num) {
    return (void *) ((size_t) fs + get_block_size(fs) * block_num);
}


// Return a pointer to the first block group descriptor in a filesystem. Real
// ext2 filesystems will have several of these, but, for simplicity, we will
// assume there is only one.
struct ext2_group_desc * get_block_group(void * fs, __u32 block_group_num) {
    return (struct ext2_group_desc *) ((size_t) get_super_block(fs) + SUPERBLOCK_SIZE);
}


// first one.
struct ext2_inode * get_inode(void * fs, __u32 inode_num) {
    struct ext2_group_desc *des = get_block_group(fs, 0);
    void *bg_inode_table = get_block(fs, des->bg_inode_table);
    return (struct ext2_inode *) bg_inode_table + (inode_num - 1);
}



///////////////////////////////////////////////////////////
//  High-level code for accessing filesystem components by path.
///////////////////////////////////////////////////////////

// Chunk a filename into pieces.
// split_path("/a/b/c") will return {"a", "b", "c"}.
//
// This one's a freebie.
char ** split_path(char * path) {
    int num_slashes = 0;
    for (char * slash = path; slash != NULL; slash = strchr(slash + 1, '/')) {
        num_slashes++;
    }

    // Copy out each piece by advancing two pointers (piece_start and slash).
    char ** parts = (char **) calloc(num_slashes, sizeof(char *));
    char * piece_start = path + 1;
    int i = 0;
    for (char * slash = strchr(path + 1, '/');
         slash != NULL;
         slash = strchr(slash + 1, '/')) {
        int part_len = slash - piece_start;
        parts[i] = (char *) calloc(part_len + 1, sizeof(char));
        strncpy(parts[i], piece_start, part_len);
        piece_start = slash + 1;
        i++;
    }
    // Get the last piece.
    parts[i] = (char *) calloc(strlen(piece_start) + 1, sizeof(char));
    strncpy(parts[i], piece_start, strlen(piece_start));
    return parts;
}


// Convenience function to get the inode of the root directory.
struct ext2_inode * get_root_dir(void * fs) {
    return get_inode(fs, EXT2_ROOT_INO);
}


// Given the inode for a directory and a filename, return the inode number of
// that file inside that directory, or 0 if it doesn't exist there.
// 
// name should be a single component: "foo.txt", not "/files/foo.txt".
__u32 get_inode_from_dir(void * fs, struct ext2_inode * dir, 
        char * name) {
    __u32 block_size = get_block_size(fs);

    for (int i = 0; i < EXT2_N_BLOCKS; i++) {
        void *block = get_block(fs, dir->i_block[i]); 
        struct ext2_dir_entry_2 *p = (struct ext2_dir_entry_2 *) block;
        struct ext2_dir_entry_2 *end = (struct ext2_dir_entry_2 *) ((size_t) block + block_size);
        while (p < end && p->rec_len != 0) {
            if (strncmp(p->name, name, p->name_len) == 0) {
                return p->inode;             
            }
            p = (struct ext2_dir_entry_2 *) ((size_t) p + p->rec_len);
        }
    } 

    return 0;
}


// Find the inode number for a file by its full path.
// This is the functionality that ext2cat ultimately needs.
__u32 get_inode_by_path(void * fs, char * path) {
    struct ext2_inode *p = get_root_dir(fs);

    int num_slashes = 0;
    for (char * slash = path; slash != NULL; slash = strchr(slash + 1, '/')) {
        num_slashes++;
    }

    char **parts = split_path(path);                

    __u32 num;

    for (int i = 0; i < num_slashes; i++) {
        num = get_inode_from_dir(fs, p, parts[i]);
        free(parts[i]);
        if (num == 0) {
            break;
        }
        p = get_inode(fs, num);
    }
    
    free(parts);
    return num;
}

