#include "messageGenerator.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <time.h>

#include "../StringRecognizer/fastStringParser.h"
#include "../InputParser/InputParser_C.h"



#define MAXIMUM_FILENAME_WITH_EXTENSION 1024
#define MAXIMUM_LINE_LENGTH 1024
#define MAXIMUM_LEVELS 123
#define ACTIVATED_LEVELS 3



unsigned int varTypesListNumber = 6;
char * varTypesList[] = {   "int64"   , "unsigned long" , "atoi(%s);"               ,
                            "int32"   , "unsigned int"  , "atoi(%s);"               ,
                            "int"     , "unsigned long" , "atoi(%s);"               ,
                            "float32" , "float"         , "atof(%s);"               ,
                            "float64" , "double"        , "atof(%s);"               ,
                            "bool"    , "int"           , "atoi(%s);"
                        };



int typeExists(const char * rosType)
{
  int retval=0;
  unsigned int i=0;
  for (i=0; i<varTypesListNumber; i++)
  {
     if (strcasecmp(rosType,varTypesList[i*3+0])==0) { retval=1; }
  }

  return retval;
}


int writeType(
                 FILE * fp ,
                 const char * rosType
                )
{
  int retval=0;

  unsigned int i=0;
  for (i=0; i<varTypesListNumber; i++)
  {
     if (strcasecmp(rosType,varTypesList[i*3+0])==0) { fprintf(fp,"%s",varTypesList[i*3+1]); retval=1; }
  }

  return retval;
}


int writeConversion(
                 FILE * fp ,
                 const char * rosType,
                 const char * varName
                )
{
  int retval=0;
  unsigned int i=0;
  for (i=0; i<varTypesListNumber; i++)
  {
     if (strcasecmp(rosType,varTypesList[i*3+0])==0) { fprintf(fp,varTypesList[i*3+2],varName); retval=1; }
  }
 return retval;
}


int writeServerCallback(
                           FILE * fp ,
                           const char *  functionName,
                           struct fastStringParser * fsp
                       )
{
  fprintf(fp,"static void * %sHTTPServer(struct AmmServer_DynamicRequest  * rqst)\n",functionName);
  fprintf(fp,"{\n");

  fprintf(fp,"char value[256]={0};\n");

 char variableType[256]={0};
 char variableID[256]  ={0};
 struct InputParserC * ipc = InputParser_Create(512,5);
 InputParser_SetDelimeter(ipc,1,' ');

  unsigned int i=0;
  for (i=0; i<fsp->stringsLoaded; i++)
  {
    int arguments = InputParser_SeperateWordsCC(ipc,fsp->contents[i].str,1);
    if (arguments==2)
    {
        InputParser_GetWord(ipc,0,variableType,256);
        InputParser_GetWord(ipc,1,variableID,256);

        if (typeExists(variableType))
        {
          fprintf(fp," if ( _GETcpy(rqst,(char*)\"%s\",value,255) )   {  %sStatic.%s=",variableID,functionName,variableID);
          writeConversion(
                          fp ,
                          variableType,
                          "value"
                         );
          fprintf(fp,"  }  \n");
        }
    }

  }

  fprintf(fp,"write_person(&%sBridge,&%sStatic);",functionName,functionName);


  fprintf(fp,"snprintf(rqst->content,rqst->MAXcontentSize,\"<html><body>OK</body></html>\");\n");
  fprintf(fp,"rqst->contentSize=strlen(rqst->content);\n\n");
  fprintf(fp,"return 0;\n");

  InputParser_Destroy(ipc);



  fprintf(fp,"  \n");
  fprintf(fp,"}\n\n");
 return 1;
}






