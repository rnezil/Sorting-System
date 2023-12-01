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

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdlib.h>
#include "lcd.h"
#include "stepper.h"
#include "LinkedQueue.h"

//#define PRECALIBRATION_MODE
//#define TIMER_CALIBRATION_MODE
//#define EXIT_CALIBRATION_MODE
//#define CALIBRATION_MODE

#ifndef SYSTEM_PARAMETERS
#define SYSTEM_PARMETERS
#define NO_ITEM_THRESHOLD	992	// Lowest sensor value when no item is present
#define ALUMINIUM_MAX		255	// Highest expected value for aluminium
#define STEEL_MAX		750	// Highest expected value for steel
#define WHITE_MAX		900	// Highest expected value for white plastic
#define BELT_SPEED		38	// Duty cycle %
#define ADC_STOPWATCH		5805	// Divide by 125 to get ms
#define NO_TURN_DELAY		20	// ms
#define QUARTER_TURN_DELAY	10	// ms
#define HALF_TURN_DELAY		100	// ms
#define REVERSAL_DELAY		220	// ms
#define EXIT_INT_DELAY		4000	// Divide by 125 to get ms
#endif

#ifdef TIMER_CALIBRATION_MODE
#define NO_ITEM_TIME		5000	// Divide by 125 to get ms
#define AMBIENT_DEVIANCE	8	// Sensor value +/- due to ambient lighting
#endif

#ifdef EXIT_CALIBRATION_MODE
volatile int is_double_count = 0;
#endif

// ADC conversion result
volatile unsigned int ADC_result;

// State
volatile int ADC_result_flag = 0;
volatile int inbound = 0;
volatile int running = 1;
volatile int ramp_down = 0;
volatile int turning = 0;
volatile int converting = 0;

// Item tracking
volatile unsigned int plastic = 0;
volatile unsigned int steel = 0;
volatile unsigned int alum = 0;
volatile int num_items = 0;
volatile link* head;
volatile link* tail;
volatile link* oldItem;

// Millisecond timer
void mTimer(int count);

