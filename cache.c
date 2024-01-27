#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "net.h"
#include "cache.h"
#include "jbod.h"

//Uncomment the below code before implementing cache functions.
static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;
static int inserted = 0;

int cache_create(int num_entries) {
  if (num_entries < 2 || num_entries > 4096) {
    return -1;
  }
  if (cache_size != 0) {
    return -1;
  }
  cache_size = num_entries;
  cache = (cache_entry_t *) malloc(sizeof(cache_entry_t) * num_entries);

  return 1;

}

int cache_destroy(void) {
  if (cache_size == 0) {
    return -1;
  }
  cache = NULL;
  cache_size = 0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  clock++;
  if (buf == NULL) {
    return -1;
  }
  if (cache_size == 0) {
    return -1;
  }
  if (disk_num < 0 || disk_num >= 16 || block_num < 0 || block_num >= 256) {
    return -1;
  }
  num_queries++;

  int i;
  for (i = 0; i < cache_size; i++) {
    cache_entry_t entry = *(cache+i);
    if (entry.valid == false) {
      return -1;
    }
    if (entry.block_num == block_num && entry.disk_num == disk_num) {
      num_hits++;

      int j;
      for (j = 0; j < JBOD_BLOCK_SIZE; j++) {
        *(buf+j) = entry.block[j];
      }
      
      entry.clock_accesses = clock;
      *(cache+i) = entry;
      return 1;
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  clock++;
  if (buf == NULL) {
    return;
  }
  if (cache_size == 0) {
    return;
  }
  if (disk_num < 0 || disk_num >= 16 || block_num < 0 || block_num >= 256) {
    return;
  }
  int i;
  for (i = 0; i < cache_size; i++) {
    
    cache_entry_t entry = *(cache+i);
    if (entry.block_num == block_num && entry.disk_num == disk_num) {
      int j;
      for (j = 0; j < JBOD_BLOCK_SIZE; j++) {
        entry.block[j] = *(buf+j);
      }
      entry.clock_accesses = clock;
      *(cache+i) = entry;
    }

  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  clock++;
  if (buf == NULL) {
    return -1;
  }
  if (cache_size == 0) {
    return -1;
  }
  if (disk_num < 0 || disk_num >= 16 || block_num < 0 || block_num >= 256) {
    return -1;
  }
  int maxtime = 0;
  int maxindex = 0;
  int i;
  for (i = 0; i < cache_size; i++) {
    cache_entry_t newentry = *(cache+i);
    if (newentry.valid == false) {
      inserted++;
      newentry.valid = true;
      newentry.disk_num = disk_num;
      newentry.block_num = block_num;
      int j;
      for (j = 0; j < JBOD_BLOCK_SIZE; j++) {
        newentry.block[j] = *(buf+j);
      }
      newentry.clock_accesses = clock;

      *(cache+i) = newentry;
      return 1;
    }
    cache_entry_t current = *(cache+i);

    if (current.block_num == block_num && current.disk_num == disk_num) {
      return -1;

    }
    if (maxtime < current.clock_accesses) {
      maxtime = current.clock_accesses;
      maxindex = i;
    }
  }
  cache_entry_t newentry;
  newentry.valid = true;
  newentry.disk_num = disk_num;
  newentry.block_num = block_num;
  int j;
  for (j = 0; j < JBOD_BLOCK_SIZE; j++) {
    newentry.block[j] = *(buf+j);
  }
  newentry.clock_accesses = clock;
  *(cache+maxindex) = newentry;
  inserted++;
  return 1;
}

bool cache_enabled(void) {
  if (cache_size == 0) {
    return false;
  } else {
    return true;
  }

}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

int cache_resize(int new_num_entries) {
  if (new_num_entries < cache_size) {
    return -1;
  }
  cache = realloc(cache, sizeof(cache_entry_t) * new_num_entries);

  return 1;
}