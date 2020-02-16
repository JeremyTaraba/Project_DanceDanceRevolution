/*	Partner 1 Name & E-mail: Jeremy Taraba,  jtara006@ucr.edu
 *	Lab Section: 025
 *	Assignment: Custom Project
 *	Exercise Description: [optional - include for your own benefit]
 *  Dance dance revolution/ guitar hero style game played on LCD with 2 joysticks, LCD is connected to PORTC and PORTD
 *  
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */

//future additions: keep track of total wins for P1 and P2 and have a menu that shows those scores, also add a pause feature when playing a song

#include <avr/io.h>
#include "Custom_LCD.c"
#include "ADC.c"
#include "PWM.c"
#include "timer.c"
#include <avr/interrupt.h>
#include <avr/eeprom.h>


#define A3 (PINA & 0x08)				//joystick 1 click
#define A5 (PINA & 0x20)				//joystick 2 click
#define A6 (PINA & 0x40)				//reset button (breaks eeprom for some reason when button is wired up, don't even have to press it)
#define analog (ADC >> 2)				//ADC is 10 bits but we only need 8 bits for a char so we chop off 2 lower bits
#define AD0 (ADMUX & 0xF0)				//joystick 1 y-axis
#define AD1 (ADMUX | 0x01)				//joystick 1 x-axis
#define AD2 ((ADMUX | 0x02) & 0xF2)		//joystick 2 y-axis
#define AD4 ((ADMUX | 0x04) & 0xF4)		//joystick 2 x-axis

char* songName = " ";			 //current playing song's name
const char* arr_songs[] = {"Test Song", "Easy Song", "Hard Song"};	//list of songs
unsigned char song_cnt;			//index of current song


typedef struct task {
	int state; // Current state of the task
	unsigned long period; // Rate at which the task should tick
	unsigned long elapsedTime; // Time since task's previous tick
	int (*TickFct)(int); // Function to call for task's tick
} task;

task tasks[4];
	
//timing could be char since small values to save space but normally is long
const unsigned char tasksNum = 4;
const unsigned long tasksPeriodGCD = 25;
const unsigned long periodMenu = 50;		//has input but don't need to be as responsive as when playing, although lag is noticeable at 100ms 
const unsigned long periodPlay = 25;		//25ms for fast changing input, since only samples x or y axis on one joystick at a time, meaning joystick 1 will take 50ms to sample 
											//both x and y axis and then it will have to wait 50ms for joystick 2 to finish before sampling again = ~100 ms delay on input
const unsigned long periodSevenSeg = 100;	//changes when note score changes, dont have to be super fast
const unsigned long periodReset = 100;		//don't need to be precise but has input so 100ms (reset occurs if held for ~3 seconds)


//attempted to store song data in a struct, program stopped working after notes exceeded a size of 10 for some reason
/*struct songList
{
	char* name;
	unsigned short notes[];			//program froze if notes had more than 10 variables
	unsigned short timing[];
	unsighed char arrows[];
};

struct songList test = {"Test Song", {659}, {494}, 0xFF}};
*/



void TimerISR() {
	unsigned char i;
	for (i = 0; i < tasksNum; ++i) { // Heart of the scheduler code
		if ( tasks[i].elapsedTime >= tasks[i].period ) { // Ready
			tasks[i].state = tasks[i].TickFct(tasks[i].state);
			tasks[i].elapsedTime = 0;
		}
		tasks[i].elapsedTime += tasksPeriodGCD;
	}
}

enum menu{init, press_start, welcome, song_select, song_choice, choose_song, choose_song_release, 
	menu_play_song_confirm, menu_confirm_release, menu_play_song} menu;

