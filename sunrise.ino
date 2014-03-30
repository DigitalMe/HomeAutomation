int redLed = D0;
int greenLed = D1;
int blueLed = A0;
int cloudConnect = D2;


UDP UDP;
char string[ 17 ] = { "" };
int hour, minute, second;
unsigned int localPort = 123;
unsigned int timeZone = -7; //tz offset
const char timeServer[] = "pool.ntp.org";
const int NTP_PACKET_SIZE= 48;
byte packetBuffer[NTP_PACKET_SIZE];
bool isTimeSynced = false;
unsigned int offset = 0;
unsigned long secsSince1900 = 0;

struct colorValue{
    int red;
    int green;
    int blue;
    colorValue(int r, int g, int b){
        red = r;
        green = g;
        blue = b;
    }
    colorValue(){}
};

struct colorSequence{
    int redPin;
    int greenPin;
    int bluePin;
    struct colorValue *rgbColor;
    colorSequence(int redP, int greenP, int blueP, struct colorValue *color){
        redPin = redP;
        greenPin = greenP;
        bluePin = blueP;
        rgbColor = color;
    }
};

/*struct pin{
    enum {DIGITAL_READ, DIGITAL_READ_PULLUP, DIGITAL_READ_PULLDOWN, DIGITAL_WRITE, ANALOG_READ, ANALOG_READ_PULLUP, ANALOG_READ_PULLDOWN, ANALOG_WRITE}; //used to set the mode
    //AR    A0, A1, A2, A3, A4, A5, A6, A7
    //AW    D0, D1, A0, A1, A4, A5, A6, A7
    //DRW   ALL
    int address;                        // A0, A1 etc
    int mode;                           // use the enum
    int value;                          // 0 - 255 for output 0-1023? for input
    pin(int a,int m, int v){
        if (m >= ANALOG_READ){          //all the analog mods
            if (m <= ANALOG_READ_PULLDOWN and a <= A7) {  //only the read modes for all A pins
                //setValues
            } else if (m == ANALOG_WRITE and ((a >= D0 and a <= D1) or (a >= A0 and a <= A1) or (a >= A5 and a <= A7))) {
                //setValues
            } else {
                //setValues -1
            }
        } else if (m > ANALOG_WRITE or m < DIGITAL_READ) {
            //setValues -1
        } else {
            //setValues
        }
        address = a;
        mode = m;
        value = v;
    }
};
*/

const int MAXCYCLES = 4000;  //number of times loop() is run between each fade

// [stage][RGB]  array of RGB values that it will fade from one to the next
colorValue *acitiveSequence;
colorValue sunset [15] = {{82, 38, 13}, {85, 47, 29}, {89, 57, 48}, {84, 63, 62}, {75, 68, 72}, {65, 71, 81}, {48, 72, 90}, {26, 76, 106}, {5, 80, 122}, {1, 94, 131}, {18, 113, 110}, {1, 79, 132}, {33, 242, 240}, {244, 243, 230}, {255, 255, 255}};
colorValue sunrise [15] = {{255, 255, 255}, {244, 243, 226}, {33, 242, 240}, {1, 79, 255}, {18, 113, 227}, {1, 94, 183}, {5, 80, 122}, {26, 76, 106}, {48, 72, 90}, {65, 71, 81}, {75, 68, 72}, {84, 63, 62}, {89, 57, 48}, {85, 47, 29}, {82, 38, 13}};
// sunset 1 {{37, 28, 19}, {42, 36, 40}, {47, 44, 61}, {41, 47, 76}, {32, 52, 86}, {23, 53, 94}, {12, 54, 103}, {1, 51, 115}, {1, 50, 124}, {0, 55, 122}, {1, 63, 106}, {8, 79, 86}, {104, 143, 131}, {228, 233, 229}, {255, 255, 255}}
// sunset 2 {{48, 48, 41}, {55, 53, 53}, {74, 71, 90}, {65, 69, 92}, {60, 70, 98}, {58, 73, 108}, {44, 71, 106}, {35, 71, 111}, {21, 74, 133}, {2, 67, 142}, {1, 68, 137}, {0, 81, 120}, {33, 85, 76}, {200, 205, 195}, {255, 255, 255}}
// sunset 3 {{82, 38, 13}, {85, 47, 29}, {89, 57, 48}, {84, 63, 62}, {75, 68, 72}, {65, 71, 81}, {48, 72, 90}, {26, 76, 106}, {5, 80, 122}, {1, 94, 131}, {18, 113, 110}, {71, 125, 83}, {165, 173, 133}, {244, 243, 230}, {255, 255, 255}}
int stage = 0;
int cycle = 0;

