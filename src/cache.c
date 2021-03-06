//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"
#include <stdio.h>
#define ADDRESS_SIZE 32

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

int blockoffsetBits;
int icacheIndexBits;
int dcacheIndexBits;
int l2cacheIndexBits;
int icacheTagBits;
int dcacheTagBits;
int l2cacheTagBits;

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
  int tagBits;
  int offsetBits;
  int associativity;
  int indexBits;
};

struct cache icache;
struct cache dcache;
struct cache l2cache;

//------------------------------------//
//          Helper Functions          //
//------------------------------------//
double 
log2(double n) {   
  return (n > 1) ? 1 + log2(n/2) : 0; 
}  

void
print_binary(uint32_t decimal) {
  unsigned int oneBitMask = 1<<31;
  for (int i=0; i<32; i++) {
    char c = (decimal & oneBitMask) == 0 ? '0' : '1';
    printf("%c", c);
    oneBitMask >>= 1;
    if((i+1) % 4 == 0)
      printf(" ");
  }
  printf("\n");
}

uint32_t
parse_address(uint32_t address, int leftoffset, int rightoffset) {
  uint32_t result = address << leftoffset;
  result = result >> leftoffset;
  result = result >> rightoffset;
  return result;
}

uint32_t
rebuild_address(struct cache *cachePtr, uint32_t tag, uint32_t index, uint32_t blockoffset) {  
  uint32_t newTag = tag << (cachePtr->indexBits + cachePtr->offsetBits);
  uint32_t newIndex = index << (cachePtr->offsetBits);

  return newTag + newIndex;
}

void
update_lru(struct set *cacheSet, int wayIndex, int cacheAssoc) {
  for(int i = 0; i < cacheAssoc; i++)
    if(cacheSet->nWays[i].lru < cacheSet->nWays[wayIndex].lru)
      cacheSet->nWays[i].lru++;
  
  cacheSet->nWays[wayIndex].lru = 1;
}
 
