#include <serialize.h>
#include <stdarg.h>
#include <avr/sleep.h>

#include "packet.h"
#include "constants.h"
#include <math.h>

//#define PI 3.141592654
#define ALEX_LENGTH 20
#define ALEX_BREADTH 12
#define BUZZER 13

//powermanagement
#define PRR_TWI_MASK 0b10000000
#define PRR_SPI_MASK 0b00000100
#define ADCSRA_ADC_MASK 0b10000000
#define PRR_ADC_MASK 0b00000001
#define PRR_TIMER2_MASK 0b01000000
#define PRR_TIMER0_MASK 0b00100000
#define PRR_TIMER1_MASK 0b00001000
#define SMCR_SLEEP_ENABLE_MASK 0b00000001
#define SMCR_IDLE_MODE_MASK 0b11110001

float alexDiagonal = 0.0;
float alexCirc = 0.0;

//colour sensing
#include <Wire.h>
#include "SFE_ISL29125.h"
#include <Adafruit_NeoPixel.h>

#define LEDSTRIP 8
volatile unsigned long time_now = 0;
// Declare sensor object
SFE_ISL29125 RGB_sensor;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(8, LEDSTRIP, NEO_GRB + NEO_KHZ800);
volatile char colour;
unsigned long red = 0;
unsigned long green = 0;
unsigned long blue = 0;

//unsigned long duration = 0;
//unsigned long cm = 999;
//unsigned long inches = 999;
//volatile bool tooNear = false;

void turnOn() {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255));
    strip.show();

  }
}

void turnOff() {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
    strip.show();

  }
}

/////

typedef enum
{
STOP=0,
FORWARD=1,
REVERSE=2,
LEFT=3,
RIGHT=4,
COLOUR=5
} TDirection;

volatile TDirection dir = FORWARD;

void dbprint(char *format, ...) {
  va_list args;
  char buffer[128];
  va_start(args, format); 
  vsprintf(buffer, format, args);
  sendMessage(buffer);
}

/*
 * Alex's configuration constants
 */

// Number of ticks per revolution from the 
// wheel encoder.

#define COUNTS_PER_REV_LEFT      210
#define COUNTS_PER_REV_RIGHT     140//120 works

// Wheel circumference in cm.
// We will use this to calculate forward/backward distance traveled 
// by taking revs * WHEEL_CIRC

#define WHEEL_CIRC          20.42

// Motor control pins. You need to adjust these till
// Alex moves in the correct direction
#define LF                  6   // Left forward pin
#define LR                  5   // Left reverse pin
#define RF                  10  // Right forward pin
#define RR                  11  // Right reverse pin

/*
 *    Alex's State Variables
 */

// Store the ticks from Alex's left and
// right encoders.
volatile unsigned long leftForwardTicks; 
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks; 
volatile unsigned long rightReverseTicks;

volatile unsigned long leftForwardTicksTurns; 
volatile unsigned long rightForwardTicksTurns;
volatile unsigned long leftReverseTicksTurns; 
volatile unsigned long rightReverseTicksTurns;

// Store the revolutions on Alex's left
// and right wheels
volatile unsigned long leftRevs;
volatile unsigned long rightRevs;

// Forward and backward distance traveled
volatile unsigned long forwardDist;
volatile unsigned long reverseDist;
//unsigned long deltaDist;
//unsigned long newDist;
volatile unsigned long deltaTicks;
volatile unsigned long targetTicks;
// vars for straught movement
volatile unsigned long num_ticks_l;
volatile unsigned long num_ticks_r;
volatile int val;
volatile int power_l;
volatile int power_r;
volatile unsigned long diff_l;
volatile unsigned long diff_r;
volatile unsigned long enc_l_prev;
volatile unsigned long enc_r_prev;
volatile float num_rev;
volatile unsigned long target_count_left;
volatile unsigned long target_count_right;
volatile unsigned long timeNow;
volatile unsigned long commandTime;
const int motor_offset = 5; 
  
/*
 * 
 * Alex Communication Routines.
 * 
 */
void WDT_off(void)
{
  cli();
  
  MCUSR &= ~(1<<WDRF);

  WDTCSR |= (1<<WDCE) | (1<<WDE);

  WDTCSR =0x00;

  //EIMSK |= 0b00000011; 

  sei();
}

