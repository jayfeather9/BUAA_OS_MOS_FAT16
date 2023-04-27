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
