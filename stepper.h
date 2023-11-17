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
int stepper[4] = {0b00100011, 0b00011100, 0b00010101, 0b00101010};	// Stepper positions
int delay_a[50] = {20,19.5,19,18.5,18,17.5,17,16.5,16,15.5,15,14.5,14,13.5,13,12.5,12,11.5,11,10.5,10,9.5,9,8.5,8
					,7.5,8.5,9,9.5,10,10.5,11,11.5,12,12.5,13,13.5,14,14.5,15,15.5,16,16.5,17,17.5,18,18.5,19,19.5,20};

int delay_b[100] = {20,19.75,19.5,19.25,19,18.75,18.5,18.25,18,17.75,17.5,17.25,17,16.75,16.5,16.25,16,15.75,15.5,15.25,15,14.75,14.5,14.25,14,
					13.75,13.5,13.25,13,12.75,12.5,12.25,12,11.75,11.5,11.25,11,10.75,10.5,10.25,10,9.75,9.5,9.25,9,8.75,8.5,8.25,8,7.75,
					8,8.25,8.5,8.75,9,9.25,9.5,9.75,10,10.25,10.5,10.75,11,11.25,11.5,11.75,12,12.25,12.5,12.75,13,13.25,13.5,13.75,14,14.25,
					14.5,14.75,15,15.25,15.5,15.75,16,16.25,16.5,16.75,17,17.25,17.5,17.75,18,18.25,18.5,18.75,19,19.25,19.5,19.75,20};

#endif /* STEPPER_H_ */