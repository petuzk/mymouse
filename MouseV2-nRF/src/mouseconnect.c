#include "mouseconnect.h"

#include <bluetooth/services/nus.h>

#include "bluetooth.h"
#include "fs.h"

static int mv2_mc_send_file(const char *fname) {
	int rv;
	struct fs_dirent dirent;
	struct fs_file_t file;
	uint8_t buffer[64];
	struct __packed header {
		uint8_t magic[2];
		uint32_t filesize;
	};
	BUILD_ASSERT(sizeof(struct header) == 6, "__packed does not work?");

	rv = fs_stat(fname, &dirent);
	if (rv < 0) {
		// including -ENOENT
		return rv;
	}

	// open the file first, then send header (to not send a header without the actual data)
	fs_file_t_init(&file);
	rv = fs_open(&file, fname, FS_O_READ);
	if (rv < 0) {
		return rv;
	}

	struct header *header = (struct header *) buffer;
	header->magic[0] = 'w';
	header->magic[1] = 'r';
	header->filesize = dirent.size;

	uint32_t size = sizeof(struct header);

	while ((rv = fs_read(&file, buffer + size, sizeof(buffer) - size)) > 0) {
		size += rv;
	}

	if (rv < 0) {
		return rv;
	}

	rv = bt_nus_send(current_client, buffer, size);
	if (rv < 0) {
		return rv;
	}

	return fs_close(&file);
}

static int mv2_mc_recv_file(const char *fname, const uint8_t *const data, uint16_t len) {
	int rv;
	struct fs_file_t file;
	fs_file_t_init(&file);

	rv = fs_open(&file, fname, FS_O_CREATE | FS_O_WRITE);
	if (rv < 0) {
		return rv;
	}

	rv = fs_write(&file, data + 6, len - 6);
	if (rv < 0) {
		return rv;
	}

	return fs_close(&file);
}

static int mv2_mc_send_mtu() {
    uint32_t mtu = bt_nus_get_mtu(current_client);
	return bt_nus_send(current_client, &mtu, sizeof(mtu));
}

static void mv2_mc_nus_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len) {
	static const char *fname = MV2FS_MOUNTPOINT "/" CONFIG_PRJ_LUA_SCRIPT_NAME;

	if (len == 2 && data[0] == 'r' && data[1] == 'd') {
		mv2_mc_send_file(fname);
	}
    else if (len >= 6 && data[0] == 'w' && data[1] == 'r') {
        mv2_mc_recv_file(fname, data, len);
    }
    else if (len == 3 && data[0] == 'm' && data[1] == 't' && data[2] == 'u') {
        mv2_mc_send_mtu();
    }
}

static struct bt_nus_cb nus_cb = {
	.received = mv2_mc_nus_receive_cb,
};

int mv2_mc_init() {
    return bt_nus_init(&nus_cb);
}