int writeStruct(
                 FILE * fp ,
                 const char * line
                )
{
 struct InputParserC * ipc = InputParser_Create(512,5);
 InputParser_SetDelimeter(ipc,1,' ');

 int arguments = InputParser_SeperateWordsCC(ipc,line,1);
 char variableType[256]={0};
 char variableID[256]  ={0};

 //fprintf(stderr,"%u arguments \n",arguments);
 if (arguments==2)
 {
     InputParser_GetWord(ipc,0,variableType,256);
     if ( writeType(fp,variableType) )
     {
      InputParser_GetWord(ipc,1,variableID,256);
      fprintf(fp," %s;\n", variableID);
     } else
     {
       fprintf(fp,"//%s\n",line); //Line as a comment
     }
 } else
 {
     fprintf(fp,"//%s\n",line);
 }

 InputParser_Destroy(ipc);
return 1;
}


int compileMessage(const char * filename,const char * label,const char * pathToMMap)
{
  struct fastStringParser * fsp = fastSTringParser_createRulesFromFile(filename,64);



  const char *  functionName = label;
  unsigned int functionNameLength = strlen(functionName);
  fsp->functionName  = (char* ) malloc(sizeof(char) * (1+functionNameLength));
  if (fsp->functionName==0) { fprintf(stderr,"Could not allocate memory for function name\n"); return 0; }
  strncpy(fsp->functionName,functionName,functionNameLength);
  fsp->functionName[functionNameLength]=0;

  convertTo_ENUM_ID(fsp->functionName);


  char filenameWithExtension[MAXIMUM_FILENAME_WITH_EXTENSION+1]={0};



  //PRINT OUT THE HEADER
  snprintf(filenameWithExtension,MAXIMUM_FILENAME_WITH_EXTENSION,"%s.h",functionName);


  fprintf(stderr,"File Output %s\n",filenameWithExtension);

  FILE * fp = fopen(filenameWithExtension,"wb");
  if (fp == 0) { fprintf(stderr,"Could not open output file %s\n",functionName); return 0; }
  fflush(fp);
  fprintf(fp,"\n");


  fprintf(fp,"/** @file %s.h\n",functionName);
  fprintf(fp,"* @brief A tool that generates code that can span messages across computers\n");
  fprintf(fp,"* @author Ammar Qammaz (AmmarkoV)\n");
  fprintf(fp,"*/\n\n");

  time_t t = time(NULL);
  struct tm tm = *localtime(&t);


  fprintf(fp,"/* \
                 \nThis file was automatically generated @ %02d-%02d-%02d %02d:%02d:%02d using AmmMessages \
                 \nhttps://github.com/AmmarkoV/AmmarServer/tree/master/src/AmmMessages\
                 \nPlease note that changes you make here may be automatically overwritten \
                 \nif the AmmMessages generator runs again..!\
              \n */ \n\n" ,
          tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,   tm.tm_hour, tm.tm_min, tm.tm_sec);


  fprintf(fp,"#ifndef %s_H_INCLUDED\n",fsp->functionName);
  fprintf(fp,"#define %s_H_INCLUDED\n\n\n",fsp->functionName);

  fprintf(fp,"#include \"mmapBridge.h\" \n\n");

  fprintf(fp,"const char * pathToMMAP%s=\"%s/%s.mmap\";",functionName,pathToMMap,functionName);

  fprintf(fp,"static struct bridgeContext %sBridge={0};\n\n",functionName);

//----------------------------------------------------------------------------------------------
  fprintf(fp,"struct %sMessage\n",functionName);
  fprintf(fp,"{\n");
  fprintf(fp,"  unsigned long timestampInit;\n"); //We always want the first bytes to be like this
  unsigned int i=0;
  for (i=0; i<fsp->stringsLoaded; i++)
  {
      writeStruct( fp , fsp->contents[i].str );
  }
  fprintf(fp,"};\n\n");
//----------------------------------------------------------------------------------------------

  fprintf(fp,"\n\n/** @brief Send a %s message through the bridge\n",functionName);
  fprintf(fp,"* @ingroup stringParsing\n");
  fprintf(fp,"* @param The bridge context\n");
  fprintf(fp,"* @param The message to send\n");
  fprintf(fp,"* @retval See above enumerator*/\n");
  fprintf(fp,"static int write_%s(struct bridgeContext * nbc,struct %sMessage * msg)\n",functionName,functionName);
  fprintf(fp,"{\n");
  fprintf(fp,"    return writeBridge(nbc, (void*) msg , sizeof(struct %sMessage) );\n",functionName);
  fprintf(fp,"}\n\n");

  fprintf(fp,"\n\n/** @brief Receive a %s message through the bridge\n",functionName);
  fprintf(fp,"* @ingroup stringParsing\n");
  fprintf(fp,"* @param The bridge context\n");
  fprintf(fp,"* @param The message to receive\n");
  fprintf(fp,"* @retval See above enumerator*/\n");
  fprintf(fp,"static int read_%s(struct bridgeContext * nbc,struct %sMessage* msg)\n",functionName,functionName);
  fprintf(fp,"{\n");
  fprintf(fp,"  if (readBridge(nbc, (void*) msg , sizeof(struct %sMessage) ) )\n",functionName);
  fprintf(fp,"  {\n");
  fprintf(fp,"    nbc->lastMsgTimestamp = msg->timestampInit;\n");
  fprintf(fp,"    return 1;\n");
  fprintf(fp,"  }\n");
  fprintf(fp," return 0;\n");
  fprintf(fp,"}\n\n");


  // ------------------------------------------------------------------------------
  fprintf(fp,"\n\n/** @brief If we don't have AmmarServer included then we don't need the rest of the code*/\n");
  fprintf(fp,"#ifdef AMMSERVERLIB_H_INCLUDED\n");
  fprintf(fp,"\n\n/** @brief This is the static memory location where we receive stuff from the scope of the webserver*/\n");
  fprintf(fp,"static struct %sMessage %sStatic={0};\n\n",functionName,functionName);

  fprintf(fp,"\n\n/** @brief This is the Resource handler context that will manage requests to %s.html */\n",functionName);
  fprintf(fp,"static struct AmmServer_RH_Context %sRH={0};\n",functionName);

    writeServerCallback(
                           fp ,
                           functionName,
                           fsp
                       );

  fprintf(fp,"\n\n/** @brief This function adds %s.html to the webserver and makes it listen for GET commands and forward them to the mmaped structure %sStatic */\n",functionName,functionName);
  fprintf(fp,"static int %sAddToHTTPServer(struct AmmServer_Instance * instance)\n",functionName);
  fprintf(fp,"{\n");


  fprintf(fp,"if ( initializeWritingBridge(&%sBridge , pathToMMAP%s , sizeof(struct %sMessage)) )",functionName,functionName,functionName);
  fprintf(fp,"    { AmmServer_Success(\"Successfully initialized mmaped bridge\");");
  fprintf(fp,"      struct %sMessage empty={0};",functionName);
  fprintf(fp,"      empty.timestampInit = rand()%%10000;");
  fprintf(fp,"        if ( write_%s(&%sBridge,&empty) )",functionName,functionName);
  fprintf(fp,"      { AmmServer_Success(\"Successfully flushed mmaped region\"); }");
  fprintf(fp,"        else");
  fprintf(fp,"      { AmmServer_Error(\"Could not flush mmaped region\"); }");
  fprintf(fp,"    }   else");
  fprintf(fp,"    { AmmServer_Error(\"Could not initialize mmaped bridge \");      }");

  fprintf(fp,"  return AmmServer_AddResourceHandler(instance,&%sRH,\"/%s.html\",2048+sizeof(struct %sMessage),0,&%sHTTPServer,SAME_PAGE_FOR_ALL_CLIENTS);\n",functionName,functionName,functionName,functionName);
  fprintf(fp,"}\n\n");



  fprintf(fp,"static int %sRemoveFromHTTPServer(struct AmmServer_Instance * instance)\n",functionName);
  fprintf(fp,"{\n");
  fprintf(fp,"  AmmServer_RemoveResourceHandler(instance,&%sRH,1); \n",functionName);
  fprintf(fp,"  closeWritingBridge(&%sBridge);\n",functionName);
  fprintf(fp,"}\n\n");



  fprintf(fp,"#endif\n\n");
//----------------------------------------------------------------------------------------------





  fprintf(fp,"#endif\n");

  fclose(fp);
  return 1;
}





