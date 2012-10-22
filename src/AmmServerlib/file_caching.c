#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AmmServerlib.h"
#include "configuration.h"
#include "file_caching.h"
#include "http_tools.h"
#include "time_provider.h"


unsigned char CACHING_ENABLED=1;
unsigned char DYNAMIC_CONTENT_RESOURCE_MAPPING_ENABLED=1;
unsigned int MAX_TOTAL_ALLOCATION_IN_MB=64;
unsigned int MAX_INDIVIDUAL_CACHE_ENTRY_IN_MB=1;
unsigned int MAX_CACHE_SIZE=1000;


unsigned long loaded_cache_items_Kbytes=0;
unsigned int loaded_cache_items=0;
struct cache_item * cache=0;

/*! djb2
This algorithm (k=33) was first reported by dan bernstein many years ago in comp.lang.c. another version of this algorithm (now favored by bernstein) uses xor: hash(i) = hash(i - 1) * 33 ^ str[i]; the magic of number 33 (why it works better than many other constants, prime or not) has never been adequately explained.
Needless to say , this is our hash function..!
*/
    unsigned long hash(char *str)
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


/*
     INDEXING for the cache
     The following calls manage Indexing for the cache..
     We can create a cache item in many ways ( an empty one , a real one , a virtual one)
     In order to reduce code repetitions all the create/delete/find indexing routines ( that are the same among all the above ways to create a cache itm )
     are implemented here
     ------------------------------------------------------------------
*/

/*This is the Search Index Function , It is basically fully inefficient O(n) , it will be replaced by some binary search implementation*/
unsigned int FindCacheIndexForResource(char * resource,unsigned int * index)
{
  fprintf(stderr,"Serial slow searching for resource in cache %s ..",resource);
  if (cache==0) { fprintf(stderr,"Cache hasn't been allocated yet\n"); return 0; }
  unsigned long file_we_are_looking_for = hash(resource);
  unsigned int i=0;

  for (i=0; i<loaded_cache_items; i++)
   {
     if ( cache[i].filename_hash == file_we_are_looking_for )
      {
        ++cache[i].hits;
        if (cache[i].filesize!=0)
         { //Filesize gets pulled directly from the client.. For this reason we want to check if the client has allocated a value to prevent the possible segfault
          fprintf(stderr,"..found it here %u , %u hits , %0.2f Kbytes\n",i,cache[i].hits,(double) (*cache[i].filesize/1024) );
         }
        *index=i;
        return 1;
      }
   }

  return 0;
}

/*This is the Create Index Function , Nothing is sorted so we just append our cache item list with another one..
  Also for now only the hash is used to retrieve it ( talk about a sloppy implementation ) */
int CreateCacheIndexForResource(char * resource,unsigned int * index)
{
  if (cache==0) { fprintf(stderr,"Cache hasn't been allocated yet\n"); return 0; }
  if (MAX_CACHE_SIZE<=loaded_cache_items+1) { fprintf(stderr,"Cache is full , Could not CreateCacheIndexForResource(%s)",resource); return 0; }
  *index=loaded_cache_items++;

  cache[*index].filename_hash = hash(resource);
  return 1;
}

/*This is the Destroy Index Function , it isn't implemented , as one can see.. */
int DestroyCacheIndexForResource(unsigned int * index)
{
  fprintf(stderr,"DestroyCacheIndexForResource(%u) hasn't been implemented yet\n",*index);
  return 0;
}


/*
   ------------------------------------------------------------------
   ------------------------------------------------------------------
   ------------------------------------------------------------------
*/

int CreateGZippedVersionofCachedResource(unsigned int * index)
{
  fprintf(stderr,"CreateGZippedVersionofCachedResource for index %u not implemented yet\n",*index);
  return 0;
}


