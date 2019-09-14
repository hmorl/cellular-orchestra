/*Simplified version of library based on MAX7219LedMatrix by Daniel Eichhorn
Used to just set individual pixels, adapted to suit the wiring of my displays */
// also added digitalwritefast to save processor cycles

#include <SPI.h>
#include "LedMatrix.h"

LedMatrix::LedMatrix(byte numberOfDevices, byte slaveSelectPin) {
    myNumberOfDevices = numberOfDevices;
    mySlaveSelectPin = slaveSelectPin;
    cols = new byte[numberOfDevices * 8];
}

/* numberOfDisplays: number of connected devices
slaveSelectPin: CS (or SS) pin */
void LedMatrix::init() {
    pinMode(mySlaveSelectPin, OUTPUT);
    SPI.begin();
    SPI.setDataMode(SPI_MODE0);
    // this was causing popping issues with audio:
        // SPI.setClockDivider(SPI_CLOCK_DIV128);
    for(byte device = 0; device < myNumberOfDevices; device++) {
        sendByte(device, MAX7219_REG_SCANLIMIT, 7);   // show all 8 digits
        sendByte(device, MAX7219_REG_DECODEMODE, 0);  // using an led matrix (not digits)
        sendByte(device, MAX7219_REG_DISPLAYTEST, 0); // no display test
        sendByte(device, MAX7219_REG_INTENSITY, 0);   // character intensity: range: 0 to 15
        sendByte(device, MAX7219_REG_SHUTDOWN, 1);    // not in shutdown mode (ie. start it up)
    }
}

void LedMatrix::digitalWriteFast(uint8_t pin, uint8_t x) {
  if (pin / 8) { // pin >= 8
    PORTB ^= (-x ^ PORTB) & (1 << (pin % 8));
  }
  else {
    PORTD ^= (-x ^ PORTD) & (1 << (pin % 8));
  }
}

void LedMatrix::sendByte (const byte device, const byte reg, const byte data) {
    int offset = device;
    int maxbytes = myNumberOfDevices;

    for(int i=0;i<maxbytes;i++) {
        spidata[i] = (byte)0;
        spiregister[i] = (byte)0;
    }

    spiregister[offset] = reg;
    spidata[offset] = data;

    digitalWriteFast(mySlaveSelectPin,LOW);
    // shift out the data
    for(int i=myNumberOfDevices-1;i>=0;i--) {
        SPI.transfer(spiregister[i]);
        SPI.transfer(spidata[i]);
    }
    digitalWriteFast(mySlaveSelectPin, HIGH);
}


void LedMatrix::setIntensity(const byte intensity) {
    for (int device = 0; device < myNumberOfDevices; device++) {
        sendByte(device, MAX7219_REG_INTENSITY, intensity);
    } 
}

void LedMatrix::clear() {
    for (byte col = 0; col < myNumberOfDevices * 8; col++) {
        cols[col] = 0;
    } 
}

void LedMatrix::commit() {
    for (byte col = 0; col < myNumberOfDevices * 8; col++) {
        sendByte(col / 8, col % 8 + 1, cols[col]);
    }
}

void LedMatrix::setPixel(byte x, byte y) {
    //rotate grid to match the wiring
    bitWrite(cols[(7-y) + (x/8)*8], x%8, true);
}