int main(int argc, char* argv[])
{	
	// Initialize clock, LCD and queue
	CLKPR = 0x80;
	CLKPR = 0x01;
	InitLCD(LS_BLINK|LS_ULINE);
	LCDClear();
	link* newItem;
	setup(&head, &tail);
		
	// IO
	DDRB = 0x80;
	DDRL = 0xF0;
	PORTL |= 0x70;
	DDRA = 0xFF;
	DDRC = 0xFF;
	DDRK = 0xFF;
	DDRF = 0xC0;
	
	// Set exit timer to CTC mode, 125kHz
	TCCR4B |= _BV(WGM42);
	TCCR4B |= _BV(CS41) | _BV(CS40);
	
	// Minimum time between exit interrupts
	#ifndef EXIT_CALIBRATION_MODE
	OCR4A = EXIT_INT_DELAY;
	#else
	OCR4A = 0xFFFF;
	#endif
	
	// Set ADC conversion timer to CTC mode, 125kHz
	TCCR3B |= _BV(WGM32);
	TCCR3B |= _BV(CS31) | _BV(CS30);
	TCCR1B |= _BV(CS11);
	#ifdef TIMER_CALIBRATION_MODE
	OCR3A = 0xFFFF;
	#else
	OCR3A = STOPWATCH;
	#endif
	
	// Enable ADC with automatic interrupts after conversion success
	ADCSRA |= _BV(ADEN);
	ADCSRA |= _BV(ADIE);
	ADMUX |=_BV(REFS0);
	
	// 3.9kHz PWM
	TCCR0A |= _BV(WGM01) | _BV(WGM00);
	OCR0A = BELT_SPEED * 255 / 100;
	TCCR0A |= _BV(COM0A1);
	TCCR0B |= _BV(CS01);
	
	// Enter uninterruptable command sequence
	cli();
	
	// Set INT0 to any edge mode (kill switch)
	// Set INT1 to rising edge mode (pause resume)
	EICRA |= _BV(ISC00) | _BV(ISC10);
	EIMSK |= _BV(INT0) | _BV(INT1);
	
	// Do continuous ADC conversions and keep smallest value on screen
	#ifdef PRECALIBRATION_MODE
	sei();
	ADCSRA |= _BV(ADSC);
	while(!ADC_result_flag);
	ADC_result_flag = 0;
	mTimer(2000);
	unsigned no_item_value = 0xFFFF;
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
	
	// Run item through sensors 12 times and print minimum and maximum values
	#ifdef CALIBRATION_MODE
	sei();
	ADCSRA |= _BV(ADSC);
	while(!ADC_result_flag);
	ADC_result_flag = 0;
	unsigned current_value;
	unsigned low_value = 0xFFFF;
	unsigned high_value = 0x0000;
	LCDWriteStringXY(0,0,"Run Item 12x:");
	for(int j = 0; j < 12; j++)
	{
		current_value = 1337;
		while(!inbound);
		inbound = 0;
		LCDWriteIntXY(14,0,(j+1),2);
		TCNT3 = 0x0000;
		TIFR3 |= _BV(OCF3A);
		while(!(TIFR3 & _BV(OCF3A)))
		{
			ADCSRA |= _BV(ADSC);
			while(!ADC_result_flag);
			ADC_result_flag = 0;
			if(ADC_result < current_value) current_value = ADC_result;
		}
		LCDWriteIntXY(6,1,current_value,4);
		if(current_value < low_value) low_value = current_value;
		if(current_value > high_value) high_value = current_value;
	}
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
	LCDWriteStringXY(0,0,"TimerCalibration");
	LCDWriteStringXY(0,1,"Items Seen:");
	LCDWriteIntXY(14,1,0,2);
	ADCSRA |= _BV(ADSC);
	while(!ADC_result_flag);
	ADC_result_flag = 0;
	mTimer(1000);
	unsigned timer_values[10]; 
	for(int i = 0; i < 10; i++)
	{
		inbound = 0;
		while(!inbound);
		TCNT3 = 0x0000;
		TIFR3 |= _BV(OCF3A);
		LCDWriteIntXY(14,1,(i+1),2);
		ADCSRA |= _BV(ADSC);
		while(!ADC_result_flag);
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
	unsigned low_value = timer_values[0];
	unsigned high_value = low_value;
	for(int i = 1; i < 10; i++)
	{
		if(timer_values[i] < low_value) low_value = timer_values[i];
		if(timer_values[i] > high_value) high_value = timer_values[i];
	}
	LCDClear();
	/*
	LCDWriteIntXY(0,0,low_value,5);
	LCDWriteStringXY(6,0,"to");
	LCDWriteIntXY(10,0,high_value,5);
	*/
	LCDWriteStringXY(0,0,"Recommended");
	LCDWriteStringXY(0,1,"value:");
	LCDWriteIntXY(7,1,((low_value - NO_ITEM_TIME)/2),4);
	mTimer(5000);
	return(0);
	#endif
	
	// Set INT2 to falling edge mode (homing sensor)
	// Set INT3 to falling edge mode (ramp down)
	// Set INT4 to falling edge mode (item at end of belt)
	EICRA |= _BV(ISC21) | _BV(ISC31);
	EICRB |= _BV(ISC41);
	EIMSK |= _BV(INT2) | _BV(INT3) | _BV(INT4);
	sei();
	
	// Prepare ADC, stepper and LCD
	ADCSRA |= _BV(ADSC);
	while(!ADC_result_flag);
	ADC_result_flag = 0;
	home();
	unsigned sensor_value;
	LCDClear();
	LCDWriteStringXY(0,0,"Sorting...");

	// Main loop
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
		
		// If paused, print sorting info
		if(!running)
		{
			LCDClear();
			LCDWriteStringXY(0,1, "S:");
			LCDWriteIntXY(2,1,steel,2);
			LCDWriteStringXY(4,1, ", A:");
			LCDWriteIntXY(8,1,alum,2);
			LCDWriteStringXY(10,1, ", P:");
			LCDWriteIntXY(14,1,plastic,2);
			LCDWriteStringXY(0,0, "Items Sorted:");
			LCDWriteIntXY(14,0,items_sorted,2);
			while(!running);
		}
		
		// Process item in front of reflective sensor
		if(inbound)
		{
			// Update state
			inbound = 0;
			converting = 1;

			// Reset values and start timer
			sensor_value = 1337;
			TCNT3 = 0x0000;
			TIFR3 |= _BV(OCF3A);
			
			// Determine item value
			while(!(TIFR3 & _BV(OCF3A)))
			{
				// Do a conversion; save result if less than current minimum
				ADCSRA |= _BV(ADSC);
				while(!ADC_result_flag);
				ADC_result_flag = 0;
				if(ADC_result < sensor_value) sensor_value = ADC_result;
			}

			// Update state
			converting = 0;
			
			// Add item to queue
			initLink(&newItem);
			if(sensor_value < ALUMINIUM_MAX)
			{
				newItem->itemType = 'a';
				//LCDWriteStringXY(0,1,"Alum");
			}
			else if(sensor_value < STEEL_MAX)
			{
				newItem->itemType = 's';
				//LCDWriteStringXY(0,1,"Steel");
			}
			else if(sensor_value < WHITE_MAX)
			{
				newItem->itemType = 'w';
				//LCDWriteStringXY(0,1,"White");
			}
			else
			{
				newItem->itemType = 'b';
				//LCDWriteStringXY(0,1,"Black");
			}
			enqueue(&head,&tail,&newItem);
			num_items++;
			LCDWriteIntXY(14,0,num_items,2);
		}
	}
	
	return(0);
}

