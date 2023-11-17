/* ###################################################
# MILESTONE: 5
# PROGRAM: 1
# PROJECT: Conveyor Belt Demo
# GROUP: 2
# NAME 1: Ryland, Nezil, V00157326
# NAME 2: Reilly, Reilly, V00
# DESC: Sort items
# DATA
# REVISED ############################################*/

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdlib.h>
#include "lcd.h"
#include "stepper.h"
//#include "LinkedQueue.h"

// Note: interrupts have priority. INT0 is top priority
// external interrupt, INT7 is lowest priority external
// interrupt. Interrupt priority is used to determine
// which interrupt will be serviced first if both fire
// at the same time. You can only have ONE pending
// interrupt PER interrupt, i.e. you cannot have two
// INT0's pending at the same time. However, you CAN
// have multiple different interrupts pending, i.e. you
// could have INT1 INT2 and INT3 all pending at the same
// time while INT0 is being serviced.

// Need to use global variables because you cannot
// define a variable within an ISR
volatile unsigned char ADC_result_msbs;
volatile unsigned char ADC_result_lsbs;
volatile unsigned char ADC_result_flag;
volatile unsigned char forwards;

// Millisecond timer
void mTimer(int count);

int main(int argc, char* argv[])
{	
	// Reduce clock rate from 16MHz to 8MHz
	CLKPR = 0x80;
	CLKPR = 0x01;
	
	// Initialize display
	InitLCD(LS_BLINK|LS_ULINE);

	// Clear the screen
	LCDClear();

	/*	FIFO QUEUE	*/
	
	/*
	// Prepare the queue
	link* head;
	link* tail;
	link* temp;
	char qData;
	setup(&head, &tail);
	*/
	
	// Enter uninterruptable command system
	cli();
	
	// Set initial system state
	forwards = 0x00;
		
	// PWM Out
	DDRB = 0x80;
	
	// DC Motor
	DDRL = 0xF0;
	PORTL |= 0xF0;
	
	// Stepper Motor
	DDRA = 0xFF;
	
	// LCD Display
	DDRC = 0xFF;
	
	// LED Debug Bank
	DDRK = 0xFF;
	DDRF = 0xC0;
	
	// Set clock prescaler for timer 1B to 1/8
	TCCR1B |= _BV(CS11);
	
	// Enable external interrupts 0, 1, 2 in falling
	// edge mode
	EIMSK |= _BV(INT0) | _BV(INT1) | _BV(INT2);
	EICRA = 0x2A;
	
	// Enable ADC
	ADCSRA |= _BV(ADEN);
	
	// Enable automatic interrupt firing after each completed conversion
	ADCSRA |= _BV(ADIE);
	
	// Enable automated conversions in free running mode
	ADCSRA |= _BV(ADATE);
	
	// Set waveform generation mode to Fast PWM
	// with TOP = OCR0A, update OCRA at TOP, set
	// TOV at MAX
	TCCR0A |= _BV(WGM01) | _BV(WGM00);
	
	// Set initial duty cycle
	OCR0A = 0x3F;
	
	// Clear OC0A on Compare Match, set OC0A at BOTTOM
	TCCR0A |= _BV(COM0A1);
	
	// Set timer prescaler
	// 256 ticks per in a period
	// 8MHz/256 = 31.25KHz cycle w/ no prescaling
	// Therefore want prescaling of 1/8 to get 3.9KHz
	TCCR0B |= _BV(CS01);
	
	// Exit uninterruptable command sequence
	sei();
	
	// Initialize ADC, start one conversion at the
	// beginning
	ADCSRA |= _BV(ADSC);
	/*
	// Can be removed after testing
	char list[] = {'a','b','s','w','w','a','s','b','w','s','b'};
	LCDWriteStringXY(0,0,"Disk is homing");
	home();*/
	
	while(1)
	{	
		// 10-bit ADC test with LEDs
		if(ADC_result_flag)
		{
			PORTK = (ADC_result_msbs << 6) | (ADC_result_lsbs >> 2);
			PORTF = ADC_result_lsbs << 6;
			ADC_result_flag = 0x00;
		}
		
		/*
		// This for loop will be replaced with iteration through a linked list
		for(int i = 0; i < 11; i++){
			sort(list[i]);
			LCDClear();
			print_results();
			mTimer(2000);
		}*/
		items_sorted = 0;
	}
	
	return(0);
}

void mTimer(int count)
{
	// Initialize ms counter variable to zero
	int i = 0;
	
	// Set timer 1B to CTC compare
	TCCR1B |= _BV(WGM12);
	
	// Set TOP value for CTC comparison
	OCR1A = 0x03E8;
	
	// Set COUNT to zero
	TCNT1 = 0x0000;
	
	// Clear interrupt flag and start timer
	TIFR1 |= _BV(OCF1A);
	
	while(i < count)
	{
		// If flag set, increment counter and clear flag
		if(TIFR1 & 0x02)
		{
			TIFR1 |= _BV(OCF1A);
			i++;
		}
	}
	
	return;
}

