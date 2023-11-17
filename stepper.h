/*
 * stepper.h
 *
 * Created: 2023-11-13 4:38:45 PM
 *  Author: edwin
 */ 


#ifndef STEPPER_H_
#define STEPPER_H_

#define QUARTER_TURN 50
#define HALF_TURN 100

void home();
void move(int c);
void sort(char list_item);
void print_results();
void pause();
void stepper_delay(int i, int c, int total);

// Global variables for stepper control
volatile char disk_location;	// Black, White, Steel, Aluminum
volatile int homed_flag = 0;	// Set to 1 once homing sensor is tripped and bucket is homed on black
int position = 0;			// stepper position in stepper array(0-3)
int disk_direction = 0;		// 0 = clockwise 1 = counter clockwise
int items_sorted = 0;		// for testing only
int stepper[4] = {0b00110011, 0b00011101, 0b00101101, 0b00101011};	// Stepper positions
int delay[29] = {20,20,18,18,16,16,14,14,12,12,10,10,8,8,6,8,8,10,10,12,12,14,14,16,16,18,18,20,20};

#endif /* STEPPER_H_ */