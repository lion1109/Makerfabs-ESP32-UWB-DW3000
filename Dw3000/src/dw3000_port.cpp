/*
 * port.c
 *
 * Created: 9/10/2021 1:20:05 PM
 *  Author: Emim Eminof
 */ 

#include "dw3000_port.h"


#if USE_ARDUINO

#include "SPI.h"

uint8_t _ss;
uint8_t _rst;
uint8_t _irq;

#ifdef ESP8266
  // default ESP8266 frequency is 80 Mhz, thus divide by 4 is 20 MHz
  const SPISettings _fastSPI = SPISettings(8000000L, MSBFIRST, SPI_MODE0);
#else
  SPISettings _fastSPI = SPISettings(8000000L, MSBFIRST, SPI_MODE0);
#endif
const SPISettings _slowSPI = SPISettings(2000000L, MSBFIRST, SPI_MODE0);
const SPISettings* _currentSPI = &_fastSPI;

#define SET_FAST_SPI _currentSPI = &_fastSPI
#define SET_SLOW_SPI _currentSPI = &_slowSPI

boolean _debounceClockEnabled = false;

/* SPI configs. */
/*const SPISettings _fastSPI;
const SPISettings _slowSPI;
const SPISettings* _currentSPI;*/

  /* register caches. */
byte _syscfg[LEN_SYS_CFG];
byte _sysctrl[LEN_SYS_CTRL];
byte _sysstatus[LEN_SYS_STATUS];
byte _txfctrl[LEN_TX_FCTRL];
byte _sysmask[LEN_SYS_MASK];
byte _chanctrl[LEN_CHAN_CTRL];

uint8_t _deviceMode;

/* device status monitoring */
byte _vmeas3v3;
byte _tmeas23C;
  
/* PAN and short address. */
byte _networkAndAddress[LEN_PANADR];

void enableDebounceClock() {
    byte pmscctrl0[LEN_PMSC_CTRL0];
    memset(pmscctrl0, 0, LEN_PMSC_CTRL0);
    readBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, LEN_PMSC_CTRL0);
    setBit(pmscctrl0, LEN_PMSC_CTRL0, GPDCE_BIT, 1);
    setBit(pmscctrl0, LEN_PMSC_CTRL0, KHZCLKEN_BIT, 1);
    writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, LEN_PMSC_CTRL0);
    _debounceClockEnabled = true;
}

void sleepms(uint32_t x)
{
  //_delay_ms(x); // delay by milliseconds
}

int sleepus(uint32_t x)
{
  //_delay_us(x); // delay by microseconds
  return 0;
}

void deca_sleep(uint8_t time_ms) // wrapper for decawave sleep function
{
  sleepms(time_ms);
}

void deca_usleep(uint8_t time_us) // wrapper for decawave sleep function
{
  sleepus(time_us);
}


void spiBegin(uint8_t irq, uint8_t rst)
{
  /*DDR_SPI = _BV(DD_MOSI)|_BV(DD_SCK)|_BV(DD_SS); // Set MOSI, SCK and CS output
  DDR_SPI &= ~_BV(DD_MISO); // make sure MISO is an input
  SPCR0 = _BV(SPE)|_BV(MSTR); // Enable SPI functionality and Master SPI mode
  SPCR0 &= ~_BV(DORD); // set SPI most significant bit first (this is default on ATMEGA328pb)
  */
    // generous initial init/wake-up-idle delay
  delay(5);
  // Configure the IRQ pin as INPUT. Required for correct interrupt setting for ESP8266
      pinMode(irq, INPUT);
  // start SPI
  SPI.begin();
#ifndef ESP8266
//  SPI.usingInterrupt(digitalPinToInterrupt(irq)); // not every board support this, e.g. ESP8266
#endif
  // pin and basic member setup
  _rst        = rst;
  _irq        = irq;
  //_deviceMode = IDLE_MODE;
  // attach interrupt
  //attachInterrupt(_irq, DW1000Class::handleInterrupt, CHANGE); // todo interrupt for ESP8266
  // TODO throw error if pin is not a interrupt pin
  //attachInterrupt(digitalPinToInterrupt(_irq), DW1000Class::handleInterrupt, RISING); // todo interrupt for ESP8266
}

