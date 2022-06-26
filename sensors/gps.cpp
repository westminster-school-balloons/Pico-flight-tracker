#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#include "../misc.h"
#include "../main.h"
#include "gps.h"

#define LANDING_ALTITUDE    100

int stateChecksumOK(const char *Buffer, int Count)
{
  unsigned char XOR, i, c;

  XOR = 0;
  for (i = 1; i < (Count-4); i++)
  {
    c = Buffer[i];
    XOR ^= c;
  }

  return (Buffer[Count-4] == '*') && (Buffer[Count-3] == Hex(XOR >> 4)) && (Buffer[Count-2] == Hex(XOR & 15));
}

void FixUBXChecksum(unsigned char *Message, int Length)
{ 
  int i;
  unsigned char CK_A, CK_B;
  
  CK_A = 0;
  CK_B = 0;

  for (i=2; i<(Length-2); i++)
  {
    CK_A = CK_A + Message[i];
    CK_B = CK_B + CK_A;
  }
  
  Message[Length-2] = CK_A;
  Message[Length-1] = CK_B;
}


void SendUBX(unsigned char *msg, int len)
{
	int i;
	
	for (i=0; i<len; i++)
	{
		uart_putc(uart1, msg[i]);
	}
}

void SetFlightMode(uint8_t NewMode)
{
	// Send navigation configuration command
	unsigned char setNav[] = {0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC};

	setNav[8] = NewMode;

	FixUBXChecksum(setNav, sizeof(setNav));

	SendUBX(setNav, sizeof(setNav));

	printf ("Setting state flight mode %d\n", NewMode);
}

float FixPosition(float Position)
{
	float Minutes, Seconds;
	
	Position = Position / 100;
	
	Minutes = trunc(Position);
	Seconds = fmod(Position, 1);

	return Minutes + Seconds * 5 / 3;
}

