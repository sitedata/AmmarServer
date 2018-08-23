 
#define RESET_CLOCK_ON_NEXT_COMPILATION 0

#include <LiquidCrystal.h> //These are needed here for the IDE to understand which modules to import
#include <Wire.h>          //These are needed here for the IDE to understand which modules to import  

//See configuration.h for schematics and libraries used..
//See diagram_bbsml.png for connections on arduino
#include "configuration.h"


#include "serialCommunication.h"
AmmBusUSBProtocol ammBusUSB;

#include "joystick.h"
#include "menu.h"
#include "ammBus.h"

#include "timeCalculations.h"

//State -----------------------------------------------------
//-----------------------------------------------------------
struct joystickState joystick={0};
struct ammBusState ambs={0};

const byte numberOfMenus=17;
byte currentMenu=0;

//byte valvesTimesNormal[8]={2,2,2,2,2,2,2,2};
byte valvesTimesNormal[8]={30,30,30,30,30,30,30,30};
byte valvesTimesHigh[8]={60,60,60,60,60,60,60,60};
byte valvesTimesLow[8]={15,15,15,15,15,15,15,15};
byte *armedTimes = 0;
byte *valvesTimes = valvesTimesNormal;

byte valvesState[8]={0};
byte valvesScheduled[8]={0};
uint32_t valveStartedTimestamp[8]={0};
uint32_t valveStoppedTimestamp[8]={0};

uint32_t lastBootTime=0;

byte errorDetected = 0;
byte idleTicks=0;

byte powerSaving=1;
byte autopilotCreateNewJobs=0;
byte runningWork=0;

byte jobRunEveryXHours=5*24;
byte jobRunAtXHour=20;
byte jobRunAtXMinute=0;
byte jobConcurrency=1; //Max concurrent jobs
//-----------------------------------------------------------
//-----------------------------------------------------------


void updateShiftRegister()
{
   digitalWrite(latchPin, LOW);
   shiftOut(dataPin, clockPin, LSBFIRST, leds);
   digitalWrite(latchPin, HIGH);
}

void setRelayState( byte * valves )
{
  leds=0; 
  int i=0;
  for (i=0; i<8; i++) 
  {
    if (!valves[i] )  { bitSet(leds,i); }
  }
  updateShiftRegister();  
}


void scheduleAllValves()
{ 
  valvesScheduled[0]=ON;
  valvesScheduled[1]=ON;
  valvesScheduled[2]=ON;
  valvesScheduled[3]=ON;
  valvesScheduled[4]=ON;
  valvesScheduled[5]=ON;
  valvesScheduled[6]=ON;
  valvesScheduled[7]=ON;
}


void turnAllValvesOff()
{
 valvesState[0]=OFF;
 valvesState[1]=OFF;
 valvesState[2]=OFF;
 valvesState[3]=OFF;
 valvesState[4]=OFF;
 valvesState[5]=OFF;
 valvesState[6]=OFF;
 valvesState[7]=OFF;
 valvesScheduled[0]=OFF;
 valvesScheduled[1]=OFF;
 valvesScheduled[2]=OFF;
 valvesScheduled[3]=OFF;
 valvesScheduled[4]=OFF;
 valvesScheduled[5]=OFF;
 valvesScheduled[6]=OFF;
 valvesScheduled[7]=OFF;
 setRelayState(valvesState);
 armedTimes=0;
}


void checkForSerialInput()
{
  if ( ammBusUSB.newUSBCommands(valvesState,valvesScheduled,&clock,&dt,&idleTicks) )
    {  
     setRelayState(valvesState);
    }
}



void turnLCDOn()
{
  if (powerSaving)
  {
   digitalWrite(lcdPowerPin, HIGH);
   delay(100);
   // set up the LCD's number of columns and rows:
   lcd.begin(16, 2);
   // Print a message to the LCD.
   lcd.print(systemName);
   lcd.setCursor(0, 1);
   lcd.print("powering up..");
   powerSaving=0;
  }
}

void turnLCDOff()
{
  if (!powerSaving)
  { 
    
   lcd.setCursor(0, 0);
   lcd.print(systemName);
   lcd.setCursor(0, 1);
   lcd.print("powersaving ..  ");   
   lcd.noDisplay();  
   
   
   
   digitalWrite(7, LOW); 
   digitalWrite(8, LOW); 
   digitalWrite(9, LOW); 
   digitalWrite(10, LOW);  
   digitalWrite(11, LOW);  
   digitalWrite(12, LOW);  
     
   delay(100);
   digitalWrite(lcdPowerPin, LOW); 
   powerSaving=1;
  }
}

