/*
 * operations on IDE disk.
 */

#include "serv.h"
#include <drivers/dev_disk.h>
#include <lib.h>
#include <mmu.h>

// Overview:
//  read data from IDE disk. First issue a read request through
//  disk register and then copy data from disk buffer
//  (512 bytes, a sector) to destination array.
//
// Parameters:
//  diskno: disk number.
//  secno: start sector number.
//  dst: destination for data read from IDE disk.
//  nsecs: the number of sectors to read.
//
// Post-Condition:
//  Panic if any error occurs. (you may want to use 'panic_on')
//
// Hint: Use syscalls to access device registers and buffers.
// Hint: Use the physical address and offsets defined in 'include/drivers/dev_disk.h':
//  'DEV_DISK_ADDRESS', 'DEV_DISK_ID', 'DEV_DISK_OFFSET', 'DEV_DISK_OPERATION_READ',
//  'DEV_DISK_START_OPERATION', 'DEV_DISK_STATUS', 'DEV_DISK_BUFFER'
void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs) {
	u_int begin = secno * BY2SECT;
	u_int end = begin + nsecs * BY2SECT;

	for (u_int off = 0; begin + off < end; off += BY2SECT) {
		uint32_t temp;
		/* Exercise 5.3: Your code here. (1/2) */
	
		// write disk id
		temp = diskno;
		user_assert(syscall_write_dev(&temp, DEV_DISK_ADDRESS | DEV_DISK_ID, sizeof(uint32_t)) == 0);
		// write offset
		temp = begin + off;
		user_assert(syscall_write_dev(&temp, DEV_DISK_ADDRESS | DEV_DISK_OFFSET, sizeof(uint32_t)) == 0);
		// start read
		temp = DEV_DISK_OPERATION_READ;
		user_assert(syscall_write_dev(&temp, DEV_DISK_ADDRESS | DEV_DISK_START_OPERATION, sizeof(uint32_t)) == 0);
		// get return val
		uint32_t ret_val = 0;
		user_assert(syscall_read_dev(&ret_val, DEV_DISK_ADDRESS | DEV_DISK_STATUS, sizeof(uint32_t)) == 0);
		user_assert(ret_val != 0);
		// put disk val to array
		syscall_read_dev(dst + off, DEV_DISK_ADDRESS | DEV_DISK_BUFFER, BY2SECT);
	}
}

// Overview:
//  write data to IDE disk.
//
// Parameters:
//  diskno: disk number.
//  secno: start sector number.
//  src: the source data to write into IDE disk.
//  nsecs: the number of sectors to write.
//
// Post-Condition:
//  Panic if any error occurs.
//
// Hint: Use syscalls to access device registers and buffers.
// Hint: Use the physical address and offsets defined in 'include/drivers/dev_disk.h':
//  'DEV_DISK_ADDRESS', 'DEV_DISK_ID', 'DEV_DISK_OFFSET', 'DEV_DISK_BUFFER',
//  'DEV_DISK_OPERATION_WRITE', 'DEV_DISK_START_OPERATION', 'DEV_DISK_STATUS'
void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs) {
	u_int begin = secno * BY2SECT;
	u_int end = begin + nsecs * BY2SECT;

	for (u_int off = 0; begin + off < end; off += BY2SECT) {
		uint32_t temp;
		/* Exercise 5.3: Your code here. (2/2) */
	
		// write disk id
		temp = diskno;
		user_assert(syscall_write_dev(&temp, DEV_DISK_ADDRESS | DEV_DISK_ID, sizeof(uint32_t)) == 0);
		// write offset
		temp = begin + off;
		user_assert(syscall_write_dev(&temp, DEV_DISK_ADDRESS | DEV_DISK_OFFSET, sizeof(uint32_t)) == 0);
		// put array val to disk
		user_assert(syscall_write_dev(src + off, DEV_DISK_ADDRESS | DEV_DISK_BUFFER, BY2SECT) == 0);
		// start writing
		temp = DEV_DISK_OPERATION_WRITE;
		user_assert(syscall_write_dev(&temp, DEV_DISK_ADDRESS | DEV_DISK_START_OPERATION, sizeof(uint32_t)) == 0);
		// get return val
		uint32_t ret_val = 0;
		user_assert(syscall_read_dev(&ret_val, DEV_DISK_ADDRESS | DEV_DISK_STATUS, sizeof(uint32_t)) == 0);
		user_assert(ret_val != 0);
	}
}

int ssd_map[32];
int ssd_phy_writable[32];
int ssd_clear_cnt[32];
char clean_blk[1024];
char reserved_blk[1024];

void ssd_erase_phy(int phy_id) {
	ide_write(0, phy_id, clean_blk, 1);
	ssd_clear_cnt[phy_id]++;
	ssd_phy_writable[phy_id] = 1;
}

int ssd_alloc() {
	int target_phy_id = -1;
	int min_clear_cnt = 0xffffff;
	for (int i = 0; i < 32; i++) {
		if (!ssd_phy_writable[i]) continue;
		if (ssd_clear_cnt[i] < min_clear_cnt) {
			target_phy_id = i;
			min_clear_cnt = ssd_clear_cnt[i];
		}
	}
	if (min_clear_cnt < 5) return target_phy_id;

	int btarget_phy_id = -1;
	min_clear_cnt = 0xffffff;
	for (int i = 0; i < 32; i++) {
		if (ssd_phy_writable[i]) continue;
		if (ssd_clear_cnt[i] < min_clear_cnt) {
			btarget_phy_id = i;
			min_clear_cnt = ssd_clear_cnt[i];
		}
	}

	ide_read(0, btarget_phy_id, reserved_blk, 1);
	ide_write(0, target_phy_id, reserved_blk, 1);

	// int blogic_no;
	for (int i = 0; i < 32; i++) {
		if (ssd_map[i] == btarget_phy_id) {
			ssd_map[i] = target_phy_id;
			break;
		}
	}

	ssd_phy_writable[target_phy_id] = 0;
	ssd_erase_phy(btarget_phy_id);

	return btarget_phy_id;
}

void ssd_init() {
	for (int i = 0; i < 32; i++) {
		ssd_map[i] = -1;
		ssd_phy_writable[i] = 1;
		ssd_clear_cnt[i] = 0;
	}
	for (int i = 0; i < 1024; i++) {
		clean_blk[i] = 0;
	}
}

int ssd_read(u_int logic_no, void *dst) {
	int phy_id = ssd_map[logic_no];
	if (phy_id == -1) {
		return -1;
	}
	ide_read(0, phy_id, dst, 1);
	return 0;
}

void ssd_write(u_int logic_no, void *src) {
	int phy_id = ssd_map[logic_no];
	if (phy_id == -1) {
		phy_id = ssd_alloc();
		ssd_map[logic_no] = phy_id;
	}
	else {
		ssd_erase_phy(phy_id);
		phy_id = ssd_alloc();
		ssd_map[logic_no] = phy_id;
	}

	// write
	ide_write(0, phy_id, src, 1);

	ssd_phy_writable[phy_id] = 0;
}

void ssd_erase(u_int logic_no) {
	int phy_id = ssd_map[logic_no];
	if (phy_id == -1) {
		return;
	}
	ssd_erase_phy(phy_id);
	ssd_map[logic_no] = -1;
}