// Delay for 'count' milliseconds
void mTimer(int count)
{
	// Set timer to CTC mode at 1MHz with TOP = 1000
	int i = 0;
	TCCR1B |= _BV(WGM12);
	OCR1A = 0x03E8;
	TCNT1 = 0x0000;
	TIFR1 |= _BV(OCF1A);

	// Count to 1000 at 1MHz 'count' times
	while(i < count)
	{
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
			LCDWriteStringXY(0,1, "S:");
			LCDWriteIntXY(2,1,steel,2);
			LCDWriteStringXY(4,1, ", A:");
			LCDWriteIntXY(8,1,alum,2);
			LCDWriteStringXY(10,1, ", P:");
			LCDWriteIntXY(14,1,plastic,2);
			LCDWriteStringXY(0,1, "Items Sorted:");
			LCDWriteIntXY(14,0,items_sorted,2);
			while(!running);
		}
		
		PORTA = stepper[position];
		mTimer(11);
		position++;
		if(position == 4){
			position = 0;
		}
	}
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
				LCDWriteStringXY(0,1, "S:");
				LCDWriteIntXY(2,1,steel,2);
				LCDWriteStringXY(4,1, ", A:");
				LCDWriteIntXY(8,1,alum,2);
				LCDWriteStringXY(10,1, ", P:");
				LCDWriteIntXY(14,1,plastic,2);
				LCDWriteStringXY(0,1, "Items Sorted:");
				LCDWriteIntXY(14,0,items_sorted,2);
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
				LCDWriteStringXY(0,1, "S:");
				LCDWriteIntXY(2,1,steel,2);
				LCDWriteStringXY(4,1, ", A:");
				LCDWriteIntXY(8,1,alum,2);
				LCDWriteStringXY(10,1, ", P:");
				LCDWriteIntXY(14,1,plastic,2);
				LCDWriteStringXY(0,1, "Items Sorted:");
				LCDWriteIntXY(14,0,items_sorted,2);
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
int sort(char item)
{
	int recent_disk_direction = disk_direction;
	switch(disk_location)
	{
		case 'b':
		switch(item)
		{
			case 'b':
			return 0;
			
			case 'a':
			disk_direction = 0;
			move(QUARTER_TURN);
			disk_location = 'a';
			return (disk_direction ^ recent_disk_direction) ? 3 : 1;
			
			case 's':
			disk_direction = 1;
			move(QUARTER_TURN);
			disk_location = 's';
			return (disk_direction ^ recent_disk_direction) ? 3 : 1;
			
			case 'w':
			move(HALF_TURN);
			disk_location = 'w';
			return 2;
		}
		
		case 'a':
		switch(item)
		{
			case 'a':
			return 0;
			
			case 'b':
			disk_direction = 1;
			move(QUARTER_TURN);
			disk_location = 'b';
			return (disk_direction ^ recent_disk_direction) ? 3 : 1;
			
			case 's':
			move(HALF_TURN);
			disk_location = 's';
			return 2;
			
			case 'w':
			disk_direction = 0;
			move(QUARTER_TURN);
			disk_location = 'w';
			return (disk_direction ^ recent_disk_direction) ? 3 : 1;
		}
		
		case 'w':
		switch(item)
		{
			case 'w':
			return 0;
			
			case 'b':
			move(HALF_TURN);
			disk_location = 'b';
			return 2;
			
			case 'a':
			disk_direction = 1;
			move(QUARTER_TURN);
			disk_location = 'a';
			return (disk_direction ^ recent_disk_direction) ? 3 : 1;
			
			case 's':
			disk_direction = 0;
			move(QUARTER_TURN);
			disk_location = 's';
			return (disk_direction ^ recent_disk_direction) ? 3 : 1;
		}
		
		case 's':
		switch(item)
		{
			case 's':
			return 0;
			
			case 'b':
			disk_direction = 0;
			move(QUARTER_TURN);
			disk_location = 'b';
			return (disk_direction ^ recent_disk_direction) ? 3 : 1;
			
			case 'a':
			move(HALF_TURN);
			disk_location = 'a';
			return 2;
			
			case 'w':
			disk_direction = 1;
			move(QUARTER_TURN);
			disk_location = 'w';
			return (disk_direction ^ recent_disk_direction) ? 3 : 1;
		}
	}
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

void dequeue(link **h, link **t, link **deQueuedLink)
{
	*deQueuedLink = *h;
	
	// Special case for size 1 queue
	if( *h == *t )
	{
		*t = NULL;
	}
	
	/* Ensure it is not an empty queue */
	if (*h != NULL)
	{
		*h = (*h)->next;
	}/*if*/
	
	(*deQueuedLink)->next = NULL;
	
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
	// Debounce
	while(PIND & 0x02) mTimer(20);
	
	if( running )
	{
		// Pause
		PORTL |= 0xF0;
		running = 0;
	}
	else
	{
		// Resume
		PORTL &= 0x7F;
		running = 1;
		LCDClear();
		LCDWriteStringXY(0,0,"Sorting...");
		LCDWriteIntXY(14,0,num_items,2);
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
	if(!ramp_down)
	{
		ramp_down = 1;

		// Set 2.1s ramp down timer
		TCCR5B |= _BV(WGM52);
		TCCR5B |= _BV(CS52);
		OCR5A = 0xFFFF;

		// Start timer and enable its interrupt
		TNCT5 = 0x0000;
		TIFR5 |= _BV(OCF5A);
		TIMSK5 |= _BV(OCIE5A);
	}
}

// End of conveyor belt interrupt
ISR(INT4_vect)
{	
	// Save conversion timer value
	if(converting) unsigned tmp = TCNT3;

	#ifndef EXIT_CALIBRATION_MODE
	
	// Mask this interrupt
	EIMSK &= ~_BV(INT4);
	
	#else
	
	// Capture timer value
	unsigned double_count_time = TCNT4;
	
	// If double-count, print time between counts
	if(is_double_count)
	{
		LCDClear();
		LCDWriteStringXY(1,0,"DOUBLE TROUBLE");
		LCDWriteIntXY(5,1,double_count_time,5);
		mTimer(2000);
	}
	else
	{
		LCDClear();
		LCDWriteStringXY(0,0,"Sorting...");
		LCDWriteIntXY(14,0,num_items,2);
		is_double_count = 1;
	}
	
	#endif
	
	// Stop the belt
	PORTL |= 0xF0;
	
	// Print info
	switch(firstValue(&head))
	{
		case 'a':
		LCDWriteStringXY(0,1,"Aluminium       ");
		alum++;
		break;
		
		case 's':
		LCDWriteStringXY(0,1,"Steel           ");
		steel++;
		break;
		
		case 'b':
		LCDWriteStringXY(0,1,"Black Plastic   ");
		plastic++;
		break;
		
		case 'w':
		LCDWriteStringXY(0,1,"White Plastic   ");
		plastic++;
		break;
	}

	// Move the stepper dish
	turning = sort(firstValue(&head));
	num_items--;

	// Remove the item from the queue
	dequeue(&head, &tail, &oldItem);
	destroyLink(&oldItem);
	
	// Delay appropriately
	switch(turning)
	{
		case 0:
			mTimer(NO_TURN_DELAY);
			break;
		case 1:
			mTimer(QUARTER_TURN_DELAY);
			break;
		case 2:
			mTimer(HALF_TURN_DELAY);
			break;
		case 3:
			mTimer(REVERSAL_DELAY);
			break;
	}

	// Print info
	LCDWriteIntXY(14,0,num_items,2);
	
	// Start the exit timer and enable its interrupt
	TCNT4 = 0x0000;
	TIFR4 |= _BV(OCF4A);
	TIMSK4 |= _BV(OCIE4A);
			
	// Resume the belt
	PORTL &= 0x7F;
	
	// Resume conversion timer
	if(converting)
	{
		TCNT3 = tmp;
		TIFR3 |= _BV(OCF3A);
	}

}

// First sensor trigger
ISR(INT5_vect)
{
	inbound = 1;
}

// Exit timer interrupt
ISR(TIMER4_COMPA_vect)
{
	// Mask this interrupt
	TIMSK4 &= ~_BV(OCIE4A);
	
	#ifndef EXIT_CALIBRATION_MODE
	
	// Enable exit sensor interrupt
	EIMSK |= _BV(INT4);
	
	#else
	
	// Not a double-count if this timer maxes out
	is_double_count = 0;
	
	#endif
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