void setupPowerSaving()
{
  WDT_off();
  //WDTCSR = 0x00;
  //PRR |= PRR_TWI_MASK;
  PRR |= PRR_SPI_MASK;
  ADCSRA &= 0b01111111;
  PRR |= PRR_ADC_MASK;
  SMCR &= SMCR_IDLE_MODE_MASK;
  //DDRB |= 0b00100000;//output
  //PORTB &= 0b11011111;//logic low
}

void putArduinoToIdle()
{
  PRR |= PRR_TIMER0_MASK;
  PRR |= PRR_TIMER1_MASK;
  PRR |= PRR_TIMER2_MASK;
  SMCR &= SMCR_SLEEP_ENABLE_MASK;
  sleep_cpu();
  SMCR |= ~SMCR_SLEEP_ENABLE_MASK;
  PRR &= ~PRR_TIMER0_MASK;
  PRR &= ~PRR_TIMER1_MASK;
  PRR &= ~PRR_TIMER2_MASK;
}
 
TResult readPacket(TPacket *packet)
{
    // Reads in data from the serial port and
    // deserializes it.Returns deserialized
    // data in "packet".
    
    char buffer[PACKET_SIZE];
    int len;

    len = readSerial(buffer);

    if(len == 0)
      return PACKET_INCOMPLETE;
    else
      return deserialize(buffer, len, packet);
    
}

void sendStatus()
{
  // Implement code to send back a packet containing key
  // information like leftTicks, rightTicks, leftRevs, rightRevs
  // forwardDist and reverseDist
  // Use the params array to store this information, and set the
  // packetType and command files accordingly, then use sendResponse
  // to send out the packet. See sendMessage on how to use sendResponse.
  //
  TPacket statusPacket;
  statusPacket.packetType = PACKET_TYPE_RESPONSE;
  statusPacket.command = RESP_STATUS;
  statusPacket.params[0] = leftForwardTicks;
  statusPacket.params[1] = rightForwardTicks;
  statusPacket.params[2] = leftReverseTicks;
  statusPacket.params[3] = rightReverseTicks;
  statusPacket.params[4] = leftForwardTicksTurns;
  statusPacket.params[5] = rightForwardTicksTurns;
  statusPacket.params[6] = leftReverseTicksTurns;
  statusPacket.params[7] = rightReverseTicksTurns;
  statusPacket.params[8] = forwardDist;
  statusPacket.params[9] = reverseDist;

  sendResponse(&statusPacket);
}

void sendColour(char c)
{
  //sense colour
  turnOn();
  time_now = millis();   
  while(millis() < time_now + 100);

  red = 0;
  green = 0;
  blue = 0;

  for (int i = 0; i < 10; i ++)
  {
    red += RGB_sensor.readRed();
    green += RGB_sensor.readGreen();
    blue += RGB_sensor.readBlue();
    time_now = millis();   
    while(millis() < time_now + 100);
  }
  red /= 10;
  green/=10;
  blue /= 10;
  if (red > green)
  {
    c = 'R';
    
  }
  else
  {
    c = 'G';
  }
  
  TPacket statusPacket;
  statusPacket.packetType = PACKET_TYPE_RESPONSE;
  statusPacket.command = RESP_COLOUR;//COMMAND_COLOUR;
  statusPacket.params[0] = c;
  statusPacket.params[1] = map(red, 400, 5000, 0, 255);
  statusPacket.params[2] = map(green, 400, 5000, 0, 255);
  statusPacket.params[3] = map(blue, 400, 5000, 0, 255);
  
  turnOff();
  sendResponse(&statusPacket);
}

void sendMessage(const char *message)
{
  // Sends text messages back to the Pi. Useful
  // for debugging.
  
  TPacket messagePacket;
  messagePacket.packetType=PACKET_TYPE_MESSAGE;
  strncpy(messagePacket.data, message, MAX_STR_LEN);
  sendResponse(&messagePacket);
}

void sendBadPacket()
{
  // Tell the Pi that it sent us a packet with a bad
  // magic number.
  
  TPacket badPacket;
  badPacket.packetType = PACKET_TYPE_ERROR;
  badPacket.command = RESP_BAD_PACKET;
  sendResponse(&badPacket);
  
}

