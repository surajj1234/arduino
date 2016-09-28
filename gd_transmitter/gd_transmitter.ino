#include <util/delay.h>

#define SetPin(Port, Bit)    (Port |= (1 << Bit))
#define ClearPin(Port, Bit)  (Port &= ~(1 << Bit))

#define isHigh(Port, Bit)    ((Port >> (Bit)) & 0x01)
#define isLow(Port, Bit)     (!isHigh(Port, Bit))

// Arduino Mega 2560 definitions
static const int chip_select_pin = 2;   // PE4
#define CHIP_SELECT_OUT_PORT    PORTE
#define CHIP_SELECT_IN_PORT     PINE
#define CHIP_SELECT_PIN         4

static const int clock_pin = 3;         // PE5
#define CLOCK_OUT_PORT          PORTE
#define CLOCK_IN_PORT           PINE
#define CLOCK_PIN           5

static const int data_pin = 4;          // PG5
#define DATA_OUT_PORT           PORTG
#define DATA_IN_PORT            PING
#define DATA_PIN            5

static const int tx_pin = 5;            // PE3 
#define TX_OUT_PORT             PORTE
#define TX_IN_PORT              PINE
#define TX_PIN              3


static const uint8_t SOF_BYTE = 0xFE;
static const uint8_t PKT_LEN = 0x08;

static const uint8_t SNIFF_START = '#';
static const uint8_t SNIFF_STOP = '\r';

static uint8_t rx_buffer[PKT_LEN] = {0};
static uint8_t rx_len = 0, pkt_len = 0, rx_byte = 0;

static volatile bool captureData;
static volatile uint16_t captureLen = 0;
static uint8_t captureBuf[1000] = {0};
static volatile unsigned long last_sniff_ms;

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
    Serial.begin(115200);
    Serial.println();

    pinMode(chip_select_pin, INPUT);
    pinMode(clock_pin, INPUT);
    pinMode(data_pin, INPUT);
    pinMode(tx_pin, OUTPUT);

    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    // disable timer0 overflow interrupt (millis/micros/delay wont work)
    TIMSK0 &= ~_BV(TOIE0);

    EICRB |= 1 << ISC40;  // sense any change on the INT0 pin
    EIMSK |= 1 << INT4;   // enable INT0 interrupt

    EICRB |= (1 << ISC51) | (1 << ISC50);  // sense rising edge on the INT1 pin
    EIMSK |= 1 << INT5;   // enable INT1 interrupt

    //attachInterrupt(0, cs_ISR, CHANGE);
    //attachInterrupt(1, clk_ISR, RISING);

    rx_state = RX_SOF;
    captureData = false;
    captureLen = 0;
    //last_sniff_ms = millis();
}

void loop()
{
    sniff_eeprom_packets();
    //run_rx_state_machine();
}

//void cs_ISR()
ISR(INT4_vect)
{
    int cs_pin = isHigh(CHIP_SELECT_IN_PORT, CHIP_SELECT_PIN);
    if (cs_pin)
    {
        captureData = true;
        captureBuf[captureLen++] = SNIFF_START;
    }
    else
    {
        captureData = false;
        captureBuf[captureLen++] = SNIFF_STOP;
        captureBuf[captureLen++] = '\n';
    }
}

//void clk_ISR()
ISR(INT5_vect)
{
    int data_pin = isHigh(DATA_IN_PORT, DATA_PIN);
    if (captureData)
    {
        if (data_pin)
        {
            captureBuf[captureLen++] = '1';
        }
        else
        {
            captureBuf[captureLen++] = '0';
        }
    }
}

void sniff_eeprom_packets()
{
    _delay_ms(5000);
    if (captureLen > 0)
    {

        //if (millis() - last_sniff_ms > 5000)
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