//This function rotates the stepper clockwise until the homing sensor triggers the INT2 interrupt
void home(){
	while (homed_flag != 1){
		PORTA = stepper[position];
		mTimer(20);
		position++;
		if(position == 4){
			position = 0;
		}
	}
	LCDWriteStringXY(0,0,"Disk homed Black");	// Can be removed after testing
	mTimer(1000);
}

//This function moves the stepper clockwise(0) or counter clockwise(1) 90 degrees or 180 degrees
void move(int c){
	if (disk_direction == 0){
		while (c > 0){
			PORTA = stepper[position];
			mTimer(20);
			position++;
			c--;
			if(position == 4){
				position = 0;
			}
			if(c == 0){
				break;
			}
		}
	}
	if (disk_direction == 1){
		while (c > 0){
			PORTA = stepper[position];
			mTimer(20);
			position--;
			c--;
			if(position < 0){
				position = 3;
			}
			if(c == 0){
				break;
			}
		}
	}
}

// This function moves the sorting bucket to a location based on part in list
void sort(char list_item)
{
	switch(disk_location)
	{
		case 'b':
		switch(list_item)
		{
			case 'b':
			break;
			
			case 'a':
			disk_direction = 0;
			move(QUARTER_TURN);
			disk_location = 'a';
			break;
			
			case 's':
			disk_direction = 1;
			move(QUARTER_TURN);
			disk_location = 's';
			break;
			
			case 'w':
			move(HALF_TURN);
			disk_location = 'w';
			break;
			
			default:
			// exit with error code
			exit(1);
		}
		break;
		
		case 'a':
		switch(list_item)
		{
			case 'a':
			break;
			
			case 'b':
			disk_direction = 1;
			move(QUARTER_TURN);
			disk_location = 'b';
			break;
			
			case 's':
			move(HALF_TURN);
			disk_location = 's';
			break;
			
			case 'w':
			disk_direction = 0;
			move(QUARTER_TURN);
			disk_location = 'w';
			break;
			
			default:
			// exit with error code
			exit(1);
		}
		break;
		
		case 'w':
		switch(list_item)
		{
			case 'w':
			break;
			
			case 'b':
			move(HALF_TURN);
			disk_location = 'b';
			break;
			
			case 'a':
			disk_direction = 1;
			move(QUARTER_TURN);
			disk_location = 'a';
			break;
			
			case 's':
			disk_direction = 0;
			move(QUARTER_TURN);
			disk_location = 's';
			break;
			
			default:
			// exit with error code
			exit(1);
		}
		break;
		
		case 's':
		switch(list_item)
		{
			case 's':
			break;
			
			case 'b':
			disk_direction = 0;
			move(QUARTER_TURN);
			disk_location = 'b';
			break;
			
			case 'a':
			move(HALF_TURN);
			disk_location = 'a';
			break;
			
			case 'w':
			disk_direction = 1;
			move(QUARTER_TURN);
			disk_location = 'w';
			break;
			
			default:
			// exit with error code
			exit(1);
		}
		break;
		
		default:
		// exit with error code
		exit(1);
	}
	
	items_sorted++;
	// dump item into bucket
}

// Function to test sorting of a list
void print_results(){
	if (disk_location == 'b'){
		LCDWriteStringXY(0, 0, "Disk on Black" );
		LCDWriteStringXY(0, 1, "Items Sorted: ");
		LCDWriteIntXY(14,1, items_sorted, 2);
	}
	else if (disk_location == 'w'){
		LCDWriteStringXY(0, 0, "Disk on White" );
		LCDWriteStringXY(0, 1, "Items Sorted: ");
		LCDWriteIntXY(14,1, items_sorted, 2);
	}
	else if (disk_location == 's'){
		LCDWriteStringXY(0, 0, "Disk on Steel" );
		LCDWriteStringXY(0, 1, "Items Sorted: ");
		LCDWriteIntXY(14,1, items_sorted, 2);
	}
	else if (disk_location == 'a'){
		LCDWriteStringXY(0, 0, "Disk on Aluminum" );
		LCDWriteStringXY(0, 1, "Items Sorted: ");
		LCDWriteIntXY(14,1, items_sorted, 2);
	}
}

/*
// Killswitch ISR
ISR(INT0_vect)
{
	// Stop motor and wait for reset
	PORTL |= 0xF0;
	while(1);
}
*/

// Stop conveyor belt ISR
ISR(INT1_vect)
{
	// Don't brake low + debounce
	PORTL |= 0xF0;
	mTimer(20);
	
	if( forwards )
	{
		// CW/backwards rotation
		//PORTL &= 0xBF;
		forwards = 0x00;
	}
	else
	{
		// CCW/forwards rotation
		PORTL &= 0x7F;
		forwards = 0x01;
	}
}

/*
//Stepper homing interrupt
ISR(INT2_vect){
	disk_location = 'b';
	homed_flag = 1;
	EIMSK = 0x00;	// Disables the INT2 interrupt
}*/

// ISR for ADC Conversion Completion
ISR(ADC_vect)
{
	// Get ADC result
	ADC_result_lsbs = ADCL;
	ADC_result_msbs = ADCH & 0x03;
	
	// Set flag indicating a result has been written
	ADC_result_flag = 1;
}