#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

#include "main.h"
#include "misc.h"
#include "lora.h"
#include "helpers/sd.h"

typedef enum {lmIdle, lmListening, lmSending} tLoRaMode;

struct TBinaryPacket
{
	uint8_t 	PayloadIDs;
	uint16_t	Counter;
	uint16_t	BiSeconds;
	float		Latitude;
	float		Longitude;
	int32_t  	Altitude;
};  //  __attribute__ ((packed));


static tLoRaMode LoRaMode;
static char PayloadID[32];
static int CallingCount=0;
static int RTTYCount=0;
static int InRTTYMode=0;
static int SendingRTTY=0;
static unsigned long TimeToSendIfNoGPS=0;
static int RTTYIndex, RTTYMask, RTTYLength;
static int FSKBitRate, FSKOverSample, RTTYBitLength;
static struct TBinaryPacket PacketToRepeat;
static uint8_t SendRepeatedPacket, RepeatedPacketType=0;
static unsigned char Sentence[256];
static int ImplicitOrExplicit;

static int lora_sd_line_count = 0;
static int lora_sd_file_count = 0;

static inline void cs_select()
{
    asm volatile("nop \n nop \n nop");
    gpio_put(CS_LOR, 0);  // Active low
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect()
{
    asm volatile("nop \n nop \n nop");
    gpio_put(CS_LOR, 1);
    asm volatile("nop \n nop \n nop");
}

static void writeRegister(uint8_t reg, uint8_t data)
{
    uint8_t buf[2];
	
    buf[0] = reg | 0x80;
    buf[1] = data;
    cs_select();
    spi_write_blocking(SPI_PORT_1, buf, 2);
    cs_deselect();
	
    sleep_ms(1);
}

static uint8_t readRegister(uint8_t addr)
{
    uint8_t buf[1];
	
	addr &= 0x7F;
	
    cs_select();
    spi_write_blocking(SPI_PORT_1, &addr, 1);
    sleep_ms(1);
    spi_read_blocking(SPI_PORT_1, 0, buf, 1);
    cs_deselect();
	
    sleep_ms(1);
	
	// printf("READ %02X\n", buf[0]);
	
	return buf[0];
}

void SetDeviceMode(uint8_t newMode)
{
	static uint8_t currentMode = 0xFF;
	
	if (newMode == currentMode)
		return;  
  
	switch (newMode) 
	{
		case RF98_MODE_TX:
		writeRegister(REG_LNA, LNA_OFF_GAIN);  // TURN LNA OFF FOR TRANSMITT
		writeRegister(REG_PA_CONFIG, PA_MAX_UK);
		writeRegister(REG_OPMODE, newMode);
		currentMode = newMode; 
		break;

		case RF98_MODE_RX_CONTINUOUS:
		writeRegister(REG_PA_CONFIG, PA_OFF_BOOST);  // TURN PA OFF FOR RECIEVE??
		writeRegister(REG_LNA, LNA_MAX_GAIN);  // MAX GAIN FOR RECIEVE
		writeRegister(REG_OPMODE, newMode);
		currentMode = newMode; 
		break;

		case RF98_MODE_SLEEP:
		writeRegister(REG_OPMODE, newMode);
		currentMode = newMode; 
		break;

		case RF98_MODE_STANDBY:
		writeRegister(REG_OPMODE, newMode);
		currentMode = newMode; 
		break;

		default: return;
	} 
  
	if (newMode != RF98_MODE_SLEEP)
	{
		sleep_ms(1);
	}
   
	return;
}

void SetModemToLoRaMode()
{
	// printf("Setting LoRa Mode\n");
	SetDeviceMode(RF98_MODE_SLEEP);
	writeRegister(REG_OPMODE,0x80);

	// printf("LoRa Mode Set\n");
}


void SetFrequency(double Frequency)
{
	unsigned long FrequencyValue;

	printf("Frequency is %.3f", Frequency);

	Frequency = Frequency * 7110656 / 434;
	FrequencyValue = (unsigned long)(Frequency);

	//printf("FrequencyValue is %lu\n", FrequencyValue);

	writeRegister(0x06, (FrequencyValue >> 16) & 0xFF);    // Set frequency
	writeRegister(0x07, (FrequencyValue >> 8) & 0xFF);
	writeRegister(0x08, FrequencyValue & 0xFF);
}

void SetupRFM98(float Frequency, int Mode)
{
	int ErrorCoding;
	int Bandwidth;
	int SpreadingFactor;
	int LowDataRateOptimize;
	int PayloadLength;

	SetModemToLoRaMode();

	// Frequency
	SetFrequency(Frequency);	//  + LORA_OFFSET / 1000.0);

	// LoRa settings for various modes.  We support modes 2 (repeater mode), 1 (normally used for SSDV) and 0 (normal slow telemetry mode).

	if (Mode == 5)
	{
		ImplicitOrExplicit = EXPLICIT_MODE;
		ErrorCoding = ERROR_CODING_4_8;
		Bandwidth = BANDWIDTH_41K7;
		SpreadingFactor = SPREADING_11;
		LowDataRateOptimize = 0;		
	}
	else if (Mode == 2)
	{
		ImplicitOrExplicit = EXPLICIT_MODE;
		ErrorCoding = ERROR_CODING_4_8;
		Bandwidth = BANDWIDTH_62K5;
		SpreadingFactor = SPREADING_8;
		LowDataRateOptimize = 0;		
	}
	else if (Mode == 1)
	{
		ImplicitOrExplicit = IMPLICIT_MODE;
		ErrorCoding = ERROR_CODING_4_5;
		Bandwidth = BANDWIDTH_20K8;
		SpreadingFactor = SPREADING_6;
		LowDataRateOptimize = 0;    
	}
	else // if (Mode == 0)
	{
		ImplicitOrExplicit = EXPLICIT_MODE;
		ErrorCoding = ERROR_CODING_4_8;
		Bandwidth = BANDWIDTH_20K8;
		SpreadingFactor = SPREADING_11;
		LowDataRateOptimize = 0x08;		
	}

	PayloadLength = ImplicitOrExplicit == IMPLICIT_MODE ? 255 : 0;

	writeRegister(REG_MODEM_CONFIG, ImplicitOrExplicit | ErrorCoding | Bandwidth);
	writeRegister(REG_MODEM_CONFIG2, SpreadingFactor | CRC_ON);
	writeRegister(REG_MODEM_CONFIG3, 0x04 | LowDataRateOptimize);									// 0x04: AGC sets LNA gain

	// writeRegister(REG_DETECT_OPT, (SpreadingFactor == SPREADING_6) ? 0x05 : 0x03);					// 0x05 For SF6; 0x03 otherwise
	writeRegister(REG_DETECT_OPT, (readRegister(REG_DETECT_OPT) & 0xF8) | ((SpreadingFactor == SPREADING_6) ? 0x05 : 0x03));  // 0x05 For SF6; 0x03 otherwise

	writeRegister(REG_DETECTION_THRESHOLD, (SpreadingFactor == SPREADING_6) ? 0x0C : 0x0A);		// 0x0C for SF6, 0x0A otherwise  

	writeRegister(REG_PAYLOAD_LENGTH, PayloadLength);
	writeRegister(REG_RX_NB_BYTES, PayloadLength);

	// Change the DIO mapping to 01 so we can listen for TxDone on the interrupt
	writeRegister(REG_DIO_MAPPING_1,0x40);
	writeRegister(REG_DIO_MAPPING_2,0x00);

	// Go to standby mode
	SetDeviceMode(RF98_MODE_STANDBY);

	// printf("Setup Complete\n");
}

int BuildLoRaCall(unsigned char *TxLine)
{
	/*
	char Frequency[8];
	
	dtostrf(LORA_FREQUENCY, 7, 3, Frequency);

	sprintf((char *)TxLine, "^^%s,%s,%d,%d,%d,%d,%d",
    			        LORA_PAYLOAD_ID, Frequency,
        			    LORA_MODE == 1 ? 1 : 0, 
        			    LORA_MODE == 1 ? ERROR_CODING_4_5 : ERROR_CODING_4_8,
        			    LORA_MODE == 2 ? BANDWIDTH_62K5 : BANDWIDTH_20K8,
        			    LORA_MODE == 2 ? SPREADING_8 : (LORA_MODE == 1 ? SPREADING_6 : SPREADING_11),
        			    LORA_MODE == 0 ? 0x08 : 0);
			
	return strlen((char *)TxLine) + 1;
	*/
	return 0;
}

int BuildLoRaPositionPacket(struct STATE *state, unsigned char *TxLine)
{
	
	// struct TBinaryPacket BinaryPacket;

	// SentenceCounter++;

	// BinaryPacket.PayloadIDs = 0xC0 | (LORA_ID << 3) | LORA_ID;
	// BinaryPacket.Counter = SentenceCounter;
	// BinaryPacket.BiSeconds = state->SecondsInDay / 2L;
	// BinaryPacket.Latitude = state->Latitude;
	// BinaryPacket.Longitude = state->Longitude;
	// BinaryPacket.Altitude = state->Altitude;

	// memcpy(TxLine, &BinaryPacket, sizeof(BinaryPacket));

	// return sizeof(struct TBinaryPacket);
	return 0;
}

int TimeToSend(struct STATE *s)
{
	int CycleSeconds;

	SendRepeatedPacket = 0;

	if (LORA_CYCLETIME == 0)
	{
		// Not using time to decide when we can send
		return 1;
	}

	/*
	if ((millis() > (LastLoRaTX + LORA_CYCLETIME*1000+2000)) && (TimeToSendIfNoGPS))
	{
		// Timed out
		printf("Using Timeout\n");
		return 1;
	}
	*/

	if (s->Satellites > 0)
	{
		/*
		static int LastCycleSeconds=-1;

		// Can't Tx twice at the same time
		CycleSeconds = (state->SecondsInDay+LORA_CYCLETIME-17) % LORA_CYCLETIME;   // Could just use GPS time, but it's nice to see the slot agree with UTC

		if (CycleSeconds != LastCycleSeconds)
		{
			LastCycleSeconds = CycleSeconds;

			if (CycleSeconds == LORA_SLOT)
			{
				printf("Using GPS Timing\n");
				SendRepeatedPacket = 0;
				return 1;
			}

			if (RepeatedPacketType && ((CycleSeconds == LORA_REPEAT_SLOT_1) || (CycleSeconds == LORA_REPEAT_SLOT_2)))
			{
				printf("Time to repeat\n");
				SendRepeatedPacket = RepeatedPacketType;
				RepeatedPacketType = 0;
				return 1;
			}
		}
		*/
	}
	/*
	else if ((TimeToSendIfNoGPS > 0) && (millis() >= TimeToSendIfNoGPS))
	{
		printf("Using LoRa Timing\n");
		SendRepeatedPacket = 0;
		return 1;
	}
	*/
	
	return 0;
}

int LoRaIsFree(struct STATE *state)
{
	if ((LoRaMode != lmSending) || gpio_get(DIO0))
	{
		// Either not sending, or was but now it's sent.  Clear the flag if we need to
		if (LoRaMode == lmSending)
		{
		  // Clear that IRQ flag
			writeRegister( REG_IRQ_FLAGS, 0x08); 
			LoRaMode = lmIdle;
		}
					
		// Now we test to see if we're doing TDM or not
		// For TDM, if it's not a slot that we send in, then we should be in listening mode
		// Otherwise, we just send
					
		if (TimeToSend(state))
		{
		  // Either sending continuously, or it's our slot to send in
		  // printf("Channel %d is free\n", Channel);
						
			return 1;
		}

		/*
		if (LORA_CYCLETIME > 0)
		{
			// TDM system and not time to send, so we can listen
			if (LoRaMode == lmIdle)
			{
				startReceiving();
			}
		}
		*/
	}

	debug("> (1) Lora is busy\n");
	return 0;
}

void initLora()
{

    gpio_set_function(MISO_1, GPIO_FUNC_SPI);
    gpio_init(CS_LOR);
    gpio_set_function(SCLK_1,  GPIO_FUNC_SPI);
    gpio_set_function(MOSI_1, GPIO_FUNC_SPI);

    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(CS_LOR, GPIO_OUT);
    gpio_put(CS_LOR, 1);
	
	// DIO0 is input
    gpio_set_dir(DIO0, GPIO_IN);
	
	SetupRFM98(FREQUENCY, LORA_MODE);
	
	strcpy(PayloadID, CALLSIGN);
                           
}

void SendLoRaPacket(unsigned char *buffer, int Length, int CallingPacket)
{
	uint8_t buf[1];

	// LastLoRaTX = millis();
	TimeToSendIfNoGPS = 0;

	//  if (InRTTYMode != 0)
	//  {
	//    setupRFM98(LORA_FREQUENCY, LORA_MODE);
	//    InRTTYMode = 0;
	//  }
	InRTTYMode = 0;

	/*
	if (CallingPacket)
	{
		SsetupRFM98(LORA_CALL_FREQ, LORA_CALL_MODE);
	}
	else
	{
		setupRFM98(LORA_FREQUENCY, LORA_MODE);
	}
	*/

	// printf("Sending %d bytes\n", Length);

	SetDeviceMode(RF98_MODE_STANDBY);

	writeRegister(REG_DIO_MAPPING_1, 0x40);		// 01 00 00 00 maps DIO0 to TxDone
	writeRegister(REG_FIFO_TX_BASE_AD, 0x00);  // Update the address ptr to the current tx base address
	writeRegister(REG_FIFO_ADDR_PTR, 0x00); 
	if (ImplicitOrExplicit == EXPLICIT_MODE)
	{
		writeRegister(REG_PAYLOAD_LENGTH, Length);
	}
	
	
    cs_select();
    buf[0] = REG_FIFO | 0x80;
    cs_select();
	
    spi_write_blocking(SPI_PORT_1, buf, 1);
    spi_write_blocking(SPI_PORT_1, buffer, Length);
	
    cs_deselect();

	// go into transmit mode
	SetDeviceMode(RF98_MODE_TX);

	LoRaMode = lmSending;
	SendingRTTY = 0;
}

void check_lora(struct STATE *state)
{
	// CheckFSKBuffer();

	// CheckLoRaRx();
	if (LoRaIsFree(state))
	{		
		//printf("LoRa is free\n");
		if (SendRepeatedPacket == 3)
		{
			// Repeat ASCII sentence
			Sentence[0] = '%';
			SendLoRaPacket(Sentence, strlen((char *)Sentence)+1, 0);

			RepeatedPacketType = 0;
			SendRepeatedPacket = 0;
		}
		else if (SendRepeatedPacket == 2)
		{
			printf("Repeating uplink packet\n");

			// 0x80 | (LORA_ID << 3) | TargetID
			SendLoRaPacket((unsigned char *)&PacketToRepeat, sizeof(PacketToRepeat), 0);

			RepeatedPacketType = 0;
			SendRepeatedPacket = 0;
		}
		else if (SendRepeatedPacket == 1)
		{
			printf("Repeating balloon packet\n");

			// 0x80 | (LORA_ID << 3) | TargetID

			RepeatedPacketType = 0;
			SendRepeatedPacket = 0;
		}
		else			
		{
			int PacketLength;

			if (++RTTYCount >= (LORA_RTTY_COUNT + LORA_RTTY_EVERY))
			{
				RTTYCount = 0;
			}

			if (RTTYCount < LORA_RTTY_COUNT)
			{
				/*
				// Send RTTY packet
				PacketLength = BuildSentence(state, (char *)Sentence, PayloadID);
				printf("LoRa: Tx RTTY packet\n");
				SendLoRaRTTY(PacketLength);    
				*/
			}
			else
			{
				if ((LORA_CALL_COUNT > 0) && (++CallingCount > LORA_CALL_COUNT))
				{
					CallingCount = 0;
					SetupRFM98(LORA_CALL_FREQ, LORA_CALL_MODE);
					PacketLength = BuildLoRaCall(Sentence);
					printf("LoRa: Calling Mode");
					SendLoRaPacket(Sentence, PacketLength, 1);
				}
				else
				{
			//  		    if ((LORA_CALL_COUNT > 0) && (CallingCount == 1))
			//  		    {
			//  			    setupRFM98(LORA_FREQUENCY, LORA_MODE);
			//  		    }

					if (LORA_BINARY)
					{
						// 0x80 | (LORA_ID << 3) | TargetID
						PacketLength = BuildLoRaPositionPacket(state, Sentence);
						printf("LoRa: Tx Binary packet");
					}
					else
					{
						
						PacketLength = BuildSentence(state, (char *)Sentence, PayloadID);
						// printf("LoRa: Tx ASCII Sentence\n");
						printf("> (1) ");
						printf("%s\r", (char *)Sentence);

						// Write sentence to SD card - filename is lora_log[xx].txt
						lora_sd_line_count++;

						if (lora_sd_line_count > SD_MAX_LINES) {
							lora_sd_line_count = 0;
							lora_sd_file_count++;
						}

						char lora_filename[20];
						sprintf(lora_filename, "lora_log%d.txt", lora_sd_file_count);

						debug("> (1) Logging lora data to SD...");
						logStringToSD((char *)Sentence, lora_filename);
						debug("Done\n");
					}

					if (LORA_TRANSMITTING) {
						SendLoRaPacket(Sentence, PacketLength, 0);
					}  
				}
			}
		}
	}
	
}

int BuildSentence(struct STATE *state, char *TxLine, const char *PayloadID)
{
	static unsigned int SentenceCounter=0;
    int Count, i, j;
    unsigned char c;
    unsigned int CRC, xPolynomial;
    char CRCString[8];
	
    SentenceCounter++;
	
	// TODO: Modify to transmit new sensor data
    sprintf(TxLine,
            // SENTENCE_LENGTH-6,
            "$$%s,%d,%02d:%02d:%02d,%.5f,%.5f,%05.5ld,%u,%.1f,%.1f,%.1f,%.0f,%.1f,%.1f,%.1f,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.2f,%.2f",
            PayloadID,
            SentenceCounter,
			state->Hours, state->Minutes, state->Seconds,
            state->Latitude,
            state->Longitude,
            state->Altitude,
			state->Satellites,
            state->BatteryVoltage,
			state->InternalTemperature,
			state->BMETemperature,
			state->BMEPressure,
			state->BMEHumidity,
			state->PMTemperature,
			state->PMHumidity,
			state->HasCutDown,
			state->NO2WE,
			state->NO2AE,
			state->Solar0,
			state->Solar1,
			state->Solar2,
			state->PM1,
			state->PM2,
			state->PM10,
			state->PMSamplePeriod,
			state->PMFlowRate,
			state->AHT20Temperature,
			state->AHT20Humidity,
			state->TMP117Temperature
            );
    Count = strlen(TxLine);
	// DEBUG: printf("Message length: %d \n", Count);

    CRC = 0xffff;           // Seed
    xPolynomial = 0x1021;
   
     for (i = 2; i < Count; i++)
     {   // For speed, repeat calculation instead of looping for each bit
        CRC ^= (((unsigned int)TxLine[i]) << 8);
        for (j=0; j<8; j++)
        {
            if (CRC & 0x8000)
                CRC = (CRC << 1) ^ 0x1021;
            else
                CRC <<= 1;
        }
     }

    TxLine[Count++] = '*';
    TxLine[Count++] = Hex((CRC >> 12) & 15);
    TxLine[Count++] = Hex((CRC >> 8) & 15);
    TxLine[Count++] = Hex((CRC >> 4) & 15);
    TxLine[Count++] = Hex(CRC & 15);
  	TxLine[Count++] = '\n';  
	  TxLine[Count++] = '\0';
	
	return strlen(TxLine) + 1;
}