void reselect(uint8_t ss) {
  _ss = ss;
  pinMode(_ss, OUTPUT);
  digitalWrite(_ss, HIGH);
}

void readBytes(byte cmd, uint16_t offset, byte data[], uint16_t n) {
  byte header[3];
  uint8_t headerLen = 1;
  uint16_t i = 0;
  
  // build SPI header
  if(offset == NO_SUB) {
    header[0] = READ | cmd;
  } else {
    header[0] = READ_SUB | cmd;
    if(offset < 128) {
      header[1] = (byte)offset;
      headerLen++;
    } else {
      header[1] = RW_SUB_EXT | (byte)offset;
      header[2] = (byte)(offset >> 7);
      headerLen += 2;
    }
  }
  SPI.beginTransaction(*_currentSPI);
  digitalWrite(_ss, LOW);
  for(i = 0; i < headerLen; i++) {
    SPI.transfer(header[i]); // send header
  }
  for(i = 0; i < n; i++) {
    data[i] = SPI.transfer(JUNK); // read values
  }
  delayMicroseconds(5);
  digitalWrite(_ss, HIGH);
  SPI.endTransaction();
}


int readfromspi(uint16_t headerLength, uint8_t *headerBuffer, uint16_t readLength, uint8_t *readBuffer)
{

  SPI.beginTransaction(*_currentSPI);
  digitalWrite(_ss, LOW);
  for(int i = 0; i < headerLength; i++) {
    SPI.transfer(headerBuffer[i]); // send header
  }
  for(int i = 0; i < readLength; i++) {
    readBuffer[i] = SPI.transfer(JUNK); // read values
  }
  delayMicroseconds(5);
  digitalWrite(_ss, HIGH);
  SPI.endTransaction();

  return 0;
}


int writetospi(uint16_t headerLength, uint8_t *headerBuffer, uint16_t bodyLength, uint8_t *bodyBuffer)
{
  SPI.beginTransaction(*_currentSPI);
  digitalWrite(_ss, LOW);
  for(int i = 0; i < headerLength; i++) {
    SPI.transfer(headerBuffer[i]); // send header
  }
  for(int i = 0; i < bodyLength; i++) {
    SPI.transfer(bodyBuffer[i]); // write values
  }
  delayMicroseconds(5);
  digitalWrite(_ss, HIGH);
  SPI.endTransaction();

  return 0;
}

#else

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

#define TAG "dw3000_port"

typedef uint8_t byte;
typedef uint8_t boolean;

#define bitSet(x, n)   ((x) |= (1U << (n)))
#define bitClear(x, n) ((x) &= ~(1U << (n)))
#define bitRead(x, n)  (((x) >> (n)) & 0x1)
#define bitWrite(x,n,b) ((b) ? bitSet(x,n) : bitClear(x,n))

// Pins (anpassen)
#define PIN_SPI_MOSI (gpio_num_t)23
#define PIN_SPI_MISO (gpio_num_t)19
#define PIN_SPI_SCLK (gpio_num_t)18

#define PIN_SPI_CS   (gpio_num_t)4
#define PIN_IRQ      (gpio_num_t)34

#define CS_MANUAL 1

// gpio pins
static gpio_num_t _ss = PIN_SPI_CS;  // spi chip select pin
static gpio_num_t _rst; // reset pin
static gpio_num_t _irq = PIN_IRQ; // irq pin

// SPI-Handle
static spi_device_handle_t spi_fast = nullptr;
static spi_device_handle_t spi_slow = nullptr;
static spi_device_handle_t *spi_current = &spi_fast;

static boolean spi_init_bus()
{
  //ESP_LOGD(TAG, "spi_init_bus()");
  spi_bus_config_t buscfg = {};
  buscfg.miso_io_num = PIN_SPI_MISO;
  buscfg.mosi_io_num = PIN_SPI_MOSI;
  buscfg.sclk_io_num = PIN_SPI_SCLK;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

  return true;
}
static boolean dummy_bool = spi_init_bus();