void ProcessLine(struct STATE *state, unsigned char *b, int Count)
{
    float utc_time, latitude, longitude, hdop, altitude, speed, course;
	int lock, satellites, date;
	char active, ns, ew, units, speedstring[16], coursestring[16];
	const char *Buffer = reinterpret_cast<const char*>(b);
    if (stateChecksumOK(Buffer, Count))
	{
		
		satellites = 0;
		if (strncmp(Buffer+3, "GGA", 3) == 0)
		{
			if (sscanf(Buffer+7, "%f,%f,%c,%f,%c,%d,%d,%f,%f,%c", &utc_time, &latitude, &ns, &longitude, &ew, &lock, &satellites, &hdop, &altitude, &units) >= 1)
			{	
				// $GPGGA,124943.00,5157.01557,N,00232.66381,W,1,09,1.01,149.3,M,48.6,M,,*42
				state->Time = utc_time;
				state->Hours = state->Time / 10000;
				state->Minutes = (state->Time / 100) % 100;
				state->Seconds = state->Time % 100;
				state->SecondsInDay = state->Hours * 3600 + state->Minutes * 60 + state->Seconds;					
				if (state->UseHostPosition)
				{
					state->UseHostPosition--;
				}
				else if (satellites >= 4)
				{
					state->Latitude = FixPosition(latitude);
					if (ns == 'S') state->Latitude = -state->Latitude;
					state->Longitude = FixPosition(longitude);
					if (ew == 'W') state->Longitude = -state->Longitude;
					state->Altitude = altitude;
				}

				state->Satellites = satellites;
				
				
				if (state->Altitude > state->MaximumAltitude)
				{
					state->MaximumAltitude = state->Altitude;
				}

				if ((state->Altitude < state->MinimumAltitude) || (state->MinimumAltitude == 0))
				{
					state->MinimumAltitude = state->Altitude;           
				}

				// Launched?
				if ((state->AscentRate >= 1.0) && (state->Altitude > (state->MinimumAltitude+150)) && (state->FlightMode == fmIdle))
				{
					state->FlightMode = fmLaunched;
					printf("*** LAUNCHED ***\n");
				}

				// Burst?
				if ((state->AscentRate < -10.0) && (state->Altitude < (state->MaximumAltitude+50)) && (state->MaximumAltitude >= (state->MinimumAltitude+2000)) && (state->FlightMode == fmLaunched))
				{
					state->FlightMode = fmDescending;
					printf("*** DESCENDING ***\n");
				}

				// Landed?
				if ((state->AscentRate >= -0.1) && (state->Altitude <= LANDING_ALTITUDE+2000) && (state->FlightMode >= fmDescending) && (state->FlightMode < fmLanded))
				{
					state->FlightMode = fmLanded;
					printf("*** LANDED ***\n");
				}      
			}
		}
		else if (strncmp(Buffer+3, "RMC", 3) == 0)
		{
			speedstring[0] = '\0';
			coursestring[0] = '\0';
			if (sscanf(Buffer+7, "%f,%c,%f,%c,%f,%c,%[^','],%[^','],%d", &utc_time, &active, &latitude, &ns, &longitude, &ew, speedstring, coursestring, &date) >= 7)
			{
				// $GPRMC,124943.00,A,5157.01557,N,00232.66381,W,0.039,,200314,,,A*6C
				
				speed = atof(speedstring);
				course = atof(coursestring);
				
				state->Speed = (int)speed;
				state->Direction = (int)course;
			}
		}
		else if (strncmp(Buffer+3, "GSV", 3) == 0)
        {
            // Disable GSV
            printf("Disabling GSV\r\n");
            unsigned char setGSV[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x39 };
            SendUBX(setGSV, sizeof(setGSV));
        }
		else if (strncmp(Buffer+3, "GLL", 3) == 0)
        {
            // Disable GLL
            printf("Disabling GLL\r\n");
            unsigned char setGLL[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x2B };
            SendUBX(setGLL, sizeof(setGLL));
        }
		else if (strncmp(Buffer+3, "GSA", 3) == 0)
        {
            // Disable GSA
            printf("Disabling GSA\r\n");
            unsigned char setGSA[] = { 0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x32 };
            SendUBX(setGSA, sizeof(setGSA));
        }
		else if (strncmp(Buffer+3, "VTG", 3) == 0)
        {
            // Disable VTG
            printf("Disabling VTG\r\n");
            unsigned char setVTG[] = {0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x47};
            SendUBX(setVTG, sizeof(setVTG));
        }
		else if (strncmp(Buffer+3, "TXT", 3) == 0)
        {
			if (strncmp(Buffer+7, "01,01,01,More than 100 frame errors, UART RX was disabled*70", 60) == 0) {
				printf("<!> (0) GPS error... Wait for watchdog\n");
				sleep_ms(5000);
			}
		}
        else
        {
            printf("Unknown NMEA sentence: %s\n", Buffer);
        }
    }
    else
    {
       printf("Bad checksum\r\n");
	}
}


void initGPS()
{
	// Initialise UART 1
	uart_init(uart1, 9600);
	gpio_set_function(GPS_TX, GPIO_FUNC_UART);
	gpio_set_function(GPS_RX, GPIO_FUNC_UART);
}

void readGPS(struct STATE *state)
{
	static uint64_t ModeTime = 0; 
	static unsigned char Line[100];
	static int Length=0;
	
	// SetFlightMode();

	while (uart_is_readable(uart1))
	{
		char Character;
		
		Character = uart_getc(uart1);
		
		// putchar(Character);

		if (Character == '$')
		{
			Line[0] = Character;
			Length = 1;
		}
		else if (Length > 90)
		{
			Length = 0;
		}
		else if ((Length > 0) && (Character != '\r'))
		{
			Line[Length++] = Character;
			if (Character == '\n')
			{
				Line[Length] = '\0';
				printf("> (0) %s", Line);
				ProcessLine(state, Line, Length);
				Length = 0;
			}
		}
	}
	
}

void writeFlightMode(struct STATE * state) {
	int RequiredFlightMode;
		
	RequiredFlightMode = (state->Altitude > 1000) ? 6 : 3;    // 6 is airborne <1g mode; 3=Pedestrian mode

	if (RequiredFlightMode != state->GPSFlightMode)
	{
		state->GPSFlightMode = RequiredFlightMode;

		SetFlightMode(RequiredFlightMode);
	}
}