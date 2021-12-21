#ifndef MOUSEV2_FILESYSTEM_H
#define MOUSEV2_FILESYSTEM_H

#include <zephyr.h>
#include <device.h>
#include <fs/fs.h>
#include <fs/littlefs.h>
#include <storage/flash_map.h>

#define PARTITION_NODE    DT_NODELABEL(littlefs)
#define MV2FS_MOUNTPOINT  DT_PROP(PARTITION_NODE, mount_point)

FS_FSTAB_DECLARE_ENTRY(PARTITION_NODE);

int mv2_fs_init();
int mv2_fs_deinit();

#endif // MOUSEV2_FILESYSTEM_H
