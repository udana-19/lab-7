#include "mbed.h"

// --- SAFETY SWITCH ---
// Set to 1 to enable I2C LCD (requires TextLCD files in src folder)
// Set to 0 to disable LCD if you get 'Fatal Error'
#define USE_LCD 1 

#if USE_LCD
    #include "TextLCD.h"
    // I2C Pins: SDA=PB_9, SCL=PB_8
    I2C i2c_lcd(PB_9, PB_8); 
    // Address 0x4E is standard. If blank, try 0x27
    TextLCD_I2C lcd(&i2c_lcd, 0x4E, TextLCD::LCD16x2); 
#endif

// ─── Serial monitor ────────────────────────────────────────
static BufferedSerial pc(USBTX, USBRX, 115200);

// ─── Hardware Pins ─────────────────────────────────────────
DigitalOut relayFwd(D4, 0);    
DigitalOut relayRev(D5, 0);    
DigitalOut relayBulb(D6, 0);   
DigitalOut ledGreen(D7, 0);    
DigitalOut ledRed(D8, 0);      

InterruptIn btnOpen(D2, PullUp);
InterruptIn btnClose(D3, PullUp);
InterruptIn btnStop(D9, PullUp);
InterruptIn btnAlarm(D10, PullUp);
InterruptIn pirSensor(A0);

DigitalOut rowPins[4] = { PC_0, PC_1, PC_2, PC_3 };
DigitalIn  colPins[4] = { DigitalIn(PC_4, PullUp), DigitalIn(PC_5, PullUp), DigitalIn(PC_6, PullUp), DigitalIn(PC_7, PullUp) };

const char keyMap[4][4] = { {'1','2','3','A'}, {'4','5','6','B'}, {'7','8','9','C'}, {'*','0','#','D'} };

// ─── Logic Variables ───────────────────────────────────────
Timer debounceTimer;
typedef enum { MOTOR_IDLE, MOTOR_OPEN, MOTOR_CLOSE } MotorState;
volatile MotorState motorState = MOTOR_IDLE;
volatile bool alarmActive = false, flagOpen = false, flagClose = false, flagStop = false, flagAlarm = false, flagPIR = false;
volatile int lastBtnTime = 0;
const int DEBOUNCE_MS = 200;

// ─── Functions ─────────────────────────────────────────────
static bool debounceOk() {
    int now = (int)(debounceTimer.elapsed_time().count() / 1000);
    if (now - lastBtnTime > DEBOUNCE_MS) { lastBtnTime = now; return true; }
    return false;
}

void motorStop() { relayFwd=0; relayRev=0; ledGreen=0; ledRed=0; motorState=MOTOR_IDLE; }
void motorForward() { relayRev=0; thread_sleep_for(10); relayFwd=1; ledGreen=1; ledRed=0; motorState=MOTOR_OPEN; }
void motorReverse() { relayFwd=0; thread_sleep_for(10); relayRev=1; ledGreen=0; ledRed=1; motorState=MOTOR_CLOSE; }

void isrOpen()  { if (debounceOk()) flagOpen = true; }
void isrClose() { if (debounceOk()) flagClose = true; }
void isrStop()  { if (debounceOk()) flagStop = true; }
void isrAlarm() { if (debounceOk()) flagAlarm = true; }
void isrPIR()   { flagPIR = true; }

char scanKeypad() {
    for (int r = 0; r < 4; r++) {
        for (int i = 0; i < 4; i++) rowPins[i] = (i == r) ? 0 : 1;
        thread_sleep_for(2);
        for (int c = 0; c < 4; c++) {
            if (colPins[c].read() == 0) {
                while (colPins[c].read() == 0) thread_sleep_for(5);
                for (int i = 0; i < 4; i++) rowPins[i] = 1;
                return keyMap[r][c];
            }
        }
    }
    for (int i = 0; i < 4; i++) rowPins[i] = 1;
    return '\0';
}

// ─── Main ──────────────────────────────────────────────────
int main() {
    debounceTimer.start();
    
    #if USE_LCD
        lcd.cls();
        lcd.printf("System Ready");
    #endif

    btnOpen.fall(&isrOpen); btnClose.fall(&isrClose);
    btnStop.fall(&isrStop); btnAlarm.fall(&isrAlarm);
    pirSensor.rise(&isrPIR);

    while (true) {
        if (flagOpen)  { flagOpen = false; motorForward(); }
        if (flagClose) { flagClose = false; motorReverse(); }
        if (flagStop)  { flagStop = false; motorStop(); }
        if (flagAlarm) { flagAlarm = false; alarmActive = !alarmActive; relayBulb = alarmActive; }
        if (flagPIR)   { flagPIR = false; relayBulb = 1; ledRed = 1; }

        char key = scanKeypad();
        if (key != '\0') {
            if (key == '1') motorForward();
            else if (key == '3') motorReverse();
            else if (key == '2') motorStop();
            else if (key == 'A') { alarmActive = !alarmActive; relayBulb = alarmActive; }
        }
        thread_sleep_for(50);
    }
}