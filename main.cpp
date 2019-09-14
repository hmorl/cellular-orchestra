/*
Cellular Orchestra
Harry Morley
September 2019

--ARDUINO--
 
Each creature runs this code, only the ID number changes.
The creatures are connected in a ring network via serial, in order for them to generate
a 1D automaton that wraps around.

Uses the Mozzi library by Tim Barrass for audio synthesis,
and a modified version of the LED Matrix library by Daniel Eichhorn.
*/

#define ID 1 //set to 0, 1, 2 or 3 depending on which creature
#define NUM_NODES 4 //4 creatures
#define WIDTH 4 //width of 1 automaton in bytes (4 * 8bits = 32 cells wide)
#define HEIGHT 8 //in pixels (8)
#define CONTROL_RATE 64 // mozzi control rate 64
#define CS_PIN 10 // (10)

//INCLUDES-------------------
#include "LedMatrix.h"
#include <MozziGuts.h>
#include <mozzi_analog.h>
#include <Oscil.h>
#include <tables/cos2048_int8.h> // for filter modulation
#include <tables/saw8192_int8.h>
#include <tables/brownnoise8192_int8.h> // noise table
#include <Ead.h> // exponential attack decay
#include <LowPassFilter.h>
#include <mozzi_rand.h>
#include <EventDelay.h>

//musical parameters
uint8_t population; //total population
bool isSounding;
int8_t filterModPh;

//musical objects
Oscil <SAW8192_NUM_CELLS, AUDIO_RATE> aSaw(SAW8192_DATA);
Oscil<COS2048_NUM_CELLS, CONTROL_RATE> kFilterMod(COS2048_DATA);
Oscil<BROWNNOISE8192_NUM_CELLS, AUDIO_RATE> aNoise(BROWNNOISE8192_DATA);
Oscil<COS2048_NUM_CELLS, AUDIO_RATE> toneSine(COS2048_DATA);
LowPassFilter lpf;

//1D ruleset. 30 = 00011110
const uint8_t RULE = 30; //a byte

//stores HEIGHT number of 1D histories
uint8_t state[WIDTH][HEIGHT];
uint8_t currentRow = 0;
uint16_t generation = 0;
uint16_t prevTri = 0;
uint8_t offset;

//tracks on/off state
bool isDisplaying;
bool wasDisplaying;
//chance the creature will go to sleep after 30 generations/seconds
uint8_t sleepChance;

//neighbouring left/right hand side bit
uint8_t nLeftBit;
uint8_t nRightBit;
bool leftReceived;
bool rightReceived;

bool sendingReadyMsg;
bool sentFirst;

EventDelay evDelay;
EventDelay glitchDelay;

uint8_t threegain;

//envelope for triangle trigger
Ead triEnvelope(CONTROL_RATE);
int triEnvGain = 0;
bool triTrig;

//LED matrix object
LedMatrix ledMatrix = LedMatrix(4, CS_PIN);

// mod that works with negative numbers, e.g. mod(-1, 3) = 2
int mod(int x, int n) {
  return (x % n + n) % n;
}

void display() {
  ledMatrix.clear();
  population = 0;

  //set intensity of LED matrix only when its state changes
  //dim the display when it is not sounding
  if(isDisplaying != wasDisplaying){
    if(isDisplaying) ledMatrix.setIntensity(1);
    else ledMatrix.setIntensity(0);
    wasDisplaying = isDisplaying;
  }

  for(int j = 7; j >= 0; j--) { //j is y
    for(int i = 0; i < WIDTH; i++) { //i is bytes left to right
      for(int b = 7; b >= 0; b--) { //b is bits left to right
        int y = j - currentRow;
        if (y <= 0) y = 8+y;
        int x = (7-b) + (i * 8);
        bool isOn = bitRead(state[i][j], b);
        if(isOn){
          ledMatrix.setPixel(x, y-1);
          population++; //count total population
        }
      }
    }
  }
  ledMatrix.commit();
}

void sendLeftRight() {
  //send leftmost and rightmost cells on the network
  uint8_t myLeftBit = bitRead(state[0][currentRow], 7);
  uint8_t myRightBit = bitRead(state[WIDTH-1][currentRow], 0);
  uint8_t msgToSend = 0;

  //encode ID, the left bit and the right bit into a byte (saves sending multiple messages yo)
  msgToSend = (ID << 2) + (myLeftBit << 1) + (myRightBit);
  Serial.write(msgToSend);
}

void sendReadyMsg(){
  //carried out by creature 0 only
  uint8_t msgToSend = 129;
  Serial.write(msgToSend);
}

