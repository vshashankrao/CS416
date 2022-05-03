/*
 *  Copyright (C) 2022 CS416/518 Rutgers CS
 *	RU File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>

#include "block.h"
#include "rufs.h"

char diskfile_path[PATH_MAX];
char* disk_path = "./disk";

// Declare your in-memory data structures here
struct superblock *super_block;
bitmap_t inode_bit_map;
bitmap_t data_bit_map;
int inode_blocksize;



/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	if(inode_bit_map == NULL){
		printf("inode Bitmap is Null\n");
		return -1;
	}

	// Step 1: Read inode bitmap from disk
	//printInodeBitMap();
	uint8_t indexOfOpenNode;
	
	
	// Step 2: Traverse inode bitmap to find an available slot
	for(uint8_t i = 0; i < inode_blocksize; i++){
		uint8_t inodeBitmapIdx = get_bitmap(inode_bit_map, i);
		if(inodeBitmapIdx == 0){
			indexOfOpenNode = i;
			break;
		}
	}
	//calculate correct index value
	int idxOfNodeFile = super_block->i_start_blk + indexOfOpenNode;

	// Step 3: Update inode bitmap and write to disk 
	
	set_bitmap(inode_bit_map, indexOfOpenNode);

	bio_write(super_block->i_bitmap_blk, inode_bit_map);

	//printf("get_avail_ino() func END\n\n");


	return idxOfNodeFile;
}


/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	//printf("Entering get_avail_blkno() func\n");

	// Step 1: Read data block bitmap from disk
	if(data_bit_map == NULL){
		printf("Data bit map is Null\n");
		return -1;
	}
	//printDataBitMap();
	
	// Step 2: Traverse data block bitmap to find an available slot
	uint8_t indexOfOpenNode;
	for(int i = 0; i < MAX_DNUM; i++){
		uint8_t dataBitmapIdx = get_bitmap(data_bit_map, i);
		if(dataBitmapIdx == 0){
			indexOfOpenNode = i;
			break;
		}
	}
	
	//calculate correct index value
	int idxOfBlockFile = super_block->d_start_blk + indexOfOpenNode;

	// Step 3: Update data block bitmap and write to disk 
	set_bitmap(data_bit_map, indexOfOpenNode);
	bio_write(super_block->d_bitmap_blk, data_bit_map);

	//printf("get_avail_blkno() func END\n\n");

	return idxOfBlockFile;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number
  uint16_t i;
  uint16_t j = 0;
  uint16_t inodeDiskBlockNumber;

  for(i = super_block->i_start_blk; j < inode_blocksize; i++, j++){
	  struct inode *temp = malloc(sizeof(struct inode));
	  bio_read(i, temp);
	  if(temp->ino == ino){
		  //printf("TEMP value found\n");
		  inodeDiskBlockNumber = i;
		  bio_read(inodeDiskBlockNumber, inode);
		  return 0;
	  }
  }

  // Step 2: Get offset of the inode in the inode on-disk block

  // Step 3: Read the block from disk and then copy into inode structure

	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	uint16_t i;
	uint16_t j = 0;
	uint16_t inodeDiskBlockNumber;

	for(i = super_block->i_start_blk; j < inode_blocksize; i++, j++){
		struct inode *temp = malloc(sizeof(struct inode));
		bio_read(i, temp);
		if(temp->ino == ino){
			//printf("TEMP value found\n");
			inodeDiskBlockNumber = i;
			bio_write(inodeDiskBlockNumber, inode);
			return 0;
		}
	}
	
	// Step 2: Get the offset in the block where this inode resides on disk

	// Step 3: Write inode to disk 

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
  struct inode *inode = malloc(sizeof(struct inode));

  readi(ino, inode);

  // Step 2: Get data block of current directory from inode

  for(int i = 0; i < 16; i++){
	  if(inode->direct_ptr[i] != 0){
		  struct dirent *directoryEntry = malloc(sizeof(struct dirent));
		  bio_read((super_block->d_start_blk + inode->direct_ptr[i]), directoryEntry);
			if(strcmp(fname, directoryEntry->name) == 0){
				dirent->ino = directoryEntry->ino;
				dirent->valid = directoryEntry->valid;
				strcpy(dirent->name, directoryEntry->name);
				return 0;
			}

	  }
  }

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	for(int i = 0; i < 16; i++){
		if(dir_inode.direct_ptr[i] != 0){
			struct dirent *directoryEntry = malloc(sizeof(struct dirent));
			bio_read(dir_inode.direct_ptr[i], directoryEntry);
			if(strcmp(fname, directoryEntry->name) == 0){
				printf("File name exists \n");
				return 0;
			}


		}
	}
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	for(int i = 0; i < 16; i++){
		if(dir_inode.direct_ptr[i] == 0){
			int nextAvailBlock = get_avail_blkno();
			dir_inode.direct_ptr[i] = nextAvailBlock;
			struct dirent *newEntry = malloc(sizeof(struct dirent));
			newEntry->ino = f_ino;
			newEntry->valid = 1;
			strcpy(newEntry->name, fname);
			bio_write(nextAvailBlock, newEntry);
			break;
		}


	}

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	bool fileExists = false;
	int idxOfFile;

	// Step 2: Check if fname exist
	for(int i = 0; i < 16; i++){
		if(dir_inode.direct_ptr[i] != 0){
			struct dirent *directoryEntry = malloc(sizeof(struct dirent));
			bio_read(dir_inode.direct_ptr[i], directoryEntry);

			if(strcmp(fname, directoryEntry->name) == 0){
				idxOfFile = i;
				fileExists = true;
			}
		}
	}
	if(fileExists == false){
		printf("FNF Error \n");
		return 0;
	}

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	dir_inode.direct_ptr[idxOfFile] = 0;
	bio_write(super_block->i_start_blk + dir_inode.ino, &dir_inode);

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(disk_path);
	//printf("DISK PATh is %s\n", disk_path);


	int inodeMem = (sizeof(struct inode) * MAX_INUM);
	inode_blocksize = inodeMem / BLOCK_SIZE;


	// write superblock information
	super_block = malloc(sizeof(struct superblock));
	super_block->magic_num = MAGIC_NUM;
	super_block->max_inum = MAX_INUM;
	super_block->max_dnum = MAX_INUM;
	super_block->i_bitmap_blk = 1;
	super_block->d_bitmap_blk = 2;
	super_block->i_start_blk = 3;
	super_block->d_start_blk = (3 + inode_blocksize) + 1;

	dev_open(disk_path);
	bio_write(0, super_block);

	// initialize inode bitmap
	inode_bit_map = malloc(inode_blocksize * sizeof(bitmap_t));

	// initialize data block bitmap
	data_bit_map = malloc(MAX_DNUM * sizeof(bitmap_t));

	// update bitmap information for root directory
	set_bitmap(inode_bit_map, 0);
	set_bitmap(data_bit_map, 0);

	bio_write(super_block->i_bitmap_blk, inode_bit_map);
	bio_write(super_block->d_bitmap_blk, data_bit_map);

	struct inode *root = malloc(sizeof(struct inode));

	// update inode for root directory
	root->ino = 0;
	root->valid = 1;
	root->vstat.st_uid = getuid();
	root->vstat.st_gid = getgid();
	root->vstat.st_ino = 0;
	root->vstat.st_mode = 0755;
	root->vstat.st_size = -1;
	root->vstat.st_blksize = BLOCK_SIZE;
	root->vstat.st_blocks = -1;

	bio_write(super_block->i_start_blk, &root);
	struct dirent *rootDirent = malloc(sizeof(struct dirent));
	rootDirent->ino = 0;
	rootDirent->valid = 1;
	strcpy(rootDirent->name, "./");

	bio_write(super_block->d_start_blk, &rootDirent);

	for(int i = 0; i < inode_blocksize; i++){
		struct inode *newInode = malloc(sizeof(struct inode));
		newInode->ino = i;
		newInode->valid = 0;
		newInode->vstat.st_ino = i;
		newInode->vstat.st_mode = 0666;
		newInode->vstat.st_size = -1;
		newInode->vstat.st_blksize = BLOCK_SIZE;
		newInode->vstat.st_blocks = -1;
		bio_write(super_block->i_start_blk+i, &newInode);
	}

	//printf("RUFS MKFS func is complete\n\n");


	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	if(access(disk_path, F_OK) == -1){
		FILE *fp;
		fp = fopen(disk_path, "w+");
		fclose(fp);
	}
	rufs_mkfs();

  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(inode_bit_map);
	free(data_bit_map);
	free(super_block);

	// Step 2: Close diskfile
	dev_close();

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, inode);

	// Step 2: fill attribute of file into stbuf from inode

		//if the path is a directory
		//printf("PATH IS %s\n", path);
		if(strcmp(path, "/") == 0){
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		}
		else{
			stbuf->st_mode = S_IFREG | 0644;
			stbuf->st_nlink = 1;
			stbuf->st_size = 1024;
		}

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *pathINode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, pathINode);

	// Step 2: If not find, return -1

	if(pathINode == NULL){
		printf("Inode from path not found\n");
		return -1;
	}

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, inode);

	// Step 2: Read directory entries from its data blocks, and copy them to filler
	for(int i = 0; i < 16; i++){
		//readi();
	}

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *targetDirectory = "";
	char targetDirPath[100];

	char *targetToken;
	char dirpath[100];
	char tmp[100];

	const char delimiter[4] = "/";
	strcpy(dirpath, delimiter);


	char *token;
	char filepath[100];
	const char delim[4] = "/";

	strcpy(filepath, delim);



	while(token != 0){
		token = strtok(filepath, delim);
		if(strcmp(token, targetDirectory) == 0){
			break;
		}

		strcat(targetDirectory, token);
		strcat(targetDirPath, "/");
	}



	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *parentDirectoryNode = malloc(sizeof(struct inode));
	get_node_by_path(targetDirPath, 0, parentDirectoryNode);

	// Step 3: Call get_avail_ino() to get an available inode number
	int nextAvailInode = get_avail_ino();
	struct inode *tempInode = malloc(sizeof(struct inode));
	readi(nextAvailInode, tempInode);

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(*parentDirectoryNode, tempInode->ino, targetDirectory, 20);

	// Step 5: Update inode for target directory


	// Step 6: Call writei() to write inode to disk
	writei(parentDirectoryNode->ino, parentDirectoryNode);

	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *targetDirectory = "";
	char targetDirPath[100];

	char *targetToken;
	char dirpath[100];
	char tmp[100];

	const char delimiter[4] = "/";
	strcpy(dirpath, delimiter);
	targetToken = strtok(dirpath, delimiter);

	while(targetToken != 0) {
		strcpy(tmp, targetToken);
		targetToken = strtok(0, delimiter);
	}
	strcpy(targetDirectory, tmp);


	char *token;
	char filepath[100];
	const char delim[4] = "/";

	strcpy(filepath, path);
	token = strcpy(filepath, delim);


	while(token != 0){
		token = strtok(0, filepath);
		if(strcmp(token, targetDirectory) == 0){
			break;
		}

		strcat(targetDirectory, token);
		strcat(targetDirPath, "/");
	}




	// Step 2: Call get_node_by_path() to get inode of target directory
	struct inode *targetDirectoryNode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, targetDirectoryNode);

	// Step 3: Clear data block bitmap of target directory
	unset_bitmap(data_bit_map, targetDirectoryNode->ino);

	// Step 4: Clear inode bitmap and its data block
	unset_bitmap(inode_bit_map, targetDirectoryNode->ino);


	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode *parentDirectoryNode = malloc(sizeof(struct inode));
	dir_remove(*parentDirectoryNode, targetDirectory, 20);

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
	bio_write(super_block->i_bitmap_blk, inode_bit_map);
	bio_write(super_block->d_bitmap_blk, data_bit_map);

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *targetFileName = "";
	char parentDirectoryPath[100];

	char* targetToken;
	char dirpath[100];
	char tmp[100];

	const char delimiter[4] = "/";

	targetToken = strtok(dirpath, delimiter);

	while(targetToken != 0){
		strcpy(tmp, targetToken);
		targetToken = strtok(0, delimiter);
	}
	strcpy(targetFileName, tmp);

	char* token;
	char filepath[100];
	const char delim[4] = "/";

	strcpy(filepath, delim);

	while(token != 0){
		token = strtok(0, delim);
		if(strcmp(token, targetFileName)){
			break;
		}
		strcat(parentDirectoryPath, token);
		strcat(parentDirectoryPath, "/");
	}


	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *parentDirectory = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, parentDirectory);

	// Step 3: Call get_avail_ino() to get an available inode number
	int nextAvailInode = get_avail_ino();

	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	dir_add(*parentDirectory, nextAvailInode, targetFileName, 20);

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk
	//writei(parentDirectoryNode->ino, parentDirectoryNode);

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *pathNode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, pathNode);


	// Step 2: If not find, return -1
	if(pathNode == NULL){
		printf("Path Node not found\n");
		return -1;
	}

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, inode);

	// Step 2: Based on size and offset, read its data blocks from disk
	for(int i = 0; i < size; i++){

	}


	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode *inode = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, inode);


	// Step 2: Based on size and offset, read its data blocks from disk
	for(int i = 0; i < size; i++){

	}


	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name
	char *targetDirectory = "";
	char targetDirectoryPath[100];

	char *targetToken;
	char dirpath[100];
	char tmp[100];

	const char delimiter[4] = "/";
	strcpy(dirpath, path);

	targetToken = strtok(dirpath, delimiter);

	while(targetToken != 0){
		strcpy(tmp, targetToken);
		targetToken = strtok(0, delimiter);
	}

	char* token;
	char filepath[100];
	const char delim[4] = "/";

	strcpy(filepath, path);

	token = strtok(filepath, delim);

	while (token != 0) {
		token = strtok(0, delim);
		if(strcmp(token, targetDirectory) == 0){
			break;
		}
		strcat(targetDirectoryPath, token);
		strcat(targetDirectoryPath, "/");
	}


	// Step 2: Call get_node_by_path() to get inode of target file
	struct inode *targetFile  = malloc(sizeof(struct inode));
	get_node_by_path(path, 0, targetFile);

	// Step 3: Clear data block bitmap of target file
	unset_bitmap(data_bit_map, targetFile->ino);

	// Step 4: Clear inode bitmap and its data block
	unset_bitmap(inode_bit_map, targetFile->ino);

	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode *parentDirectory  = malloc(sizeof(struct inode));
	get_node_by_path(targetDirectoryPath, 0, parentDirectory);

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
	dir_remove(*parentDirectory, targetDirectory, 0);

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

