/*
 * stepper.h
 *
 * Created: 2023-11-13 4:38:45 PM
 *  Author: edwin
 */ 


#ifndef STEPPER_H_
#define STEPPER_H_

#define QUARTER_TURN 5
#define HALF_TURN 10

void home();
void move(int c);
void sort(char list_item);
void print_results();

// Global variables for stepper control
volatile char disk_location;	// Black, White, Steel, Aluminum
volatile int homed_flag = 0;	// Set to 1 once homing sensor is tripped and bucket is homed on black
int position = 0;			// stepper position in stepper array(0-3)
int disk_direction = 0;		// 0 = clockwise 1 = counter clockwise
int items_sorted = 0;		// for testing only
int stepper[4] = {0b00000011, 0b00011000, 0b00000101, 0b00101000};	// Stepper positions

#endif /* STEPPER_H_ */