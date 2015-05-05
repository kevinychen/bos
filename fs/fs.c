#include <inc/string.h>
#include <inc/partition.h>

#include "fs.h"

// --------------------------------------------------------------
// Super block
// --------------------------------------------------------------

// Validate the file system super-block.
void
check_super(void)
{
	if (super->s_magic != FS_MAGIC)
		panic("bad file system magic number");

	if (super->s_nblocks > DISKSIZE/BLKSIZE)
		panic("file system is too large");

	cprintf("superblock is good\n");
}

// --------------------------------------------------------------
// Free block bitmap
// --------------------------------------------------------------

// Check to see if the block bitmap indicates that block 'blockno' is free.
// Return 1 if the block is free, 0 if not.
bool
block_is_free(uint32_t blockno)
{
	if (super == 0 || blockno >= super->s_nblocks)
		return 0;
	if (bitmap[blockno / 32] & (1 << (blockno % 32)))
		return 1;
	return 0;
}

// Mark a block free in the bitmap
void
free_block(uint32_t blockno)
{
	// Blockno zero is the null pointer of block numbers.
	if (blockno == 0)
		panic("attempt to free zero block");
	bitmap[blockno/32] |= 1<<(blockno%32);
}

// Search the bitmap for a free block and allocate it.  When you
// allocate a block, immediately flush the changed bitmap block
// to disk.
//
// Return block number allocated on success,
// -E_NO_DISK if we are out of blocks.
int
alloc_block(void)
{
	// The bitmap consists of one or more blocks.  A single bitmap block
	// contains the in-use bits for BLKBITSIZE blocks.  There are
	// super->s_nblocks blocks in the disk altogether.
    uint32_t blockno;
    for (blockno = 0; blockno < super->s_nblocks; blockno++)
        if (block_is_free(blockno)) {
            bitmap[blockno / 32] &= ~(1 << (blockno % 32));
            flush_block(diskaddr(blockno));
            return blockno;
        }

	return -E_NO_DISK;
}

// Validate the file system bitmap.
//
// Check that all reserved blocks -- 0, 1, and the bitmap blocks themselves --
// are all marked as in-use.
void
check_bitmap(void)
{
	uint32_t i;

	// Make sure all bitmap blocks are marked in-use
	for (i = 0; i * BLKBITSIZE < super->s_nblocks; i++)
		assert(!block_is_free(2+i));

	// Make sure the reserved and root blocks are marked in-use.
	assert(!block_is_free(0));
	assert(!block_is_free(1));

	cprintf("bitmap is good\n");
}

// --------------------------------------------------------------
// File system structures
// --------------------------------------------------------------



// Initialize the file system
void
fs_init(void)
{
	static_assert(sizeof(struct File) == 256);

       // Find a JOS disk.  Use the second IDE disk (number 1) if availabl
       if (ide_probe_disk1())
               ide_set_disk(1);
       else
               ide_set_disk(0);
	bc_init();

	// Set "super" to point to the super block.
	super = diskaddr(1);
	check_super();

	// Set "bitmap" to the beginning of the first bitmap block.
	bitmap = diskaddr(2);
	check_bitmap();
	
}

// Find the disk block number slot for the 'filebno'th block in file 'f'.
// Set '*ppdiskbno' to point to that slot.
// The slot will be one of the f->f_direct[] entries,
// or an entry in the indirect block.
// When 'alloc' is set, this function will allocate an indirect block
// if necessary.
//
// Returns:
//	0 on success (but note that *ppdiskbno might equal 0).
//	-E_NOT_FOUND if the function needed to allocate an indirect block, but
//		alloc was 0.
//	-E_NO_DISK if there's no space on the disk for an indirect block.
//	-E_INVAL if filebno is out of range (it's >= NDIRECT + NINDIRECT).
//
// Analogy: This is like pgdir_walk for files.
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
    if (filebno < NDIRECT) {
        *ppdiskbno = &f->f_direct[filebno];
    } else if (filebno < NDIRECT + NINDIRECT) {
        // Allocate indirect block if necessary
        if (!f->f_indirect) {
            if (!alloc)
                return -E_NOT_FOUND;

            int blockno = alloc_block();
            if (blockno < 0)
                return blockno;
            f->f_indirect = blockno;
            memset(diskaddr(blockno), 0, BLKSIZE);
        }

        // Get correct entry of indirect block
        uint32_t *indirect = diskaddr(f->f_indirect);
        *ppdiskbno = &indirect[filebno - NDIRECT];
    } else {
        return -E_INVAL;
    }

    return 0;
}