int tick_Menu(){			//logic for the menu, 3 possible songs in songlist although playing songs leads to the same rhythm and note pattern
	static unsigned char cnt;				//used for timing when printing "Welcome"
	static unsigned char sound_cnt;			//timing on welcome sound
	static unsigned char letters_cnt;		//timing for erasing "welcome" and for printing "song select"
	static unsigned char no;				//flag for user not confirming a song
	static unsigned char yes;				//flag for user confirming to play a song
	const unsigned char SONGMAX = 2;		//index number of songs [0 - 2] = 3 songs
	
	
	unsigned char arr_welcome[] = {'W', 'E', 'L', 'C', 'O', 'M', 'E'};
	short sound[] = {261, 0, 329, 0, 392, 0, 523, 0};		//start up sound
	unsigned char arr_song_select[] = {'S', 'o', 'n', 'g', ' ', 'S', 'e', 'l', 'e', 'c', 't', ':'};	
	
	
	
	switch(menu){
		case 0:
			LCD_DisplayString(1, "Press any button");
			LCD_DisplayString(20, "to start!");
			letters_cnt = 0;
			cnt = 0;
			sound_cnt = 0;
			song_cnt = 0;
			yes = 0;
			no = 0;
			
			menu = press_start;
			
			break;
		case press_start:		//wait for user to press a button
			if(!A3 || !A5){
				menu = welcome;
				LCD_ClearScreen();
			}
			else{
				menu = press_start;
			}
			break;
		case welcome:			//welcome animation and sound
			if(cnt <= 14){
				if(cnt % 2 == 0){
					LCD_Cursor(letters_cnt+5);
					LCD_WriteData(arr_welcome[letters_cnt]);
					LCD_Cursor(0);
					set_PWM(sound[sound_cnt]);
					sound_cnt++;
					letters_cnt++;
				}
				cnt++;
				if(cnt >= 14){
					letters_cnt = 0;
				}
			}
			else{
				if(cnt >= 21){	//wait before erasing
					if(cnt % 2 == 0){
						LCD_Cursor(letters_cnt+5);
						LCD_WriteData(' ');
						if(letters_cnt >= 6){	//were done erasing
							menu = song_select;
							letters_cnt = 0;
							sound_cnt= 0;
							cnt = 0;
						}
						else{
							letters_cnt++;
						}
					}
				}
				cnt++;
			}		
			break;
		case song_select:
			LCD_Cursor(letters_cnt+1);
			LCD_WriteData(arr_song_select[letters_cnt]);
			if(letters_cnt >= 11){
				menu = song_choice;
				letters_cnt = 0;
			}
			else{
				letters_cnt++;
			}
			
			break;
		case song_choice:				//setting up selecting a song, dont want to print everything again whenever menu ticks
			LCD_DisplayString(19, arr_songs[song_cnt]);
			LCD_Cursor(17);
			LCD_WriteData(0x7F);	//left arrow);
			LCD_Cursor(32);
			LCD_WriteData(0x7E);	//right arrow);
			LCD_Cursor(18);
			LCD_WriteData(' ');
			LCD_Cursor(13);
			LCD_WriteData('1' + song_cnt);	//numbers
			LCD_Cursor(0);
			ADMUX == AD1;
			menu = choose_song;
			break;
		case choose_song:			//changes which song is currently selected, cycles through song names
			if(ADMUX == AD0){
				if(analog == 0xFC){		//down
					if(song_cnt == 0){
						song_cnt = SONGMAX;
					}
					else{
						song_cnt--;
					}
					LCD_Cursor(17);
					LCD_WriteData(0);
					LCD_DisplayString(19, "             ");
					LCD_Cursor(17);
					menu = song_choice;
				}
				else if(analog <= 0x10){	//up
					if(song_cnt == SONGMAX){
						song_cnt = 0;
					}
					else{
						song_cnt++;
					}
					LCD_Cursor(32);
					LCD_WriteData(1);			//invert right arrow);
					LCD_DisplayString(19, "              ");
					menu = song_choice;
				}
				ADMUX = AD1;
			}
			
			else if(ADMUX == AD1){
				if(analog == 0xFC){		//left
					if(song_cnt == 0){
						song_cnt = SONGMAX;
					}
					else{
						song_cnt--;	
					}
					LCD_Cursor(17);
					LCD_WriteData(0);
					LCD_DisplayString(19, "             ");
					LCD_Cursor(17);
					menu = song_choice;
				}
				else if(analog <= 0x10){	//right
					if(song_cnt == SONGMAX){
						song_cnt = 0;
					}
					else{
						song_cnt++;
					}
					LCD_Cursor(32);
					LCD_WriteData(1);						//invert right arrow
					LCD_DisplayString(19, "              ");
					menu = song_choice;
				}
				ADMUX = AD0;
			}
			if(!A3){
				menu = choose_song_release;
			}
			break;
		case choose_song_release:		//setting up confirming song
			if(!A3){
				menu = choose_song_release;
			}
			else{
				menu = menu_play_song_confirm;
				LCD_ClearScreen();
				LCD_DisplayString(1, "Play ");
				LCD_DisplayString(6, arr_songs[song_cnt]);
				LCD_DisplayString(17, "   ");
				LCD_DisplayString(22, "     ");
				LCD_DisplayString(30, "   ");
				LCD_DisplayString(27, "yes");
				LCD_DisplayString(20, "no");
				LCD_Cursor(0);
			}
			break;
		case menu_play_song_confirm:	//since we already set up everything we only print the ">" when a user hovers over "yes" or "no"
			ADMUX = AD1;
			if(analog == 0xFC){		//left
				LCD_DisplayString(26, " yes");
				LCD_DisplayString(19, ">no");
				LCD_Cursor(0);
				no = 1;
				yes = 0;
			}
			else if(analog <= 0x10){	//right
				LCD_DisplayString(19, " no");
				LCD_DisplayString(26, ">yes");
				LCD_Cursor(0);
				yes = 1;
				no = 0;
			}
			if(!A3){
				if(yes){
					yes = 0;
					menu = menu_confirm_release;
				}
				if(no){
					no = 0;
					LCD_ClearScreen();
					menu = song_select;
				}
			}
			else{
				menu = menu_play_song_confirm;
			}
			break;
		case menu_confirm_release:
			if(!A3){
				menu = menu_confirm_release;
			}
			else{
				songName = arr_song_select[letters_cnt];		//setting songName to song being played
				menu = menu_play_song;
			}
			break;
		case menu_play_song:			//currently playing a song, waiting for it to finish
			if(songName == " "){
				menu = song_select;
			}
			else{
				menu = menu_play_song;
			}
			break;
		default:
			menu = 0;
			break;	
	}
	
	switch(menu){		//actions, all are done before switching states or during the state alongside their respective transitions
		default:
			break;
	}

	
	return menu;
}


