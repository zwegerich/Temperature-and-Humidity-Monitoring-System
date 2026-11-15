

/**
 * main.c
 */


#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "launchpad.h"
#include "seg7.h"
#include "sensor.h"

// TESTING


// Data struct for storing the temperature and humidity
typedef struct {

    int humidity;
    int temp;
    bool dataReady;

} sensorData;

sensorData DHT11;



// State struct for display mode (temp or humidity)
typedef struct {
    enum {Temperature, Humidity} display_mode;
} SysState;

SysState sys = {Temperature};  // default is to display temp



// State struct for seven segment display
Seg7Display seg7;


// The events
Event trigger_sensor_reading_event;
Event sensor_read_event;
Event push_button_event;




// Trigger the ranger reading every 2 seconds
// Max sampling rate is 1Hz, so cant sample more than once per second
void TriggerSensorReading(Event *event)
{
    SensorTriggerReading();
    int delay = 2000;
    EventScheduleAfter(event, delay);
}


// Convert the humidity data (call this in the print function)
float GetHumidity()
{
     int humidityInteger = ConvertBits(sensor.dataBits, 0, 8);   //bits 0-7 are integer part of humidity
     int humidityDecimal = ConvertBits(sensor.dataBits, 8, 8);   //bits 8-15 are decimal part of humidity (always 0 on DHT11 tho)

     float humidity = humidityInteger + (humidityDecimal/10.0);  //convert to a xx.x form

     return humidity;
}


// Convert the temperature data (call this in the print function)
float GetTemperature()
{
    int tempInteger = ConvertBits(sensor.dataBits, 16, 8);      //bits 16-23 are integer part of temp
    int tempDecimal = ConvertBits(sensor.dataBits, 24, 8);      //bits 24-31 are decimal part of temp

    float temp = tempInteger + (tempDecimal/10.0);              //convert to a xx.x form

    return temp;

}

void CheckPushButton(Event *event)
{
    int button = PushButtonRead();       //Read the pressed button

    if(button == 1){
        sys.display_mode = Temperature;  //SW1: display temp
    }
    else if (button == 2){
        sys.display_mode = Humidity;     //SW2: display humidity
    }
}

// Print the temperature and humidity data
void SensorDataPrint(Event *event)
{
    //call functions to get the temp and humidity doubles (for terminal printing)
    double humidity = GetHumidity();
    double temp = GetTemperature();

    //convert the doubles to integer numbers so modulo can be used (for seven seg)
    int humidityInt = humidity;     //dont need the decimals when converting to int for humidity (decimal part always 0)
    int tempInt = temp * 100;       //DO need decimal values for temp; convert 45.56 --> 4556, etc.

    DHT11.dataReady = SensorDataReady();

    if(DHT11.dataReady == 1)                    // If data has been collected, display it
    {
        if (sys.display_mode == Temperature) {  //Check if temp mode is selected

          //Display the temperature
          seg7.digit[0] = tempInt % 10;         //display hundreths digit
          seg7.digit[1] = (tempInt/10) % 10;    //display tenths digit
          seg7.digit[2] = (tempInt/100) % 10;   //display ones digit
          seg7.digit[3] = (tempInt/1000) % 10;  //display tens digit

          seg7.colon_on = true;                 //turn on the colon to represent decimal
          Seg7Update(&seg7);                    //update the 7-seg display

          }
        else {
          //Display humidity (no decimal)
          seg7.digit[0] = humidityInt % 10;     //display hundreths digit
          seg7.digit[1] = (humidityInt/10) % 10;//display tenths digit


          seg7.digit[2] = 10;        //blank
          seg7.digit[3] = 10;        //blank

          seg7.colon_on = false;     //turn off the colon
          Seg7Update(&seg7);         //update the 7-seg display


    }
        //Print the temp and humidity in the terminal
        uprintf("Temperature: %.2lf °C   Humidity: %d%%\n\r", temp, humidityInt);
    }

    else // If data is not ready yet for whatever reason
    {
        uprintf("Gathering data\n\r");
    }

}



/*******************************************
 * The main function
 ******************************************/
void main(void)
{
    // Initialize the LaunchPad and peripherals
    LaunchPadInit();
    SensorInit();
    Seg7Init();     // (for later use)


    // Initialize the events
    EventInit(&trigger_sensor_reading_event, TriggerSensorReading);
    EventInit(&sensor_read_event, SensorDataPrint);
    EventInit(&push_button_event, CheckPushButton);  //Initialize the pushbutton event

    // Register event
    SensorEventRegister(&sensor_read_event);
    PushButtonEventRegister(&push_button_event);  //Register the pushbutton callback

    // Schedule time event
    EventSchedule(&trigger_sensor_reading_event, 100);

//TEST COMMENT



    while (true)
    {
        // Process any pending events
        EventProcess();

        // Wait for new events; CPU will sleep
        EventWait();
    }
}

