/*Simplified version of library based on MAX7219LedMatrix by Daniel Eichhorn
Used to just set individual pixels, adapted to suit the wiring of my displays */
// also added digitalwritefast to save processor cycles


#include <Arduino.h>

// max7219 registers
#define MAX7219_REG_NOOP         0x0
#define MAX7219_REG_DIGIT0       0x1
#define MAX7219_REG_DIGIT1       0x2
#define MAX7219_REG_DIGIT2       0x3
#define MAX7219_REG_DIGIT3       0x4
#define MAX7219_REG_DIGIT4       0x5
#define MAX7219_REG_DIGIT5       0x6
#define MAX7219_REG_DIGIT6       0x7
#define MAX7219_REG_DIGIT7       0x8
#define MAX7219_REG_DECODEMODE   0x9
#define MAX7219_REG_INTENSITY    0xA
#define MAX7219_REG_SCANLIMIT    0xB
#define MAX7219_REG_SHUTDOWN     0xC
#define MAX7219_REG_DISPLAYTEST  0xF

class LedMatrix {
    
public:
    /* numberOfDisplays: number of connected devices
       slaveSelectPin: CS (or SS) pin */
    LedMatrix(byte numberOfDisplays, byte slaveSelectPin);
    
    // Initializes SPI interface
    void init();

    void digitalWriteFast(uint8_t pin, uint8_t x);

    // Sets intensity
    void setIntensity(byte intensity);

    // Send a byte to specific device
    void sendByte (const byte device, const byte reg, const byte data);


    //Set individual pixel in buffer to on
    void setPixel(byte x, byte y);
    
    //Clear the buffer
    void clear();
    
    //Writes buffer to the led matrix
    void commit();
    
private:
    byte* cols;
    byte spiregister[8];
    byte spidata[8];
    byte myNumberOfDevices = 0;
    byte mySlaveSelectPin = 0;
};