unsigned char user1_score;
unsigned char user2_score;
unsigned char MaxNotes = 26;
unsigned cd_cnt;

enum play{start, wait_song, play_song_setup, countdown, play_song, score_screen_setup, score_screen, score_screen_release, score_screen_winner, score_screen_winner_release} play;

int tick_Play(){					//logic for actually playing the game, only one rhythm is set up tho
	static unsigned char notes;		//index for LCD cell so print the arrows
	static unsigned char note_flag;	//flag set for when to delete cell 15 and 16 after arrow has reached the end
	unsigned char three_two_one[] = {0xAD, 0x3D, 0x88, 0x00};	//3-2-1-0
	static unsigned char countdown_cnt;		//counter for the countdown
	short testSongNotes[] = {330, 294, 261, 294, 330, 330, 330, 294, 294, 294, 330, 392, 392, 330, 294, 261, 294, 330, 330, 330, 330, 294, 294, 330, 294, 261};		//song notes sent to PWM
	static signed short songTiming[] =  {1, 1, 1, 1, 1, 1, 1, 28, 1, 1, 28, 1, 1, 28, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};		//timing for when notes are sent, 1 means immediately
																																//after the last note has finished
	static unsigned char songTiming_cnt;		//cnt for the songTiming
	unsigned char arrows[] = {0x7E, 0x7F, 3, 2, 0x7F, 3, 3, 2, 2, 0x7F, 0x7E, 3, 2, 0x7E, 0x7F, 2, 3, 2, 0x7E, 3, 3, 2, 0x7F, 0x7E, 2, 3};	//right, left, up, down...
	static unsigned char saveTiming;		//value that holds the current timing while we decrement it
	static unsigned char saveTiming_flag;	//flag to say we need to save the current timing
	static unsigned char curr_arrow;		//hold current arrow to write to LCD
	static unsigned char user1_arrow;		//joystick direction of user1
	static unsigned char user2_arrow;		//joystick direction of user 2
	static unsigned char winner_flag;		//we already printed who won, dont want to keep printing it to LCD
	unsigned char tmpD;						//used for red and green lights (indicates if note was correct or not)
	
	switch(play){
		case 0:				
			user1_score = 0;
			user2_score = 0;
			saveTiming_flag = 0;	
			user1_arrow = 0xFF;
			user2_arrow = 0xFF;
			cd_cnt = 0;
			winner_flag = 1;
			user1_score = 0;
			user2_score = 0;
			songTiming_cnt = 0;
			countdown_cnt = 0;
			note_flag = 0;
			notes = 4;
			play = wait_song;
			break;
		case wait_song:				//waiting for the songName to change meaning the user has confirmed they want to play that song
			if(songName != " "){
				play = play_song_setup;
			}
			else{
				play = wait_song;
			}
			break;
		case play_song_setup:				//set up the game field on LCD
			LCD_ClearScreen();
			LCD_DisplayString(1, "P1:" );
			LCD_DisplayString(17, "P2:" );
			LCD_Cursor(16);
			LCD_WriteData(0xFF);
			LCD_Cursor(32);
			LCD_WriteData(0xFF);
			play = countdown;
			break;
		case countdown:					//7-seg countdown 3-2-1 then start song
			if(countdown_cnt == 0){
				set_PWM(0);
			}
			if(countdown_cnt >= 20){
				PORTB = three_two_one[cd_cnt];
				cd_cnt++;
				countdown_cnt = 0;
				if(cd_cnt <= 2){
					set_PWM(262);
				}
				else if(cd_cnt == 3){
					set_PWM(1047);
				}
				if(cd_cnt == 4){
					set_PWM(0);
					play = play_song;
				}
			}
			else{
				countdown_cnt++;
			}
			break;
		case play_song:						//logic for during a song
			if(songTiming_cnt == MaxNotes){
				play = score_screen_setup;	
			}
			else if(songTiming[songTiming_cnt] < 14){	//if note is coming up send arrow, takes 14 * 50ms to finished one arrow
				if(note_flag == 0){
					LCD_Cursor(notes);
					LCD_WriteData(curr_arrow);
					LCD_Cursor(notes + 16);
					LCD_WriteData(curr_arrow);
					notes++;
				}
				if((notes >= 6) && (note_flag == 0)){
					LCD_Cursor(notes - 2);
					LCD_WriteData(' ');
					LCD_Cursor(notes - 2 + 16);
					LCD_WriteData(' ');
				}
				if(note_flag == 1){						//note has reached end (cell 16), delete trail 1, sound on for 50ms
					set_PWM(testSongNotes[songTiming_cnt]);
					LCD_Cursor(14);
					LCD_WriteData(' ');
					LCD_Cursor(30);
					LCD_WriteData(' ');
					note_flag = 2;
					if(user1_arrow == curr_arrow){
						tmpD = 0x01;
						user1_score++;
					}
					else{
						tmpD = 0x02;
					}
					if(user2_arrow == curr_arrow){
						tmpD = tmpD | 0x04;
						user2_score++;
					}
					else{
						tmpD = tmpD | 0x08;
					}
					PORTD = tmpD;
				}
				else if(note_flag == 2){						//note has reached end for 50ms, delete trail 2 after deleting trail 1, turn off sound
					PORTD = 0x00;
					tmpD = 0x00;
					set_PWM(0);
					LCD_Cursor(15);
					LCD_WriteData(' ');
					LCD_Cursor(31);
					LCD_WriteData(' ');
					note_flag = 0;								//reset note flag, can now print next notes
					notes = 4;									//reset arrow position
					songTiming[songTiming_cnt] = saveTiming;	//restore last note
					songTiming_cnt++;							//move to next note
					saveTiming_flag = 0;						//set flag to save next note
				}
				if(notes == 16 && note_flag == 0){	//if note reached end stop printing
					note_flag = 1;
				}
			}
				
				
				
			
			if(saveTiming_flag == 0){
				saveTiming = songTiming[songTiming_cnt];	//save current note before decrementing it
				saveTiming_flag = 1;
				curr_arrow = arrows[songTiming_cnt];		//store current arrow to print
			}
			else if(saveTiming_flag == 1){					//if we already saved the timing
				songTiming[songTiming_cnt]--;				//we can decrement the timing
			}
			
			
			
			//code for joysticks
											//first joystick
			if(ADMUX == AD0){
				if(analog == 0xFC){			//down
					LCD_Cursor(16);
					LCD_WriteData(4);		//invert down
					user1_arrow = 3;
				}
				else if(analog <= 0x10){	//up
					LCD_Cursor(16);
					LCD_WriteData(5);		//invert up
					user1_arrow = 2;
				}
				else{						//nothing pressed
					ADMUX = AD1;
				}
				if(ADMUX == AD0){
					ADMUX = AD2;
				}
			}
			else if(ADMUX == AD1){
				if(analog == 0xFC){			//left
					LCD_Cursor(16);
					LCD_WriteData(0);		//invert left
					user1_arrow = 0x7F;
				}
				else if(analog <= 0x10){	//right
					LCD_Cursor(16);
					LCD_WriteData(1);		//invert right
					user1_arrow = 0x7E;
				}
				ADMUX = AD2;
			}								//second joystick
			else if(ADMUX == AD2){
				if(analog == 0xFC){			//down
					LCD_Cursor(32);
					LCD_WriteData(4);		//invert down
					user2_arrow = 3;
				}
				else if(analog <= 0x10){	//up
					LCD_Cursor(32);
					LCD_WriteData(5);		//invert up
					user2_arrow = 2;
				}
				else{						//nothing pressed
					ADMUX = AD4;
				}
				if(ADMUX == AD2){
					ADMUX = AD0;
				}
			}
			else if(ADMUX == AD4){
				if(analog == 0xFC){			//left
					LCD_Cursor(32);
					LCD_WriteData(0);		//invert left
					user2_arrow = 0x7F;
				}
				else if(analog <= 0x10){	//right
					LCD_Cursor(32);
					LCD_WriteData(1);		//invert right
					user2_arrow = 0x7E;
				}
				ADMUX = AD0;
			}		
			//end joystick code
			
			LCD_Cursor(0);	
			break;
		case score_screen_setup:
			LCD_ClearScreen();
			LCD_DisplayString(1, "P1 SCORE:");
			LCD_DisplayString(17, "P2 SCORE:");
									
			LCD_Cursor(11);									//P1 score
			LCD_WriteData((user1_score / 10) + '0');
			LCD_Cursor(12);
			LCD_WriteData((user1_score % 10) + '0');
			LCD_Cursor(13);
			LCD_WriteData('/');
			LCD_Cursor(14);
			if(MaxNotes >= 10){
				LCD_WriteData((MaxNotes / 10) + '0');
				LCD_Cursor(15);
				LCD_WriteData((MaxNotes % 10) + '0');
			}
			else{
				LCD_WriteData(MaxNotes + '0');
			}
			
															//P2 score
			LCD_Cursor(27);
			LCD_WriteData((user2_score / 10) + '0');
			LCD_Cursor(28);
			LCD_WriteData((user2_score % 10) + '0');
			LCD_Cursor(29);
			LCD_WriteData('/');
			LCD_Cursor(30);
			if(MaxNotes >= 10){
				LCD_WriteData((MaxNotes / 10) + '0');
				LCD_Cursor(31);
				LCD_WriteData((MaxNotes % 10) + '0');
			}
			else{
				LCD_WriteData(MaxNotes + '0');
			}
			
			LCD_Cursor(0);	
			play = score_screen;
			break;
		case score_screen:
			if(!A3 || !A5){
				ADMUX = AD1;
				play = score_screen_winner_release;
			}
			else{
				play = score_screen;
			}
			break;
		case score_screen_winner_release:
			if(A3 && A5){
				play = score_screen_winner;
			}
			else{
				play = score_screen_winner_release;
			}
			
			break;
		case score_screen_winner:
			if(!A3 || !A5){
				play = score_screen_release;
			}
			if(user1_score > user2_score && winner_flag == 1){			//user 1 has higher score
				LCD_ClearScreen();
				LCD_DisplayString(4, "P1 WINS!!!");
			}	
			else if(user1_score < user2_score && winner_flag == 1){		//user 2 has higher score
				LCD_ClearScreen();
				LCD_DisplayString(4, "P2 WINS!!!");
			}
			else if (winner_flag == 1){									//tie
				LCD_ClearScreen();
				LCD_DisplayString(4, "Tie Game!");
			}
			
			winner_flag = 0;
			break;
		case score_screen_release:
			if(A3 && A5){
				LCD_ClearScreen();
				songName = " ";
				play = 0;
			}
			break;
		default:
			play = 0;
			break;		
	}
	
	switch(play){		//actions, all are done before switching states or during the state alongside their respective transitions
		default:
			break;
	}
	
	
	
	return play;
}