// This routine runs only once upon reset
void setup() 
{
	UDP.begin(localPort);   //for the time sync
    pinMode(redLed, OUTPUT);    //setup the RED PWM pin
    pinMode(greenLed, OUTPUT);  //setup the GREEN PWM pin
    pinMode(blueLed, OUTPUT);   //setup the BLUE PWM pin
}

// This routine loops forever 
void loop() 
{
    delay(30);               // Wait for 10ms
	if (!isTimeSynced) {
	    if (cycle % 6000==0) {
        	if (!Spark.connected())
        	    Spark.connect();
    		setRGB(redLed, greenLed, blueLed, colorValue(245,255,255));
    		sendNTPpacket(timeServer); //send NTP request
    		delay(1500); //wait for a response
        	isTimeSynced = receiveTimeTick(); //try to read a packet. if receiveTimeTick() fails, status will still be 0, so loop repeats and another request is sent.
            cycle = 0;
	    }
        if (!isTimeSynced) 
            cycle++;
        else {
    		setRGB(redLed, greenLed, blueLed, colorValue (255,255,255));
        }
	}
	if (isTimeSynced) {
        if (stage < 15) {  //if the a stage is in process continue it. the last stage is 13
            fade(redLed, acitiveSequence[stage].red, acitiveSequence[(stage+1)].red, cycle, MAXCYCLES);
            fade(greenLed, acitiveSequence[stage].green, acitiveSequence[(stage+1)].green, cycle, MAXCYCLES);
            fade(blueLed, acitiveSequence[stage].blue, acitiveSequence[(stage+1)].blue, cycle, MAXCYCLES);
            cycle++;
            if (cycle >= MAXCYCLES) {
                stage++;
                cycle = 0;
            }
    	} else { // if not currently running a cycle, check for actions to perform
    		displayTime(); //convert the time into usable chunks
    		if (hour == 22 && minute == 00) {
    		    fadeInit(sunset);//run sunset
    		} else if (hour ==21 && minute == 30) {
    		    setRGB(redLed, greenLed, blueLed, colorValue(15,15,15));
    		} else if (hour == 8 && minute == 30) {
    		    setRGB(redLed, greenLed, blueLed, colorValue(255,255,255));
    		} else if (hour == 7 && minute == 30) {
       		    fadeInit(sunrise);//run sunrise
    		}
    	}
    	if (!Spark.connected() && digitalRead(cloudConnect) == HIGH){
    	    Spark.connect();
    	} else if (Spark.connected() && digitalRead(cloudConnect) == LOW){
    		Spark.disconnect();
    	}
	}
}

void fadeInit (struct colorValue *passedSequence) {
    acitiveSequence = passedSequence;
    stage = 0;
    cycle = 0;
}

void fade (int pin, int beginValue, int endValue, int cycleNumber, int maxCycle) {
    int outputValue = beginValue - (beginValue - endValue) * ((double)cycleNumber / maxCycle);
    analogWrite(pin, outputValue);
}

void setRGB (int rPin, int gPin, int bPin, struct colorValue color){
    analogWrite(rPin, color.red);
    analogWrite(gPin, color.green);
    analogWrite(bPin, color.blue);
    cycle = MAXCYCLES;
    stage = 15;
}

//all code below was developed by others
void displayTime() {
	const unsigned long seventyYears = 2208988800UL;    
	unsigned long epoch = secsSince1900 - seventyYears;
	epoch += ((millis() - offset) / 1000.0);
	hour = (epoch % 86400L) / 3600;         
	minute = (epoch % 3600) / 60;
	second = (epoch % 60);
}

bool receiveTimeTick() {
    bool isSynced = false;
	if ( UDP.parsePacket() ) {
		UDP.read(packetBuffer, NTP_PACKET_SIZE);
		unsigned long highWord = (packetBuffer[40] << 8) + packetBuffer[41];
		unsigned long lowWord = (packetBuffer[42] << 8) + packetBuffer[43];
		secsSince1900 = highWord << 16 | lowWord;
		secsSince1900 += timeZone*60*60;
		offset = millis();
		isSynced = true;
	}
	while ( UDP.parsePacket() ) {UDP.read(packetBuffer, NTP_PACKET_SIZE);}
	return isSynced;
}

unsigned long sendNTPpacket(const char *address)
{
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	packetBuffer[0] = 0b11100011;	 // LI, Version, Mode
	packetBuffer[1] = 0;		 // Stratum, or type of clock
	packetBuffer[2] = 6;		 // Polling Interval
	packetBuffer[3] = 0xEC;	// Peer Clock Precision
	packetBuffer[12]	= 49;
	packetBuffer[13]	= 0x4E;
	packetBuffer[14]	= 49;
	packetBuffer[15]	= 52;
	UDP.beginPacket(address, 123);
	UDP.write(packetBuffer, NTP_PACKET_SIZE); //NTP requests are to port 123
	UDP.endPacket();
}