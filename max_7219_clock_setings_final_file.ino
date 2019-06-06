/*
 CONNECTIONS:
 
 pin 12 is connected to MAX7219 DataIn
 pin 11 is connected to MAX7219 CLK
 pin 10 is connected to MAX7219 LOAD/CS
 pins A4/A5 are connected to RTC module (or D6/D5 via SoftI2C)
 pin8 - mode/debug button
 BT module to RX=D1
 pin 3 - audio to base via 220ohm, emiter to GND, speaker to emiter and resistor 10ohm to VCC
 pin A0 - thermistor to VCC, A0 via R=10k to GND
*/


// Sketch uses 15,634 bytes (98%) of program storage space. Maximum is 15,872 bytes.

// NOKIA tunes:
// Triple
const unsigned char mess[] PROGMEM = "T:d=8,o=5,b=635:c,e,g,c,e,g,c,e,g,c6,e6,g6,c6,e6,g6,c6,e6,g6,c7,e7,g7,c7,e7,g7,c7,e7,g7";
// Happy birthday
const unsigned char bday[] PROGMEM = "Happy birthday:d=4,o=5,b=125:8d.,16d,e,d,g,2f#,8d.,16d,e,d,a,2g,8d.,16d,d6,b,g,f#,2e,8c.6,16c6,b,g,a,2g";

#define LOW_MEM_DEBUG 0
#define USE_RTC       1  // 1 for RTC clock module (default)
#define USEHW         1 // 1 for hw I2C RTC on A4/A5 pins
#define NUM_MAX       4
#include <Wire.h>
// MAX7219 matrices pins
#define DIN_PIN 12
#define CS_PIN  11
#define CLK_PIN 10
#define ROTATE 90  // old hardware change angle
#define BUTTON_1        4                    
#define BUTTON_2        3                      
#define BUTTON_3        2               
#define BUTTON_MODE     4

#define AUDIO_PIN      5
#define THERMISTOR_PIN A0

#define CHIME_START   7
#define CHIME_END     23

#define BDAY_START    9
#define BDAY_END      22

#define NIGHT_START   23
#define NIGHT_END     6

int hour=19,minute=21,second=0;
int year=2016,month=8,day=20,dayOfWeek=6;
#include "rtc.h"

#include "audio.h"
#include "max7219.h"
#include "fonts.h"

// ----------------------------------
#define CLOCKBIG    1
#define CLOCKMED    2
#define CLOCK       3
#define DATE        4
#define DATE1       5
#define DATE2       6
#define ANIMATED    7
#define TEMP        8
#define CLOCKBIGJMP 9
#define SPECIAL     10
#define DISPMAX    10

int dx = 0;
int dy = 0;
int pos = 8;
int cnt = -1;
int h1, h0, m1, m0, s1, s0, secFr, lastSec=-1, lastDay=-1;
int d1,d0,mn1,mn0,y1,y0,dw;
int mode = 0, prevMode = 0;
int stx=1;
int sty=1;
int st = 1;
int disp = 1, prevDisp = 1;
int tr1 = 0, tr2 = 0;
int trdisp1 = 1, trdisp2 = 1;
int trans = 0, prevTrans = 0;
int dots = 0;
int dots2=0;   
int del = 40;
int commandMode = 0;
float temp = 0;
uint32_t startTime, diffTime, zeroTime;
int charCnt=0;
char charBuf[7];
int setMode=0;  
int brightSet=16;   
int key=0;                                       
int keyOld=0;                                    
int keySpeed=0;                                  
int prevF;                                         
unsigned long now;            
const int timerMenu=10000; 
#define MAX_DIGITS 5
byte dig[MAX_DIGITS]={0};
byte digold[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};
//==============================================================
void klav(){                                       //
  key=0;                                           //
  if(digitalRead(BUTTON_1)==LOW) key=1;          
  if(digitalRead(BUTTON_2)==LOW) key=2;           
  if(digitalRead(BUTTON_3)==LOW) key=3;          
  if(key!=0){                                     
    delay(keySpeed<8? 200:20);                     
    keySpeed++;                                   
  }                                               
  if(key!=keyOld) keySpeed=0;                     
  keyOld=key;                                   
}             
void setup() 
{
 
  digitalWrite(BUTTON_1, HIGH);                    
  digitalWrite(BUTTON_2, HIGH);                    
  digitalWrite(BUTTON_3, HIGH);                
  pinMode(AUDIO_PIN, OUTPUT);
  
  initMAX7219();
  clr();
  refreshAll();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY,0);
  //Serial.println("Init");
  Wire.begin();
  temp = readTherm();
  playChime();
  
  zeroTime = millis();