void sendBadChecksum()
{
  // Tell the Pi that it sent us a packet with a bad
  // checksum.
  
  TPacket badChecksum;
  badChecksum.packetType = PACKET_TYPE_ERROR;
  badChecksum.command = RESP_BAD_CHECKSUM;
  sendResponse(&badChecksum);  
}

void sendBadCommand()
{
  // Tell the Pi that we don't understand its
  // command sent to us.
  
  TPacket badCommand;
  badCommand.packetType=PACKET_TYPE_ERROR;
  badCommand.command=RESP_BAD_COMMAND;
  sendResponse(&badCommand);

}

void sendBadResponse()
{
  TPacket badResponse;
  badResponse.packetType = PACKET_TYPE_ERROR;
  badResponse.command = RESP_BAD_RESPONSE;
  sendResponse(&badResponse);
}

void sendOK()
{
  TPacket okPacket;
  okPacket.packetType = PACKET_TYPE_RESPONSE;
  okPacket.command = RESP_OK;
  sendResponse(&okPacket);  
}

void sendResponse(TPacket *packet)
{
  // Takes a packet, serializes it then sends it out
  // over the serial port.
  char buffer[PACKET_SIZE];
  int len;

  len = serialize(buffer, packet, sizeof(TPacket));
  writeSerial(buffer, len);
}


/*
 * Setup and start codes for external interrupts and 
 * pullup resistors.
 * 
 */
// Enable pull up resistors on pins 2 and 3
void enablePullups()
{
  // Use bare-metal to enable the pull-up resistors on pins
  // 2 and 3. These are pins PD2 and PD3 respectively.
  // We set bits 2 and 3 in DDRD to 0 to make them inputs.
  DDRD &= 0b11110011; 
  PORTD |= 0b00001100;
  
}

// Functions to be called by INT0 and INT1 ISRs.
void leftISR()
{
  if (dir == FORWARD) {
    leftForwardTicks++;
    forwardDist = (unsigned long) ((float) leftForwardTicks / COUNTS_PER_REV_LEFT * WHEEL_CIRC);
  } else if (dir == REVERSE) {
    leftReverseTicks++;
    reverseDist = (unsigned long) ((float) leftReverseTicks / COUNTS_PER_REV_LEFT * WHEEL_CIRC);
  } else if (dir == LEFT) {
    leftReverseTicksTurns++;
  } else if (dir == RIGHT) {
    leftForwardTicksTurns++;
  }
  //Serial.print(leftForwardTicks);
  //dbprint("%d", leftForwardTicks);
}

void rightISR()
{
  if (dir == FORWARD) {
    rightForwardTicks++;
    forwardDist = (unsigned long) ((float) rightForwardTicks / COUNTS_PER_REV_RIGHT * WHEEL_CIRC);
  } else if (dir == REVERSE) {
    rightReverseTicks++;
    reverseDist = (unsigned long) ((float) rightReverseTicks / COUNTS_PER_REV_RIGHT * WHEEL_CIRC);
  } else if (dir == LEFT) {
    rightForwardTicksTurns++;
  } else if (dir == RIGHT) {
    rightReverseTicksTurns++;
  }
}

// Set up the external interrupt pins INT0 and INT1
// for falling edge triggered. Use bare-metal.
void setupEINT()
{
  // Use bare-metal to configure pins 2 and 3 to be
  // falling edge triggered. Remember to enable
  // the INT0 and INT1 interrupts.
  EICRA |= 0b00001010;
  EIMSK |= 0b00000011;

}

// Implement the external interrupt ISRs below.
// INT0 ISR should call leftISR while INT1 ISR
// should call rightISR.

ISR(INT0_vect)
{
  leftISR();
}


ISR(INT1_vect)
{
  rightISR();
}

// Implement INT0 and INT1 ISRs above.

/*
 * Setup and start codes for serial communications
 * 
 */
// Set up the serial connection. For now we are using 
// Arduino Wiring, you will replace this later
// with bare-metal code.
void setupSerial()
{
  // To replace later with bare-metal.
  Serial.begin(9600);
}

// Start the serial connection. For now we are using
// Arduino wiring and this function is empty. We will
// replace this later with bare-metal code.

