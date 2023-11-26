/* ##################################################
# MILESTONE: 5
# PROGRAM: 1
# PROJECT: Conveyor Belt Demo
# GROUP: 2
# NAME 1: Ryland, Nezil, V00157326
# NAME 2: Reilly, Reilly, V00894910
# DESC: Sort steel, aluminium, black plastic and white plastic blocks
# DATA
# REVISED ############################################*/

//#define PRECALIBRATION_MODE
#define CALIBRATION_MODE
//#define TIMER_CALIBRATION_MODE

#ifdef TIMER_CALIBRATION_MODE

// Determines how long reflective sensor must pick up
// readings above the no-item threshold before item
// processing can be completed.
// NO_ITEM_TIME 2000 corresponds to ~5ms no-item time
// NO_ITEM_TIME 4000 corresponds to ~10ms no-item time
// etc.
#define NO_ITEM_TIME		8000

// Deviance from NO_ITEM_THRESHOLD that can
// still be counted as no-item status
#define AMBIENT_DEVIANCE	7

#endif

#ifndef SENSOR_VALUES
#define SENSOR_VALUES

#define NO_ITEM_THRESHOLD	992

#define STEEL_LOW			255
#define STEEL_HIGH			527

#define ALUMINIUM_LOW		35
#define ALUMINIUM_HIGH		48

#define BLACK_PLASTIC_LOW	931
#define BLACK_PLASTIC_HIGH	952

#define WHITE_PLASTIC_LOW	871
#define WHITE_PLASTIC_HIGH	896

// Time to run ADC conversions for upon seeing item
// Divide by 125 to get value in ms
#define STOPWATCH			25888

#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdlib.h>
#include "lcd.h"
#include "stepper.h"
#include "LinkedQueue.h"

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

// Stores ADC conversion result
volatile unsigned int ADC_result;

// Indicates ADC has completed a conversion
volatile int ADC_result_flag = 0;

// Indicates item has reached first optical sensor
volatile int inbound = 0;

// Indicates whether conveyor belt is running
volatile int running = 1;

// Indicates whether system is in ramp-down mode
volatile int ramp_down = 0;

// (description)
volatile unsigned int plastic = 0;
volatile unsigned int steel = 0;
volatile unsigned int alum = 0;
volatile unsigned char pause_flag = 0;
	