enum seg{start_seg, menu_seg, wait_countDown, during_song, calculate_score} seg;
	

int tick_SevenSeg(){				//7-seg display and eeprom for score keeping on the 7-seg
	unsigned char S_letter = 0xA7;	//7-seg uses B0-5 and B7, B6 is for PWM
	unsigned char A_letter = 0x9F;
	unsigned char B_letter = 0xB3;
	unsigned char C_letter = 0x36;
	unsigned char D_letter = 0xB9;
	unsigned char E_letter = 0x37;
	unsigned char F_letter = 0x17;
	static unsigned char arr_scores[3];		//holds the scores for each song
	static unsigned char tmpB;				//print char to 7-seg on PORTB
	static unsigned char saveIt;			//flag for if score is higher than previous score we will save it
	static unsigned char combinedScore;		//user1 + user2 score
	unsigned char MaxScore = 2 * MaxNotes;	//highest score on the song
	
	switch(seg){
		case start_seg:
			arr_scores[0] = eeprom_read_byte (( uint8_t *) 2);	
			arr_scores[1] = eeprom_read_byte (( uint8_t *) 3);
			arr_scores[2] = eeprom_read_byte (( uint8_t *) 4);
			tmpB = 0;
			saveIt = 0;
			for(int i = 0; i < sizeof(arr_scores); i++){
				if(arr_scores[i] == 0xFF){		//0xFF is default value
					arr_scores[i] = F_letter;
				}
			}
			seg = menu_seg;
			break;
		case menu_seg:		//display the highscore for current song
			if( (songName == " ") && (menu == choose_song) ){	//we are in the menu
				tmpB = arr_scores[song_cnt];
			}
			else if(songName != " "){						//we have started song
				tmpB = 0x00;
				combinedScore = 0;
				seg = wait_countDown;
			}

			break;
		case wait_countDown:		//dont touch PORTB until countdown in play is finished
			if(cd_cnt == 4){						//waiting for play to finish countdown
				tmpB = F_letter;
				seg = during_song;
			}
			else{
				seg = wait_countDown;
			}
			break;
		case during_song:				//calculate the current score for how many notes were correct so far
			if(combinedScore == 0){
				tmpB = F_letter;
			}
			else if( combinedScore < ( MaxScore / 2 - (MaxScore / 4) ) ){
				tmpB = E_letter;
			}
			else if( combinedScore < ( MaxScore / 2 - ( MaxScore / 10) ) ){
				tmpB = D_letter;
			}
			else if( combinedScore < (MaxScore / 2) ){
				tmpB = C_letter;
			}
			else if( combinedScore < ( MaxScore - (MaxScore / 4) ) ){
				tmpB = B_letter;
			}
			else if( combinedScore < MaxScore){
				tmpB = A_letter;
			}
			else if( combinedScore == MaxScore ){
				tmpB = S_letter;
			}
			
			if(songName == " "){									//we stopped playing the song
				seg = calculate_score;
			}
			else{
				seg = during_song;
			}
			combinedScore = user1_score + user2_score;
			break;
			
		case calculate_score:										//if need to save final score save it, if not don't
			if(arr_scores[song_cnt] == F_letter){
				saveIt = 1;
			}
			else if(arr_scores[song_cnt] == D_letter){
				if(tmpB != F_letter){
					saveIt = 1;
				}
			}
			else if(arr_scores[song_cnt] == E_letter){
				if(tmpB != D_letter && tmpB != F_letter){
					saveIt = 1;
				}
			}
			else if(arr_scores[song_cnt] == C_letter){
				if(tmpB != E_letter && tmpB != D_letter && tmpB != F_letter){
					saveIt = 1;
				}
			}
			else if(arr_scores[song_cnt] == B_letter){
				if(tmpB == S_letter || tmpB == A_letter){
					saveIt = 1;
				}
			}
			else if(arr_scores[song_cnt] == A_letter){
				if(tmpB == S_letter){
					saveIt = 1;
				}
			}
			
			if(saveIt == 1){
				arr_scores[song_cnt] = tmpB;
				eeprom_update_byte (( uint8_t *)  2 + song_cnt, arr_scores[song_cnt]);
				saveIt = 0;
			}
			
			seg = menu_seg;
			break;
		default:
			seg = 0;
			break;
	}
	
	switch(seg){		//actions relating to PORTB
		case menu_seg:
			PORTB = tmpB;
			break;
		case during_song:
			PORTB = tmpB;
			break;
		default:
			break;
		
	}
	
	return seg;
}


