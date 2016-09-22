#define SetPin(Port, Bit)    (Port |= (1 << Bit))
#define ClearPin(Port, Bit)  (Port &= ~(1 << Bit))

#define GetPin(Port, Bit)    (Port & (1 << Bit))
#define isHigh(Port, Bit)    (Port & (1 << Bit))
#define isLow(Port, Bit)     (!isHigh(Port, Bit))

// Arduino Mega 2560 definitions
static const int chip_select_pin = 2;   // PE4
#define CHIP_SELECT_PORT    PORTE
#define CHIP_SELECT_PIN     4

static const int clock_pin = 3;         // PE5
#define CLOCK_PORT          PORTE
#define CLOCK_PIN           5

static const int data_pin = 4;          // PG5
#define DATA_PORT           PORTG
#define DATA_PIN            5

static const int tx_pin = 5;            // PE3 
#define TX_PORT             PORTE
#define TX_PIN              3


static const uint8_t SOF_BYTE = 0xFE;
static const uint8_t PKT_LEN = 0x08;

static const uint8_t SNIFF_START = 0xDE;
static const uint8_t SNIFF_STOP = 0xAD;

static uint8_t rx_buffer[PKT_LEN] = {0};
static uint8_t rx_len = 0, pkt_len = 0, rx_byte = 0;

static volatile bool captureData;
static uint16_t captureLen = 0;
static uint8_t captureBuf[1000] = {0};
static unsigned long last_sniff_us;

typedef enum RxState
{
    RX_SOF,
    RX_LEN,
    RX_DATA,
} RxState;

static RxState rx_state = RX_SOF;

void sniff_eeprom_packets();
void run_rx_state_machine();
void handleRxStart();
void handleRxLen();
void handleRxData();
void cs_ISR();
void clk_ISR();

void setup()
{
    pinMode(chip_select_pin, INPUT);
    pinMode(clock_pin, INPUT);
    pinMode(data_pin, INPUT);
    pinMode(tx_pin, OUTPUT);

    attachInterrupt(0, cs_ISR, CHANGE);
    attachInterrupt(1, clk_ISR, RISING);

    Serial.begin(115200);
    Serial.println();

    rx_state = RX_SOF;
    captureData = false;
    captureLen = 0;
    last_sniff_us = micros();
}

void loop()
{
    sniff_eeprom_packets();
    run_rx_state_machine();
}

void cs_ISR()
{
    if (isHigh(CHIP_SELECT_PORT, CHIP_SELECT_PIN))
    {
        captureData = true;
        captureBuf[captureLen++] = SNIFF_START;
    }
    else
    {
        captureData = false;
        captureBuf[captureLen++] = SNIFF_STOP;
    }
    last_sniff_us = micros();
}

void clk_ISR()
{
    if (captureData)
    {
        captureBuf[captureLen++] = GetPin(DATA_PORT, DATA_PIN);
    }
}

void sniff_eeprom_packets()
{
    if (captureLen > 0)
    {
        if (micros() - last_sniff_us > 10000)
        {
            Serial.write(captureBuf, captureLen);
            captureLen = 0;
        }
    }
}

void run_rx_state_machine()
{
    switch(rx_state)
    {
         case RX_SOF:
             handleRxStart();
             break;
         case RX_LEN:
             handleRxLen();
             break;
         case RX_DATA:
             handleRxData();
             break;
         default:
             break;
    }
}

void handleRxStart()
{
    if (rx_byte == SOF_BYTE)
    {
        rx_state = RX_LEN;
    }
}

void handleRxLen()
{
    if (rx_byte == PKT_LEN)
    {
        pkt_len = 0;
        rx_state = RX_DATA;
    }
    else
    {
        rx_len = rx_byte;
        rx_state = RX_SOF;
    }
}

void handleRxData()
{
    rx_buffer[pkt_len++] = rx_byte;
    rx_len--;

    if (rx_len == 0)
    {
        rx_state = RX_SOF;
    }
}