static boolean spi_device_initialized = false;
static uint32_t spi_fast_frequency = 8'000'000UL; // 8 MHz
static boolean spi_init_device()
{
  //ESP_LOGD(TAG, "spi_init_device(), _ss: %d, frequency: %ld", _ss, spi_fast_frequency);
  printf("spi_init_device(), _ss: %d, frequency: %ld\n", _ss, spi_fast_frequency);
  spi_device_interface_config_t devcfg = {};
  devcfg.clock_speed_hz = spi_fast_frequency; // 8000000; // 8 MHz
  devcfg.mode = 0; // SPI_MODE0
#if CS_MANUAL
  devcfg.spics_io_num = -1;
#else
  devcfg.spics_io_num = _ss;
#endif
  devcfg.queue_size = 2;
  if (spi_fast)
      spi_bus_remove_device(spi_fast);
  if (spi_slow)
      spi_bus_remove_device(spi_slow);
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_fast));
  devcfg.clock_speed_hz = 2000000; // 2 MHz
  ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_slow));
  spi_device_initialized = true;
  return true;
}
//boolean dummy_bool = spi_init_device();

#define SET_FAST_SPI spi_current = &spi_fast;
#define SET_SLOW_SPI spi_current = &spi_slow;

// System-Register
#define LEN_PANADR 4
#define LEN_PMSC_CTRL0 4

static uint8_t _syscfg[LEN_SYS_CFG];
static uint8_t _sysctrl[LEN_SYS_CTRL];
static uint8_t _sysstatus[LEN_SYS_STATUS];
static uint8_t _txfctrl[LEN_TX_FCTRL];
static uint8_t _sysmask[LEN_SYS_MASK];
static uint8_t _chanctrl[LEN_CHAN_CTRL];
static uint8_t _networkAndAddress[LEN_PANADR];
static bool _debounceClockEnabled = false;
    
// Delay ersetzt Arduino delay(ms)
#define delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))

// GPIO Abstraktion
#define PIN_HIGH(pin) gpio_set_level(pin, 1)
#define PIN_LOW(pin)  gpio_set_level(pin, 0)
#define PIN_OUTPUT(pin) gpio_set_direction(pin, GPIO_MODE_OUTPUT)
#define PIN_INPUT(pin)  gpio_set_direction(pin, GPIO_MODE_INPUT)

// GPIO Select / Deselect
static void select_device()
{ // printf("select_device(), _ss: %d\n", _ss);
  PIN_LOW(_ss);
}
static void deselect_device()
{ //printf("deselect_device(), _ss: %d\n", _ss);
  PIN_HIGH(_ss);
}

uint8_t _deviceMode;

/* device status monitoring */
byte _vmeas3v3;
byte _tmeas23C;
  
// SPI Begin
void spiBegin(uint8_t irq, uint8_t rst)
{
  delay(5); // wait after wakeup
  _irq = (gpio_num_t)irq;
  _rst = (gpio_num_t)rst;
  PIN_INPUT(_irq);
}

void reselect(gpio_num_t ss) {
  //printf("reselect(_ss = %d, old: %d)\n", ss, _ss);
  if (ss == _ss && spi_device_initialized) return;

  _ss = ss;
  PIN_OUTPUT(_ss);
#if CS_MANUAL
  deselect_device();
#endif

  if (!spi_device_initialized) spi_init_device();
}

void spiFastFrequency(uint32_t freq)
{
  uint8_t changed = (spi_fast_frequency != freq);
  spi_fast_frequency = freq;

  if (changed && spi_device_initialized) spi_init_device();
}

#define JUNK 0x00
int readfromspi(uint16_t headerLength, uint8_t *headerBuffer, uint16_t readLength, uint8_t *readBuffer)
{
  // printf("readfromspi(...), _ss = %d)\n", _ss);
  spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    // Gesamtlänge = Header + zu lesende Bytes
    uint16_t totalLength = headerLength + readLength;
    uint8_t tx[totalLength];
    uint8_t rx[totalLength];

    for(int i = 0; i < headerLength; i++) { // Header in tx-Buffer kopieren
        tx[i] = headerBuffer[i];
    }
    for(int i = headerLength; i < totalLength; i++) { // fill rx with dummy bytes 0x00
        tx[i] = 0x00;
    }

    t.length = totalLength * 8;   // Länge in Bits
    t.tx_buffer = tx;
    t.rx_buffer = rx;

#if CS_MANUAL
    select_device();
#endif
    esp_err_t ret = spi_device_transmit(*spi_current, &t); // synchrones SPI-Transfer
#if CS_MANUAL
    deselect_device();
#endif
    if (ret != ESP_OK) {
        return -1; // Fehler
    }

    // Rx-Daten ab Header kopieren
    for(int i = 0; i < readLength; i++) {
        readBuffer[i] = rx[headerLength + i];
    }

    esp_rom_delay_us(5);
    return 0;
}