void setup() 
{    
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);  
  pinMode(clockPin, OUTPUT);   
  setRelayState(valvesState);
  
  pinMode(lcdPowerPin, OUTPUT);
  digitalWrite(lcdPowerPin, LOW); 
  
  turnLCDOn();
  
  
  pinMode(SW_pin, INPUT);
  digitalWrite(SW_pin, HIGH);
  Serial.begin(9600);
  
  
  clock.begin();

  // Set sketch compiling time
  if (RESET_CLOCK_ON_NEXT_COMPILATION)
    { clock.setDateTime(__DATE__, __TIME__); }
    
  dt = clock.getDateTime();
  lastBootTime  = dt.unixtime;

  // Set from UNIX timestamp
  // clock.setDateTime(1397408400);

  // Manual (YYYY, MM, DD, HH, II, SS
  // clock.setDateTime(2016, 12, 9, 11, 46, 00);
  
  initializeAmmBusState(&ambs); 
}





void grabMeasurements()
{   
 //Get Time
 dt = clock.getDateTime();  
 ambs.currentTime = dt.unixtime;  
 
 //Joystick Reading  
 getJoystickState(
                  X_pin,
                  Y_pin,
                  SW_pin,
                  &joystick
                 );  
 
  //DHT Reading 
  if (dt.unixtime - lastDHT11SampleTime >= 1 )
  {
   //DHT requires sampling at 1Hz
   if (dht11.read(pinDHT11, &temperature, &humidity, dataDHT11))  { errorDetected=1; } 
   lastDHT11SampleTime=dt.unixtime;
  } 

 
}



void idleMessageTicker(int seconds)
{
  #define MESSAGES 7
  int messageTicker = (seconds/2) % MESSAGES;
  unsigned int elapsedTime = dt.unixtime-lastBootTime;
  lcd.setCursor(0,0);
  switch (messageTicker)
  {
    case 0 :  
             printAllValveState(&lcd ,  valvesState, valvesScheduled );
    break; 
    case 1 : 
             if (autopilotCreateNewJobs)
               { 
                 lcd.print("Every ");  
                 if (jobRunEveryXHours<24)
                   {
                     lcd.print((int) jobRunEveryXHours);
                     lcd.print(" hours    ");
                   } else
                   {
                     lcd.print((float) jobRunEveryXHours/24);
                     lcd.print(" days    ");
                   }  
                lcd.setCursor(0, 1);
                
                lcd.print("Start @ ");
                if (jobRunAtXHour<10) { lcd.print("0"); } 
                lcd.print((int) jobRunAtXHour);
                lcd.print(":");
                if (jobRunAtXMinute<10) { lcd.print("0"); } 
                lcd.print((int) jobRunAtXMinute); 
                lcd.print("   ");
               } else
               {
                lcd.print(systemName);  
                lcd.setCursor(0, 1); 
                lcd.print("No Job Scheduled");
               }    
             
    break; 
    case 2 :  
             lcd.print("Temp / Humidity ");
             lcd.setCursor(0, 1);  
             lcd.print("  ");
             lcd.print((int)temperature); lcd.print(" *C, ");
             lcd.print((int)humidity); lcd.print(" %   ");  
    break; 
    case 3 : 
             lcd.print(clock.dateFormat(" d F Y   ",  dt));
             lcd.setCursor(0, 1);
             lcd.print(clock.dateFormat("    H:i:s   ",  dt));
    break; 
    case 4 : 
             lcd.print(systemName);  
             lcd.setCursor(0, 1); 
             lcd.print("Uptime : ");
               
             if (elapsedTime>26400) {  
                                    lcd.print((unsigned int) elapsedTime/26400);
                                    lcd.print(" days   "); 
                                   } else
             if (elapsedTime>3600) {  
                                    lcd.print((unsigned int) elapsedTime/3600);
                                    lcd.print(" hours   "); 
                                   } else
             if (elapsedTime>60)  {  
                                    lcd.print((unsigned int) elapsedTime/60);
                                    lcd.print(" min   "); 
                                  } else
                                  {  
                                    lcd.print((unsigned int) elapsedTime);
                                    lcd.print(" sec   "); 
                                  }
    break;   
    case 5 :   
             printAllValveState(&lcd ,  valvesState, valvesScheduled );
    break; 
    case 6 : 
             lcd.print(systemName);  
             lcd.setCursor(0, 1); 
             lcd.print(systemVersion);  
    break; 
    default :
             lcd.print("TODO:");  
             lcd.setCursor(0, 1); 
             lcd.print("messages");  
    break;

    //REMEMBER TO UPDATE numberOfMenus 
  }
}