void startSerial()
{
  // Empty for now. To be replaced with bare-metal code
  // later on.
  
}

// Read the serial port. Returns the read character in
// ch if available. Also returns TRUE if ch is valid. 
// This will be replaced later with bare-metal code.

int readSerial(char *buffer)
{

  int count=0;

  while(Serial.available())
    buffer[count++] = Serial.read();

  return count;
}

// Write to the serial port. Replaced later with
// bare-metal code

void writeSerial(const char *buffer, int len)
{
  Serial.write(buffer, len);
}

/*
 * Alex's motor drivers.
 * 
 */

// Set up Alex's motors. Right now this is empty, but
// later you will replace it with code to set up the PWMs
// to drive the motors.
ISR(TIMER0_COMPA_vect)
{
  
}
ISR(TIMER0_COMPB_vect)
{
  
}
ISR(TIMER1_COMPB_vect)
{
  
}
ISR(TIMER2_COMPA_vect)
{
  
}
void setupMotors()
{
  DDRD |= 0b01100000;
  DDRB |= 0b00001100;
  TCNT0 = 0;
  TCCR0A |= 0b11110001;
  TIMSK0 |= 0b00000110;

  TCNT1 = 0;
  TIMSK1 |= 0b00000100;
  TCCR1A |= 0b00110001;
  
  TCNT2 = 0;
  TCCR2A |= 0b11000001;
  TIMSK2 |= 0b00000010;
  /* Our motor set up is:  
   *    A1IN - Pin 5, PD5, OC0B
   *    A2IN - Pin 6, PD6, OC0A
   *    B1IN - Pin 10, PB2, OC1B
   *    B2In - pIN 11, PB3, OC2A
   */
}

// Start the PWM for Alex's motors.
// We will implement this later. For now it is
// blank.
void startMotors()
{
  OCR0A = 0;
  OCR0B = 0;
  TCCR0B |= 0b00000001;

  OCR1B = 0;
  TCCR1B |= 0b00000001;

  OCR2A = 0;
  TCCR2B |= 0b00000001;
}

// Convert percentages to PWM values
int pwmVal(float speed)
{
  if(speed < 0.0)
    speed = 0;

  if(speed > 100.0)
    speed = 100.0;

  return (int) ((speed / 100.0) * 255.0);
}

// Move Alex forward "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// move forward at half speed.
// Specifying a distance of 0 means Alex will
// continue moving forward indefinitely.
void forward(float dist, float speed)
{
  clearCounters();
  dir = FORWARD;
  
  val = pwmVal(speed);
  power_l = val;
  power_r = val;

  enc_l_prev = leftForwardTicks;
  enc_r_prev = rightForwardTicks;

  num_rev = dist / WHEEL_CIRC;
  target_count_left = (num_rev * COUNTS_PER_REV_LEFT);// + leftForwardTicks;
  target_count_right = (num_rev * COUNTS_PER_REV_RIGHT);// + rightForwardTicks;
  commandTime = millis();
  //drive();
}

// Reverse Alex "dist" cm at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// reverse at half speed.
// Specifying a distance of 0 means Alex will
// continue reversing indefinitely.
void reverse(float dist, float speed)
{
  clearCounters();
  dir = REVERSE;
  val = pwmVal(speed);
  power_l = -val;
  power_r = -val;

  enc_l_prev = leftReverseTicks;
  enc_r_prev = rightReverseTicks;
  
  num_rev = dist / WHEEL_CIRC;
  target_count_left = (num_rev * COUNTS_PER_REV_LEFT); //+ leftReverseTicks;
  target_count_right = (num_rev * COUNTS_PER_REV_RIGHT); //+ rightReverseTicks;
  commandTime = millis();
  //drive();
//  OCR0A = 0;//LF
//  OCR1B = 0; //RF
//  OCR0B = val; //LR
//  OCR2A = val; //RR
}


unsigned long computeDeltaTicks(float ang, int counts)
{
  unsigned long ticks = (unsigned long) ((ang * alexCirc * counts) / (360.0 * WHEEL_CIRC));

  return ticks;
}

