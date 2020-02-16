#include <avr/io.h>
#include "ADC.h"

void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
	
	//ADMUX = ADC0D; //samples A0
	//ADMUX = ADC1D;	//samples A1, how to split them and sample at same time?
	// ADEN: setting this bit enables analog-to-digital conversion.
	// ADSC: setting this bit starts the first conversion.
	// ADATE: setting this bit enables auto-triggering. Since we are
	//        in Free Running Mode, a new conversion will trigger whenever
	//        the previous conversion completes.
}