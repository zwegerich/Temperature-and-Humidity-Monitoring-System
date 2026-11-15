/*
 * sensor.h
 *
 *  Created on: Nov 9, 2026
 *      Author: zoewegerich
 */

#ifndef SENSOR_H_
#define SENSOR_H_
#include <stdint.h>
#include <stdbool.h>
#include <inc/hw_memmap.h>
#include <inc/hw_ints.h>
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>
#include <driverlib/pin_map.h>
#include <driverlib/timer.h>
#include "launchpad.h"


typedef struct {
    Event *callback_event;
    enum {
        IDLE,                    // Sensor is idle
        EXPECT_START_EVENT_1,    // Expecting the rising edge of the start pulse
        EXPECT_START_EVENT_2,    // Expecting the falling edge of the start pulse
        EXPECT_LOW_SIGNAL_EVENT, // Expecting the 50 microsecond low voltage pulse it sensor sends before each data pulse
        EXPECT_DATA_EVENT_1,     // Expecting the rising edge of the data pulse
        EXPECT_DATA_EVENT_2      // Expecting the falling edge of the data pulse

    } state;

    bool new_data_ready;   // Data transfer from sensor complete (T/F)
    int start;             // Start time of a signal pulse
    int pulsewidth;        // Total time from rising edge to falling edge of signal pulses
    int dataBits[40];      // Array of the data bits being stored
    int bit_number;        // Number of bits stored in the data array


} SensorState;
extern volatile SensorState sensor;     // Extern so it can be used in .c


// Initialize the sensor.
void SensorInit();

// Send start pulse
void SensorTriggerReading();

// Check if any new ranger data is ready (used for polling)
bool SensorDataReady();

// ISR function
void SensorISR();

// Register an event object for the ISR to trigger
void SensorEventRegister(Event *event);

//function to convert binary data to integers
int ConvertBits(volatile int bits[], int startBit, int numBits);


#endif /* SENSOR_H_ */
