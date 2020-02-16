Spec_Doc: Has all the details about the Project including software and hardware features as well as Pinouts on the atmega 1284

PWM.c: contains the code for initializing and running the PWM on atmega 1284

Timer.c: contains code for initializing a timer and setting it 

Custom_LCD.c: contains code for an LCD connected to PORTC with data on PORTD and includes custom character functionality. I got custom character help from https://www.electronicwings.com/avr-atmega/lcd-custom-character-display-using-atmega-16-32-

ADC.c: contains code for initializing and using the ADC on the atmega 1284

DDR_main.c: The main code for the game. Includes game logic, menu logic, score calculations, ADC functionality with 2 joysticks, and eeprom logic for saving high scores.
Eeprom help from https://www.avrfreaks.net/forum/cant-write-eeprom-atmega1284
