const int tx_button = 4;
const int fake_tx_pin = 7;
const byte interruptPin = 2;

void isr();
void fake_tx_pulse(int high_us, int low_us);
void transmit_fake_frame(uint8_t frame[], uint8_t length);
void run_state_machine();
void handleStart();
void handleWaitForCommand();
void handleFakeTx();
void handleBeginTx();
void handleWaitForSync();
void handleAssembleFrame1();
void handleAssembleFrame2();
void handlePrintFrames();
void handleEndDelay();
void tx_on();
void tx_off();

typedef enum State
{
    START,
    WAIT_FOR_COMMAND,
    FAKE_TX,
    BEGIN_TX,
    WAIT_FOR_SYNC,
    ASSEMBLE_FRAME1,
    ASSEMBLE_FRAME2,
    PRINT_FRAMES,
    END_DELAY,
} State;

State current_state = START, next_state = START;
bool transitioned_state = false;
unsigned long state_start, now, activeTimeStart, inactiveTimeStart;
unsigned long activeTime, inactiveTime;
bool highPulseDetected = false, lowPulseDetected = false;
int bitCounter = 0;
unsigned long inactiveList[40] = {0};
unsigned long activeList[40] = {0};

void setup()
{
    Serial.begin(115200);
    pinMode(fake_tx_pin, OUTPUT);
    digitalWrite(fake_tx_pin, LOW);
    pinMode(tx_button, OUTPUT);
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(0, isr, CHANGE);

    tx_off();
    activeTimeStart = micros();
    inactiveTimeStart = micros();
    Serial.println();
}

void loop()
{
    run_state_machine();
}

void run_state_machine()
{
    switch(current_state)
    {
         case START:
             handleStart();
             break;
         case WAIT_FOR_COMMAND:
             handleWaitForCommand();
             break;
         case FAKE_TX:
             handleFakeTx();
             break;
         case BEGIN_TX:
             handleBeginTx();
             break;
         case WAIT_FOR_SYNC:
             handleWaitForSync();
             break;
         case ASSEMBLE_FRAME1:
             handleAssembleFrame1();
             break;
         case ASSEMBLE_FRAME2:
             handleAssembleFrame2();
             break;
         case PRINT_FRAMES:
             handlePrintFrames();
             break;
         case END_DELAY:
             handleEndDelay();
             break;
         default:
             break;
    }

    if (next_state != current_state)
    {
        transitioned_state = true;
        current_state = next_state;
    }
    else
    {
        transitioned_state = false;
    }
}

void isr()
{
    unsigned long now = micros();

    if (digitalRead(interruptPin) == HIGH)
    {
        inactiveTime = now - inactiveTimeStart;
        activeTimeStart = now;
        lowPulseDetected = true;
    }
    else
    {
        activeTime = now - activeTimeStart;
        inactiveTimeStart = now;
        highPulseDetected = true;
    }
}

void handleStart()
{
     next_state = WAIT_FOR_COMMAND;
}

void handleWaitForCommand()
{
    if (Serial.available() > 0)
    {
        uint8_t incomingByte = Serial.read();
        if (incomingByte == 's')            // Status request
        {
            Serial.println("#UNO\r\n");
        }
        else if (incomingByte == 't')       // Transmit a legit packet
        {
            next_state = BEGIN_TX;
        }
        else if (incomingByte == 'f')       // Transmit a fake packet 
        {
            next_state = FAKE_TX;
        }
    }
}

void handleFakeTx()
{
    uint8_t bytes_read;
    uint8_t buffer[40] = {0};

    while(bytes_read < 40)
    {
        while (Serial.available() == 0);    //TODO: Add timeout
        buffer[bytes_read] = Serial.read();
        bytes_read++;
    }
    Serial.write(buffer, 20);
    Serial.write("\t");
    Serial.write(buffer+20, 20);
    Serial.write("\n");

    transmit_fake_frame(buffer, 40);
    next_state = WAIT_FOR_COMMAND;
}