//code for reset button on A6
enum reset {reset_start, wait_reset, confirm_reset, reset_game} reset;

int tick_reset(){
	static unsigned char reset_cnt;
	switch(reset){					//transitions
		case reset_start:
			reset = wait_reset;
			break;
		case wait_reset:
			if(!A6){
				reset = confirm_reset;
			}
			else{
				reset = wait_reset;
			}
			break;
		case confirm_reset:
			if(!A6){						//still holding reset button for 30 * 100ms = 3000ms = 3 seconds
				if(reset_cnt >= 30){
					reset = reset_game;
				}			
			}
			else{							//stopped holding it, don't reset
				reset = wait_reset;
			}
			break;
		case reset_game:
			reset = wait_reset;
			break;
		default:
			reset = reset_start;
			break;
	}
	
	switch(reset){			//actions
		case wait_reset:
			reset_cnt = 0;
			break;
		case confirm_reset:
			reset_cnt++;
			break;
		case reset_game:
			LCD_ClearScreen();
			eeprom_update_byte (( uint8_t *) 2, 0xFF );
			eeprom_update_byte (( uint8_t *) 3, 0xFF );
			eeprom_update_byte (( uint8_t *) 4, 0xFF );
			ADMUX = AD1;
			menu = 0;
			play = 0;
			seg = 0;
			break;
		default:
			break;
	}
	return reset;
}


