//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"
#include <math.h>
#include <stdio.h>

//
// TODO:Student Information
//
const char *studentName = "Kellian Hunt, Angelique Taylor";
const char *studentID   = "A53244070, A53230147";
const char *email       = "kchunt@eng.ucsd.edu, amt062@eng.ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;     // Number of sets in the I$
uint32_t icacheAssoc;    // Associativity of the I$
uint32_t icacheHitTime;  // Hit Time of the I$

uint32_t dcacheSets;     // Number of sets in the D$
uint32_t dcacheAssoc;    // Associativity of the D$
uint32_t dcacheHitTime;  // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$
uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize;      // Block/Line size
uint32_t memspeed;       // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

//
//TODO: Add your Cache data structures here
//
int blockoffsetBits;
int icacheIndexBits;
int dcacheIndexBits;
int l2cacheIndexBits;
int icacheTagBits;
int dcacheTagBits;
int l2cacheTagBits;

int count = 0;

struct way {
  int validBit;
  int tag;
  int index;
  int blockoffset;
  int lru;
};

struct set {
  struct way *nWays;
};

struct cache {
  struct set *sets;
};

struct cache icache;
struct cache dcache;
struct cache l2cache;

//------------------------------------//
//          Helper Functions          //
//------------------------------------//
void
print_binary(uint32_t decimal) {
  unsigned int mask = 1<<31;
  for (int i=0; i<8; i++) {
    for (int j=0; j<4; j++) {
      // check current bit, and print
      unsigned char c = (decimal & mask) == 0 ? '0' : '1';
      printf("%c", c);
      // move down one bit
      mask >>= 1;
    }
    // print a space very 4 bits
    printf(" ");
  }
  printf("\n");
}

uint32_t
parse_address(uint32_t address, int leftoffset, int rightoffset) {
  if(count < 5) { printf("\taddress :                "); print_binary(address); }
  if (count < 5) { printf("\tleft: %d, right: %d\n", leftoffset, rightoffset ); }
  uint32_t result = address << leftoffset;
  if (count < 5) { printf("\taddress << leftoffset :  " ); print_binary(result); }
  result = result >> leftoffset;
  if (count < 5) { printf("\taddress >> leftoffset :  " ); print_binary(result); }
  result = result >> rightoffset;
  if (count < 5) { printf("\taddress >> rightoffset : " ); print_binary(result); printf("\n");}
  return result;
}

void
update_lru(struct set *cacheSet, int wayIndex, int cacheAssoc) {
  for(int i = 0; i < cacheAssoc; i++)
    if(cacheSet->nWays[i].lru < cacheSet->nWays[wayIndex].lru)
      cacheSet->nWays[i].lru++;
  
  if (count < 5) { printf("lru %d to 0", cacheSet->nWays[wayIndex].lru); }
  cacheSet->nWays[wayIndex].lru = 0;
}

int ADDRESS_SIZE = 32;
//------------------------------------//
//          Cache Functions           //
//------------------------------------//

// Initialize the Cache Hierarchy
//
void
init_cache()
{
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;
  
  //
  //TODO: Initialize Cache Simulator Data Structures
  //
  icacheIndexBits = log2(icacheSets);
  dcacheIndexBits = log2(dcacheSets);
  l2cacheIndexBits = log2(l2cacheSets);
  blockoffsetBits = log2(blocksize);
  icacheTagBits = ADDRESS_SIZE - icacheIndexBits - blockoffsetBits;
  dcacheTagBits = ADDRESS_SIZE - dcacheIndexBits - blockoffsetBits;
  l2cacheTagBits = ADDRESS_SIZE - dcacheIndexBits - blockoffsetBits;
  
  icache.sets = malloc(icacheSets * sizeof(struct set));
  for (int i = 0; i < icacheSets; i++){
    icache.sets[i].nWays = malloc(icacheAssoc * sizeof(struct way));
    for (int j = 0; j < icacheAssoc; j++) {
      icache.sets[i].nWays[j].validBit = 0;
      icache.sets[i].nWays[j].tag = 0;
      icache.sets[i].nWays[j].index = 0;
      icache.sets[i].nWays[j].blockoffset = 0;
      icache.sets[i].nWays[j].lru = icacheAssoc;
    }
  }

  dcache.sets = malloc(dcacheSets * sizeof(struct set));
  for (int i = 0; i < dcacheSets; i++){
    dcache.sets[i].nWays = malloc(dcacheAssoc * sizeof(struct way));
    for (int j = 0; j < dcacheAssoc; j++) {
      dcache.sets[i].nWays[j].validBit = 0;
      dcache.sets[i].nWays[j].tag = 0;
      dcache.sets[i].nWays[j].index = 0;
      dcache.sets[i].nWays[j].blockoffset = 0;
      dcache.sets[i].nWays[j].lru = dcacheAssoc;
    }
  }

  l2cache.sets = malloc(l2cacheSets * sizeof(struct set));
  for (int i = 0; i < l2cacheSets; i++){
    l2cache.sets[i].nWays = malloc(l2cacheAssoc * sizeof(struct way));
    for (int j = 0; j < icacheAssoc; j++) {
      l2cache.sets[i].nWays[j].validBit = 0;
      l2cache.sets[i].nWays[j].tag = 0;
      l2cache.sets[i].nWays[j].index = 0;
      l2cache.sets[i].nWays[j].blockoffset = 0;
      l2cache.sets[i].nWays[j].lru = l2cacheAssoc;
    }
  }

}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  //
  //TODO: Implement I$
  //
  if(count < 5) { printf("tag bit: %d, index bits: %d, blockoffset bits: %d\n", icacheTagBits, icacheIndexBits, blockoffsetBits); }
  int index = parse_address(addr, icacheTagBits, blockoffsetBits);
  int tag = parse_address(addr, 0, icacheTagBits + blockoffsetBits);
  int blockoffset = parse_address(addr, icacheTagBits + icacheIndexBits, 0);
  if(count < 5) { printf("\tResult--- index: %d, tag: %d, blockoffset: %d\n\n\n", index, tag, blockoffset); }
  count++;

  // Index into the cache
  struct set setTemp = icache.sets[index];
  for (int i = 0; i < icacheAssoc; i++) {
    if(setTemp.nWays[i].tag == tag){
      //Check valid bit
      if(setTemp.nWays[i].validBit == 1){ //hit
        update_lru(&icache.sets[index], i, icacheAssoc);
      } else { // miss

      }
    }
  }
  

  // Check valid bit in cache


  return memspeed;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  //
  //TODO: Implement D$
  //
  uint32_t index = parse_address(addr, dcacheTagBits, blockoffsetBits);
  uint32_t tag = parse_address(addr, 0, dcacheIndexBits + blockoffsetBits);
  uint32_t blockoffset = parse_address(addr, dcacheTagBits + dcacheIndexBits, 0);

  return memspeed;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  //
  //TODO: Implement L2$
  //
  int index = parse_address(addr, l2cacheTagBits, blockoffsetBits);
  int tag = parse_address(addr, 0, l2cacheIndexBits + blockoffsetBits);
  int blockoffset = parse_address(addr, l2cacheTagBits + l2cacheIndexBits, 0);

  return memspeed;
}