// Turn Alex left "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn left indefinitely.
void left(float ang, float speed)
{
  dir = LEFT;
  clearCounters();
  val = pwmVal(speed);
  power_l = -val;
  power_r = val;

  enc_l_prev = leftReverseTicks;
  enc_r_prev = rightReverseTicks;
  
  //num_rev = ang/360;
  target_count_left = computeDeltaTicks(ang, COUNTS_PER_REV_LEFT)/2;// + leftReverseTicksTurns;//(num_rev * COUNTS_PER_REV_LEFT) + leftReverseTicksTurns;
  target_count_right = computeDeltaTicks(ang, COUNTS_PER_REV_RIGHT)/2;// + rightForwardTicksTurns; //(num_rev * COUNTS_PER_REV_RIGHT) + rightForwardTicksTurns;

  commandTime = millis();
  //drive();
//  OCR0A = 0;//LF
//  OCR1B = val; //RF
//  OCR0B = val; //LR
//  OCR2A = 0; //RR
}

// Turn Alex right "ang" degrees at speed "speed".
// "speed" is expressed as a percentage. E.g. 50 is
// turn left at half speed.
// Specifying an angle of 0 degrees will cause Alex to
// turn right indefinitely.
void right(float ang, float speed)
{
  dir = RIGHT;
  clearCounters();
  val = pwmVal(speed);
  power_l = val;
  power_r = -val;

  enc_l_prev = leftReverseTicks;
  enc_r_prev = rightReverseTicks;
  
//  num_rev = (ang/360); //* WHEEL_CIRC;
  target_count_left = computeDeltaTicks(ang, COUNTS_PER_REV_LEFT)/2;// + leftForwardTicksTurns;//(num_rev * COUNTS_PER_REV_LEFT) + leftForwardTicksTurns;
  target_count_right = computeDeltaTicks(ang, COUNTS_PER_REV_RIGHT)/2;// + rightReverseTicksTurns;//(num_rev * COUNTS_PER_REV_RIGHT) + rightReverseTicksTurns;

  commandTime = millis();
  //drive();
//  OCR0A = val;//LF
//  OCR1B = 0; //RF
//  OCR0B = 0; //LR
//  OCR2A = val; //RR
}

// Alex. To replace with bare-metal code later.
void stop()
{
//  analogWrite(LF, 0);
//  analogWrite(LR, 0);
//  analogWrite(RF, 0);
//  analogWrite(RR, 0);
  dir = STOP;
  //clearCounters();
  OCR0A = 0;//LF
  OCR1B = 0; //RF
  OCR0B = 0; //LR
  OCR2A = 0; //RR
  putArduinoToIdle();
}

/*
 * Alex's setup and run codes
 * 
 */

// Clears all our counters
void clearCounters()
{
  leftForwardTicks=0;
  rightForwardTicks=0;
  leftReverseTicks=0;
  rightReverseTicks=0;
  leftForwardTicksTurns=0;
  rightForwardTicksTurns=0;
  leftReverseTicksTurns=0;
  rightReverseTicksTurns=0;
  leftRevs=0;
  rightRevs=0;
  forwardDist=0;
  reverseDist=0; 
}

// Clears one particular counter
void clearOneCounter(int which)
{
  clearCounters();
}
// Intialize Vincet's internal states

void initializeState()
{
  clearCounters();
}

