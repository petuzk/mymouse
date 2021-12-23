#include "mouseconnect.h"

#include <bluetooth/services/nus.h>

#include "bluetooth.h"
#include "fs.h"


struct __packed mv2_mc_file_transfer_header {
	uint8_t magic[2];  // either "rd" or "wr"
	uint32_t filesize;
};
BUILD_ASSERT(sizeof(struct mv2_mc_file_transfer_header) == 6, "__packed does not work?");

enum mv2_mc_active_command {
	NONE,
	SEND_FILE,
	RECV_FILE,
};

struct mv2_mc_file_transfer_data {
	const char *path;
	struct fs_file_t file;
	uint32_t remaining_size;
};

struct mv2_mc_state {
	uint32_t transaction_id;
	enum mv2_mc_active_command active_command;
	union {
		struct mv2_mc_file_transfer_data ftd;
		void *ptr;
	} command_data;
};

static struct mv2_mc_state mv2_mc_state = {
	.transaction_id = 0,
	.active_command = NONE,
	.command_data = {{ 0 }}
};

static inline void mv2_mc_fill_file_transfer_header(void *buf, uint8_t mode, uint32_t filesize) {
	struct mv2_mc_file_transfer_header *header = buf;

	switch (mode) {
		case FS_O_READ:  header->magic[0] = 'r'; header->magic[1] = 'd'; break;
		case FS_O_WRITE: header->magic[0] = 'w'; header->magic[1] = 'r'; break;
		default:
			return;
	}

	header->filesize = filesize;
}

static inline uint32_t mv2_mc_file_transfer_header_get_size(const void *buf) {
	const struct mv2_mc_file_transfer_header *header = buf;
	return header->filesize;
}

static int mv2_mc_start_send_file() {
	int rv;
	struct fs_dirent dirent;
	const char *fname = mv2_mc_state.command_data.ftd.path = MV2FS_MOUNTPOINT "/" CONFIG_PRJ_LUA_SCRIPT_NAME;

	// check if file exists, get file size
	rv = fs_stat(fname, &dirent);
	if (rv < 0) {
		// including -ENOENT
		mv2_mc_state.active_command = NONE;
		return rv;
	}

	// open the file
	struct fs_file_t *file = &mv2_mc_state.command_data.ftd.file;
	fs_file_t_init(file);
	rv = fs_open(file, fname, FS_O_READ);
	if (rv < 0) {
		mv2_mc_state.active_command = NONE;
		return rv;
	}

	uint32_t buffer_size = bt_nus_get_mtu(current_client);
	if (buffer_size <= sizeof(struct mv2_mc_file_transfer_header)) {
		mv2_mc_state.active_command = NONE;
		return -ENOMEDIUM;
	}
	else if (buffer_size > dirent.size + sizeof(struct mv2_mc_file_transfer_header)) {
		mv2_mc_state.active_command = NONE;  // only single transaction is needed
		buffer_size = dirent.size + sizeof(struct mv2_mc_file_transfer_header);
	}

	uint8_t buffer[buffer_size];
	mv2_mc_fill_file_transfer_header(buffer, FS_O_WRITE, dirent.size);

	uint32_t size = sizeof(struct mv2_mc_file_transfer_header);
	while (size < buffer_size) {
		rv = fs_read(file, buffer + size, buffer_size - size);
		if (rv < 0) {
			mv2_mc_state.active_command = NONE;
			return rv;
		} else {
			size += rv;
		}
	}

	rv = bt_nus_send(current_client, buffer, buffer_size);
	if (rv < 0) {
		return rv;
	}

	if (mv2_mc_state.active_command == NONE) {
		return fs_close(file);
	} else {
		mv2_mc_state.command_data.ftd.remaining_size = dirent.size - (buffer_size - sizeof(struct mv2_mc_file_transfer_header));
		return 0;
	}
}