// Set *ppdiskbno to the pointer to the block in memory
// where the filebno'th block of file 'f' would be mapped.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_NO_DISK if a block needed to be allocated but the disk is full.
//	-E_INVAL if filebno is out of range.
int
file_get_diskbno(struct File *f, uint32_t filebno, uint32_t **ppdiskbno) {
    int result = file_block_walk(f, filebno, ppdiskbno, 1);
    if (result < 0)
        return result;

    if (**ppdiskbno == 0) {
        int blockno = alloc_block();
        if (blockno < 0)
            return blockno;
        **ppdiskbno = blockno;
    }

    return 0;
}

// Set *blk to the address in memory where the filebno'th
// block of file 'f' would be mapped.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_NO_DISK if a block needed to be allocated but the disk is full.
//	-E_INVAL if filebno is out of range.
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
    uint32_t *diskbno;
    int r = file_get_diskbno(f, filebno, &diskbno);
    if (r < 0)
        return r;
    *blk = diskaddr(*diskbno);
    return 0;
}

// Copies blk to a newly allocated block.
int
copy_block(struct File *f, int i, uint32_t *diskbno)
{
    int r;

    // if diskbno is the same as diskbno of previous file, copy block.
    if (f->f_next_file) {
        uint32_t *next_diskbno;
        if ((r = file_get_diskbno(f->f_next_file, i, &next_diskbno)) < 0)
            return r;
        if (*diskbno == *next_diskbno) {
            int new_blockno = alloc_block();
            if (new_blockno < 0)
                return new_blockno;
            void *blk = ROUNDDOWN(diskaddr(*diskbno), BLKSIZE);
            memcpy(diskaddr(new_blockno), blk, BLKSIZE);
            *diskbno = new_blockno;
        }
    }
    return 0;
}

