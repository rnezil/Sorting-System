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

#ifndef SENSOR_THRESHOLD_VALUES
#define SENSOR_THRESHOLD_VALUES

#define FERROMAGNETIC_NO_ITEM_THRESHOLD	123
#define REFLECTIVE_NO_ITEM_THRESHOLD	123
#define METALLIC_THRESHOLD		123	// Above->plastic, below->metal
#define METAL_REFLECTIVITY_THRESHOLD	300	// Above->steel, below->aluminium
#define PLASTIC_REFLECTIVITY_THRESHOLD	1000	// Above->black, below->white

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

// Variable for storing ADC conversion result
volatile unsigned int ADC_result;

// Indicates whether conveyor belt is running
volatile unsigned char running;

// Indicates whether system is in ramp-down mode
char ramp_down = 0x00;

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
	
	// Enter uninterruptable command sequence
	cli();
	
	// Set initial system state
	running = 0x00;
		
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
	
	// Set INT4 to falling edge mode (item at end of belt)
	// Set INT3 to falling edge mode (ramp down)
	// Set INT2 to falling edge mode (homing sensor)
	// Set INT1 to rising edge mode (pause resume)
	// Set INT0 to any edge mode (kill switch)
	EICRA |= _BV(ISC00) | _BV(ISC10) | _BV(ISC11) | _BV(ISC21) | _BV(ISC31);
	EICRB |= _BV(ISC41);
	EIMSK |= _BV(INT0) | _BV(INT1) | _BV(INT2) | _BV(INT3) | _BV(INT4);
	
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
	
	// Home the stepper
	home();

	// Sensor values for current item
	unsigned ferromagnetic_value;
	unsigned reflective_value;
	
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
			return(0);
		}

		// Wait until sensors detect an item
		if( ((ADMUX & _BV(MUX0)) && (ADC_result < FERROMAGNETIC_NO_ITEM_THRESHOLD))
				|| ((ADMUX & _BV(MUX0) ^ _BV(MUX0)) && (ADC_result < REFLECTIVE_NO_ITEM_THRESHOLD)) )
		{
			// Initialize new item to be queued
			initLink(&newItem);

			// Reset sensor values
			ferromagnetic_value = FERROMAGNETIC_NO_ITEM_THRESHOLD;
			reflective_value = REFLECTIVE_NO_ITEM_THRESHOLD;

			// Delay to work around sensor value deviations causing the while loop
			// below to end prematurely
			mTimer(10);

			while( ((ADMUX & _BV(MUX0)) && (ADC_result < FERROMAGNETIC_NO_ITEM_THRESHOLD))
				|| ((ADMUX & _BV(MUX0) ^ _BV(MUX0)) && (ADC_result < REFLECTIVE_NO_ITEM_THRESHOLD)) )
			{
				if(ADMUX & _BV(MUX0))
				{
					// Ferromagnetic value
					if(ADC_result < ferromagnetic_value) ferromagnetic_value = ADC_result;
				}
				else
				{
					// Reflective value
					if(ADC_result < reflective_value) reflective_value = ADC_result;
				}
			}

			// Determine what item type is and add to queue
			if(ferromagnetic_value < METALLIC_THRESHOLD)
			{
				// Item is metal
				if(reflective_value < METAL_REFLECTIVITY_THRESHOLD)
				{
					// Item is aluminium
					newItem->itemType = 'a';
				}
				else
				{
					// Item is steel
					newItem->itemType = 's';
				}
			}
			else
			{
				// Item is plastic
				if(reflective_value < PLASTIC_REFLECTIVITY_THRESHOLD)
				{
					// Item is white plastic
					newItem->itemType = 'w';
				}
				else
				{
					// Item is black plastic
					newItem->itemType = 'b';
				}
			}

			// Add new item to queue
			enqueue(&head, &tail, &newItem);

			// Print item to LCD
			LCDClear();
			switch(newItem->itemType)
			{
				case 's':
					LCDWriteStringXY(0,0,"Steel");
					break;
				case 'a':
					LCDWriteStringXY(0,0,"Aluminium");
					break;
				case 'b':
					LCDWriteStringXY(0,0,"Black Plastic");
					break;
				case 'w':
					LCDWriteStringXY(0,0,"White Plastic");
					break;
				default:
					LCDWriteStringXY(0,0,"Scanning Error");
					break;
			}
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
	while (homed_flag != 1){
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

void pause(){
	if(pause_flag == 2){
		LCDClear();
		LCDWriteStringXY(0,0, "S:");
		LCDWriteIntXY(2,0,steel,2);
		LCDWriteStringXY(4,0, ", A:");
		LCDWriteIntXY(8,0,alum,2);
		LCDWriteStringXY(10,0, ", P:");
		LCDWriteIntXY(14,0,plastic,2);
		LCDWriteStringXY(0,1, "Items Sorted: ");
		LCDWriteIntXY(13,1,items_sorted,2);
		while((PIND & (1<<PIND1)) != (1<<PIND1)){
			if(pause_flag == 0){
				break;
			}
		}
	}
	else if(pause_flag == 4){
//		LCDWriteStringXY(0,0,"Program resumed");
//		LCDClear();
//		LCDWriteIntXY(0,1,pause_flag,1);
		pause_flag = 0;
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
		(*deadLink)->next = NULL;
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
	//mTimer(25);
	while((PIND & (1<<PIND1)) == (1<<PIND1)){
		mTimer(20);
	}
	pause_flag++;
	pause();
//	LCDWriteIntXY(0,0,pause_flag,2);

/*	// Efficient pause/resume (for performance)
	// Brake high + debounce
	PORTL |= 0xF0;
	mTimer(25);
	
	if( running )
	{
		// Pause
		PORTL &= 0xFF;
		running = 0x00;
	}
	else
	{
		// Resume
		PORTL &= 0x7F;
		running = 0x01;
	}*/
}

// Stepper homing interrupt
ISR(INT2_vect)
{
	disk_location = 'b';
	homed_flag = 1;
	EIMSK |= _BV(INT0) | _BV(INT1) | _BV(INT3) ;	// Disables the INT2 interrupt
}

// Ramp down interrupt
ISR(INT3_vect)
{
	ramp_down = 0x01;
}

// End of conveyor belt interrupt
ISR(INT4_vect)
{
	// Stop the belt
	PORTL &= 0xFF;

	// Move the stepper dish
	sort(firstValue(&head));

	// Remove the item from the queue
	dequeue(&head, &oldLink);

	// Deallocate item
	destroyLink(&oldLink);

	// Resume the belt
	PORTL &= 0x7F;
}
	

// ISR for ADC Conversion Completion
ISR(ADC_vect)
{
	// Get ADC result
	ADC_result = 0x00;
	ADC_result |= ADCL;
	ADC_result |= (ADCH & 0x03) << 8;

	// Change ADC input channel
	if( ADMUX & _BV(MUX0) )
	{
		// Select ferromagnetic input for next conversion
		ADMUX &= !_BV(MUX0);
	}
	else
	{
		// Select reflective input for next conversion
		ADMUX |= _BV(MUX0);
	}
}

ISR(BADISR_vect)
{
	LCDClear();
	LCDWriteStringXY(1,0, "Something went");
	LCDWriteStringXY(6,1, "wrong!");
	while(1);
}