int KeepFileInMemoryIndex(char *filename,unsigned int * index)
{

  /*Now we will do the following things
    1) Open the file and find how large it is
    2) Allocate a large enough chunk of memory
    3) Read it all in memory and close the file */

  FILE * pFile = fopen (filename, "rb" );
  if (pFile==0) { fprintf(stderr,"Could not open file to cache it.. %s\n",filename); return 0;}
  /*We have opened the file.. */

  if ( fseek (pFile , 0 , SEEK_END)!=0 ) { fprintf(stderr,"Could not find file size to cache client..!\nUnable to serve client\n"); fclose(pFile); return 0; }
  unsigned long lSize = ftell (pFile);
  //lSize now holds the size of the file..

  //We check if the file size is ok with our configuration limits
  if (MAX_INDIVIDUAL_CACHE_ENTRY_IN_MB*1024*1024<lSize) { fprintf(stderr,"This file exceedes the maximum cache size for individual files , it will not be cached\n"); fclose(pFile); return 0;  }
  if (MAX_TOTAL_ALLOCATION_IN_MB*1024<loaded_cache_items_Kbytes+lSize/1024)  { fprintf(stderr,"We have reached the soft cache limit of %u MB\n",MAX_TOTAL_ALLOCATION_IN_MB); fclose(pFile); return 0; }

  //We are ok with the file size , we will now rewind the file to start reading it from the beginning..!
  rewind (pFile);
  //We will allocate the required memory..!
  char * buffer = (char*) malloc ( sizeof(char) * (lSize));
  if (buffer == 0 ) { fprintf(stderr,"Could not allocate enough memory to cache this file..!\n"); fclose(pFile); return 0;  }
  //We have allocated the new memory chunk so we will update our loaded cache counter..!
  loaded_cache_items_Kbytes+=(unsigned int) lSize/1024;

  // copy the file into the buffer:
  size_t result;
  result = fread (buffer,1,lSize,pFile);
  if (result != lSize) { fprintf(stderr,"Reading error , while filling in newly allocated cache item %s \n",filename); free (buffer); fclose (pFile); return 0; }

  fprintf(stderr,"File %s has %u bytes cached with index %u \n",filename,(unsigned int ) lSize,*index);
  fclose(pFile);
  // file has been cached , so time to fill in its details..
  cache[*index].mem = buffer;
  if ( cache[*index].filesize!= 0 ) { free (cache[*index].filesize); cache[*index].filesize=0; }
  cache[*index].filesize = (unsigned long * ) malloc(sizeof (unsigned long));
  *cache[*index].filesize = lSize;


  /*This could be a good place to make the gzipped version of the buffer..!*/
  if (!CreateGZippedVersionofCachedResource(index)) {  fprintf(stderr,"Could not create a gzipped version of the file..\n"); }

  return 1;
}


int AddFileToCache(char * filename,unsigned int * index,struct stat * last_modification)
{
  if (!CreateCacheIndexForResource(filename,index)) { /*We couldn't allocate a new index for this file */ return 0; }

  fprintf(stderr,"Adding file %s to cache ( %0.2f / %u MB )\n",filename,(float) loaded_cache_items_Kbytes/1048576 , MAX_TOTAL_ALLOCATION_IN_MB);

  if (!KeepFileInMemoryIndex(filename,index))
   {
       fprintf(stderr,"Could not read file %s into memory\n",filename);
       fprintf(stderr,"Erasing index from memory\n");
       DestroyCacheIndexForResource(index);
       *index=0;
       return 0;
   }

  cache[*index].hits = 0;
  cache[*index].prepare_mem_callback=0; // No callback for this file..
  cache[*index].context=0;
  cache[*index].doNOTCache=0;

   //Save modification time..! These are not used yet.. !
   if (last_modification!=0)
   {
    struct tm * ptm = gmtime ( &last_modification->st_mtime ); //This is not a particularly thread safe call , must add a mutex or something here..!
    cache[*index].hour=ptm->tm_hour;
    cache[*index].minute=ptm->tm_min;
    cache[*index].second=ptm->tm_sec;
    cache[*index].wday=ptm->tm_wday;
    cache[*index].day=ptm->tm_mday;
    cache[*index].month=ptm->tm_mon;
    cache[*index].year=EPOCH_YEAR_IN_TM_YEAR+ptm->tm_year;
   }


  return 1;
}



int DoNotCacheResource(char * filename)
{
   unsigned int index=0;
   if (FindCacheIndexForResource(filename,&index))  { cache[index].doNOTCache=1; }
    else
     {
       //File Doesn't exist, we have to create a cache index for it , and then mark it as uncachable..!
       if (!CreateCacheIndexForResource(filename,&index) ) { return 0; }
       if (FindCacheIndexForResource(filename,&index)) { cache[index].doNOTCache=1; } else
                                                        { return 0; } //Could not set doNOTCache..!
     }
   return 1;
}


int RemoveFileFromCache(char * filename)
{
   fprintf(stderr,"RemoveFileFromCache(%s) not implemented\n",filename);
   return 0;
}