//  delay(1000);
//  playMessage();
//  delay(1000);
//  alarmCnt = 1; playAlarm();
}


// ----------------------------------
/*
float readTherm() 
{
  float pad = 9960;          // balance/pad resistor value - 10k
  float thermr = 10000;      // thermistor nominal resistance - 10k
  long resist = pad*((1024.0 / analogRead(THERMISTOR_PIN)) - 1);
  float temp = log(resist);
  temp = 1 / (0.001129148 + (0.000234125 * temp) + (0.0000000876741 * temp * temp * temp));
  temp -= 273.15;
  return temp;
}
*/

// ----------------------------------
void updateTime()
{
#if USE_RTC==1
  getRTCDateTime();
#else
  diffTime = (millis() - zeroTime)/1000;
  if(diffTime<=0) return;
  zeroTime += diffTime*1000;
  for(int i=0;i<diffTime;i++) {
    second++;
    if(second>=60) {
      second=0;
      minute++;
      if(minute>=60) {
        minute=0;
        hour++;
        if(hour>=24) {
          hour=0;
          dayOfWeek++;
          if(dayOfWeek>7)
            dayOfWeek=1;
          day++;
          if(day>31) {
            day=1;
            month++;
            if(month>12) {
              month=1;
              year++;
            }
          }
        }
      }
    }
  }
#endif
}

// ----------------------------------