int writetospi(uint16_t headerLength, uint8_t *headerBuffer,
               uint16_t bodyLength, uint8_t *bodyBuffer)
{
  // printf("writetopi(...), _ss = %d)\n", _ss);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    // Gesamtlänge
    uint16_t totalLength = headerLength + bodyLength;

    // einfacher statischer Buffer (robust!)
    static uint8_t tx[512];

    if (totalLength > sizeof(tx)) {
        return -1; // Schutz gegen Overflow
    }

    // Header + Body zusammenkopieren
    memcpy(tx, headerBuffer, headerLength);
    memcpy(tx + headerLength, bodyBuffer, bodyLength);

    t.length = totalLength * 8;   // Bits!
    t.tx_buffer = tx;
    t.rx_buffer = NULL;           // wir lesen nichts

#if CS_MANUAL
    select_device();
#endif
    esp_err_t ret = spi_device_transmit(*spi_current, &t);
#if CS_MANUAL
    deselect_device();
#endif

    if (ret != ESP_OK) {
        return -1;
    }

    esp_rom_delay_us(5); // like delayMicroseconds(5)

    return 0;
}

#endif


void deca_sleep(uint8_t time_ms) // wrapper for decawave sleep function
{
  delay(time_ms);
}

void deca_usleep(uint8_t time_us) // wrapper for decawave sleep function
{
  debug_assert(time_us < 100,'deca_usleep(t) is just for dw3000 timing with (t < 100)');
  esp_rom_delay_us(time_us);
}


void readBytes(uint8_t cmd, uint16_t offset, uint8_t *data, uint16_t n)
{
    uint8_t header[3];
    uint16_t headerLen = 1;

    if (offset == NO_SUB) {
        header[0] = READ | cmd;
    } else {
        header[0] = READ_SUB | cmd;
        if (offset < 128) {
            header[1] = (uint8_t)offset;
            headerLen++;
        } else {
            header[1] = RW_SUB_EXT | (uint8_t)offset;
            header[2] = (uint8_t)(offset >> 7);
            headerLen += 2;
        }
    }

    readfromspi(headerLen, header, n, data);
}


/*
 * Write bytes to the DW1000. Single bytes can be written to registers via sub-addressing.
 * @param cmd
 *    The register address (see Chapter 7 in the DW1000 user manual).
 * @param offset
 *    The offset to select register sub-parts for writing, or 0x00 to disable
 *    sub-adressing.
 * @param data
 *    The data array to be written.
 * @param data_size
 *    The number of bytes to be written (take care not to go out of bounds of
 *    the register).
 */
// TODO offset really bigger than byte?
void writeBytes(byte cmd, uint16_t offset, byte data[], uint16_t data_size) {
  byte header[3];
  uint8_t  headerLen = 1;
  
  // TODO proper error handling: address out of bounds
  // build SPI header
  if(offset == NO_SUB) {
    header[0] = WRITE | cmd;
  } else {
    header[0] = WRITE_SUB | cmd;
    if(offset < 128) {
      header[1] = (byte)offset;
      headerLen++;
    } else {
      header[1] = RW_SUB_EXT | (byte)offset;
      header[2] = (byte)(offset >> 7);
      headerLen += 2;
    }
  }

  writetospi(headerLen, header, data_size, data);
}


// Helper to set a single register
void writeByte(byte cmd, uint16_t offset, byte data) {
  writeBytes(cmd, offset, &data, 1);
}