// Make queue head and tail global so that it can be
// accessed by ISR's
volatile link* head;
volatile link* tail;
volatile link* oldItem;

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

	// Prepare the queue
	link* newItem;
	setup(&head, &tail);
		
	// PWM Out
	DDRB = 0x80;
	
	// DC Motor
	DDRL = 0xF0;
	PORTL |= 0x70;
	
	// Stepper Motor
	DDRA = 0xFF;
	
	// LCD Display
	DDRC = 0xFF;
	
	// LED Debug Bank
	DDRK = 0xFF;
	DDRF = 0xC0;
	
	// Set timer 3 to CTC mode
	TCCR3B |= _BV(WGM32);
	
	// Set timer 3 clock prescaler to 1/64
	// -> timer 3 runs at 8MHz/64 = 125kHz
	// -> timer period = 0.52s
	TCCR3B |= _BV(CS31) | _BV(CS30);
	
	// Set clock prescaler for timer 1B to 1/8
	TCCR1B |= _BV(CS11);
	
	// Set based on timer calibration results
	#ifdef TIMER_CALIBRATION_MODE
	
	OCR3A = 0xFFFF;
	
	#else
	
	OCR3A = STOPWATCH;
	
	#endif
	
	// Enable ADC
	ADCSRA |= _BV(ADEN);
	
	// Enable automatic interrupt firing after each completed conversion
	ADCSRA |= _BV(ADIE);
	
	// Set waveform generation mode to Fast PWM
	// with TOP = OCR0A, update OCRA at TOP, set
	// TOV at MAX
	TCCR0A |= _BV(WGM01) | _BV(WGM00);
	
	// Set initial duty cycle
	OCR0A = 96;
	
	// Clear OC0A on Compare Match, set OC0A at BOTTOM
	TCCR0A |= _BV(COM0A1);
	
	// Set timer prescaler
	// 256 ticks per in a period
	// 8MHz/256 = 31.25KHz cycle w/ no prescaling
	// Therefore want prescaling of 1/8 to get 3.9KHz
	TCCR0B |= _BV(CS01);
	
	// Enter uninterruptable command sequence
	cli();
	
	// Set INT0 to any edge mode (kill switch)
	// Set INT1 to rising edge mode (pause resume)
	EICRA |= _BV(ISC00) | _BV(ISC11);
	EIMSK |= _BV(INT0) | _BV(INT1);
	
	// Precalibration mode: determine sensor value for
	// no-item case
	#ifdef PRECALIBRATION_MODE
	
	// Last result: 992
	
	sei();
	
	// Initialize ADC
	ADCSRA |= _BV(ADSC);
	while(!ADC_result_flag);
	ADC_result_flag = 0;
	
	// Give it a second or two
	mTimer(2000);
	
	// Determine the lowest value
	unsigned no_item_value = 0xFFFF;

	// Print values
	while(1)
	{
		ADCSRA |= _BV(ADSC);
		while(!ADC_result_flag);
		ADC_result_flag = 0;
		LCDClear();
		if(ADC_result < no_item_value) no_item_value = ADC_result;
		LCDWriteInt(no_item_value,4);
		mTimer(10);
	}
	
	#endif

	// Set INT5 to rising edge mode (first optical sensor)
	EICRB |= _BV(ISC51) | _BV(ISC50);
	EIMSK |= _BV(INT5);
	
	// Calibrate system: run each type through 10 times, take 
	// the range of the minimums of each piece
	#ifdef CALIBRATION_MODE
	
	sei();
	
	// Initialize ADC
	ADCSRA |= _BV(ADSC);
	while(!ADC_result_flag);
	ADC_result_flag = 0;

	// Track sensor values
	unsigned current_value;
	unsigned low_value = 0xFFFF;
	unsigned high_value = 0x0000;

	// Print info
	LCDWriteStringXY(0,0,"Run Item 10x:");
	
	// Process 10 items
	for(int j = 0; j < 10; j++)
	{
		// Reset current value to bogus number
		current_value = 1337;
		
		// Wait for item
		while(!inbound);
		inbound = 0;
		
		// Indicate item seen
		LCDWriteIntXY(14,0,(j+1),2);
		
		// Zero the timer
		TCNT3 = 0x0000;
	
		// Start timer after recognizing item
		TIFR3 |= _BV(OCF3A);
		
		// Determine item value
		while(!(TIFR3 & _BV(OCF3A)))
		{
			// Do a conversion; save result if less than current minimum
			ADCSRA |= _BV(ADSC);
			while(!ADC_result_flag);
			ADC_result_flag = 0;
			if(ADC_result < current_value) current_value = ADC_result;
		}
		
		// Save values
		LCDWriteIntXY(6,1,current_value,4);
		if(current_value < low_value) low_value = current_value;
		if(current_value > high_value) high_value = current_value;
	}

	// Print results
	LCDClear();
	LCDWriteStringXY(0,0,"Results:");
	LCDWriteIntXY(0,1,low_value,4);
	LCDWriteStringXY(5,1,"to");
	LCDWriteIntXY(8,1,high_value,4);
	mTimer(5000);

	return(0);
	
	#endif

	// Determine how long it takes for an item to go through the sensor
	#ifdef TIMER_CALIBRATION_MODE

	sei();
	
	// Print info
	LCDWriteStringXY(0,0,"TimerCalibration");
	LCDWriteStringXY(0,1,"Items Seen:");
	LCDWriteIntXY(14,1,0,2);

	// Initialize ADC
	ADCSRA |= _BV(ADSC);
	while(!ADC_result_flag);
	ADC_result_flag = 0;
	
	// Give it a second
	mTimer(1000);

	// Store timer values
	unsigned timer_values[10]; 

	for(int i = 0; i < 10; i++)
	{
		// Start the timer
		inbound = 0;
		while(!inbound);
		TCNT3 = 0x0000;
		TIFR3 |= _BV(OCF3A);
		
		// Indicate item seen
		LCDWriteIntXY(14,1,(i+1),2);

		// Time how long it takes for item to go through
		ADCSRA |= _BV(ADSC);
		while(!ADC_result_flag);
		
		// no_item_time gets incremented for each time the
		// ADC result is greater than the no-item threshold,
		// and it gets reset each time the ADC result is beneath
		// this threshold. 
		unsigned no_item_time = 0;
		while(no_item_time < NO_ITEM_TIME)
		{
			ADC_result_flag = 0;
			ADCSRA |= _BV(ADSC);
			while(!ADC_result_flag);
			if(ADC_result >= NO_ITEM_THRESHOLD - AMBIENT_DEVIANCE)
			{
				no_item_time++;
			}
			else
			{
				no_item_time = 0;
			}
		}
		timer_values[i] = TCNT3;
	}

	// Print results
	unsigned low_value = timer_values[0];
	unsigned high_value = low_value;
	for(int i = 1; i < 10; i++)
	{
		if(timer_values[i] < low_value) low_value = timer_values[i];
		if(timer_values[i] > high_value) high_value = timer_values[i];
	}
	LCDClear();
	LCDWriteIntXY(0,0,low_value,5);
	LCDWriteStringXY(6,0,"to");
	LCDWriteIntXY(10,0,high_value,5);
	mTimer(5000);

	return(0);

	#endif
	
	// Set INT2 to falling edge mode (homing sensor)
	// Set INT3 to falling edge mode (ramp down)
	// Set INT4 to falling edge mode (item at end of belt)
	EICRA |= _BV(ISC21) | _BV(ISC31);
	EICRB |= _BV(ISC41);
	EIMSK |= _BV(INT2) | _BV(INT3) | _BV(INT4);
	
	// Exit uninterruptable command sequence
	sei();
	
	// Initialize ADC
	ADCSRA |= _BV(ADSC);
	while(!ADC_result_flag);
	ADC_result_flag = 0;
	
	// Home the stepper
	home();

	// Sensor value
	unsigned sensor_value;
	
	while(1)
	{	
		// If ramp-down mode is active, wait for currently enqueued items to
		// be processed then exit
		if(ramp_down)
		{
			LCDClear();
			LCDWriteStringXY(0,0,"Ramping down...");
			while(!isEmpty(&head));
			LCDWriteStringXY(0,1,"complete.");
			mTimer(1000);
			DDRL |= 0xF0;
			return(0);
		}
		
		if(!running)
		{
			LCDClear();
			LCDWriteStringXY(0,0, "S:");
			LCDWriteIntXY(2,0,steel,2);
			LCDWriteStringXY(4,0, ", A:");
			LCDWriteIntXY(8,0,alum,2);
			LCDWriteStringXY(10,0, ", P:");
			LCDWriteIntXY(14,0,plastic,2);
			LCDWriteStringXY(0,1, "Items Sorted: ");
			LCDWriteIntXY(13,1,items_sorted,2);
			while(!running);
		}
		
		
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
	while (!homed_flag){
		if(!running)
		{
			LCDClear();
			LCDWriteStringXY(0,0, "S:");
			LCDWriteIntXY(2,0,steel,2);
			LCDWriteStringXY(4,0, ", A:");
			LCDWriteIntXY(8,0,alum,2);
			LCDWriteStringXY(10,0, ", P:");
			LCDWriteIntXY(14,0,plastic,2);
			LCDWriteStringXY(0,1, "Items Sorted: ");
			LCDWriteIntXY(13,1,items_sorted,2);
			while(!running);
		}
		
		PORTA = stepper[position];
		mTimer(20);
		position++;
		if(position == 4){
			position = 0;
		}
	}
	
	LCDWriteStringXY(0,0,"Disk homed Black");	// Can be removed after testing
//	LCDWriteStringXY(0,1,"Position: ");
//	LCDWriteIntXY(11,1,position,2);
	mTimer(1000);
}


