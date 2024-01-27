#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf) {
  uint8_t buffer[len];
  if (read(fd, buffer, sizeof(buffer)) != sizeof(buffer)) {
    return false;
  }
  int i;
  for (i = 0; i < len; i++) {
    *(buf + i) = *(buffer + i);
  }

  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf) {
  // int i;
  
  if (write(fd, buf, len) != len) {
    return false;
  }
  // for (i = 0; i < len; i++) {
  //   uint8_t byte = *(buf+i);
  //   if (write(fd, &byte, sizeof(byte)) != sizeof(byte)) {
  //     return false;
  //   }
  // }
  return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint8_t *ret, uint8_t *block) {
  uint8_t buffer[HEADER_LEN + JBOD_BLOCK_SIZE];
  
  if (!nread(fd, HEADER_LEN, buffer)) {
    return false;
  }
  *(op) = *((uint32_t*) buffer);
  *op = ntohl(*op);
  
  uint8_t info = buffer[4];
  if (info%2 == 0) {
    *(ret) = 0;
  } else {
    *(ret) = -1;
  }

  if (info&0x2) { // second to last byte
    if (!nread(fd, JBOD_BLOCK_SIZE, buffer+5)) {
      return false;
    }
    memcpy(block, buffer+5, JBOD_BLOCK_SIZE*(sizeof(buffer[0])));
  }

  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t info;
  
  uint8_t *buffer = malloc(sizeof(uint8_t) * (JBOD_BLOCK_SIZE + HEADER_LEN));

  uint32_t *opcode = (uint32_t *) buffer;
  *opcode = htonl(op);

  // opcode into buffer
  op = op >> 12;
  op = op & 0x000000FF; // get command bits
  if (op == JBOD_WRITE_BLOCK) {
    info = 2;
  } else {
    info = 0;
  }
  *(buffer+4) = info;

  if (info == 2) {
    int i;
    // if command is write block, add payload
    for (i = 0; i < 256; i++) {
      *(buffer + 5 + i) = *(block+i);
    }
  }
  if (info == 2) {
    if(!nwrite(sd, 261, buffer)) {
      return false;
    }
  } else {
    if(!nwrite(sd, 5, buffer)) {
      return false;
    }
  }
  return true;
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port) {
  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1) {
    printf("Error on socket creation [%s]\n", strerror(errno));
    return false;
  }
  // create socket
  
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(JBOD_PORT);
  if (inet_aton(JBOD_SERVER, &addr.sin_addr) == 0 ) {
    return false;
  }

  if (connect(cli_sd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
    printf("Error on socket connection [%s]\n",strerror(errno));
    return false;
  }
  return true;
}

void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}

int jbod_client_operation(uint32_t op, uint8_t *block) {
  if (!send_packet(cli_sd, op, block)) {
    return -1;
  }
  uint8_t info = 0;
  if (!recv_packet(cli_sd, &op, &info, block)) {
    return -1;
  }
  if (info%2 == 0) {
    return 0;
  } else {
    return -1;
  }
}