// always 4 bytes
// TODO why always 4 bytes? can be different, see p. 58 table 10 otp memory map
void readBytesOTP(uint16_t address, byte data[]) {
  byte addressBytes[LEN_OTP_ADDR];
  
  // p60 - 6.3.3 Reading a value from OTP memory
  // bytes of address
  addressBytes[0] = (address & 0xFF);
  addressBytes[1] = ((address >> 8) & 0xFF);
  // set address
  writeBytes(OTP_IF, OTP_ADDR_SUB, addressBytes, LEN_OTP_ADDR);
  // switch into read mode
  writeByte(OTP_IF, OTP_CTRL_SUB, 0x03); // OTPRDEN | OTPREAD
  writeByte(OTP_IF, OTP_CTRL_SUB, 0x01); // OTPRDEN
  // read value/block - 4 bytes
  readBytes(OTP_IF, OTP_RDAT_SUB, data, LEN_OTP_RDAT);
  // end read mode
  writeByte(OTP_IF, OTP_CTRL_SUB, 0x00);
}

void enableClock(byte clock) {
  byte pmscctrl0[LEN_PMSC_CTRL0];
  memset(pmscctrl0, 0, LEN_PMSC_CTRL0);
  readBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, LEN_PMSC_CTRL0);
  if(clock == AUTO_CLOCK) {
    SET_FAST_SPI;
    pmscctrl0[0] = AUTO_CLOCK;
    pmscctrl0[1] &= 0xFE;
  } else if(clock == XTI_CLOCK) {
    SET_SLOW_SPI;
    pmscctrl0[0] &= 0xFC;
    pmscctrl0[0] |= XTI_CLOCK;
  } else if(clock == PLL_CLOCK) {
    SET_FAST_SPI;
    pmscctrl0[0] &= 0xFC;
    pmscctrl0[0] |= PLL_CLOCK;
  } else {
    // TODO deliver proper warning
  }
  writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 2);
}

void reset() {
  printf("reset() _rst = %d)\n", _rst);
  if(_rst == 0xff) {
    softReset();
  } else {
    // dw1000 data sheet v2.08 §5.6.1 page 20, the RSTn pin should not be driven high but left floating.
    PIN_OUTPUT(_rst);
    PIN_LOW(_rst);
    delay(2);  // dw1000 data sheet v2.08 §5.6.1 page 20: nominal 50ns, to be safe take more time
    PIN_INPUT(_rst);
    delay(10); // dwm1000 data sheet v1.2 page 5: nominal 3 ms, to be safe take more time
    // force into idle mode (although it should be already after reset)
    idle();
  }
}

void softReset() {
  byte pmscctrl0[LEN_PMSC_CTRL0];
  readBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, LEN_PMSC_CTRL0);
  pmscctrl0[0] = 0x01;
  writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, LEN_PMSC_CTRL0);
  pmscctrl0[3] = 0x00;
  writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, LEN_PMSC_CTRL0);
  delay(10);
  pmscctrl0[0] = 0x00;
  pmscctrl0[3] = 0xF0;
  writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, LEN_PMSC_CTRL0);
  // force into idle mode
  idle();
}

void setBit(byte data[], uint16_t n, uint16_t bit, boolean val) {
  uint16_t idx;
  uint8_t shift;
  
  idx = bit/8;
  if(idx >= n) {
    return; // TODO proper error handling: out of bounds
  }
  byte* targetByte = &data[idx];
  shift = bit%8;
  if(val) {
    bitSet(*targetByte, shift);
  } else {
    bitClear(*targetByte, shift);
  }
}

/*
 * Check the value of a bit in an array of bytes that are considered
 * consecutive and stored from MSB to LSB.
 * @param data
 *    The number as byte array.
 * @param n
 *    The number of bytes in the array.
 * @param bit
 *    The position of the bit to be checked.
 */
boolean getBit(byte data[], uint16_t n, uint16_t bit) {
  uint16_t idx;
  uint8_t  shift;
  
  idx = bit/8;
  if(idx >= n) {
    return false; // TODO proper error handling: out of bounds
  }
  byte targetByte = data[idx];
  shift = bit%8;
  
  return bitRead(targetByte, shift); // TODO wrong type returned byte instead of boolean
}

void writeValueToBytes(byte data[], int32_t val, uint16_t n) {
  uint16_t i;
  for(i = 0; i < n; i++) {
    data[i] = ((val >> (i*8)) & 0xFF); // TODO bad types - signed unsigned problem
  }
}