void valveAutopilot()
{
  unsigned int changes=0;
  unsigned int i=0;
  unsigned int valvesRunning=0;
  unsigned int valvesRemaining=0;
  
  for (i=0; i<NUMBER_OF_SWITCHES; i++)
  {
    if (valvesState[i])     {++valvesRunning;} 
    if (valvesScheduled[i]) {++valvesRemaining;} 
  }
  
  if (valvesRunning>0)
  {  
   //Check if a valve needs to be closed.. 
   for (i=0; i<NUMBER_OF_SWITCHES; i++)
   {
    if (valvesState[i])
    {
      //This valve is running, should it stop?  
      unsigned int runningTime =  getValveRunningTimeMinutes(
                                                             valveStartedTimestamp[i],
                                                             valvesState[i],
                                                             dt.unixtime  
                                                            );
      if ( runningTime > valvesTimes[i] ) 
         { 
           //This valve has run its course so we stop it
           valvesScheduled[i]=0;
           valvesState[i]=0;
           //We mark the time of stopping it
           valveStoppedTimestamp[i]=dt.unixtime;
           ++changes;
         }
    }
   }
  }
  
  
  //If autopilotCreateNewJobs is set
  if (autopilotCreateNewJobs)
  {   
    
   //Check which valves should be scheduled for start
   for (i=0; i<NUMBER_OF_SWITCHES; i++)
    {
        if ( getValveOfflineTimeSeconds(valveStoppedTimestamp[i],valvesState[i],dt.unixtime) >  getTimeAfterWhichWeNeedToReactivateValveInSeconds(jobRunEveryXHours) )
           {  
              if (currentTimeIsCloseEnoughToHourMinute(dt.unixtime,jobRunAtXHour,jobRunAtXMinute))
              {
                valvesScheduled[i]=1;
              }
           }
    } 
    
    
  //Recount valve status  
  for (i=0; i<NUMBER_OF_SWITCHES; i++)
  {
    if (valvesState[i])     {++valvesRunning;} 
    if (valvesScheduled[i]) {++valvesRemaining;} 
  }
    
      
  //We can accomodate more jobs..
  if ( (valvesRunning<jobConcurrency) && (valvesRemaining>0)  )
  {
    for (i=0; i<NUMBER_OF_SWITCHES; i++)
    {
      //Open first possible scheduled valve 
      if ( (valvesScheduled[i]) && (!valvesState[i]) && (valvesRunning<jobConcurrency) )
      {    
            ++changes;
            ++valvesRunning;
            valvesState[i]=1;
            valveStartedTimestamp[i]=dt.unixtime; 
      }
    } 
  }
  }
  
  
  
  if (changes) 
  {
   setRelayState(valvesState);  
  }
}








