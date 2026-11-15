/*
 * sensor.c
 *
 *  Created on: Nov 9, 2026
 *      Author: zoewegerich
 */

#include "sensor.h"

/*
 * Global/static variables and declarations
 */

volatile SensorState sensor;

void SensorISR();

/*
 * Initialize the sensor, connected to PD1. The timer is WTIMER2 and the sub-Timer is A.
 */
void SensorInit()
{
    // Initial sensor state in memory
    sensor.callback_event = NULL;
    sensor.new_data_ready = false;

    // Enable GPIO Port D and Wide Timer 2
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WTIMER2);

    // Configure PD1 as timer pin and connect it to WT2CCP1
    // Do NOT enable the time as this time
    GPIOPinTypeTimer(GPIO_PORTD_BASE, GPIO_PIN_1);
    GPIOPinConfigure(GPIO_PD1_WT2CCP1);

    // Count both edges as event
    TimerControlLevel(WTIMER2_BASE, TIMER_B, true);
    TimerControlEvent(WTIMER2_BASE, TIMER_B, TIMER_EVENT_BOTH_EDGES);

    // Register an ISR to deal with the timeout event
    TimerIntRegister(WTIMER2_BASE, TIMER_B, SensorISR);
    TimerIntEnable(WTIMER2_BASE, TIMER_CAPB_EVENT);
}

/*
 * Send the start pulse to the sensor. The pin being used will be switched to timer input pin at the end.
 */
void SensorTriggerReading()
{
    // Clean data ready flag and set the sensor state (no new data yet)
    sensor.new_data_ready = false;
    sensor.state = EXPECT_START_EVENT_1;

    // Disable timer before reconfiguring
    TimerDisable(WTIMER2_BASE, TIMER_B);

    // Need 18 ms low time, 20-40 microseconds of high time
    // 50 MHz clock cycle -> 18ms = 900,000 ticks , 30us = 1500 ticks
    TimerConfigure(WTIMER2_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_B_PWM);       // Configure timer to PWM

    TimerLoadSet(WTIMER2_BASE, TIMER_B, 900000);    // 18 ms low time

    TimerMatchSet(WTIMER2_BASE, TIMER_B, 1500);     // 30 us high time

    TimerIntClear(WTIMER2_BASE, TIMER_CAPB_EVENT);

    // Enable the timer, and start PWM waveform (start signal)
    TimerEnable(WTIMER2_BASE, TIMER_B);
}

/*
 * If any new data is ready (for polling)
 */
bool SensorDataReady()
{
    return sensor.new_data_ready;
}


/*
 * Interrupt routine
 */

void SensorISR()
{
    // Every time the function is called new current time is saved
    // Use the current times for calculations of pulse widths
    int currentTime = TimerValueGet(WTIMER2_BASE, TIMER_B);

    switch (sensor.state)
    {
    case EXPECT_START_EVENT_1:

        // The start pulse has been sent


        // Need to ignore first pulse from sensor (the beginning 50 us low pulse is NOT data)
        // Only start putting the pulse data into array after 50 us pusle ends
        // So start the array index at -1 to avoid storing the invalid data
        sensor.bit_number = -1;

        //switch to next state: sensor expects falling edge of start pulse
        sensor.state = EXPECT_START_EVENT_2;
        break;

    case EXPECT_START_EVENT_2:

        //disable timer before reconfiguring
        TimerDisable(WTIMER2_BASE, TIMER_B);

        //configure timer to time capture
        TimerConfigure(WTIMER2_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_B_CAP_TIME);

        //change load value to large number
        TimerLoadSet(WTIMER2_BASE, TIMER_B, 0xFFFFFFFF);

        //count both edges as an event
        TimerControlEvent(WTIMER2_BASE, TIMER_B, TIMER_EVENT_BOTH_EDGES);

        //clear interrupts
        TimerIntClear(WTIMER2_BASE, TIMER_CAPB_EVENT);

        //re-enable timer
        TimerEnable(WTIMER2_BASE, TIMER_B);

        //change the state after everything is configured
        //the 50 us low pulse is the next expected signal
        sensor.state = EXPECT_LOW_SIGNAL_EVENT;
        break;

    case EXPECT_LOW_SIGNAL_EVENT:

        //the first low signal from the sensor is not actual data, just the intitial low pulse before each bit is transmitted
        //after this pulse, data will start being sent --> expect first rising edge of data
        sensor.state = EXPECT_DATA_EVENT_1;
        break;


    case EXPECT_DATA_EVENT_1:
        //capture time of rising edge
        sensor.start = currentTime;

        //change the state to expect the falling edge
        sensor.state = EXPECT_DATA_EVENT_2;
        break;

    case EXPECT_DATA_EVENT_2:

        //calculate difference of times
        //we got the start time during the last case
        //find difference between then and the current time
        sensor.pulsewidth = sensor.start - currentTime;

        if (sensor.bit_number >= 0){                    //ignore the "-1st" bit - it is not data
            if(sensor.pulsewidth > 1750){
               sensor.dataBits[sensor.bit_number] = 1;  //pulse > 1750 ticks = 1
            }
            else{
                sensor.dataBits[sensor.bit_number] = 0; //pulse < 1750 ticks = 0
            }
        }
        sensor.bit_number++;


        //if we have gathered < 40 bits, go back to expect data event 1 to start another data read
        if(sensor.bit_number < 40){
            sensor.state = EXPECT_DATA_EVENT_1;
        }

        //once all 40 bits are gathered, go idle, say there is new data ready
        else{
            sensor.state = IDLE;
            sensor.new_data_ready = true;
            if (sensor.callback_event != NULL){
                EventSchedule(sensor.callback_event, 0);
            }
        }
        break;
    }

    TimerIntClear(WTIMER2_BASE, TIMER_CAPB_EVENT);
}



//set callback event for sensor ISR
void SensorEventRegister(Event *event)
{
    sensor.callback_event = event;

}

//function to convert the bits stored in the array to an integer
//in array bits[], convert numBits bits in total, starting at index startBit.
int ConvertBits(volatile int bits[], int startBit, int numBits)
{
    int number = 0;   //accumulated value starts at 0, shifting will calculate the integer value (multiplying by 2 over and over)
    int i;
    for (i = startBit; i < startBit + numBits; i++){
        number = (number << 1) | (bits[i]);         //shift accumulated value left (multiplies the whole thing by 2)
                                                    //put the next bit into that newly opened space
    }
    return number;    //return the accumulated value (now an int)
}


