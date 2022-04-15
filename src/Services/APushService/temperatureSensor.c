#include "temperatureSensor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "utilities.h"
//This function prepares the content of  random_chars context , ( random_chars.content )

#define HIGH_TEMPERATURE 29
#define LOW_TEMPERATURE 19

int logTemperature(
                   const char * filename,
                   const char * deviceID,
                   float temperature,
                   float  humidity,
                   char   alarmed
                  )
{
 FILE * fp = fopen(filename,"a");
 if (fp!=0)
 {
   time_t clock = time(NULL);
   struct tm * ptm = localtime ( &clock ); //gmtime
   fprintf(fp,"%s|",deviceID);
   fprintf(fp,"%lld|%u|%u|%u|%u|%u|%u|",(long long) clock,ptm->tm_mday,1+ptm->tm_mon,EPOCH_YEAR_IN_TM_YEAR+ptm->tm_year,ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
   fprintf(fp,"%0.2f|%0.2f|%u\n",temperature,humidity,alarmed);
   fclose(fp);
   return 1;
 }

 fprint(stderr,"Failed to log temperature/humidity %0.2f/%0.2f to %s!!!!\n",filename);
 return 0;
}



int getTemperatureAndHumidityFromRequest(struct AmmServer_DynamicRequest  * rqst,float * temperature , float * humidity)
{
  unsigned int receivedData=0;
  char buffer[128]={0};
  if ( _GETcpy(rqst,"t",buffer,128) ) { ++receivedData; *temperature=atof(buffer); }
  if ( _GETcpy(rqst,"h",buffer,128) ) { ++receivedData; *humidity=atof(buffer);    }

  return (receivedData==2);
}



int temperatureSensorSystemNotification(struct deviceObject *device,char * message)
{
  if  (
       sendEmail(
                 device->email,
                 "Sensor System Notification",
                 message
                )
       )
       {
         return 1;
       }
  return 0;
}


int temperatureSensorTestCallback(
                                      struct deviceObject *device,
                                      struct AmmServer_DynamicRequest  * rqst
                                    )
{
  float temperature,humidity;
  if ( getTemperatureAndHumidityFromRequest(rqst,&temperature,&humidity) )
   {

     char logFile[64];
     snprintf(logFile,64,"log/APushService/temperature_%s.log",device->deviceID);

     fprintf(stderr,"Test Button Temperature: %0.2f , Humidity: %0.2f\n",temperature,humidity);
     logTemperature(
                           logFile,
                           device->deviceID,
                           temperature,
                           humidity,
                           0 //NotAlarming
                          );

     updateDeviceHeartbeat(device,0,temperature,humidity);

     time_t clock = time(NULL);
     struct tm * ptm = localtime ( &clock ); //gmtime

     char message[512];
     snprintf(
              message,512,"This message has been generated by a button press on the sensor #%s(%s) @ %u/%u/%u %u:%u:%u Room Temp:%0.2f / Humidity: %0.2f",
              device->deviceLabel,
              device->deviceID,
              ptm->tm_mday,
              1+ptm->tm_mon,
              EPOCH_YEAR_IN_TM_YEAR+ptm->tm_year,
              ptm->tm_hour,
              ptm->tm_min,
              ptm->tm_sec,
              temperature,
              humidity
             );

     if (
          sendEmail(
                    device->email,
                    "Sensor Manual Test",
                    message
                   )
       )
       {
         //----------------------------------------
         generalSuccessResponseToRequest(rqst);
         return 1;
       }
   }

  //----------------------------------------
  generalFailureResponseToRequest(rqst);
 return 0;
}




//This function prepares the content of  random_chars context , ( random_chars.content )
int temperatureSensorAlarmCallback(struct deviceObject *device, struct AmmServer_DynamicRequest  * rqst)
{
   float temperature,humidity;
   if ( getTemperatureAndHumidityFromRequest(rqst,&temperature,&humidity) )
   {
     fprintf(stderr,"Emergency Temperature: %0.2f , Humidity: %0.2f\n",temperature,humidity);

     updateDeviceHeartbeat(device,1,temperature,humidity);

     char logFile[64];
     snprintf(logFile,64,"log/APushService/temperature_%s.log",device->deviceID);

     if (!logTemperature(
                           logFile,
                           device->deviceID,
                           temperature,
                           humidity,
                           1 //Alarming Temperature
                          )
         )
         {
            fprintf(stderr,"Failed to log Emergency temperature..!\n");
         }
   }


  if (isDeviceAutheticated(device->deviceID,device->devicePublicKey))
       {
           time_t clock = time(NULL);
           struct tm * ptm = localtime ( &clock ); //gmtime

           if ( (temperature<=LOW_TEMPERATURE) || (temperature>=HIGH_TEMPERATURE) )
           {
             if (clock - device->lastEMailNotification > TIME_IN_SECONDS_BETWEEN_EMAILS )
             {
              char message[512];
              snprintf(message,512,"The sensor detected abnormal temperatures #%s @ %u/%u/%u %u:%u:%u Room Temp:%0.2f / Humidity: %0.2f.",
                       device->deviceID,ptm->tm_mday,1+ptm->tm_mon,EPOCH_YEAR_IN_TM_YEAR+ptm->tm_year,ptm->tm_hour,ptm->tm_min,ptm->tm_sec,temperature,humidity);

              if (
                   sendEmail(
                              device->email,
                              "Sensor Alarm",
                              message
                            )
                 )
                 {
                  device->lastEMailNotification=clock;
                  //----------------------------------------
                  generalSuccessResponseToRequest(rqst);
                  return 1;
                 }
              }
              else
              {
               fprintf(stderr,"Will not spam alert e-mails , throttling mail..\n");
              }
            }  else
            {
             fprintf(stderr,"Device is alarmed but we are overriding its alarm setting..\n");
             //This still counts as a success
             generalSuccessResponseToRequest(rqst);
             return 1;
            }
       }

  //----------------------------------------
  generalFailureResponseToRequest(rqst);
  return 0;
}


//This function prepares the content of  random_chars context , ( random_chars.content )
int temperatureSensorHeartBeatCallback(struct deviceObject *device,struct AmmServer_DynamicRequest  * rqst)
{
  float temperature,humidity;
   if ( getTemperatureAndHumidityFromRequest(rqst,&temperature,&humidity) )
   {
     fprintf(stderr,"Regular Temperature: %0.2f , Humidity: %0.2f\n",temperature,humidity);

     updateDeviceHeartbeat(device,0,temperature,humidity);

     char logFile[64];
     snprintf(logFile,64,"log/APushService/temperature_%s.log",device->deviceID);

        if (
            logTemperature(
                           logFile,
                           device->deviceID,
                           temperature,
                           humidity,
                           0 //NotAlarming
                          )
           )
         {
           //----------------------------------------
           generalSuccessResponseToRequest(rqst);
           return 1;
         }
     }

  //----------------------------------------
  generalFailureResponseToRequest(rqst);
  return 0;
}



int getAFastAndRecentGraph(
                            struct deviceObject *device,
                            unsigned int sensorBufferID,
                            const char * command,
                            char * content,
                            unsigned long contentMaxLength,
                            unsigned long * contentLength
                          )
{
 int haveANewGraph=0;
 time_t clock = time(NULL);
 struct tm * ptm = localtime( &clock );  //gmtime

 if (
       (clock - device->info.graphs[sensorBufferID].lastGraphTime > TIME_IN_SECONDS_BETWEEN_GRAPHS) ||
       (device->info.graphs[sensorBufferID].data==0) ||
       (device->info.graphs[sensorBufferID].lastGraphTime==0)
     )
      {
        if (device->info.graphs[sensorBufferID].data==0)
         {
           device->info.graphs[sensorBufferID].data = (char*) malloc(sizeof(char) * MAX_GRAPH_IMAGE_SIZE_IN_BYTES);
           device->info.graphs[sensorBufferID].dataMaxSize = MAX_GRAPH_IMAGE_SIZE_IN_BYTES;
           device->info.graphs[sensorBufferID].dataSize    = 0; //Initially empty..
         }


        if (device->info.graphs[sensorBufferID].data!=0)
        {
        fprintf(stderr,"Fresh graphs..\n");
         if (
              AmmServer_ExecuteCommandLineAndRetreiveAllResultsTimed(
                                                                 command,
                                                                 device->info.graphs[sensorBufferID].data,
                                                                 device->info.graphs[sensorBufferID].dataMaxSize,
                                                                &device->info.graphs[sensorBufferID].dataSize,
                                                                 1000
                                                               )
            )
            {
              device->info.graphs[sensorBufferID].lastGraphTime=clock;
              haveANewGraph=1;
            }
        }
      } else
      {
       fprintf(stderr,"Canned graphs..\n");
      }


  if (device->info.graphs[sensorBufferID].data!=0)
        {
           *contentLength = device->info.graphs[sensorBufferID].dataSize;
           memcpy(content,device->info.graphs[sensorBufferID].data,device->info.graphs[sensorBufferID].dataSize);
           return 1;
        }

  return 0;
}




int temperatureSensorPlotImageCallback(
                                          struct deviceObject *device,
                                          struct AmmServer_DynamicRequest  * rqst
                                         )
{

  char buffer[128]={0};
  if ( _GETcpy(rqst,"t",buffer,128) )
  {
    char command[512];
    int haveACommand=0;
    unsigned int sensorBuffer=0;
    unsigned int numberOfSamplesToPlot=512;

    if (strcmp("0",buffer)==0)
    {
        haveACommand=1;
        sensorBuffer=0;
        snprintf(command,512,"tail -n %u \"log/APushService/temperature_%s.log\" | gnuplot -c src/Services/APushService/temperature.gnuplot",numberOfSamplesToPlot,device->deviceID);
    }
     else
    if (strcmp("1",buffer)==0)
    {
        haveACommand=1;
        sensorBuffer=1;
        snprintf(command,512,"tail -n %u \"log/APushService/temperature_%s.log\" | gnuplot -c src/Services/APushService/humidity.gnuplot",numberOfSamplesToPlot,device->deviceID);
    }

   if (haveACommand)
   {
    if (
         getAFastAndRecentGraph(
                                device,
                                sensorBuffer,
                                command,
                                rqst->content,
                                rqst->MAXcontentSize,
                                &rqst->contentSize
                               )
    /* AmmServer_ExecuteCommandLineAndRetreiveAllResults(
                                                           command,
                                                           rqst->content,
                                                           rqst->MAXcontentSize,
                                                           &rqst->contentSize
                                                         )*/
       )
       { return 1; }
   }
  }



 return 0;
}