static int mv2_mc_continue_send_file() {
	int rv;
	struct fs_file_t *file = &mv2_mc_state.command_data.ftd.file;

	uint32_t buffer_size = bt_nus_get_mtu(current_client);
	if (!buffer_size) {
		mv2_mc_state.active_command = NONE;
		return -ENOMEDIUM;
	}
	else if (buffer_size > mv2_mc_state.command_data.ftd.remaining_size) {
		mv2_mc_state.active_command = NONE;  // this is last transaction
		buffer_size = mv2_mc_state.command_data.ftd.remaining_size;
	}

	uint8_t buffer[buffer_size];
	uint32_t size = 0;
	while (size < buffer_size) {
		rv = fs_read(file, buffer + size, buffer_size - size);
		if (rv < 0) {
			mv2_mc_state.active_command = NONE;
			return rv;
		} else {
			size += rv;
		}
	}

	rv = bt_nus_send(current_client, buffer, buffer_size);
	if (rv < 0) {
		return rv;
	}

	if (mv2_mc_state.active_command == NONE) {
		return fs_close(file);
	} else {
		mv2_mc_state.command_data.ftd.remaining_size -= buffer_size;
		return 0;
	}
}

static int mv2_mc_start_recv_file(const uint8_t *data, uint16_t len) {
	int rv;
	const char *fname = mv2_mc_state.command_data.ftd.path = MV2FS_MOUNTPOINT "/" CONFIG_PRJ_LUA_SCRIPT_NAME;

	mv2_mc_state.command_data.ftd.remaining_size = mv2_mc_file_transfer_header_get_size(data);
	data += sizeof(struct mv2_mc_file_transfer_header);
	len -= sizeof(struct mv2_mc_file_transfer_header);

	if (mv2_mc_state.command_data.ftd.remaining_size <= len) {
		mv2_mc_state.active_command = NONE;  // this is last transaction
	}

	struct fs_file_t *file = &mv2_mc_state.command_data.ftd.file;
	fs_file_t_init(file);
	rv = fs_open(file, fname, FS_O_CREATE | FS_O_WRITE);
	if (rv < 0) {
		mv2_mc_state.active_command = NONE;
		return rv;
	}

	rv = fs_write(file, data, len);
	if (rv < 0) {
		return rv;
	}

	if (mv2_mc_state.active_command == NONE) {
		return fs_close(file);
	} else {
		mv2_mc_state.command_data.ftd.remaining_size -= len;
		return 0;
	}
}

static int mv2_mc_continue_recv_file(const uint8_t *const data, uint16_t len) {
	int rv;

	if (mv2_mc_state.command_data.ftd.remaining_size <= len) {
		mv2_mc_state.active_command = NONE;  // this is last transaction
	}

	struct fs_file_t *file = &mv2_mc_state.command_data.ftd.file;
	rv = fs_write(file, data, len);
	if (rv < 0) {
		return rv;
	}

	if (mv2_mc_state.active_command == NONE) {
		return fs_close(file);
	} else {
		mv2_mc_state.command_data.ftd.remaining_size -= len;
		return 0;
	}
}

static int mv2_mc_send_mtu() {
    uint32_t mtu = bt_nus_get_mtu(current_client);
	return bt_nus_send(current_client, (const uint8_t *) &mtu, sizeof(mtu));
}

// todo: replace `start` with `init` and `perform`, `continue` with `perform`

static void mv2_mc_nus_received_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len) {
	mv2_mc_state.transaction_id++;

	switch (mv2_mc_state.active_command) {
		case RECV_FILE:
			mv2_mc_continue_recv_file(data, len);
			return;
		default:
			break;
	}

	mv2_mc_state.transaction_id = 0;

	if (len == 2 && data[0] == 'r' && data[1] == 'd') {
		mv2_mc_state.active_command = SEND_FILE;
		mv2_mc_start_send_file();
	}
    else if (len >= 6 && data[0] == 'w' && data[1] == 'r') {
		mv2_mc_state.active_command = RECV_FILE;
        mv2_mc_start_recv_file(data, len);
    }
    else if (len == 3 && data[0] == 'm' && data[1] == 't' && data[2] == 'u') {
		// short command, does not require multiple send/recv
        mv2_mc_send_mtu();
    }
}

static void mv2_mc_nus_sent_cb(struct bt_conn *conn) {
	switch (mv2_mc_state.active_command) {
		case SEND_FILE:
			mv2_mc_continue_send_file();
			return;
		default:
			break;
	}
}

static struct bt_nus_cb nus_cb = {
	.received = mv2_mc_nus_received_cb,
	.sent = mv2_mc_nus_sent_cb,
};

int mv2_mc_init() {
    return bt_nus_init(&nus_cb);
}