//This function moves the stepper clockwise(0) or counter clockwise(1) 90 degrees or 180 degrees
void move(int c){
	int i = 0;
	int total = c;
	if (disk_direction == 0){
		while(c > 0){
			if(!running)
			{
				LCDClear();
				LCDWriteStringXY(0,0, "S:");
				LCDWriteIntXY(2,0,steel,2);
				LCDWriteStringXY(4,0, ", A:");
				LCDWriteIntXY(8,0,alum,2);
				LCDWriteStringXY(10,0, ", P:");
				LCDWriteIntXY(14,0,plastic,2);
				LCDWriteStringXY(0,1, "Items Sorted: ");
				LCDWriteIntXY(13,1,items_sorted,2);
				while(!running);
			}
			
			PORTA = stepper[position];
			if(total == 90){
				mTimer(delay_a[i]);
			}
			else{mTimer(delay_b[i]);}
			i++;		
			position++;
			c--;			
			if(position == 4){
				position = 0;
			}
			if(c == 0){
		/*		LCDClear();
				LCDWriteStringXY(0,1,"Position: ");
				LCDWriteIntXY(11,1,position,2);				
				mTimer(2000);*/
				break;
			}			
		}
	}
	if (disk_direction == 1){
		while (c > 0){
			if(!running)
			{
				LCDClear();
				LCDWriteStringXY(0,0, "S:");
				LCDWriteIntXY(2,0,steel,2);
				LCDWriteStringXY(4,0, ", A:");
				LCDWriteIntXY(8,0,alum,2);
				LCDWriteStringXY(10,0, ", P:");
				LCDWriteIntXY(14,0,plastic,2);
				LCDWriteStringXY(0,1, "Items Sorted: ");
				LCDWriteIntXY(13,1,items_sorted,2);
				while(!running);
			}
			
			PORTA = stepper[position];
			if(total == 90){
				mTimer(delay_a[i]);
			}
			else{mTimer(delay_b[i]);}
			i++;
			position--;
			c--;
			if(position < 0){
				position = 3;
			}
			if(c == 0){
			/*	LCDClear();
				LCDWriteStringXY(0,1,"Position: ");
				LCDWriteIntXY(11,1,position,2);
				mTimer(2000);*/
				break;
			}
		}
	}
}