byte dispTab[20] = {
  CLOCKMED, CLOCKMED, CLOCKMED, CLOCKMED, CLOCK, CLOCK, CLOCK,  // 7/20
  SPECIAL, DATE1, // 2/20
  CLOCKBIG,CLOCKBIG,CLOCKBIG,CLOCKBIG,ANIMATED,ANIMATED,ANIMATED,ANIMATED, // 8/20
  DATE2,TEMP,CLOCKMED // 3/20
};
//=======================================================
void loop()
{
  startTime = millis();
  updateTime();
  int HOUR;
if (hour > 12 )
{
HOUR = hour - 12;
}
else
{
HOUR = hour;
}
if (HOUR < 10)
{
 h0 = HOUR;
 h1 = HOUR/10;
}
else if (HOUR >=  10 && HOUR < 24)
{
 h0 = HOUR % 10;
 h1 = HOUR / 10;
}
  //h1 = hour / 10;
  //h0 = hour % 10;
  m1 = minute / 10;
  m0 = minute % 10;
  s1 = second / 10;
  s0 = second % 10;
  d1 = day / 10;
  d0 = day % 10;
  mn1 = month / 10;
  mn0 = month % 10;
  y1 = (year - 2000)/10;
  y0 = (year - 2000)%10;
  dw = dayOfWeek; // dw=0..6, dayOfWeek=1..7
  if(dw>6) dw -= 7;

  if(second!=lastSec){                                                    
    lastSec=second;                                                    
    secFr=0;                                                          
  } else secFr++;                                                        
  dots=(secFr<20)? 1:0;                                                 
  dots2=(second%2)? 1:0;                                                  
 
 
   
//==========================================
  klav();                                                                 // Interrogate keys 0- not pressed, 1-3 keys pressed
  if((key==2||key==3)&&setMode==0&&mode!=10){                             // ENTRANCE to display mode change mode
       playChime();                                                                   
    if(key==2){if(++mode>9) mode=0;}                                      // choose the next mode
    else if(--mode<0) mode=9;                                             //choose the previous mode
    disp=mode;                                                            //
    clr();                                                                //
    dx=dy=0;                                                              //
  if (! mode) showString (0, "P-Auto"); // Display "P-AUTO" if mode = 0
     else {showString (0, "Mode"); // Display "Mode"
       showDigit (mode, 27, dig5x8rn); // and mode number
    }                                                                     //
    refreshAll();                                                         //
    delay(500);                                                           //
  }                                                                       //
     if(key==1){                                              
      dx=dy=0;                                                            //
      clr();
      playClockSet();      //
    showString (1, "setting"); // Display "INSTALLATION"
       refreshAll (); //
       delay (500); //
       setMode = 1; // enable installation flag
       setUp (); // go to the settings subroutine
      key=0;                                                              //
    } 
    
   
  prevDisp = disp;
  switch(mode) {
    case 0: autoDisp(); break;
    default: disp = mode; break;
  }
  
  // night mode - only big clock, low intensity
  if(hour==NIGHT_START && minute==0 && second==0 && secFr==0) {
    prevMode = mode;
    mode = CLOCKBIG;
    sendCmdAll(CMD_INTENSITY, 0);
  } else  
  if(hour==NIGHT_END && minute==0 && second==0 && secFr==0)
    mode = prevMode;

  clr();
  if(disp!=prevDisp) {
    trans = 1 + (prevTrans % 4);
    prevTrans = trans;
    switch(trans) {
      case 1:  tr1 = 0; tr2 = -38; st = +1; break;
      case 2:  tr1 = 0; tr2 =  38; st = -1; break;
      case 3:  tr1 = 0; tr2 = -11<<1; st = +1; break;
      case 4:  tr1 = 0; tr2 =  11<<1; st = -1; break;
    }
    trdisp1 = prevDisp;
    trdisp2 = disp;
    if(prevDisp==CLOCKBIGJMP || disp==CLOCKBIGJMP) { trans=dx=dy=0; }
  }

  if(!trans) {
    render(disp);
  } else {
    if(trans==1 || trans==2) dx = tr1; else dy = tr1>>1;
    render(trdisp1);
    if(trans==1 || trans==2) dx = tr2; else dy = tr2>>1;
    render(trdisp2);
    tr1 += st;
    tr2 += st;
    if(tr2==0) trans = dx = dy = 0;
  }
  
  refreshAll();

  // play chime every full hour
  if(hour>=CHIME_START && hour<CHIME_END && minute==0 && second==0 && secFr==0)
    playChime();

 // play bday tune 5 minutes after full hour  
 if(hour>=BDAY_START && hour<BDAY_END && minute==5 && disp==SPECIAL && trans==0 && isBDay()>=0)  playRTTTL(bday);
  while (millis() - startTime < 25);
}
//================================================================================================================================
void autoDisp()
{
  // seconds 0-59 -> 0-19, 3s steps
  disp = dispTab[second/3];
}


void playMessage()
{
#if LOW_MEM_DEBUG==0
  playRTTTL(mess);
#endif
}

// ----------------------------------

void playBirthday()
{
#if LOW_MEM_DEBUG==0
  playRTTTL(bday);
#endif
}

// ----------------------------------

void playChime()
{
  playSound(AUDIO_PIN,2000,40);
  delay(200);
 // playSound(AUDIO_PIN,2000,40);
}

// ----------------------------------

void playClockSet()
{
  playSound(AUDIO_PIN,1500,60);
  delay(100);
  playSound(AUDIO_PIN,1500,60);
  delay(100);
  playSound(AUDIO_PIN,1500,60);
}

// ----------------------------------

// ----------------------------------
void showTemp()
{
//  int t = (readIntTemp() / 10000) - 2;
//  if(t/10) showDigit(t / 10, 6, dig5x8sq);
//  showDigit(t % 10, 12, dig5x8sq);
//  showDigit(7, 18, dweek_pl);
  if(secFr==0) temp = readTherm() + 0.05;
//  Serial.println(temp);
  if(temp>0 && temp<99) {
    int t1=(int)temp/10;
    int t0=(int)temp%10;
    int tf=(temp-int(temp))*10.0;
    if(t1) showDigit(t1, 2, dig5x8sq);
    showDigit(t0, 8, dig5x8sq);
    showDigit(tf, 16, dig5x8sq);
  }
  setCol(14, 0x80);
  showDigit(7, 22, dweek_en);
}