void transmit_fake_frame(uint8_t frame[], uint8_t length)
{
    if (length != 40)
        return;

    for (int i = 0; i < 40; i++)
    {
        if (frame[i] != '0' && frame[i] != '1' && frame[i] != '2')
            return;
    }

    for (int ctr = 0; ctr < 4; ctr++)
    {
        // Output Frame 1 sync bit
        fake_tx_pulse(500, 1000);

        // Output Frame 1
        for (int i = 0; i < 20; i++)
        {
            if (frame[i] == '0')
            {
                fake_tx_pulse(500, 1000);
            }
            else if (frame[i] == '1')
            {
                fake_tx_pulse(500, 500);
            }
            else if (frame[i] == '2')
            {
                fake_tx_pulse(1000, 500);
            }
        }
        // Delay 60 ms
        delay(60);

        // Output Frame 2 sync bit
        fake_tx_pulse(1500, 1000);
        // Output Frame 2
        for (int i = 20; i < 40; i++)
        {
            if (frame[i] == '0')
            {
                fake_tx_pulse(500, 1000);   // Active - inactive < -350 us
            }
            else if (frame[i] == '1')
            {
                fake_tx_pulse(500, 500);    // -350 < Active - inactive < 350
            }
            else if (frame[i] == '2')       // Active - inactive > 350 us
            {
                fake_tx_pulse(1000, 500);
            }
        }
        // Delay 60 ms
        delay(60);
    }
}

void fake_tx_pulse(int active_time_us, int inactive_time_us)
{
    digitalWrite(fake_tx_pin, HIGH);
    delayMicroseconds(active_time_us);
    digitalWrite(fake_tx_pin, LOW);
    delayMicroseconds(inactive_time_us);
}

void handleBeginTx()
{
    highPulseDetected = false;
    lowPulseDetected = false;
    bitCounter = 0;

    tx_on();
    delay(10);
    tx_off();

    next_state = WAIT_FOR_SYNC;
}

void handleWaitForSync()
{
    if (highPulseDetected)
    {
        if (activeTime < 750)
        {
            next_state = ASSEMBLE_FRAME1;
        }
        else if (activeTime < 1750)
        {
            next_state = ASSEMBLE_FRAME2;
        }
        else
        {
            next_state = WAIT_FOR_COMMAND;
        }
        highPulseDetected = false;
        lowPulseDetected = false;
    }
}

void handleAssembleFrame1()
{
    if (lowPulseDetected && highPulseDetected)
    {
        activeList[bitCounter] = activeTime;
        inactiveList[bitCounter] = inactiveTime;
        bitCounter++;

        highPulseDetected = false;
        lowPulseDetected = false;
    }

    if (bitCounter >= 20)
    {
        next_state = WAIT_FOR_SYNC;
    }
}

void handleAssembleFrame2()
{
    if (lowPulseDetected && highPulseDetected)
    {
        activeList[bitCounter] = activeTime;
        inactiveList[bitCounter] = inactiveTime;
        bitCounter++;

        highPulseDetected = false;
        lowPulseDetected = false;
    }

    if (bitCounter >= 40)
    {
        next_state = PRINT_FRAMES;
    }
}

void handlePrintFrames()
{
    int i, bitVal;

    Serial.print("#");
    for (i = 0; i < bitCounter; i++)
    {
        if (i == 20)
        {
            Serial.print("\t");
        }

        int pulseDiff = activeList[i] - inactiveList[i];

        if (pulseDiff < -380)
        {
            bitVal = 0;
        }
        else if (pulseDiff > -380 && pulseDiff < 380)
        {
            bitVal = 1;
        }
        else if (pulseDiff > 380)
        {
            bitVal = 2;
        }

        Serial.print(bitVal);
    }

    Serial.println("\r");
    next_state = END_DELAY;
}

void handleEndDelay()
{
    if(transitioned_state)
    {
        state_start = millis();
    }

    if ((millis() - state_start) > 1000)
    {
        next_state = WAIT_FOR_COMMAND;
    }
}

void tx_on()
{
    digitalWrite(tx_button, LOW);
}

void tx_off()
{
    digitalWrite(tx_button, HIGH);
}