void readSystemConfigurationRegister() {
  readBytes(SYS_CFG, NO_SUB, _syscfg, LEN_SYS_CFG);
}

void writeSystemConfigurationRegister() {
  writeBytes(SYS_CFG, NO_SUB, _syscfg, LEN_SYS_CFG);
}

void readSystemEventStatusRegister() {
  readBytes(SYS_STATUS, NO_SUB, _sysstatus, LEN_SYS_STATUS);
}

void readNetworkIdAndDeviceAddress() {
  readBytes(PANADR, NO_SUB, _networkAndAddress, LEN_PANADR);
}

void writeNetworkIdAndDeviceAddress() {
  writeBytes(PANADR, NO_SUB, _networkAndAddress, LEN_PANADR);
}

void readSystemEventMaskRegister() {
  readBytes(SYS_MASK, NO_SUB, _sysmask, LEN_SYS_MASK);
}

void writeSystemEventMaskRegister() {
  writeBytes(SYS_MASK, NO_SUB, _sysmask, LEN_SYS_MASK);
}

void readChannelControlRegister() {
  readBytes(CHAN_CTRL, NO_SUB, _chanctrl, LEN_CHAN_CTRL);
}

void writeChannelControlRegister() {
  writeBytes(CHAN_CTRL, NO_SUB, _chanctrl, LEN_CHAN_CTRL);
}

void readTransmitFrameControlRegister() {
  readBytes(TX_FCTRL, NO_SUB, _txfctrl, LEN_TX_FCTRL);
}

void writeTransmitFrameControlRegister() {
  writeBytes(TX_FCTRL, NO_SUB, _txfctrl, LEN_TX_FCTRL);
}

void idle() {
  memset(_sysctrl, 0, LEN_SYS_CTRL);
  setBit(_sysctrl, LEN_SYS_CTRL, TRXOFF_BIT, true);
  _deviceMode = IDLE_MODE;
  writeBytes(SYS_CTRL, NO_SUB, _sysctrl, LEN_SYS_CTRL);
}

void setDoubleBuffering(boolean val) {
  setBit(_syscfg, LEN_SYS_CFG, DIS_DRXB_BIT, !val);
}

void setInterruptPolarity(boolean val) {
  setBit(_syscfg, LEN_SYS_CFG, HIRQ_POL_BIT, val);
}

void clearInterrupts() {
  memset(_sysmask, 0, LEN_SYS_MASK);
}

void manageLDE() {
  // transfer any ldo tune values
  byte ldoTune[LEN_OTP_RDAT];
  readBytesOTP(0x04, ldoTune); // TODO #define
  if(ldoTune[0] != 0) {
    // TODO tuning available, copy over to RAM: use OTP_LDO bit
  }
  // tell the chip to load the LDE microcode
  // TODO remove clock-related code (PMSC_CTRL) as handled separately
  byte pmscctrl0[LEN_PMSC_CTRL0];
  byte otpctrl[LEN_OTP_CTRL];
  memset(pmscctrl0, 0, LEN_PMSC_CTRL0);
  memset(otpctrl, 0, LEN_OTP_CTRL);
  readBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, LEN_PMSC_CTRL0);
  readBytes(OTP_IF, OTP_CTRL_SUB, otpctrl, LEN_OTP_CTRL);
  pmscctrl0[0] = 0x01;
  pmscctrl0[1] = 0x03;
  otpctrl[0]   = 0x00;
  otpctrl[1]   = 0x80;
  writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 2);
  writeBytes(OTP_IF, OTP_CTRL_SUB, otpctrl, 2);
  delay(5);
  pmscctrl0[0] = 0x00;
  pmscctrl0[1] &= 0x02;
  writeBytes(PMSC, PMSC_CTRL0_SUB, pmscctrl0, 2);
}

void Sleep(uint32_t d) {
    delay(d);
}