void detectTri() {
  //detects a large "gap" (i.e. a long string of 0s), meaning 
  //there is a triangle. if one has been detected, stop checking for 8 generations
  //(stops it going crazy)

  if ((generation - prevTri) > 8) {
    int streak = 0;
    int maxStreak = 0;
    for(int j = 7; j >= 0; j--) { //j is y
      for(int i = 0; i < WIDTH; i++) { //i is bytes left to right
        for(int b = 7; b >= 0; b--) { //b is bits left to right
          if (bitRead(state[i][j], b) == 0) {
            streak++;
          } else {
            if (streak > maxStreak) {
              maxStreak = streak;
            }
            streak = 0;
          }
        }
      }
    }

    if (maxStreak > 6) { //if streak of 0s is greater than 6 (big triangle).
      //trigger the tone
      prevTri = generation;
      uint16_t duration = map(maxStreak, 7, 14, 1000, 6000);
      uint16_t attack = 10; // +5 so the internal step size is more likely to be >0
      uint16_t decay = duration - attack;
      //start the "bloop" envelope
      triEnvelope.start(attack, decay);

      if(ID == 1 || ID == 2) {
        int pitch;
        int ran;
        ran = rand(6);
        switch(ran){
          case 0:
            pitch = 220;
            break;
          case 1:
            pitch = 247.5;
            break;
          case 2:
            pitch = 293;
            break;
          case 3:
            pitch = 366;
            break;
          case 4:
            pitch = 413;
            break;
          case 5:
            pitch = 110;
            break;
        }
        //random pitch
        if(ID == 1){
          toneSine.setFreq(pitch);
        }else {
          aSaw.setFreq(pitch/2);
        }
      }
    }
  }
}

void sonify() {
    if(ID == 0){ //set creature 0 popping frequency
      float freq = map(population, 100, 180, 4, 18);
      kFilterMod.setFreq(population / 2000.f);
      aSaw.setFreq(freq);
    }
    detectTri();
}

void setup() {
  Serial.begin(57600);
  randomSeed(analogRead(0));
  //give each creature a generation offset so they don't all change state
  //at the same time
  offset = random(256);
  
  //initialise LED matix
  ledMatrix.init();
  ledMatrix.setIntensity(1);
  ledMatrix.clear();
  ledMatrix.commit();

  isDisplaying = true;
  sentFirst = false;

  //initials all cells to 0
  for (int i = 0; i < WIDTH; i++) {
    for(int j = 0; j < HEIGHT; j++) {
      state[i][j] = 0;
    }
  }

  //set initial state (row 0) to random
  for (int i = 0; i < WIDTH; i++) {
    state[i][0] = random(256);
  }

  display();

  kFilterMod.setFreq(.03f);

  //1 and 2 use noise, so initialise for them only.
  if(ID == 1 || ID == 2){
    aNoise.setFreq((float)AUDIO_RATE/BROWNNOISE8192_SAMPLERATE);
  }

  randSeed();
  lpf.setResonance(230);
  delay(1000);
  startMozzi(CONTROL_RATE);
  switch(ID){ //every cell get 
    case 0:
      sleepChance = 30;
      toneSine.setFreq(200);
      break;
    case 1:
      sleepChance = 40;
      toneSine.setFreq(220);
      break;
    case 2:
      sleepChance = 50;
      toneSine.setFreq(300);
      break;
    case 3:
      sleepChance = 60;
      toneSine.setFreq(297);
      break;
  }

  aSaw.setFreq(10);

  if(ID==0){
    sendingReadyMsg = true;
    sonify();
    evDelay.set(1000); // 1 second between evolutions
  }else if (ID ==2) {
    triEnvelope.start();
  }
}

void evolve() {
  for (int i = 0; i < WIDTH; i++) {
    //for each bit of the byte (8), from MSB to LSB
    for (int b = 7; b >= 0; b--) {
      uint8_t left, self, right;

      if(b==7 && i==0) {
        left = nLeftBit;
      } else if (b==7 && i!=0) {
        left = bitRead(state[mod(i - 1, WIDTH)][currentRow], 0);
      } else {
        left = bitRead(state[i][currentRow], b + 1);
      }
      //uncomment when running 1 by itself-----
      // if (b==7) {
      //   left = bitRead(state[mod(i - 1, WIDTH)][currentRow], 0);
      // } else {
      //   left = bitRead(state[i][currentRow], b + 1);
      // }

      //check self
      self = bitRead(state[i][currentRow], b);

      if(b==0 && i==(WIDTH-1)) {
        right = nRightBit;
      } else if (b==0 && i!=(WIDTH-1)) {
        right = bitRead(state[(i + 1) % WIDTH][currentRow], 7);
      } else {
        right = bitRead(state[i][currentRow], b - 1);
      }

      //uncomment when running 1 by itself-----
      // if (b==0) {
      //   right = bitRead(state[mod(i + 1, WIDTH)][currentRow], 7);
      // } else {
      //   right = bitRead(state[i][currentRow], b - 1);
      // }
      uint8_t ruleIdx = (4 * left) + (2 * self) + right;
      int n = bitRead(RULE, ruleIdx);
      bitWrite(state[i][(generation+1) % 8], b, n);
    }
  }

  if((generation+offset) % 30 == 0){
    int r = random(100);
    if(r < sleepChance && isDisplaying){
      isDisplaying = false;
    }else if(r < 90 && !isDisplaying){
      isDisplaying = true;
    }
  }
}