// Try to find a file named "name" in dir.  If so, set *file to it.
//
// Returns 0 and sets *file on success, < 0 on error.  Errors are:
//	-E_NOT_FOUND if the file is not found
static int
dir_lookup(struct File *dir, const char *name, struct File **file)
{
	int r;
	uint32_t i, j, nblock;
	char *blk;
	struct File *f;

	// Search dir for name.
	// We maintain the invariant that the size of a directory-file
	// is always a multiple of the file system's block size.
	assert((dir->f_size % BLKSIZE) == 0);
	nblock = dir->f_size / BLKSIZE;
	for (i = 0; i < nblock; i++) {
		if ((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		f = (struct File*) blk;
		for (j = 0; j < BLKFILES; j++)
			if (strcmp(f[j].f_name, name) == 0) {
				*file = &f[j];
				return 0;
			}
	}
	return -E_NOT_FOUND;
}

static int
alloc_file(struct File **file)
{
    static struct File *f = NULL;
    int i;

    if (f == NULL) {
        int blockno = alloc_block();
        if (blockno < 0)
            return blockno;
        f = diskaddr(blockno);
    }

    for (i = 0; i < BLKFILES - 1; i++)
        if (f[i].f_name[0] == '\0') {
            *file = &f[i];
            return 0;
        }
    *file = &f[i];
    f = NULL;
    return 0;
}

// Set *file to point at a free File structure in dir.  The caller is
// responsible for filling in the File fields.
static int
dir_alloc_file(struct File *dir, struct File **file)
{
	int r;
	uint32_t nblock, i, j;
	char *blk;
	struct File *f;

	assert((dir->f_size % BLKSIZE) == 0);
	nblock = dir->f_size / BLKSIZE;
	for (i = 0; i < nblock; i++) {
        uint32_t *diskbno;
        if ((r = file_get_diskbno(dir, i, &diskbno)) < 0)
            return r;

        f = (struct File*) diskaddr(*diskbno);
        for (j = 0; j < BLKFILES; j++)
            if (f[j].f_name[0] == '\0') {
                copy_block(dir, i, diskbno);
                *file = ((struct File*) diskaddr(*diskbno)) + j;
                break;
            }
    }
    if (i == nblock) {
        dir->f_size += BLKSIZE;
        if ((r = file_get_block(dir, nblock, &blk)) < 0)
            return r;
        *file = (struct File*) blk;
    }
    dir->f_dirty = true;
    return 0;
}

// Skip over slashes.
static const char*
skip_slash(const char *p)
{
	while (*p == '/')
		p++;
	return p;
}

static time_t
get_timestamp_from_path(const char *p)
{
    while (*p && *p != '@')
        p++;
    if (*p == '@')
        p++;
    return parse_time(p, sys_time_msec());
}

// Find correct time version of a file.
static int
find_time_version(const time_t timestamp, struct File **f)
{
    while ((*f) && (*f)->f_timestamp > timestamp)
        *f = (*f)->f_next_file;
    if (*f)
        return 0;
    else
        return -E_NOT_FOUND;
}

// Evaluate a path name, starting at the root.
// On success, set *pf to the file we found
// and set *pdir to the directory the file is in.
// If we cannot find the file but find the directory
// it should be in, set *pdir and copy the final path
// element into lastelem.
static int
walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem)
{
	const char *p;
	char name[MAXNAMELEN];
	struct File *dir, *f;
	int r;
    time_t timestamp;

	// if (*path != '/')
	//	return -E_BAD_PATH;
	path = skip_slash(path);
    timestamp = get_timestamp_from_path(path);

    // Find the correct time version of root
	f = &super->s_root;
    if ((r = find_time_version(timestamp, &f)) < 0)
        return r;

	dir = 0;
	name[0] = 0;

	if (pdir)
		*pdir = 0;
	*pf = 0;
	while (*path != '\0' && *path != '@') {
		dir = f;
		p = path;
		while (*path != '/' && *path != '\0' && *path != '@')
			path++;
		if (path - p >= MAXNAMELEN)
			return -E_BAD_PATH;
		memmove(name, p, path - p);
		name[path - p] = '\0';
		path = skip_slash(path);

		if (dir->f_type != FTYPE_DIR)
			return -E_NOT_FOUND;

		if ((r = dir_lookup(dir, name, &f)) < 0) {
			if (r == -E_NOT_FOUND && (*path == '\0' || *path == '@')) {
				if (pdir)
					*pdir = dir;
				if (lastelem)
					strcpy(lastelem, name);
				*pf = 0;
			}
			return r;
		}
        if ((r = find_time_version(timestamp, &f)) < 0)
            return r;
	}

	if (pdir)
		*pdir = dir;
	*pf = f;
	return 0;
}

// --------------------------------------------------------------
// File operations
// --------------------------------------------------------------

// Create "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
int
file_create(const char *path, bool isdir, struct File **pf)
{
	char name[MAXNAMELEN];
	int r;
	struct File *dir, *f;

    time_t timestamp = sys_time_msec();
	if ((r = walk_path(path, &dir, &f, name)) == 0)
		return -E_FILE_EXISTS;
	if (r != -E_NOT_FOUND || dir == 0)
		return r;
	if ((r = dir_alloc_file(dir, &f)) < 0)
		return r;

	strcpy(f->f_name, name);
    file_set_size(f, 0);
    f->f_type = isdir ? FTYPE_DIR : FTYPE_REG;
    f->f_next_file = NULL;
    f->f_timestamp = timestamp;
    f->f_dirty = true;

	*pf = f;
	file_flush(dir, timestamp);
	return 0;
}

// Open "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
int
file_open(const char *path, struct File **pf)
{
	return walk_path(path, 0, pf, 0);
}

// Read count bytes from f into buf, starting from seek position
// offset.  This meant to mimic the standard pread function.
// Returns the number of bytes read, < 0 on error.
ssize_t
file_read(struct File *f, void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;

	if (offset >= f->f_size)
		return 0;

	count = MIN(count, f->f_size - offset);

	for (pos = offset; pos < offset + count; ) {
		if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(buf, blk + pos % BLKSIZE, bn);
		pos += bn;
		buf += bn;
	}

	return count;
}


// Write count bytes from buf into f, starting at seek position
// offset.  This is meant to mimic the standard pwrite function.
// Extends the file if necessary.
// Returns the number of bytes written, < 0 on error.
int
file_write(struct File *f, const void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;

	// Extend file if necessary
	if (offset + count > f->f_size)
		if ((r = file_set_size(f, offset + count)) < 0)
			return r;

	for (pos = offset; pos < offset + count; ) {
        uint32_t *diskbno;
        if ((r = file_get_diskbno(f, pos / BLKSIZE, &diskbno)) < 0)
            return r;
        copy_block(f, pos / BLKSIZE, diskbno);

		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
        memmove(diskaddr(*diskbno) + pos % BLKSIZE, buf, bn);
		pos += bn;
		buf += bn;
	}

    f->f_dirty = true;
	return count;
}

int
file_history(struct File *f, time_t *buf, size_t count, off_t offset)
{
    time_t *buf_ptr = buf;
    if (!f->f_dirty)
        f = f->f_next_file;
    while (f) {
        if (offset)
            offset--;
        else if (count--)
            *buf_ptr++ = f->f_timestamp;
        else
            break;

        f = f->f_next_file;
    }
    return buf_ptr - buf;
}

// Remove a block from file f.  If it's not there, just silently succeed.
// Returns 0 on success, < 0 on error.
static int
file_free_block(struct File *f, uint32_t filebno)
{
	int r;
	uint32_t *ptr;

	if ((r = file_block_walk(f, filebno, &ptr, 0)) < 0)
		return r;
	if (*ptr) {
		free_block(*ptr);
		*ptr = 0;
	}
	return 0;
}

// Set the size of file f, truncating or extending as necessary.
int
file_set_size(struct File *f, off_t newsize)
{
	f->f_size = newsize;
	flush_block(f);
	return 0;
}

// Flush the contents and metadata of file f out to disk.
// Loop over all the blocks in file.
// Translate the file block number into a disk block number
// and then check whether that disk block is dirty.  If so, write it out.
void
file_flush(struct File *f, time_t timestamp)
{
	int i;
	uint32_t *pdiskbno;

    if (f->f_dirty) {
        f->f_dirty = false;
        f->f_timestamp = timestamp;

        // Copy file struct: F -> F2 -> ... to F -> F' -> F2 -> ...
        struct File *next_file;
        int r = alloc_file(&next_file);
        if (r < 0)
            panic("out of memory in file_flush");

        memcpy(next_file, f, sizeof(struct File));
        f->f_next_file = next_file;
        flush_block(next_file);
    }

	for (i = 0; i < (f->f_size + BLKSIZE - 1) / BLKSIZE; i++) {
		if (file_block_walk(f, i, &pdiskbno, 0) < 0 ||
		    pdiskbno == NULL || *pdiskbno == 0)
			continue;
		flush_block(diskaddr(*pdiskbno));
	}
	flush_block(f);
	if (f->f_indirect)
		flush_block(diskaddr(f->f_indirect));
}


// Sync the entire file system.  A big hammer.
void
fs_sync(void)
{
	int i;
	for (i = 1; i < super->s_nblocks; i++)
		flush_block(diskaddr(i));
}