// 4+1+4+2+4+1+4+2+4+1+4 = 31
//  [ 4. 1.16]
void showDate(){                                                          //
  showDigit(d1, 0, dig4x8);                                               // if(d1) 
  showDigit(d0, 5, dig4x8);                                               //
  showDigit(mn1, 12, dig4x8);                                             // if(mn1) 
  showDigit(mn0, 17, dig4x8);                                             //
  showDigit(y1, 23, dig4x8);                                              //
  showDigit(y0, 28, dig4x8);                                              // 
  setCol(10, 0x80);                                                    
  setCol(22, 0x80);                                                      
}                       

//  [ 4. 1.2016]
void showDate1()
{
  if(d1) showDigit(d1, 0, dig3x8);
  showDigit(d0, 4, dig3x8);
  if(mn1) showDigit(mn1, 9, dig3x8);
  showDigit(mn0, 12, dig3x8);
  showDigit(2, 18, dig3x8);
  showDigit(0, 22, dig3x8);
  showDigit(y1, 26, dig3x8);
  showDigit(y0, 29, dig3x8);
  setCol(8, 0x80);
  setCol(16,0x80);
}

// 4+1+4+ 3 +4+1+4 = 21 +1 +10 =32
//  [ 4. 1 MO]
void showDate2()
{
  if(d1) showDigit(d1, 0, dig4x8);
  showDigit(d0, 5, dig4x8);
  if(mn1) showDigit(mn1, 11, dig4x8);
  showDigit(mn0, 16, dig4x8);
//  showDigit(dw, 22, dweek_en);
  showDigit(dw, 22, dweek_en);
  setCol(10, 0x80);
}

// 4+1+4+3+4+1+4=21 + 3+1+3
void showClock()
{
 // if (h1 > 0) showDigit(h1, h1 == 2 ? 0 : 1, dig4x8);
  showDigit(h1, 0, dig4x8);
  showDigit(h0, 5, dig4x8);
  showDigit(m1, 12, dig4x8);
  showDigit(m0, 17, dig4x8);
  showDigit(s1, 24, dig3x7);
  showDigit(s0, 28, dig3x7);
  setCol(10, dots ? 0x24 : 0);
}


//6+2+6+3+6+2+6 = 31
void showClockBig(int jump=0){                                            //
  if(jump && !trans) {                                                    //
    dx+=stx; if(dx>25 || dx<-25) stx=-stx;                                //
    dy+=sty; if(dy>6 || dy<-6) sty=-sty;                                  //
    delay(40);                                                            //
  }                                                                       //
  if (h1 > 0) showDigit(h1, h1 == 2 ? 0 : 2, dig6x8);                     //
  showDigit(h0, 7, dig6x8);                                               //
  showDigit(m1, 19, dig6x8);                                              //
  showDigit(m0, 26, dig6x8);                                              //
                                                                          // 
    if (dots){                                                            //
      setCol(15, 0x66);                                                   //
      setCol(16, 0x66);                                                   //
    }                                                                     //
  }                     

// 5+1+5+3+5+1+5+ 1+3+1+3=33
void showClockMed()
{
  if (h1 > 0) showDigit(h1, 0, dig5x8rn);
  showDigit(h0, h1==2 ? 6 : 5, dig5x8rn); // <20h display 1 pixel earlier for better looking dots
  showDigit(m1, 13, dig5x8rn);
  showDigit(m0, 19, dig5x8rn);
  showDigit(s1, 25, dig3x6);
  showDigit(s0, 29, dig3x6);
  setCol((hour==20) ? 12 : 11, dots ? 0x24 : 0); // 20:xx - dots 1 pixel later
}
void showAnimClock()
{
  byte digPos[4]={0,7,19,26};
  int digHt = 12;
  int num = 4; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
   
   dig[0] = hour/10 ;
    dig[1] = hour%10;
    dig[2] = minute/10;
    dig[3] = minute%10;
   
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig6x8);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig6x8);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig6x8);
      digtrans[i]--;
    }
  }
  dy=0;
     if (dots){                                                            //
      setCol(15, 0x66);                                                   //
      setCol(16, 0x66);                                                   //
    }      
}
// ----------------------------------