void spiSelect(uint8_t ss) {
  printf("spiSelect(_ss = %d)\n", _ss);
  reselect((gpio_num_t)ss);
  // try locking clock at PLL speed (should be done already,
  // but just to be sure)
  enableClock(AUTO_CLOCK);
  delay(5);
  // reset chip (either soft or hard)
  if(_rst != 0xff) {
    // dw1000 data sheet v2.08 §5.6.1 page 20, the RSTn pin should not be driven high but left floating.
    PIN_INPUT(_rst);
  }
  reset();
  // default network and node id
  writeValueToBytes(_networkAndAddress, 0xFF, LEN_PANADR);
  writeNetworkIdAndDeviceAddress();
  // default system configuration
  memset(_syscfg, 0, LEN_SYS_CFG);
  setDoubleBuffering(false);
  setInterruptPolarity(true);
  writeSystemConfigurationRegister();
  // default interrupt mask, i.e. no interrupts
  clearInterrupts();
  writeSystemEventMaskRegister();
  // load LDE micro-code
  enableClock(XTI_CLOCK);
  delay(5);
  manageLDE();
  delay(5);
  enableClock(AUTO_CLOCK);
  delay(5);
  
  // read the temp and vbat readings from OTP that were recorded during production test
  // see 6.3.1 OTP memory map
  byte buf_otp[4];
  readBytesOTP(0x008, buf_otp); // the stored 3.3 V reading
  _vmeas3v3 = buf_otp[0];
  readBytesOTP(0x009, buf_otp); // the stored 23C reading
  _tmeas23C = buf_otp[0];
}


void wakeup_device_with_io() {
    printf("wakeup(), _ss = %d)\n", _ss);
    select_device();
    delay(2);
    deselect_device();
    if (_debounceClockEnabled){
            enableDebounceClock();
    }
}


void port_set_dw_ic_spi_fastrate(uint8_t irq, uint8_t rst, uint8_t ss) {
    spiBegin(irq, rst);
    spiSelect(ss);
}

uint32_t port_GetEXT_IRQStatus(void) {
  return 0;
}

uint32_t port_CheckEXT_IRQ(void) {
  return 0;
}

void port_DisableEXT_IRQ(void) {

}

void port_EnableEXT_IRQ(void) {

}

/* DW IC IRQ handler definition. */
static port_dwic_isr_t port_dwic_isr = NULL;

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn port_set_dwic_isr()
 *
 * @brief This function is used to install the handling function for DW IC IRQ.
 *
 * NOTE:
 *   - The user application shall ensure that a proper handler is set by calling this function before any DW IC IRQ occurs.
 *   - This function deactivates the DW IC IRQ line while the handler is installed.
 *
 * @param deca_isr function pointer to DW IC interrupt handler to install
 *
 * @return none
 */
void port_set_dwic_isr(port_dwic_isr_t dwic_isr)
{
    /* Check DW IC IRQ activation status. */
    //ITStatus en = port_GetEXT_IRQStatus();

    /* If needed, deactivate DW IC IRQ during the installation of the new handler. */
    //port_DisableEXT_IRQ();
    portDISABLE_INTERRUPTS();
    port_dwic_isr = dwic_isr;
    portENABLE_INTERRUPTS();
/*
    if (!en)
    {
        port_EnableEXT_IRQ();
    }*/
}


#if 0
void open_spi(void)
{
  //PORTB &= ~_BV(PORTB2); // set SS pin to LOW to enable SPI
}

void close_spi(void)
{
  //PORTB |= _BV(PORTB2); // set SS pin to HIGH to disable SPI
}

int spi_tranceiver (uint8_t *data) // send single byte
{
  /*SPDR0 = data; // Load data into the buffer
  sleepus(1);
  while(!(SPSR0 & _BV(SPIF) )); // Wait until transmission complete
  
  // Return received data
  return(SPDR0);*/
  return (0);
}



void port_set_dw_ic_spi_slowrate(void)
{
  //SPSR0 &= ~_BV(SPI2X); // turn off fast speed
}

void port_set_dw_ic_spi_fastrate(void)
{
  //SPSR0 |= _BV(SPI2X); // set fast speed by changing oscillator speed to FOSC / 2
}

void reset_DWIC(void) // currently not used as we are using softreset()
{
  /*DDR_PORTD |= _BV(DD_RESET_PIN); // set reset PIN as output
  PORTD &= ~_BV(PORTD7); // set reset pin to low for brief amount of time

  sleepus(1);

  DDR_PORTD &= ~_BV(DD_RESET_PIN); // set reset pin to input again

  sleepms(2); // allow for chip to turn back on*/

}

#endif