int AddDirectResourceToCache(struct AmmServer_RH_Context * context)
{
  if ( ! DYNAMIC_CONTENT_RESOURCE_MAPPING_ENABLED )
   {
     fprintf(stderr,"Dynamic content is disabled..!\n");
     return 0;
   }

  //Create the full path to distinguish from different root_paths ( virutal servers ) ..!
  char full_filename[(MAX_RESOURCE*2)+1]={0};
  //These direct resource functions come from inside our program so we can cpy/cat them here
  //( they dont pass through http_header_analysis.c so this can't be done another way..!

  strncpy(full_filename,context->web_root_path,MAX_RESOURCE);
  strncat(full_filename,context->resource_name,MAX_RESOURCE);
  ReducePathSlashes_Inplace(full_filename);

  unsigned int index=0;
  if (! CreateCacheIndexForResource(full_filename,&index) ) { return 0; }

  cache[index].context = context;
  cache[index].mem = context->content;
  cache[index].filesize = &context->content_size;
  cache[index].hits = 0;
  cache[index].prepare_mem_callback = context->prepare_content_callback;

  return 1;
}


int RemoveDirectResourceToCache(struct AmmServer_RH_Context * context,unsigned char free_mem)
{
       context->MAX_content_size=0;
       if ((free_mem)&&(context->content!=0)) { free(context->content); context->content=0; }
       return RemoveFileFromCache(context->resource_name);
}

unsigned int GetHashForCacheItem(unsigned int index)
{
    return cache[index].filename_hash;
}

int CachedVersionExists(char * verified_filename,unsigned int * index)
{
    if (FindCacheIndexForResource(verified_filename,index)) { return 1; }
    return 0;
}

char * CheckForCachedVersionOfThePage(struct HTTPRequest * request,char * verified_filename,unsigned int * index,unsigned long *filesize,struct stat * last_modification,unsigned char gzip_supported)
{
      if (!CACHING_ENABLED)
      {
        fprintf(stderr,"Caching deactivated..!\n");
        return 0;
      }

       if (FindCacheIndexForResource(verified_filename,index)) //This can be avoided by adding an index as a parameter to this function call
        {
           if (cache[*index].doNOTCache)
            {
              fprintf(stderr,"We do not want to serve a cached version of this file..\n");
              return 0;
            }  else
           if (cache[*index].mem!=0)
           {
             if (cache[*index].prepare_mem_callback!=0)
              {
                //TODO some good explanation here.>!
                struct AmmServer_RH_Context * shared_context = cache[*index].context;

                unsigned long now=0; //If there is no callback limits the time of the call will always be 0
                //That doesnt bother anything or anyone..

                if (shared_context-> callback_every_x_msec!=0)
                { //Dynamic pages without time limits dont have to call the "expensive" GetTickCount
                 now=GetTickCount();
                 if ( now-shared_context->last_callback < shared_context-> callback_every_x_msec )
                    {
                      //The request came too fast.. We will serve our existing file..!
                      shared_context->callback_cooldown=1;
                      *filesize=*cache[*index].filesize;
                      return cache[*index].mem;
                    } else
                    {
                      fprintf(stderr,"Request deserves fresh page , %u last gen, %u now , %u cooldown\n",shared_context->last_callback,now,shared_context-> callback_every_x_msec);
                    }
                }

                /*Do callback here*/
                shared_context->callback_cooldown=0;
                shared_context->last_callback = now;
                void ( *DoCallback) (unsigned int)=0 ;
                DoCallback = cache[*index].prepare_mem_callback;


                /*If we have GET or POST request variables , lets pass them through to our shared context.. */
                shared_context->GET_request = request->GETquery;
                if (shared_context->GET_request!=0) { shared_context->GET_request_length = strlen(shared_context->GET_request); } else
                                                     { shared_context->GET_request_length = 0; }

                shared_context->POST_request = request->POSTquery;
                if (shared_context->POST_request!=0) { shared_context->POST_request_length = strlen(shared_context->POST_request); } else
                                                     { shared_context->POST_request_length = 0; }

                unsigned int UNUSED=666; // <- These variables are associated with this page ( POST / GET vars )
                //They are an id ov the var_caching.c list so that the callback function can produce information based on them..!
                DoCallback(UNUSED);
              }
             *filesize=*cache[*index].filesize;
             return cache[*index].mem;
           }
        } else
        {
           /* A cached copy doesn't seem to exist , lets make one and then claim it exists! */
           if ( AddFileToCache(verified_filename,index,last_modification) )
            {
              *filesize=*cache[*index].filesize; //We return the filesize after the operation..
              return cache[*index].mem;
            }
        }
       //If we are here we are unlocky , our file wasn't in cache and to make things worse we also failed to load it so
       //regular file sending it is ..!

       *filesize=0;
       fprintf(stderr,"Cache could not find file %s , filesize %u , gzip support %u \n",verified_filename,(unsigned int) *filesize,gzip_supported);

       return 0;
}