void handleCommand(TPacket *command)
{
  switch(command->command)
  {
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_STOP:
        sendOK();
        stop();
      break;
    case COMMAND_FORWARD:
        sendOK();
        forward((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_REVERSE:
        sendOK();
        reverse((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_TURN_LEFT:
        sendOK();
        left((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_TURN_RIGHT:
        sendOK();
        right((float) command->params[0], (float) command->params[1]);
      break;

    case COMMAND_GET_STATS:
      sendStatus();
      break;

    case COMMAND_CLEAR_STATS:
      sendOK();
      clearOneCounter(command->params[0]);
      clearOneCounter(command->params[1]);
      clearOneCounter(command->params[2]);
      clearOneCounter(command->params[3]);
      clearOneCounter(command->params[4]);
      clearOneCounter(command->params[5]);
      clearOneCounter(command->params[6]);
      clearOneCounter(command->params[7]);
      clearOneCounter(command->params[8]);
      clearOneCounter(command->params[9]);
      break;
      
    case COMMAND_COLOUR:
      sendOK();
      sendColour(colour);
      break;

    default:
      sendBadCommand();
  }
}

void waitForHello()
{
  int exit=0;

  while(!exit)
  {
    TPacket hello;
    TResult result;
    
    do
    {
      result = readPacket(&hello);
    } while (result == PACKET_INCOMPLETE);

    if(result == PACKET_OK)
    {
      if(hello.packetType == PACKET_TYPE_HELLO)
      {
     

        sendOK();
        exit=1;
      }
      else
        sendBadResponse();
    }
    else
      if(result == PACKET_BAD)
      {
        sendBadPacket();
      }
      else
        if(result == PACKET_CHECKSUM_BAD)
          sendBadChecksum();
  } // !exit
}

void setup() {
  // put your setup code here, to run once:
  timeNow = millis();
  commandTime = millis();
  alexDiagonal = sqrt((ALEX_LENGTH * ALEX_LENGTH) + (ALEX_BREADTH * ALEX_BREADTH));
  alexCirc = PI * alexDiagonal;
 
  DDRB |= 0b01000010;
  DDRB &= 0b11101111;

  red  = 0;
  green = 0;
  blue = 0;
  // Initialize the ISL29125 with simple configuration so it starts sampling
  if (RGB_sensor.init())
  {
    //Serial.println("Sensor Initialization Successful\n\r");
  }

  strip.begin();
  strip.show();
  //set pin 8 as output
  DDRB |= 0b00000001;
  
  cli();
  setupEINT();
  setupSerial();
  startSerial();
  setupMotors();
  startMotors();
  enablePullups();
  initializeState();
  setupPowerSaving();
  sei();
}

void handlePacket(TPacket *packet)
{
  switch(packet->packetType)
  {
    case PACKET_TYPE_COMMAND:
      handleCommand(packet);
      break;

    case PACKET_TYPE_RESPONSE:
      break;

    case PACKET_TYPE_ERROR:
      break;

    case PACKET_TYPE_MESSAGE:
      break;

    case PACKET_TYPE_HELLO:
      break;
  }
}


void drive()
{
//  power_l = constrain(power_l, -255, 255);
//  power_r = constrain(power_r, -255, 255); 

  if (power_l < 0)
  {
    OCR0B = abs(power_l); 
    OCR0A = 0;   
  }
  else
  {
    OCR0A = power_l;
    OCR0B = 0;
  }

  if (power_r < 0)
  {
    OCR2A = abs(power_r);
    OCR1B = 0;
  }
  else
  {
    OCR1B = power_r;
    OCR2A = 0;
  }
}

void slower_l()
{
  if (power_l >= 0)
  {
    if (power_l - motor_offset >= 0)
    {
      power_l -= motor_offset;  
    }
    else
    {
      power_l = 0;
    }
  }

  else
  {
    if (power_l + motor_offset >= 0)
    {
      power_l = 0;
    }
    else
    {
      power_l += motor_offset;
    }
  }
}


void slower_r()
{
  if (power_r >= 0)
  {
    if (power_r - motor_offset >= 0)
    {
      power_r -= motor_offset;
      /*
      if (dir == FORWARD){
        power_r--;
      }
      */
    }
    else
    {
      power_r = 0;
    }
  }

  else
  {
    if (power_r + motor_offset >= 0)
    {
      power_r = 0;
    }
    else
    {
      power_r += motor_offset;
    }
  }
}

void faster_r()
{
  if (power_r >= 0)
  {
    if (power_r + motor_offset >= 255)
    {
      power_r = 255;
    }
    else
    {
      power_r += motor_offset;
      /*
      if (dir == FORWARD){
          power_r++;
      }
      */
    }
  }
  else
  {
    if (power_r - motor_offset <= -255)
    {
      power_r = -255;
    }
    else
    {
      power_r -= motor_offset;
    }
  }
}

void faster_l()
{
  if (power_l >= 0)
  {
    if (power_l + motor_offset >= 255)
    {
      power_l = 255;
    }
    else
    {
      power_l += motor_offset;
    }
  }
  else
  {
    if (power_l - motor_offset <= -255)
    {
      power_l = -255;
    }
    else
    {
      power_l -= motor_offset;
    }
  }
}

//void alarm() {
//  tone(BUZZER, 1000);
//}

//void offAlarm(){
//  noTone(BUZZER);
//}

void adjustMove()
{
  int left_dir_ticks;
  int right_dir_ticks;
  
  if (dir == FORWARD)
  {
    left_dir_ticks = leftForwardTicks;
    right_dir_ticks = rightForwardTicks;
  }

  else if (dir == REVERSE)
  {
    left_dir_ticks = leftReverseTicks;
    right_dir_ticks = rightReverseTicks;
  }
  else if (dir == LEFT)
  {
    left_dir_ticks = leftReverseTicksTurns;
    right_dir_ticks = rightForwardTicksTurns;
  }
  else if (dir == RIGHT)
  {
    left_dir_ticks = leftForwardTicksTurns;
    right_dir_ticks = rightReverseTicksTurns;
  }
    
    if ( (left_dir_ticks < target_count_left) && (right_dir_ticks < target_count_right) ) 
  {

    // Sample number of encoder ticks
    num_ticks_l = left_dir_ticks;
    num_ticks_r = right_dir_ticks;

    // Drive
    
//    drive();

    // Number of ticks counted since last time
    diff_l = num_ticks_l - enc_l_prev;
    diff_r = num_ticks_r - enc_r_prev;

    // Store current tick counter for next time
    enc_l_prev = num_ticks_l;
    enc_r_prev = num_ticks_r;

    // If left is faster, slow it down and speed up right
    //(float)diff_l / COUNTS_PER_REV_LEFT > ((float)diff_r / COUNTS_PER_REV_RIGHT)
    if ( diff_l > diff_r  && millis() - timeNow >= 0 ) //&& millis() - commandTime >= 500 
    {
//      power_l -= motor_offset;
//      power_r += motor_offset;i
        slower_l();
        faster_r();

      timeNow = millis();
    }

    // If right is faster, slow it down and speed up left
    //(float)diff_l / COUNTS_PER_REV_LEFT < ((float)diff_r / COUNTS_PER_REV_RIGHT) && millis() - timeNow >= 20 
    if ( diff_l < diff_r && millis() - timeNow >= 0 )//&& millis() - commandTime >= 500 
    {
//      power_l += motor_offset;
//      power_r -= motor_offset;
        faster_l();
        slower_r();
      timeNow = millis();
    }

    drive();
  }

  else
  {
    // Brake
    stop();
  }

  
}

void loop() {

// Uncomment the code below for Step 2 of Activity 3 in Week 8 Studio 2

 //forward(0, 100);

// Uncomment the code below for Week 9 Studio 2


 // put your main code here, to run repeatedly:
 
  TPacket recvPacket; // This holds commands from the Pi

  TResult result = readPacket(&recvPacket);

//   PORTB &= 0b11111101;
//    delayMicroseconds(5);
//    PORTB |= 0b00000010;
//    delayMicroseconds(10);
//    PORTB &= 0b11111101;
//
//    duration = pulseIn(12, HIGH);
//
//    cm = (duration/2) / 29.1;
//    inches = (duration/2) / 74;
//    
//    unsigned long change_cm = cm;
//
//    char buff[3];
//    buff[0] = '0';
//    buff[1] = '0';
//    buff[2] = '0';
//    
//    int digit;
//    char character;
//    int index = 2;
//    while (change_cm != 0 && index >= 0) {
//        digit = change_cm % 10;
//        character = digit + 48;
//        change_cm = change_cm / 10;
//        buff[index] = character;
//        index--;
//    }

  
  if(result == PACKET_OK)
  {
    handlePacket(&recvPacket);
    //putArduinoToIdle();//wake up
  }    
  else
  {
    if(result == PACKET_BAD)
    {
      sendBadPacket();
    }
    
    else if(result == PACKET_CHECKSUM_BAD)
    {
      sendBadChecksum();
    } 
  }

    if (dir == STOP)
    {
      stop();
    }
//    else if (cm < 6 && dir == FORWARD) {
//      if (!tooNear) {
//          //dbprint(buff);
//          sendMessage(buff);
//          tooNear = true; 
//      }
//      stop();
//    }
    else
    {
//      tooNear = false;
      adjustMove();
    }

}
