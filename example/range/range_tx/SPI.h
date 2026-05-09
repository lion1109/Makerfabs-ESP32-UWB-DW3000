// no spi implementation needed


#include "dw3000.h"

#define MSBFIRST  0x0
#define SPI_MODE0 0x0

//typedef struct { long freq; uint8_t endianess; uint8_t mode; } SPISettings;

//SPISettings _fastSPI = { 8000'000L, MSBFIRST, SPI_MODE0 };

class SPISettings {
	public: inline SPISettings(uint32_t freq, uint8_t endianess, uint8_t mode) { spiFastFrequency(freq); }
};