int InitializeCache(unsigned int max_seperate_items , unsigned int max_total_allocation_MB , unsigned int max_allocation_per_entry_MB)
{
  MAX_TOTAL_ALLOCATION_IN_MB=max_total_allocation_MB;
  MAX_INDIVIDUAL_CACHE_ENTRY_IN_MB=max_allocation_per_entry_MB;
  MAX_CACHE_SIZE=max_seperate_items;
  if (cache==0)
   {
     cache = (struct cache_item *) malloc(sizeof(struct cache_item) * (MAX_CACHE_SIZE+1));
     if (cache == 0) { fprintf(stderr,"Unable to allocate initial cache memory\n"); return 0; }
   }


//  InitializeVariableCache(max_seperate_variables, max_total_var_allocation_MB , max_var_allocation_per_entry_MB);


   unsigned int i=0;
   for (i=0; i<MAX_CACHE_SIZE; i++) { cache[i].mem=0; cache[i].filesize=0; cache[i].prepare_mem_callback=0; }
   return 1;
}

int DestroyCache()
{
  fprintf(stderr,"Destroying cache..\n");

//   if (!DestroyVariableCache()) { fprintf(stderr,"Failed destroying Variable Cache\n"); }

   if (cache==0)
    {
       fprintf(stderr,"Cache already destroyed \n");
       loaded_cache_items=0;
       loaded_cache_items_Kbytes=0;
      return 1;
    }

   unsigned int i=0;
   for (i=0; i<loaded_cache_items; i++)
   {
     if ( cache[i].prepare_mem_callback !=0)
     {
        //This cache item is "dynamic content" as a result
        //it has its own memory handler , so we just dereference it..
        cache[i].prepare_mem_callback=0;
        cache[i].mem=0;
        cache[i].filesize=0;
     }
       else
     {
      if ( cache[i].mem != 0 )
       {
          free(cache[i].mem);
          cache[i].mem=0;
       }
      if ( cache[i].filesize != 0 )
       {
          free(cache[i].filesize);
          cache[i].filesize=0;
       }
     }
   }

   free(cache);
   cache = 0;
   loaded_cache_items=0;
   loaded_cache_items_Kbytes=0;

   return 1;
}

int ChangeRequestIfInternalRequestIsAddressed(char * request,char * templates_root)
{
  if (!ENABLE_INTERNAL_RESOURCES_RESOLVE)  { return 0; }
  //The role of request caching is to intercept incoming requests and if they are referring
  //to an internal resource using the TemplatesInternalURI URI we want to redirect the request
  //to our templates folder ..!
  //If the request was indeed a change request returns 1 else 0
  if ( strlen(request)>strlen(TemplatesInternalURI)+64 )
   {
       fprintf(stderr,"\nWARNING : Skipping ChangeRequestIfInternalRequestIsAddressed due to a very large request\n");
       return 0;
   }

  char tmp_cmp[MAX_FILE_PATH]={0};
  char * res=strstr(request,TemplatesInternalURI);
  char * res_skipped=res;
  unsigned int template_size = strlen(TemplatesInternalURI);

  if ( res!=0 )
   {
      res_skipped=res+template_size;
      fprintf(stderr,"We've got a result , %s ( skipped %s )\n",res,res_skipped);
      if (strlen(res_skipped)+strlen(templates_root)<MAX_FILE_PATH)
       {
         strcpy(tmp_cmp,templates_root);
         strcat(tmp_cmp,res_skipped);
       } else
       {
           fprintf(stderr,"Internal request too long , not thoroughly tested\n");
           return 0;
       }
      fprintf(stderr,"Internal request to string %s -> %s \n",request,tmp_cmp);
      strcpy(request,tmp_cmp);
      return 1;
   }
  return 0;
}