// put below more special days, holidays, birthdays, etc.

const byte specialDays[] PROGMEM = {
 
27,4, //     day,month
15,5,
28,7,
1,9,
};

// this way it takes less memory than in PROGMEM
char specialText[][23] = {  //max lanth of arry is special text+1
                            //all special text size is same size

 "\224Happy b'day Rajnikant" ,
 "\224Happy b'day Richa    ",
 "\224Happy b'day Gayatri  ",
 "\224Happy b'day Preet    ",

};

int isBDay()
{
  for(int i=0;i<sizeof(specialDays)/2;i++)
    if(day==pgm_read_byte(specialDays+i*2) && month==pgm_read_byte(specialDays+i*2+1)) return i;
  return -1;
}

void showSpecial()
{
//day=14; month=2;
  int bd = isBDay();
  if(bd<0)
    showClockBig();
  else
      scrollString(specialText[bd],50);
}

// ----------------------------------

void render(int d)
{
  switch(d) {
    case ANIMATED:    showAnimClock(); break;
    case CLOCKBIG:    showClockBig(); break;
    case CLOCKBIGJMP: showClockBig(1); break;
    case CLOCKMED:    showClockMed(); break;
    case CLOCK:       showClock(); break;
    case DATE:        showDate(); break;
    case DATE1:       showDate1(); break;
    case DATE2:       showDate2(); break;
    case TEMP:        showTemp(); break;
    case SPECIAL:     showSpecial(); break;
  //case ALARM:       showAlarm(); break;
//  case EMPTY:       break;
    default:          showClockMed(); break;
  }
}

// ----------------------------------