void updateSerial() {
  //ring network implementation
  while(Serial.available() > 0) {
    uint8_t msg = Serial.read();
    if(msg == 129){ //if special ready message received
      if(ID==0) {
        // if ID==0 and ready msg received, it means the message has come full circle,
        // so the system is ready.
        sendingReadyMsg = false;
        sendLeftRight(); //
      }else { //pass on the ready message to the next node in the network
        Serial.write(msg);
      }
    } else {
      uint8_t incomingID = msg >> 2; //extract the sender ID from the byte
      bool validID = (incomingID >= 0) && (incomingID <= 3); //validate (can only be 0, 1, 2 or 3)

      if(generation == 0 && !sentFirst && validID){ //if just started and haven't sent the initial data, send it
        sendLeftRight();
        sentFirst = true;
      }

      //gets the neighbouring left and right bits in order for the automaton to wrap around
      if(validID && (incomingID == mod((ID-1),NUM_NODES)) && (incomingID == mod((ID+1),NUM_NODES))){
        //for group sizes of two only
        nLeftBit = bitRead(msg, 0);
        leftReceived = true;
        nRightBit = bitRead(msg, 1);
        rightReceived = true;
        //pass on the message
        Serial.write(msg);
      } else if(validID && (incomingID == mod((ID-1),NUM_NODES))){
        //then left neighbour, so extract the sender's right bit
        nLeftBit = bitRead(msg, 0);
        leftReceived = true;
        //pass on the message
        Serial.write(msg);
      } else if (validID && (incomingID == mod((ID+1), NUM_NODES))){
        //then right neighbour, so extract the sender's left bit
        nRightBit = bitRead(msg, 1);
        rightReceived = true;
        //pass on the message
        Serial.write(msg);
      } else if (incomingID != ID) {
        //pass on message to others who may need it
        Serial.write(msg);
      } //else{} if message has come full circle it will be ignored (otherwise infinite serial loop)
    }
  }
}

void cycle() {
  evolve();
  generation++;
  currentRow = generation % 8;
  display();
  sonify();
  sendLeftRight();
  leftReceived = false;
  rightReceived = false;
}

void updateControl() {
  updateSerial();

  if(ID == 1 || ID == 2){ //stops the random table from looping
    aNoise.setPhase(rand((unsigned int)BROWNNOISE8192_NUM_CELLS));
  }

  if(sendingReadyMsg && evDelay.ready()) {
    sendReadyMsg();
    evDelay.start();
  }else if(!sendingReadyMsg){
    if(ID == 0 && evDelay.ready()){
      if(leftReceived && rightReceived) {
        cycle();
        evDelay.start();
      }
    }else if(ID != 0){
      if(leftReceived && rightReceived) {
        cycle();
      }
    }
  }
    //update filter modulator and set filter cutoff  
    filterModPh = kFilterMod.next();
    byte cutoff_freq1 = 100 + filterModPh/2; // 100 ± 63
    lpf.setCutoffFreq(cutoff_freq1);

  //for ID 3, set a glitchy sine wave rhythm by linking the population to the random
  if(ID == 3){
    if(rand(population) > 50){
      threegain = 1;
    } else {
      threegain = 0;
    }
  }

  //update the triangle trigger envelope
  triEnvGain = (int) triEnvelope.next();
}

//audio callback function
int updateAudio() {
  long mix = 0;

  if(isDisplaying){ //only make sound if awake
    if(ID == 0){
      mix = ((toneSine.next()*triEnvGain)>>8) + lpf.next(aSaw.next()); //popping sound and bloops
      mix = mix >> 2;
    }else if(ID ==1){
      mix = ((toneSine.next()*triEnvGain)>>8) + lpf.next(aNoise.next()); //noise and bloops
      mix = mix >> 2;
    }else if(ID == 2){
      mix = triEnvGain*(lpf.next(aSaw.next()) + lpf.next(aNoise.next())/2); //enveloped noise
      mix = mix >> 10;
    }else if(ID == 3){
      mix = threegain * toneSine.next() + population; //glitchy sine wave, affected by the total population
      mix = mix >> 1;
    }
  }

  return (int) mix;
}

void loop() {
  audioHook();
}
