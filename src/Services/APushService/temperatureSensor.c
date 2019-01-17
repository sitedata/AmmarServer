#include "temperatureSensor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "utilities.h"
//This function prepares the content of  random_chars context , ( random_chars.content )



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
   struct tm * ptm = gmtime ( &clock );
   fprintf(fp,"%s|",deviceID);
   fprintf(fp,"%lld|%u|%u|%u|%u|%u|%u|",(long long) clock,ptm->tm_mday,1+ptm->tm_mon,EPOCH_YEAR_IN_TM_YEAR+ptm->tm_year,ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
   fprintf(fp,"%0.2f|%0.2f|%u\n",temperature,humidity,alarmed);
   fclose(fp);
   return 1;
 }
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




int temperatureSensorTestCallback(
                                      struct deviceObject *device,
                                      struct AmmServer_DynamicRequest  * rqst
                                    )
{
  float temperature,humidity;
  if ( getTemperatureAndHumidityFromRequest(rqst,&temperature,&humidity) )
   {

     char logFile[64];
     snprintf(logFile,64,"temperature_%s.log",device->deviceID);

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
     struct tm * ptm = gmtime ( &clock );

     char message[512];
     snprintf(message,512,"This message has been generated by a button press on the sensor #%s @ %u/%u/%u %u:%u:%u Room Temp:%0.2f / Humidity: %0.2f",
              device->deviceID,
              ptm->tm_mday,1+ptm->tm_mon,EPOCH_YEAR_IN_TM_YEAR+ptm->tm_year,ptm->tm_hour,ptm->tm_min,ptm->tm_sec,temperature,humidity);

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
     snprintf(logFile,64,"temperature_%s.log",device->deviceID);

     logTemperature(
                           logFile,
                           device->deviceID,
                           temperature,
                           humidity,
                           1 //Alarming Temperature
                          );
   }


  if (isDeviceAutheticated(device->deviceID,device->devicePublicKey))
       {
           time_t clock = time(NULL);
           struct tm * ptm = gmtime ( &clock );

           if (clock - device->lastEMailNotification > TIME_IN_SECONDS_BETWEEN_EMAILS )
           {
            char message[512];
            snprintf(message,512,"The sensor has sensed abnormal temperatures #%s @ %u/%u/%u %u:%u:%u Room Temp:%0.2f / Humidity: %0.2f.",
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
            } else
            {
             fprintf(stderr,"Will not spam alert e-mails , throttling mail..");
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
  int haveDeviceID=0 , haveDeviceKey=0;
  char deviceID[129]={0};
  char devicePublicKey[32]={0};

  if ( _GETcpy(rqst,"s",deviceID,128) )        { fprintf(stderr,"Device %s connected\n",deviceID); haveDeviceID=1; }
  if ( _GETcpy(rqst,"k",devicePublicKey,128) ) { haveDeviceKey=1; }

   if ( getTemperatureAndHumidityFromRequest(rqst,&temperature,&humidity) )
   {
     fprintf(stderr,"Regular Temperature: %0.2f , Humidity: %0.2f\n",temperature,humidity);

     updateDeviceHeartbeat(device,0,temperature,humidity);
   }


  if (
       (haveDeviceID)&&
       (haveDeviceKey)
     )
     {
      if (isDeviceAutheticated(deviceID,devicePublicKey))
       {
        char logFile[64];
        snprintf(logFile,64,"temperature_%s.log",device->deviceID);

        if (
            logTemperature(
                           logFile,
                           deviceID,
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
     }

  //----------------------------------------
  generalFailureResponseToRequest(rqst);
  return 0;
}




int temperatureSensorPlotImageCallback(
                                          struct deviceObject *device,
                                          struct AmmServer_DynamicRequest  * rqst
                                         )
{
  return 0;
}