// This function moves the sorting bucket to a location based on part in list
void sort(char item)
{
	switch(disk_location)
	{
		case 'b':
		switch(item)
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
		switch(item)
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
		switch(item)
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
		switch(item)
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

void setup(link **h,link **t)
{
	*h = NULL;		/* Point the head to NOTHING (NULL) */
	*t = NULL;		/* Point the tail to NOTHING (NULL) */
	return;
}

void initLink(link **newLink)
{
	//link *l;
	*newLink = malloc(sizeof(link));
	(*newLink)->next = NULL;
	return;
}

void destroyLink (link **oldLink)
{
	if(*oldLink != NULL)
	{
		(*oldLink)->next = NULL;
		free(*oldLink);
	}
	
	return;
}

void enqueue(link **h, link **t, link **nL)
{

	if (*t != NULL){
		/* Not an empty queue */
		(*t)->next = *nL;
		*t = *nL; //(*t)->next;
	}/*if*/
	else{
		/* It's an empty Queue */
		//(*h)->next = *nL;
		//should be this
		*h = *nL;
		*t = *nL;
	}/* else */
	
	// Ensure queue is null terminated
	(*nL)->next = NULL;
	
	return;
}

void dequeue(link **h, link **deQueuedLink)
{
	*deQueuedLink = *h;
	/* Ensure it is not an empty queue */
	if (*h != NULL)
	{
		*h = (*h)->next;
	}/*if*/
	
	return;
}

char firstValue(link **h)
{
	return((*h)->itemType);
}

void clearQueue(link **h, link **t)
{
	link *temp;

	while (*h != NULL){
		temp = *h;
		*h=(*h)->next;
		free(temp);
	}/*while*/
	
	/* Last but not least set the tail to NULL */
	*t = NULL;		

	return;
}

char isEmpty(link **h)
{
	return(*h == NULL);
}

int size(link **h, link **t){

	link 	*temp;			/* will store the link while traversing the queue */
	int 	numItems;

	numItems = 0;

	temp = *h;			/* point to the first item in the list */

	while(temp != NULL){
		numItems++;
		temp = temp->next;
	}/*while*/	
	return(numItems);
}

// Killswitch ISR
ISR(INT0_vect)
{
	// Stop motor and wait for reset
	PORTL |= 0xF0;
	LCDClear();
	LCDWriteStringXY(0,0,"Kill Switch Hit");
	while(1);
}

// Pause/resume conveyor belt ISR
ISR(INT1_vect)
{
	// Brake high + debounce
	PORTL |= 0xF0;
	mTimer(25);
	
	if( running )
	{
		// Pause
		PORTL &= 0xFF;
		running = 0;
	}
	else
	{
		// Resume
		PORTL &= 0x7F;
		running = 1;
	}
}

// Stepper homing interrupt
ISR(INT2_vect)
{
	disk_location = 'b';
	homed_flag = 1;
	EIMSK &= ~_BV(INT2);
}

// Ramp down interrupt
ISR(INT3_vect)
{
	ramp_down = 1;
}

// End of conveyor belt interrupt
ISR(INT4_vect)
{
	// Stop the belt
	PORTL |= 0xFF;

	// Move the stepper dish
	//sort(firstValue(&head));

	// Remove the item from the queue
	//dequeue(&head, &oldItem);

	// Deallocate item
	//destroyLink(&oldItem);

	// Resume the belt
	PORTL &= 0x7F;
}

// First sensor trigger
ISR(INT5_vect)
{
	inbound = 1;
}
	

// ISR for ADC Conversion Completion
ISR(ADC_vect)
{
	// Get ADC result and indicate successful conversion
	ADC_result = 0;
	ADC_result |= ADCL;
	ADC_result |= (ADCH & 0x03) << 8;
	ADC_result_flag = 1;
}

ISR(BADISR_vect)
{
	LCDClear();
	LCDWriteStringXY(1,0, "Something went");
	LCDWriteStringXY(6,1, "wrong!");
	while(1);
}

