#include <stdlib.h>     /* qsort */


#include "hashmap.h"
#include "../tools/logs.h"

/*! djb2
This algorithm (k=33) was first reported by dan bernstein many years ago in comp.lang.c. another version of this algorithm (now favored by bernstein) uses xor: hash(i) = hash(i - 1) * 33 ^ str[i]; the magic of number 33 (why it works better than many other constants, prime or not) has never been adequately explained.
Needless to say , this is our hash function..!
*/
unsigned long hashFunction(char *str)
    {
        if (str==0) return 0;
        if (str[0]==0) return 0;

        unsigned long hash = 5381; //<- magic
        int c=1;

        while (c != 0)
        {
            c = *str++;
            hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        }

        return hash;
    }


int hashMap_Grow(struct hashMap * hm,unsigned int growthSize)
{
  struct hashMapEntry * newentries = realloc(hm->entries , sizeof(struct hashMapEntry) * ( hm->maxNumberOfEntries + growthSize ) );
  if (newentries!=0)
     {
       hm->entries=newentries;
       memset(newentries+hm->maxNumberOfEntries,0,growthSize);
       hm->maxNumberOfEntries += growthSize;
       return 1;
     } else
     {
       warning("Could not grow hashMap (running out of memory?)");
     }
  return 0;
}

struct hashMap * hashMap_Create(unsigned int initialEntries,unsigned int entryAllocationStep,void * clearItemFunction)
{
  struct hashMap * hm = (struct hashMap *)  malloc(sizeof(struct hashMap));
  if (hm==0)  { error("Could not allocate a new hashmap"); return 0; }

  memset(hm,0,sizeof(struct hashMap));

  hm->entryAllocationStep=entryAllocationStep;

  if (!hashMap_Grow(hm,initialEntries) )
  {
    error("Could not grow new hashmap for the first time");
    free(hm);
    return 0;
  }

  hm->clearItemCallbackFunction = clearItemFunction;
  pthread_mutex_init(&hm->hm_addLock,0);

  return hm;
}

int hashMap_GetSize(struct hashMap * hm)
{
  if (!hashMap_IsOK(hm)) { return 0;}
  return 0;
}

int hashMap_IsOK(struct hashMap * hm)
{
    if (hm == 0)  { return 0; }
    if (hm->entries == 0)  { return 0; }
    if (hm->maxNumberOfEntries == 0)  { return 0; }
    return 1;
}

int hashMap_IsSorted(struct hashMap * hm)
{
  if (!hashMap_IsOK(hm)) { return 0; }
  int i=1;
    while ( i < hm->curNumberOfEntries )
    {
      if (hm->entries[i-1].keyHash > hm->entries[i].keyHash)
         { return 0; /*We got ourself a non sorted entry!*/ }
      ++i;
    }
  return 1;
}


void hashMap_Clear(struct hashMap * hm)
{
  if (!hashMap_IsOK(hm)) { return; }
  void ( *hashMapClearCallback) ( void * )=0 ;
  hashMapClearCallback = hm->clearItemCallbackFunction;
  unsigned int i=0;
  unsigned int entryNumber = hm->curNumberOfEntries;

  hm->curNumberOfEntries = 0;

  while (i < entryNumber)
  {
    hm->entries[i].keyHash=0;
    hm->entries[i].keyLength=0;
    hashMapClearCallback(hm->entries[i].payload);
    if (hm->entries[i].key!=0)
    {
      free(hm->entries[i].key);
      hm->entries[i].key=0;
    }
    ++i;
  }

  return;
}

void hashMap_Destroy(struct hashMap * hm)
{
  if (!hashMap_IsOK(hm)) { return; }
  hashMap_Clear(hm);

  hm->maxNumberOfEntries=0;
  free(hm->entries);
  hm->entries=0;
  hm->clearItemCallbackFunction=0;

  pthread_mutex_destroy(&hm->hm_addLock);
  return ;
}


