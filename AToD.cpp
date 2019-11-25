//AToD.cpp  Implementation file for AToD class
#include <unistd.h>				//Needed for I2C port
#include <fcntl.h>				//Needed for I2C port
#include <sys/ioctl.h>			//Needed for I2C port
#include <linux/i2c-dev.h>		//Needed for I2C port
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <wiringPi.h>
#include <math.h>
#include "AToD.h"


/**
 * @brief Construct a new AToD::AToD object for collecting data from the ADS1115 chip
 * 
 * @param i2c_filename the name of the i2c port to use, ex: "/dev/i2c-1"
 * @param ucAddress the 7-bit i2c address to use for the ADS1115.
 */
AToD::AToD(char *i2c_filename, unsigned char ucAddress) {
	m_ucAddress = ucAddress;
	m_nPGAVal = 1;
	m_nCurrentChannel = 1;
	m_bOpenedI2C_OK = true;
	m_bInitialized = false;
	m_file_i2c = open(i2c_filename, O_RDWR);
	if (m_file_i2c<0) {
		//error opening I2C
		//test
		//char sMsg[256];
		//sprintf(sMsg,"Error: %s opening I2C.\n",strerror(errno));
		//m_pShipLog->LogEntry(sMsg,true);
		//end test
		m_bOpenedI2C_OK=false;
	}
}

AToD::~AToD() {
	

}

bool AToD::InitializeAtoD(int nPGAVal, int nChannel) {//initialize ADS1115 A to D converter for 16-bit continuous mode operation
	//nPGAVal = programmable gain amplifier gain, can be 0, 1, 2, 4, 8, or 16 corresponding to gain levels of 2/3, 1, 2, 4, 8, or 16
	//nChannel = the channel used for A to D measurements, 1 to 4
	unsigned char out_buf[1];
	unsigned char config_msb=0x81, config_lsb=0xE3;//most significant and least significant bytes of 16-bit configuration register (MSB is set for single-shot mode while initializing a conversion, LSB is set for 860 samples per sec, and comparator disabled)
	unsigned char sendBytes[3];//bytes to send to ADS1115
	sendBytes[0] = 0x01;//corresponds to the configuration register
	if (!m_bOpenedI2C_OK) {
	    printf("Error, was unable to open I2C port.\n");
		return false;
	}
	if (nChannel==1) {
		config_msb|=0x40;
	}
	else if (nChannel==2) {
		config_msb|=0x50;
	}
	else if (nChannel==3) {
		config_msb|=0x60;
	}
	else if (nChannel==4) {
		config_msb|=0x70;
	}
	else {//invalid channel
		printf("Invalid channel: %d. Must be between 1 and 4.\n",nChannel);
		m_bInitialized = false;
		return false;
	}
	if (nPGAVal==1) {
		config_msb|=0x02;
	}
	else if (nPGAVal==2) {
		config_msb|=0x04;
	}
	else if (nPGAVal==4) {
		config_msb|=0x06;
	}
	else if (nPGAVal==8) {
		config_msb|=0x08;
	}
	else if (nPGAVal==16) {
		config_msb|=0x0a;
	}
	//else assume PGA of 2/3
	sendBytes[1] = config_msb;
	sendBytes[2] = config_lsb;

	if (!m_bOpenedI2C_OK) return false;

	if (ioctl(m_file_i2c, I2C_SLAVE, m_ucAddress)<0) {
		printf("Failed (error = %s) to acquire bus access and/or talk to slave A to D chip at %d.\n",
			strerror(errno),(int)m_ucAddress);
		m_bInitialized = false;
		return false;
	}	
	if (write(m_file_i2c, sendBytes, 3)!=3) {
		//error, I2C transaction failed
		printf("Failed to write the configuration bytes (error = %s) to the I2C bus.\n",
			strerror(errno));
		m_bInitialized = false;
		return false;
	}
	//pthread_mutex_unlock(m_i2c_mutex); --> need to leave mutex locked and then unlock it later in the calling function
	m_nPGAVal = nPGAVal;
	m_nCurrentChannel = nChannel;
	m_bInitialized = true;
	return true;
}

//GetMeasurement: Gets an A To D measurement on a particular channel
//nChannel: the A to D channel (1 to 4) that we want the measuremet from
//nPGAGain: the gain of the "pre-gain-amplifier", must be 0, 1, 2, 4, 8, or 16
//dMeasurementGain: the gain value to multiply by the returned voltage
//dResult: the result of the measurement (voltage from A to D, multiplied by dMeasurementGain)
bool AToD::GetMeasurement(int nChannel, int nPGAGain, double dMeasurementGain, double &dResult) {
	const int MAX_TRIES = 100;//maximum number of times to try getting measurement
	unsigned char inBuf[2];//input buffer needs to be large enough to read 2 data bytes
	unsigned char sendByte[1];//single byte to send to control whether to read the conversion register or the configuration register
	char sMsg[256];
	dResult = 0.0;
	if (!m_bOpenedI2C_OK) {
		return false;
	}
	//if (nChannel!=m_nCurrentChannel||nPGAGain!=m_nPGAVal) {
	//need to re-configure A to D
	
	double dSumCountVals = 0.0;//the summation of all the A to D count values (divided later by ATOD_NUM_TO_AVG to get average value)
	
	for (int i=0;i<ATOD_NUM_TO_AVG;i++) {
		bool bDataReady = false;
		if (!InitializeAtoD(nPGAGain, nChannel)) {
			printf("Error trying to configure A to D.\n");
			return false;
		}
        int nTryNumber=0;
		while (nTryNumber<MAX_TRIES&&!bDataReady) {
			//read in configuration bytes
			if (read(m_file_i2c,inBuf,2)!=2) {
				//ERROR HANDLING: i2c transaction failed
				printf("Failed to read the configuration bytes from the I2C bus.\n");
				return false;
			}
			if ((inBuf[0]&0x80)>0) {
				//data is now ready (no longer performing a conversion)
				bDataReady=true;
				break;
			}
            nTryNumber++;
		}
		if (!bDataReady) {
			printf("Timeout waiting for ADC data to be ready.\n");
			return false;
		}
		//read in conversion register
		sendByte[0] = 0x00;//code for configuration register
		if (write(m_file_i2c, sendByte, 1)!=1) {
			//error, I2C transaction failed
			printf("Failed to write to the I2C bus for conversion register (error = %s).\n", strerror(errno));
			m_bInitialized = false;
			return false;
		}
		//read in conversion register
		if (read(m_file_i2c,inBuf,2)!=2) {
			//ERROR HANDLING: i2c transaction failed
			printf("Failed to read the conversion bytes from the I2C bus.\n");
			return false;
		}
		int nCountVal = (inBuf[0]<<8) + inBuf[1];
		bool bNeg = false;
		if ((inBuf[0]&0x80)>0) {
			bNeg = true;
		}
		if (bNeg) {
			nCountVal = - (65536 - nCountVal);
		}
		dSumCountVals+=nCountVal;			
	}
    double dVRef = VREF;
    if (nPGAGain==0) {
        dVRef*=1.5;
    }
    else if (nPGAGain>1) {
        dVRef*=(1.0 / nPGAGain);
    }
	dResult = dMeasurementGain *  dVRef * dSumCountVals / ATOD_NUM_TO_AVG / MAX_COUNTS;
	return true;
}

