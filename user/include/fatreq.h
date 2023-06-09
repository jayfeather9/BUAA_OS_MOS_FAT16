#ifndef _FATREQ_H_
#define _FATREQ_H_

#include <fatfs.h>
#include <types.h>

// Definitions for requests from clients to file system

#define FATREQ_OPEN 1
#define FATREQ_MAP 2
#define FATREQ_SET_SIZE 3
#define FATREQ_CLOSE 4
#define FATREQ_DIRTY 5
#define FATREQ_REMOVE 6
#define FATREQ_SYNC 7
#define FATREQ_CREATE 8

struct Fatreq_open {
	char req_path[MAXPATHLEN];
	u_int req_omode;
};

struct Fatreq_map {
	int req_fileid;
	u_int req_offset;
};

struct Fatreq_set_size {
	int req_fileid;
	u_int req_size;
};

struct Fatreq_close {
	int req_fileid;
};

struct Fatreq_dirty {
	int req_fileid;
	u_int req_offset;
};

struct Fatreq_remove {
	char req_path[MAXPATHLEN];
};

struct Fatreq_create {
	char req_path[MAXPATHLEN];
};

#endif
