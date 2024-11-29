#ifndef AHT20_INCLUDED
#define AHT20_INCLUDED

void readAHT20(struct STATE *s);  // Read the sensor values and store them in the STATE struct
int initAHT20();            //Initialises the sensor

#endif // AHT20_INCLUDED