int main(void)
{
    DDRA = 0x00; PORTA = 0xFF;
    DDRB = 0xFF; PORTB = 0x00;
	DDRD = 0xFF; PORTD = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	
	
	//custom characters for LCD
	unsigned char Character1[8] = { 0b11111,0b11011,0b10111,0b00000,0b10111,0b11011,0b11111,0b11111}; //invert left arrow
	unsigned char Character2[8] = {0b11111,0b11011,0b11101,0b00000,0b11101,0b11011,0b11111,0b11111 };	//invert right arrow
	unsigned char Character3[8] = {0b00100,0b01110,0b10101,0b00100,0b00100,0b00100,0b00100,0b00100 };	//up arrow
	unsigned char Character4[8] = {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b10101, 0b01110, 0b00100 };  //down arrow
	unsigned char Character5[8] = { 0b11011,0b11011,0b11011,0b11011,0b11011,0b01010,0b10001,0b11011 };	//invert down arrow
	unsigned char Character6[8] = { 0b11011,0b10001,0b01010,0b11011,0b11011,0b11011,0b11011,0b11011 };	//invert up arrow
	LCD_Custom_Char(0, Character1);  // Build Character1 at position 0, overwrites what 0 is 
	LCD_Custom_Char(1, Character2);  // Build Character2 at position 1
	LCD_Custom_Char(2, Character3);   //Build Character3 at position 2
	LCD_Custom_Char(3, Character4);   //Build Character4 at position 3
	LCD_Custom_Char(4, Character5);  //Build Character6 at position 5
	LCD_Custom_Char(5, Character6);  //Build Character6 at position 5
		
		
	LCD_init();
	ADC_init();
	TimerSet(tasksPeriodGCD);	//50ms to get input from joystick one then 50ms to get input from joystick 2, 100 ms for each joystick to respond
	TimerOn();
	set_PWM(0);
	PWM_on();

	
	unsigned char i=0;
	tasks[i].state = 0;
	tasks[i].period = periodMenu;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &tick_Menu;
	i++;
	tasks[i].state = 1;
	tasks[i].period = periodPlay;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &tick_Play;
	i++;
	tasks[i].state = 2;
	tasks[i].period = periodSevenSeg;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &tick_SevenSeg;
	i++;
	tasks[i].state = 3;
	tasks[i].period = periodReset;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &tick_reset;
	
	
		
    while (1)
    {
    }
	
}

