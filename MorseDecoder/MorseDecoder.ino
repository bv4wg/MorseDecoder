// MorseDetector implements a decoder for morse code.
//  Copyright (C) 2014 Nicola Cimmino
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see http://www.gnu.org/licenses/.
//
// I referred to https://github.com/jacobrosenthal/Goertzel for the Goertzel 
//  implementation.

#define LIN_OUT 1 // use the linear output function
#define FHT_N 256 // set to 256 point fht
#include <FHT.h>

// Pin powering the mic pre-amp
#define AMPLI_PWR_PIN A3

// Sampling rate in samples/s
#define SAMPLING_RATE 4000

// Length of one dot. Assumes constant WPM at the moment
#define DOT_LEN 2

#define THRESHOLD 2000

// Precalculated coefficient for our goertzel filter.
float goetzelCoeff=0;

// Length of the goertzel filter
#define GOERTZEL_N 64

// The samples taken from the A/D. The A/D results are
// in fact 10-bits but we use a byte to save memory
// as we never reach values > 256.
byte sampledData[GOERTZEL_N];

enum statuses
{
    none =0,
    dot,
    dash,
    intersymbol,
    interchar
};

statuses currentStatus=none;

int statusCounter=0;

char* lookupString = ".EISH54V.3UF....2ARL.....WP..J.1TNDB6.X..KC..Y..MGZ7.Q..O.8..90..";
byte currentRangeStart = 0;
byte currentRangeEnd = 65;
char currentAssumedChar='\0';

void setup()
{
  Serial.begin(115200); 
  
  // Set ADC prescaler to 16
  _SFR_BYTE(ADCSRA) |=  _BV(ADPS2); // Set ADPS2
  _SFR_BYTE(ADCSRA) &= ~_BV(ADPS1); // Clear ADPS1
  _SFR_BYTE(ADCSRA) &= ~_BV(ADPS0); // Clear ADPS0
  
  // This sets the ADC analog reference to internally
  //  generated 1.1V to increase A/D resolution.
  analogReference(INTERNAL);

  // Power up the amplifier.
  pinMode(AMPLI_PWR_PIN, OUTPUT);
  digitalWrite(AMPLI_PWR_PIN,HIGH);
  
  float omega = (2.0 * PI * 700) / SAMPLING_RATE;
  goetzelCoeff = 2.0 * cos(omega);
}

void loop()
{
  // Sample at 4KHz 
  for (int ix = 0; ix < GOERTZEL_N; ix++)
  {
    sampledData[ix] = analogRead(A0)&0xFF; // 17uS
    delayMicroseconds(233); // total 250uS -> 4KHz
  } 
 
  // Apply goertzel filter and get amplitude of  
  float Q2 = 0;
  float Q1 = 0;
  for (int ix = 0; ix < GOERTZEL_N; ix++)
  {
      float Q0 = goetzelCoeff * Q1 - Q2 + (float) (sampledData[ix]);
      Q2 = Q1;
      Q1 = Q0;  
  }
  
  int magnitude = sqrt(Q1*Q1 + Q2*Q2 - goetzelCoeff*Q1*Q2);
  
  if(currentStatus==none && magnitude>THRESHOLD)
  {
    currentStatus=dot;
    statusCounter=0;  
  }
  else if(currentStatus==dot && magnitude>THRESHOLD && statusCounter>DOT_LEN*2)
  {
    currentStatus=dash;
  }
  else if(currentStatus==intersymbol && magnitude<THRESHOLD && statusCounter>DOT_LEN*3)
  {
    currentStatus=interchar;
  }
  else if(currentStatus!=none && currentStatus<intersymbol && magnitude<THRESHOLD)
  {
    //Serial.print((currentStatus==dot)?".":"-");
    currentAssumedChar=lookup((currentStatus==dot)?'.':'-');
    currentStatus=intersymbol;
    statusCounter=0;
    //currentStatus=none;
  }
  else if(currentStatus>dash && magnitude>THRESHOLD)
  {
    //Serial.print((currentStatus==intersymbol)?"":" ");
    if(currentStatus==interchar)
    {
      Serial.print(currentAssumedChar);
      lookup('\0');
    }
    currentStatus=none;
  }
  
  statusCounter++;
}


char lookup(char currentMark)
{
    byte halfRangeLen = floor((currentRangeEnd - currentRangeStart) / 2.0f);
    if (currentMark == '.')
    {                
        currentRangeStart++;
        currentRangeEnd = currentRangeStart + halfRangeLen;
    }
    else if (currentMark == '-')
    {
        currentRangeStart += halfRangeLen;
    }
    else if (currentMark == '\0')
    {
        currentRangeStart = 0;
        currentRangeEnd = 65;
        return '\0';
    }
    return lookupString[currentRangeStart];
}