/* qsort struct comparision function (price float field) ( See qsort call in main ) */
int cmpHashTableItems(const void *a, const void *b)
{
    struct hashMapEntry *ia = (struct hashMapEntry *)a;
    struct hashMapEntry *ib = (struct hashMapEntry *)b;

    return (ia->keyHash > ib->keyHash);
}

int hashMap_Sort(struct hashMap * hm)
{
  if (!hashMap_IsOK(hm)) { return 0; }
  qsort( hm->entries , hm->curNumberOfEntries , sizeof(struct hashMapEntry), cmpHashTableItems);
  return 1;
}



int hashMap_Add(struct hashMap * hm,char * key,void * val,unsigned int valLength)
{
  if (!hashMap_IsOK(hm)) { return 0; }
  int clearToAdd=1;
  pthread_mutex_lock (&hm->hm_addLock); // LOCK PROTECTED OPERATION -------------------------------------------

  if (hm->curNumberOfEntries >= hm->maxNumberOfEntries)
  {
    if  (!hashMap_Grow(hm,hm->entryAllocationStep))
    {
      error("Could not grow new hashmap for adding new values");
      clearToAdd = 0;
    }
  }

  if (clearToAdd)
  {
    unsigned int our_index=hm->curNumberOfEntries++;
    if (hm->entries[our_index].key!=0) { warning("While Adding a new key to hashmap , entry was not clean"); }
    hm->entries[our_index].key = (char *) malloc(sizeof(char) * (strlen(key)+1) );
    if (hm->entries[our_index].key == 0)
         {
           free(hm->entries[our_index].key);
           hm->entries[our_index].key=0;
           --hm->curNumberOfEntries;
           pthread_mutex_unlock (&hm->hm_addLock); // LOCK PROTECTED OPERATION -------------------------------------------
           warning("While Adding a new key to hashmap , couldnt allocate key");
           return 0;
         }
    hm->entries[our_index].keyLength = strlen(key);
    hm->entries[our_index].keyHash = hashFunction(key);

    if (valLength==0)
    {
      //We store and serve direct pointers! :)
      hm->entries[our_index].payload = val;
      hm->entries[our_index].payloadLength = 0;
    } else
    {
      hm->entries[our_index].payload = (void *) malloc(sizeof(char) * (valLength) );
      if (hm->entries[our_index].key == 0)
      {
        free(hm->entries[our_index].key);
        hm->entries[our_index].key=0;
        --hm->curNumberOfEntries;
         warning("While Adding a new key to hashmap , couldnt allocate payload");
        pthread_mutex_unlock (&hm->hm_addLock); // LOCK PROTECTED OPERATION -------------------------------------------
      }
      memcpy(hm->entries[our_index].payload,val,valLength);
      hm->entries[our_index].payloadLength = valLength;
    }

  }

   pthread_mutex_unlock (&hm->hm_addLock); // LOCK PROTECTED OPERATION -------------------------------------------
  return 1;
}

void * hashMap_Get(struct hashMap * hm,char * key,int * found)
{
  *found=0;
  if (!hashMap_IsOK(hm)) { return 0;}
  unsigned int i=0;
  unsigned long keyHash = hashFunction(key);
  while ( i < hm->curNumberOfEntries )
  {
    if ( hm->entries[i].keyHash == keyHash ) { *found=1; return i; }
    ++i;
  }
  return 0;
}


int hashMap_ContainsKey(struct hashMap * hm,char * key)
{
  if (!hashMap_IsOK(hm)) { return 0;}
  unsigned int i=0;
  unsigned long keyHash = hashFunction(key);
  while ( i < hm->curNumberOfEntries )
  {
    if ( hm->entries[i].keyHash == keyHash ) { return 1; }
    ++i;
  }
  return 0;
}


int hashMap_ContainsValue(struct hashMap * hm,void * val)
{
  if (!hashMap_IsOK(hm)) { return 0;}
  unsigned int i=0;
  while ( i < hm->curNumberOfEntries )
  {
    if ( hm->entries[i].payload == val ) { return 1; }
    ++i;
  }
  return 0;
}


