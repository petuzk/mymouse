#include "fs.h"

static struct fs_mount_t *mount_info = &FS_FSTAB_ENTRY(PARTITION_NODE);
static const char test_file_content[] = "-- this is a Lua script!\nhello_world()\n";

int mv2_fs_init() {
	// nothing really to do, but we're creating an example file if it does not exist
	int rv;
	struct fs_dirent dirent;
	static const char *fname = MV2FS_MOUNTPOINT "/boot.lua";

	rv = fs_stat(fname, &dirent);
	if (rv == -ENOENT) {
		struct fs_file_t file;
		fs_file_t_init(&file);

		rv = fs_open(&file, fname, FS_O_CREATE | FS_O_WRITE);
		if (rv < 0) {
			return rv;
		}

		rv = fs_write(&file, test_file_content, sizeof(test_file_content) - 1);
		if (rv < 0) {
			return rv;
		}

		return fs_close(&file);
	}

	return rv;
}

int mv2_fs_deinit() {
	return fs_unmount(mount_info);
}
