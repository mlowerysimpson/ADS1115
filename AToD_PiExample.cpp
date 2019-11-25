#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
#include "AToD.h"

using namespace std;

int main (void)
{
  const int NUM_READINGS = 100;  
  char *i2c_filename = (char*)"/dev/i2c-1";
  const unsigned char A_TO_D_ADDRESS = 0x48;
      
  AToD atod(i2c_filename, A_TO_D_ADDRESS);//constructor
  for (int i=0;i<NUM_READINGS;i++) 
  {
	  double channel_voltages[4] = {0.0,0.0,0.0,0.0};
      for (int j=0;j<4;j++) {
	    atod.GetMeasurement(j+1,0,1.0,channel_voltages[j]);
      }
	  printf("Voltages: %.3f, %.3f, %.3f, %.3f\n",channel_voltages[0],channel_voltages[1],channel_voltages[2],channel_voltages[3]);
  }
  return 0 ;
}