void showDigit(char ch, int col, const uint8_t *data)
{
  if(dy<-8 | dy>8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if(col+i>=0 && col+i<NUM_MAX*8) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
}

// ---------------------------------------------

int showChar(char ch, int col, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  if(dy<-8 | dy>8) return w;
  col += dx;
  for (i = 0; i < w; i++)
    if(col+i>=0 && col+i<NUM_MAX*8) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
  return w;
}

// ---------------------------------------------

void showString(int x, char *s) {                                         
  while(*s) {                                                             
    unsigned char c = *s++;                                              
    c -= 32;                                                              
    int w = showChar(c, x, font);                                     
    x += w + 1;                                                           
  }                                                                       
}                                                                        

// ---------------------------------------------
void setCol(int col, byte v)
{
  if(dy<-8 | dy>8) return;
  col += dx;
  if(col>=0 && col<32)
    if(!dy) scr[col] = v; else scr[col] |= dy>0 ? v>>dy : v<<-dy;
}

// ---------------------------------------------

int printChar(unsigned char ch, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[NUM_MAX*8 + i] = 0;
  return w;
}



// ---------------------------------------------
void scrollChar(unsigned char c, int del) {

  c -= 32;
  int w = printChar(c, font);
  for (int i=0; i<w+1; i++) {
    delay(del);
    scrollLeft();
    refreshAll();
  }
}

// ---------------------------------------------
void scrollString(char *s, int del)
{
  while(*s) scrollChar(*s++, del);
}
//===============================================================================================================
void setUp(){                                                            
  now = millis();                                                        
  while (millis()-now < timerMenu){                                       
    clr();                                                            
    switch(setMode){                                                      
        case 1: showString(1, "Time"); break;                           
        case 2: showString(1, "Date"); break;                             
        case 3: showString(1, "Day"); break;                               
        case 4: showString(1, "Brightness"); break;                      
                               
    }                                                                    
    refreshAll();                                                         
    klav();                                                              
    if(key == 1){                                                         //
  playClockSet();                                                       
      setMode = 0, key=0;                                                 //
      return;                                                             //
    }                                                                     //
    if(key == 3) {                                                        //
      playChime() ;                                                                   
      switch(setMode){                                                    //
        case 1: setUpTime(); break;                                       //
        case 2: setUpData(); break;                                       //
        case 3: setUpDoW(); break;                                        //                                 
      default: brightDisp(); break;                                    
      }                                                                   //
      now = millis();                                                     //
    }                                                                     //
    if(key == 2){                                                         //
      playChime() ;                                                                  
      now = millis();                                                     //
      setMode++;                                                          //
      if(setMode == 5) setMode = 1;                                       //
    }                                                                     //
  }                                                                       //
                                                         
  setMode = 0;                                                            //
}                                                                         //

//=============================================================================================================================================
void setUpTime(){                                                         //
  int i;                                                                  //
  int j = 0;                                                             
  now = millis();                                                         
  while (millis()-now < timerMenu+60000*(j==4)){                          
    clr();                                                            
    h1 = hour / 10, h0 = hour % 10;                                       //
    m1 = minute / 10, m0 = minute % 10;                                   //
    s1 = second / 10, s0 = second % 10;                                   //
    showClock();                                                         //
    i=((millis()-now)/350%2);                                             
    klav();                                                               
    if(key == 1){                                                         
    playClockSet();                                                            
      return;                                                             //
    }                                                                     //
    if(key == 2){                                                       
      playChime() ;                           
      now = millis();                                                     
      j++;                                                                //
      if(j>4) j=0;                                                        //
    }                                                                     //
    if(key == 3){                                                         
       playChime() ;                              
      now = millis();                                                     //
      i=0;                                                                //
      switch(j){                                                          //
        case 0: h1 = 0 + 1*(h1==0) + 2*(h1==1)*(h0<4); break;             //
        case 1: h0 = 0 + (h0+1)*(h0<9)*(h1<2) + (h0+1)*(h0<3)*(h1==2); break;  //
        case 2: m1 = 0 + (m1+1)*(m1<5); break;                            //
        case 3: m0 = 0 + (m0+1)*(m0<9); break;                            //
        //case 4: second = 0; break;                                      //
        default: second = 0; break;                                       
      }                                                                   //
      hour = h1*10+h0;                                                    //
      minute = m1*10+m0;                                                  //
      if(j==4) setRTCTime();                                              
      else setRTCTimeNotSec();                                            
      }                                                                   //
    if(i) clr1(j*4+3*(j==2)+4*(j==3)+7*(j>3), 5+1*(j>0)+3*(j>3));         
      refreshAll();                                                       //
  }                                                                       //
}                                                                         //

//==============================================================================================================================================================================================
void setUpData(){                                                         //
  int i;                                                                  //
  int j = 0;                                                             
  now = millis();                                                       
  while (millis()-now < timerMenu){                                      
    clr();                                                              
    getRTCDateTime();                                                   
    d1 = day / 10;                                                       
    d0 = day % 10;                                                     
    mn1 = month / 10;                                                     
    mn0 = month % 10;                                                     
    y1 = (year - 2000)/10;                                                
    y0 = (year - 2000)%10;                                                
    showDate();                                                           
    i=((millis()-now)/350%2);                                            
    klav();                                                              
    if(key == 1){                                                         
       playClockSet();                                                                       
      return;                                                            
    }                                                                    
    if(key == 2){                                                         
     playChime() ;                                                                  
      now = millis();                                                   
      j++;                                                               
      if(j>5) j=0;                                                       
    }                                                                   
    if(key == 3){                                                       
      playChime() ;                                  
      now = millis();                                                    
      i=0;                                                                //
      switch(j){                                                          //
        case 0: d1 = 0+1*(d1==0)+2*(d1==1)+3*(d1==2)*(d0<2); break;       //
        case 1: d0 = 0+(d0+1)*(d0<9)*(d1<3)+(d0+1)*(d0<1)*(d1==3); break; // 
        case 2: mn1 = 0+1*(mn1==0); break;                                // 
        case 3: mn0 = 0+(mn0+1)*(mn0<9); break;                           // 
        case 4: y1 = 0+(y1+1)*(y1<9); break;                              //
        default: y0 = 0+(y0+1)*(y0<9); break;                             // 
      }                                                                   //
      if(mn1==1 && mn0>2 || mn1==0 && mn0==0){                            // 
        mn1=1 ;                                                           //
        mn0=0;                                                            //
      }                                                                   //
      month = mn1*10+mn0;                                                 // 
      if(d1==0&&d0==0) d0 = 1;                                            // 
      if(d1==3&&d0==1&&(month==2||month==4||month==6||month==9||month==11)) d0=0; // 
      day = d1*10+d0;                                                     // 
      year = y1*10+y0+2000;                                               //
      if(month == 2 && day >= 29 && (year%4 != 0)){                       //
        d1=2;                                                             
        d0=8;                                                            
        day=28;                                                           
      }                                                                   //
      if(month == 2 && day >= 29 && (year%4 == 0)){                       // 
        d1=2;                                                            
        d0=9;                                                           
        day=29;                                                          
      }                                                                   //
      setRTCDate();                                                       
    }                                                                     //
    if(i) clr1(j*4+3*(j==2)+4*(j==3)+7*(j>3), 5+1*(j>0)*(j<4));            
    refreshAll();                                                         
  }                                                                       //
}                                                                         //

//======================================================================================================================================
void setUpDoW(){                                                          //
  int i;                                                                  //
  now = millis();                                                        
  while (millis()-now < timerMenu){                                      
    clr();                                                                //
    getRTCDateTime();                                                     //
    i=((millis()-now)/350%2);                                            
    klav();                                                               
    if(key == 1){                                                         //
        playClockSet();                                                                      
      return;                                                             //
    }                                                                     //
    if(key == 2){                                                         //
   playChime() ;                                               
      i=1;                                                                //
      dayOfWeek++;                                                        //
      if(dayOfWeek > 7) dayOfWeek = 1;                                    //
      setRTCDoW();                                                       
      now = millis();                                                     //
    }                                                                     //
    if(key == 3){                                                         //
       playChime() ;                                                                  
      i=1;                                                                //
      dayOfWeek--;                                                        //
      if(dayOfWeek == 0) dayOfWeek = 7;                                   //
      setRTCDoW();                                                       
      now = millis();                                                     //
    }                                                                     //
    if(i){                                                                //
      switch(dayOfWeek){                                                  //
        case 1: showString(2, "MON"); break;                  
        case 2: showString(3, "TUE"); break;                        
        case 3: showString(3, "WED"); break;                            
        case 4: showString(3, "THE"); break;                         
        case 5: showString(3, "FRI"); break;                          
        case 6: showString(3, "SAT"); break;                          
        case 7: showString(3, "SUN"); break;                     
      }                                                                   //
    }                                                                     //
    refreshAll();                                                         //
  }                                                                       //
                                                      
} 
//======================================================================================================================
void brightDisp(){                                                        //
  int i;                                                                  //
  now=millis();                                                          
  while(millis()-now<timerMenu){                                         
    clr();                                                           
    i=((millis()-now)/350%2);                                             
    showDigit(i==0 ? 1 :2 , 0, dig8x8);                                   
    klav();                                                               
    if(key == 1){                                                         //
        playClockSet();                                                   
      return;                                                             //
    }                                                                     //
    if(key==2){   
      playChime() ;                                                     
      now=millis(); brightSet++;            
      if(brightSet>17) brightSet=0;                                       //
    }                                                                     //
    if(key==3){
      playChime() ;                                                         
      now=millis(); brightSet--;            //
      if(brightSet<0) brightSet=17;                                       //
    }                                                                     // 
    if(brightSet<16){                                                     //
      if(brightSet/10) showDigit(brightSet/10, 13, dig6x8);               
      showDigit(brightSet%10, brightSet/10? 21: 17, dig6x8);            
      sendCmdAll(CMD_INTENSITY, brightSet);                               
    }                                                                     //
    if(brightSet==16){                                                    //
      showString(10, "Low");                                             
      sendCmdAll(CMD_INTENSITY, 2);                                       //
    }                                                                     //
    if(brightSet==17){                                                    //
      showString(12, "High");                                              
      sendCmdAll(CMD_INTENSITY, 10);                                      //
    }                                                                     //
    refreshAll();                                                         //
  }                                                                       //
                                                            
}                                 