void
invalidate(struct cache *cachePtr, uint32_t address) {
  uint32_t index = parse_address(address, cachePtr->tagBits, cachePtr->offsetBits);
  uint32_t tag = parse_address(address, 0, cachePtr->indexBits + cachePtr->offsetBits);
  uint32_t blockoffset = parse_address(address, cachePtr->tagBits + cachePtr->indexBits, 0);

  struct set setTemp = cachePtr->sets[index];
  for (int i = 0; i < cachePtr->associativity; i++) {
    if(setTemp.nWays[i].tag == tag && setTemp.nWays[i].validBit == 1){
      cachePtr->sets[index].nWays[i].validBit = 0;

      return;
    }
  }
}

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
  
  // Initialize cache data structures
  icacheIndexBits = log2(icacheSets);
  dcacheIndexBits = log2(dcacheSets);
  l2cacheIndexBits = log2(l2cacheSets);
  blockoffsetBits = log2(blocksize);
  icacheTagBits = ADDRESS_SIZE - icacheIndexBits - blockoffsetBits;
  dcacheTagBits = ADDRESS_SIZE - dcacheIndexBits - blockoffsetBits;
  l2cacheTagBits = ADDRESS_SIZE - l2cacheIndexBits - blockoffsetBits;
  
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
  icache.tagBits = icacheTagBits;
  icache.offsetBits = blockoffsetBits;
  icache.associativity = icacheAssoc;
  icache.indexBits = icacheIndexBits;

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
  dcache.tagBits = dcacheTagBits;
  dcache.offsetBits = blockoffsetBits;
  dcache.associativity = dcacheAssoc;
  dcache.indexBits = dcacheIndexBits;

  l2cache.sets = malloc(l2cacheSets * sizeof(struct set));
  for (int i = 0; i < l2cacheSets; i++){
    l2cache.sets[i].nWays = malloc(l2cacheAssoc * sizeof(struct way));
    for (int j = 0; j < l2cacheAssoc; j++) {
      l2cache.sets[i].nWays[j].validBit = 0;
      l2cache.sets[i].nWays[j].tag = 0;
      l2cache.sets[i].nWays[j].index = 0;
      l2cache.sets[i].nWays[j].blockoffset = 0;
      l2cache.sets[i].nWays[j].lru = l2cacheAssoc;
    }
  }
  l2cache.tagBits = l2cacheTagBits;
  l2cache.offsetBits = blockoffsetBits;
  l2cache.associativity = l2cacheAssoc;
  l2cache.indexBits = l2cacheIndexBits;
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  if(icacheSets > 0){
    icacheRefs++;
    uint32_t pen = 0;
    uint32_t index = parse_address(addr, icacheTagBits, blockoffsetBits);
    uint32_t tag = parse_address(addr, 0, icacheIndexBits + blockoffsetBits);
    uint32_t blockoffset = parse_address(addr, icacheTagBits + icacheIndexBits, 0);

    // index into the cache
    struct set setTemp = icache.sets[index];
    int indexOfInvalid = -1; // keep track of somewhere we can put the new entry in the case of a miss
    for (int i = 0; i < icacheAssoc; i++) {
      if(setTemp.nWays[i].tag == tag){
        // check valid bit
        if(setTemp.nWays[i].validBit == 1) { // hit!
          // update the cache
          update_lru(&icache.sets[index], i, icacheAssoc);
          return icacheHitTime;
        }

        if(indexOfInvalid < 0)
          indexOfInvalid = i;
        break;
      }

      if(setTemp.nWays[i].validBit == 0 && indexOfInvalid < 0)
        indexOfInvalid = i;    
    }

    // miss
    icacheMisses++;
    
    // check if we have room for the entry
    if (indexOfInvalid > 0) {
      // call l2cache_access to check if it has a hit
      // it returns memspeed if it doesn't have it, l2 hit time if it does
      int icachePenalty = l2cache_access(addr);
      icachePenalties += icachePenalty;

      // update the cache
      update_lru(&icache.sets[index], indexOfInvalid, icacheAssoc);
      icache.sets[index].nWays[indexOfInvalid].tag = tag;
      icache.sets[index].nWays[indexOfInvalid].index = index;
      icache.sets[index].nWays[indexOfInvalid].blockoffset = blockoffset; 
      icache.sets[index].nWays[indexOfInvalid].validBit = 1;

      return icachePenalty + icacheHitTime; 
    } 
    
    // no room - have to kick some valid entry out
    // find the LRU
    for (int i = 0; i < icacheAssoc; i++) {
      if (setTemp.nWays[i].lru == icacheAssoc) { // found LRU
        int icachePenalty = l2cache_access(addr);
        icachePenalties += icachePenalty;

        // check if l2 invalidated an entry so we can avoid kicking out a valid one
        for (int j = 0; j < icacheAssoc; j++) {
          if(setTemp.nWays[j].validBit == 0) {
            // update the cache
            update_lru(&icache.sets[index], j, icacheAssoc);
            icache.sets[index].nWays[j].tag = tag;
            icache.sets[index].nWays[j].index = index;
            icache.sets[index].nWays[j].blockoffset = blockoffset;
            icache.sets[index].nWays[j].validBit = 1; 
            return icachePenalty + icacheHitTime;
          }
        }

        // update the cache
        update_lru(&icache.sets[index], i, icacheAssoc);
        icache.sets[index].nWays[i].tag = tag;
        icache.sets[index].nWays[i].index = index;
        icache.sets[index].nWays[i].blockoffset = blockoffset;
        icache.sets[index].nWays[i].validBit = 1; 

        return icachePenalty + icacheHitTime;
      }
    } 

    icachePenalties += l2cache_access(addr);
    return icachePenalties + icacheHitTime;

  } else {
    return l2cache_access(addr);
  }
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  if(dcacheSets > 0){
    dcacheRefs++;

    uint32_t index = parse_address(addr, dcacheTagBits, blockoffsetBits);
    uint32_t tag = parse_address(addr, 0, dcacheIndexBits + blockoffsetBits);
    uint32_t blockoffset = parse_address(addr, dcacheTagBits + dcacheIndexBits, 0);

    // index into the cache
    struct set setTemp = dcache.sets[index];
    int indexOfInvalid = -1; // keep track of somewhere we can put the new entry in the case of a miss
    for (int i = 0; i < dcacheAssoc; i++) {
      if(setTemp.nWays[i].tag == tag){
        // check valid bit
        if(setTemp.nWays[i].validBit == 1) { // hit!
          // update the cache
          update_lru(&dcache.sets[index], i, dcacheAssoc);
          return dcacheHitTime;
        }

        if(indexOfInvalid < 0)
          indexOfInvalid = i;
        break;
      }

      if(setTemp.nWays[i].validBit == 0 && indexOfInvalid < 0)
        indexOfInvalid = i;    
    }

    // miss
    dcacheMisses++;
    
    // check if we have room for the entry
    if (indexOfInvalid > 0) {
      // call l2cache_access to check if it has a hit
      // it returns memspeed if it doesn't have it, l2 hit time if it does
      int dcachePenalty = l2cache_access(addr);

      // update the cache
      update_lru(&dcache.sets[index], indexOfInvalid, dcacheAssoc);
      dcache.sets[index].nWays[indexOfInvalid].tag = tag;
      dcache.sets[index].nWays[indexOfInvalid].index = index;
      dcache.sets[index].nWays[indexOfInvalid].blockoffset = blockoffset; 
      dcache.sets[index].nWays[indexOfInvalid].validBit = 1;

      dcachePenalties += dcachePenalty;
      return dcachePenalty + dcacheHitTime;
    } 
    
    // no room - have to kick some valid entry out
    // find the LRU
    for (int i = 0; i < dcacheAssoc; i++) {
      if (setTemp.nWays[i].lru == dcacheAssoc) { // found LRU
        int dcachePenalty = l2cache_access(addr);
        dcachePenalties += dcachePenalty;

        // check if l2 invalidated an entry so we can avoid kicking out a valid one
        for (int j = 0; j < dcacheAssoc; j++) {
          if(setTemp.nWays[j].validBit == 0) {
            // update the cache
            update_lru(&dcache.sets[index], j, dcacheAssoc);
            dcache.sets[index].nWays[j].tag = tag;
            dcache.sets[index].nWays[j].index = index;
            dcache.sets[index].nWays[j].blockoffset = blockoffset;
            dcache.sets[index].nWays[j].validBit = 1; 
            
            return dcachePenalty + dcacheHitTime;
          }
        }

        // update the cache
        update_lru(&dcache.sets[index], i, dcacheAssoc);
        dcache.sets[index].nWays[i].tag = tag;
        dcache.sets[index].nWays[i].index = index;
        dcache.sets[index].nWays[i].blockoffset = blockoffset;
        dcache.sets[index].nWays[i].validBit = 1; 

        return dcachePenalty + dcacheHitTime;
      }
    } 

    int dcachePenalty = l2cache_access(addr);
    dcachePenalties += dcachePenalty;
    return dcachePenalty + dcacheHitTime;

  } else {
    return l2cache_access(addr);
  }
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  l2cacheRefs++;

  uint32_t index = parse_address(addr, l2cacheTagBits, blockoffsetBits);
  uint32_t tag = parse_address(addr, 0, l2cacheIndexBits + blockoffsetBits);
  uint32_t blockoffset = parse_address(addr, l2cacheTagBits + l2cacheIndexBits, 0);

  // index into the cache
  struct set setTemp = l2cache.sets[index];
  int indexOfInvalid = -1; // keep track of somewhere we can put the new entry in the case of a miss
  for (int i = 0; i < l2cacheAssoc; i++) {
    if(setTemp.nWays[i].tag == tag){
      // check valid bit
      if(setTemp.nWays[i].validBit == 1) { // hit!
        // update the cache
        update_lru(&l2cache.sets[index], i, l2cacheAssoc);
        return l2cacheHitTime;
      }

      if(indexOfInvalid < 0)
        indexOfInvalid = i;
      break;
    }

    if(setTemp.nWays[i].validBit == 0 && indexOfInvalid < 0)
      indexOfInvalid = i;    
  }

  // miss
  l2cacheMisses++;
  
  // check if we have room for the entry
  if (indexOfInvalid > 0) {
    // update the cache
    update_lru(&l2cache.sets[index], indexOfInvalid, l2cacheAssoc);
    l2cache.sets[index].nWays[indexOfInvalid].tag = tag;
    l2cache.sets[index].nWays[indexOfInvalid].index = index;
    l2cache.sets[index].nWays[indexOfInvalid].blockoffset = blockoffset; 
    l2cache.sets[index].nWays[indexOfInvalid].validBit = 1;

    // call l2cache_access to check if it has a hit
    // it returns memspeed if it doesn't have it, l2 hit time if it does
    int l2cachePenalty = memspeed;
    l2cachePenalties += l2cachePenalty;
    return l2cachePenalty + l2cacheHitTime;
  } 
  
  // no room - have to kick some valid entry out
  // find the LRU
  for (int i = 0; i < l2cacheAssoc; i++) {
    if (setTemp.nWays[i].lru == l2cacheAssoc) { // found LRU

      if(inclusive == TRUE) { //invalidate L1 cache victim
        uint32_t invalidAddress = rebuild_address(&l2cache, 
        setTemp.nWays[i].tag, 
        index, 
        setTemp.nWays[i].blockoffset);
        invalidate(&icache, invalidAddress);
        invalidate(&dcache, invalidAddress);
      }

      // update the cache
      update_lru(&l2cache.sets[index], i, l2cacheAssoc);
      l2cache.sets[index].nWays[i].tag = tag;
      l2cache.sets[index].nWays[i].blockoffset = blockoffset;
      l2cache.sets[index].nWays[i].index = index;
      l2cache.sets[index].nWays[i].validBit = 1; 

      int l2cachePenalty = memspeed;
      l2cachePenalties += l2cachePenalty;
      return l2cachePenalty + l2cacheHitTime;
    }
  } 

  int l2cachePenalty = memspeed;
  l2cachePenalties += l2cachePenalty;
  return l2cachePenalty + l2cacheHitTime;
}
