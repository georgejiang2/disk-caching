#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"

int is_mounted = 0;
int is_written = 0;

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"
/*Use your mdadm code*/


int mdadm_mount(void) {
  // Complete your code here
  jbod_cmd_t command = JBOD_MOUNT;
  command = (command&0xff) << 12; // shift bottom 8 bits of command to bits 12-19
  int mount = jbod_client_operation(command,NULL);
  if (mount == 0) {
    return 1;
  } else {
    return -1;
  }
}

int mdadm_unmount(void) {
  jbod_cmd_t command = JBOD_UNMOUNT;
  command = (command&0xff) << 12; // shift bottom 8 bits to 12-19
  int mount = jbod_client_operation(command,NULL);
  if (mount == 0) {
    return 1;
  } else {
    return -1;
  }
}

int mdadm_write_permission(void){
	jbod_cmd_t command = JBOD_WRITE_PERMISSION;
	command = (command & 0xff) << 12; // bits 12-19
	int permission = jbod_client_operation(command, NULL);
	if (permission == 0) {
		return 1;
	} else {
		return -1;
	}
	// YOUR CODE GOES HERE

	return 0;
}


int mdadm_revoke_write_permission(void){
	jbod_cmd_t command = JBOD_WRITE_PERMISSION_ALREADY_REVOKED;
	command = (command & 0xff) << 12; // bits 12-19
	int permission = jbod_client_operation(command, NULL);
	if (permission == 0) {
		return 1;
	} else {
		return -1;
	}
	// YOUR CODE GOES HERE

	return 0;
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf) {
  //Complete your code here
  // store in read buffer
  int mounted = mdadm_mount();
  if (mounted == 1) {
    return -1; // if mdadm_mount worked, then it was unmounted before
  }
  if (start_addr >= 1048576) {
    return -1; // out of bound
  }
  if ((start_addr + read_len) > 1048576) {
    return -1; // out of bound
  }
  if (read_len > 1024) {
    return -1; // read_len too big
  }
  if (read_buf == NULL && read_len > 0) {
    return -1; // buffer null
  }
  
  int diskbit = ((start_addr) / JBOD_DISK_SIZE); // shift to disk id
  int blockbit = (((start_addr)%JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE) << 4; // shift to block id
  int bytenum = ((start_addr)%JBOD_BLOCK_SIZE);
  int disknum = (start_addr) / JBOD_DISK_SIZE;
  int blocknum = ((start_addr)%JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
  uint8_t block[256];
  int retval = read_len;
  while (read_len > 0) {

	if (cache_enabled() && (cache_lookup(disknum, blocknum, block) == 1)) {
		// do nothing
	} else {
		jbod_cmd_t command = JBOD_SEEK_TO_DISK;
		uint32_t nc = command;
		nc = (command&0xff) << 12;
		nc = (nc|diskbit); // get disk bits
		jbod_client_operation(nc, NULL); // seek to disk
		command = JBOD_SEEK_TO_BLOCK;
		nc = (command&0xff) << 12;
		nc = (nc|blockbit); // get block bits
		jbod_client_operation(nc, NULL);
		command = JBOD_READ_BLOCK;
		nc = (command&0xff) << 12;
		nc = (nc|blockbit); // get block bits
		jbod_client_operation(nc, block);
		if (cache_enabled()) {
			cache_insert(disknum,blocknum,block);
		}
	}
    blocknum++;
    blockbit = blocknum << 4;
    if (blocknum == 256) {
      blocknum = 0;
      blockbit = 0;
      disknum++;
      diskbit++;
    }
    int i;
    for (i = bytenum; i < 256 && read_len > 0; i++) {
      *(read_buf) = block[i];
      read_buf++;
      read_len--;
    }
    bytenum = 0;
  }
  
  return retval;
}

int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
  
	int mounted = mdadm_mount();
	if (mounted == 1) {
		return -1;
	}
	int permission = mdadm_write_permission();
	if (permission == 1) {
		return -1;
	}
	if (start_addr >= 1048576) {
		return -1;
	}
	if (write_len + start_addr > 1048576) {
		return -1;
	}
	if (write_len > 1024) {
		return -1;
	}
	if (write_buf == NULL && write_len > 0) {
		return -1;
	}

	int diskbit = ((start_addr) / JBOD_DISK_SIZE); // shift to disk id
	int blockbit = (((start_addr)%JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE) << 4; // shift to block id
	int bytenum = ((start_addr) % JBOD_BLOCK_SIZE);
	int disknum = (start_addr) / JBOD_DISK_SIZE;
	int blocknum = ((start_addr) % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
	uint8_t block[256];
	uint8_t block2[256];
	int retval = write_len;
	while (write_len > 0) {
		jbod_cmd_t command = JBOD_SEEK_TO_DISK;
		uint32_t nc = command;
		nc = (command&0xff) << 12;
		nc = (nc|diskbit); // get disk bits
		jbod_client_operation(nc, NULL); // seek to disk
		command = JBOD_SEEK_TO_BLOCK;
		nc = (command&0xff) << 12;
		nc = (nc|blockbit); // get block bits
		jbod_client_operation(nc, NULL);
		command = JBOD_READ_BLOCK;
		nc = (command&0xff) << 12;
		nc = (nc|blockbit); // get block bits
		jbod_client_operation(nc, block); //block now contains the bits of that block

		int i;
		for (i = bytenum; i < 256 && write_len > 0; i++) {
			block[i] = *(write_buf);
			write_buf++;
			write_len--;
		}
		command = JBOD_SEEK_TO_BLOCK;
		nc = (command&0xff) << 12;
		nc = (nc|blockbit); // get block bits
		jbod_client_operation(nc, NULL);
		command = JBOD_WRITE_BLOCK;
		nc = (command&0xff) << 12;
		nc = (nc|blockbit);
		jbod_client_operation(nc, block); // write block


		if (cache_enabled() && (cache_lookup(disknum, blocknum, block2) == 1)) {
			cache_update(disknum,blocknum,block);
		} else {
			
			cache_insert(disknum,blocknum,block);
		}

		blocknum++;
		blockbit = blocknum << 4;
		if (blocknum == 256) {
		blocknum = 0;
		blockbit = 0;
		disknum++;
		diskbit++;
		}
		bytenum = 0;
	}
	return retval;
}