void menuDisplay(int menuOption)
{
  byte valveNum = 0;
  if (menuOption>=3) { valveNum=menuOption-3; }
  byte valveHumanNum = valveNum+1;
  
  lcd.setCursor(0,0);
  
  byte *selectedSpeeds = 0;             
  switch (menuOption)
  {
    case 0 :
    case 1 :
    case 2 :
    lcd.print(systemName);  
             if (menuOption==0) { selectedSpeeds = valvesTimesNormal;  } else
             if (menuOption==1) { selectedSpeeds = valvesTimesHigh;    } else
             if (menuOption==2) { selectedSpeeds = valvesTimesLow;     }  
             
             lcd.setCursor(0, 1); 
             if (armedTimes==selectedSpeeds) 
                  { 
                    if (autopilotCreateNewJobs)
                     {
                      lcd.print(" Running "); 
                     } else
                     {
                      lcd.print(" Start "); 
                     }
                  } else
                  { 
                    if (autopilotCreateNewJobs)
                     {
                      lcd.print(" SwitchTo "); 
                     } else
                     { 
                      lcd.print("   Arm ");   
                     }
                  }
             lcd.print(valveSpeeds[menuOption]);
             lcd.print("     ");
             
              
             
             if (joystick.jButton)
               { 
                 if (armedTimes==selectedSpeeds) 
                  {
                   valvesTimes=selectedSpeeds; 
                   //Do start 
                   autopilotCreateNewJobs=1;
                   scheduleAllValves();
                  } else
                  {
                   valvesTimes=selectedSpeeds; 
                   armedTimes=selectedSpeeds;
                  }
               }
    break;   
    //------------------------------------ 
    case 3 : 
    case 4 : 
    case 5 : 
    case 6 : 
    case 7 : 
    case 8 : 
    case 9 : 
    case 10: 
             lcd.print("V");                          
             lcd.print((int) (valveHumanNum));
             lcd.print(" ");
             lcd.print(valveLabels[valveNum]);
             lcd.setCursor(0, 1); 
             if (valvesState[valveNum]) { lcd.print((char)ON_LOGO);  } else  
                                        { lcd.print((char)OFF_LOGO); }
                                        
             if ((valvesScheduled[valveNum]) && (valvesState[valveNum]) )
              {
               lcd.print(" Remain: ");
                unsigned int remainingTime = getValveRemainingTimeMinutes(
                                                                          valveStartedTimestamp[valveNum],
                                                                          valvesTimes[valveNum],
                                                                          valvesState[valveNum],
                                                                          dt.unixtime
                                                                         );
                if (remainingTime>0) { 
                                       lcd.print((int) remainingTime );
                                       lcd.print("min  ");
                                     }  
              } else
              {
               lcd.print("  Time: ");
               lcd.print((int) (valvesTimes[valveNum]));
               lcd.print("min  ");
              }  
             if ( 
                 joystickValveTimeHandler(
                                          &joystick.jDirection,
                                          &joystick.jButton,
                                          valveNum,
                                          dt.unixtime,
                                          &idleTicks,
                                          valvesState, 
                                          valvesTimes,
                                          valvesScheduled,
                                          valveStartedTimestamp,
                                          valveStoppedTimestamp 
                                         )
            
               )
             {
                setRelayState(valvesState);
             }
    break;
    //------------------------------------ 
    case 11 :
             lcd.print("  Change Date  ");
             lcd.setCursor(0, 1);  
             lcd.print(clock.dateFormat(" d F Y     ",  dt)); 
             
     break;
    case 12 :
             lcd.print("  Change Time  ");
             lcd.setCursor(0, 1);  
             lcd.print(clock.dateFormat("    H:i:s     ",  dt));
             
     break;
    //------------------------------------ 
    
    
    
    case 13 :
             lcd.print("Start Water At :  ");
             lcd.setCursor(0, 1); 
             lcd.print("     ");
             
             if (jobRunAtXHour<10) { lcd.print("0"); } 
             lcd.print((int) jobRunAtXHour);
             lcd.print(":");
             if (jobRunAtXMinute<10) { lcd.print("0"); } 
             lcd.print((int) jobRunAtXMinute); 
             lcd.print("        ");
            
            joystick24HourTimeHandler( 
                                       &joystick.jDirection , 
                                       &idleTicks , 
                                       &jobRunAtXHour,
                                       &jobRunAtXMinute
                                     );    
    break;
    case 14 :
             lcd.print("  Water Every :  ");
             lcd.setCursor(0, 1); 
             lcd.print("    ");
             if (jobRunEveryXHours<24)
                   {
                     lcd.print((int) jobRunEveryXHours);
                     lcd.print(" hours    ");
                   } else
                   {
                     lcd.print((float) jobRunEveryXHours/24);
                     lcd.print(" days    ");
                   }   
            joystickHourTimeHandler(&joystick.jDirection,&idleTicks,&jobRunEveryXHours,0,255);       
    break;
    //------------------------------------ 
    case 15 :
             lcd.print("    Stop All    ");
             lcd.setCursor(0, 1); 
             lcd.print("     Valves     ");
             if (joystick.jButton)
               { 
                 turnAllValvesOff();
               }
    break;
    //------------------------------------ 
    case 16 :
             lcd.print("    Autopilot    ");
             lcd.setCursor(0, 1); 
             if (autopilotCreateNewJobs) { lcd.print("       On       ");  } else
                                         { lcd.print("       Off       "); }
             if (joystick.jButton)
               { 
                 if (autopilotCreateNewJobs) { autopilotCreateNewJobs=0; } else
                                             { autopilotCreateNewJobs=1; }
               }
    break;
    //------------------------------------ 
  } 
}
 
void loop() 
{  
  byte prevIdleTicks = idleTicks;
  grabMeasurements(); 
  checkForSerialInput();
  int seconds = millis() / 1000; 
   
  
  
  if (joystick.jButton) 
  {
    if (powerSaving)
    {
      joystick.jButton=0; 
      idleTicks=0; 
      turnLCDOn();
    }
  }
  
   
   
  if ( 
       joystickMenuHandler(
                           &joystick.jDirection,
                           &idleTicks,
                           &currentMenu,
                           numberOfMenus
                          ) 
     )
   {
     turnLCDOn();
   }
  
  if ( (idleTicks>200) || (powerSaving) )
  {
    //Switch monitor off 
    turnLCDOff();
  } else
  if (idleTicks>45)
  {
    //Display ScreenSaver logos
    idleMessageTicker(seconds);
  } else 
  {
    //Display Menu 
    menuDisplay(currentMenu);
  }
   
   valveAutopilot(); 
 
   //If a wakeup event has happened turn on 
   if ( (idleTicks==0) && (prevIdleTicks!=0) )  
   {
     turnLCDOn();
   }
  
   //Stop counter from overflow..
   if (idleTicks<253) { ++idleTicks; } 
   
   
   //-------------------------------------
   //      Everything works at 1Hz
    if (powerSaving) { delay(1500);  } else
                     { delay(450);   }
   //-------------------------------------
}

