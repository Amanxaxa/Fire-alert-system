#include <Wire.h>  
#include "Waveshare_LCD1602.h"
#include <SD.h>
#include <SPI.h>
#include <RTClib.h>
#include <TMRpcm.h>  // Library for audio playback
#include <SoftwareSerial.h> // Library for Software Serial for GSM

// LCD
Waveshare_LCD1602 lcd(16, 2);  // 16 characters and 2 lines of display

// SD card
const int chipSelect = 53;  // CS pin for SD card

// Audio Playback
TMRpcm audio;  // Create an audio object
const int audioPin = 11;  // Audio pin for playback

// Buzzer
const int buzzerPin = 44;  // Buzzer attached to pin 44 (active-low)

// RTC module
RTC_DS3231 rtc;  // Create an RTC object

// GSM Module
SoftwareSerial gsmSerial(46, 47); // RX, TX pins for GSM module
// const int maxPhoneNumberLength = 15; // Adjust based on your needs
// const int phonenono
// char phoneNumber[maxPhoneNumberLength]; // Store phone number
const int maxno=3; //no. of phone numbers
String lines[maxno];
// String timelines[maxtime];
String myphonenumber="";
int linecount=0;
// Define the sensor pins array
int sensorPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 48, 49};
int areaNumber[]={1,2,3,4,5,6,7,8,9,11,12,13,14,15,16,17,18,21,22,23,24,32,31,30,29,28,27,26,25,10,20,19};
int numPins = sizeof(sensorPins) / sizeof(sensorPins[0]);  // Calculate the number of sensor pins
String areaNames[34];  // Store the corresponding area names
bool fireDetected = false;  // To track fire detection
int firePinIndex = -1;  // Store the index of the pin with fire detected

// Previous time string to compare
String previousTimeString = "";

// Timing for buzzer
unsigned long lastBuzzerTime = 0;
const unsigned long buzzerInterval = 1000; // 1-second interval for buzzer beep

String shiftChangeTimes[10];  // Array to store shift change times
int numShiftTimes = 0;        // To track the number of shift change times
bool playedForShift[10] = {false}; // Track sound played for each shift    

void setup() {
  Serial.begin(9600);
  gsmSerial.begin(9600);  // Start GSM communication
  
  pinMode(audioPin, OUTPUT);
  digitalWrite(audioPin, LOW);  // Keep audio pin low initially
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH); // Ensure buzzer is off initially (active-low)

  // Start-up sequence
  Serial.println("Starting system...");
  for (int i = 0; i < 3; i++) {
    digitalWrite(buzzerPin, LOW);  // Buzzer ON (active-low)
    delay(200);  // Buzzer on for 200 milliseconds
    digitalWrite(buzzerPin, HIGH); // Buzzer OFF
    delay(200);  // Pause for 200 milliseconds between beeps
  }

  lcd.init();
  lcd.setCursor(0, 0);
  lcd.send_string("System Start");
  
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    lcd.setCursor(0, 1);
    lcd.send_string("SD Init Failed");
    return;
  }
  Serial.println("SD card initialized.");
  lcd.setCursor(0, 1);
  lcd.send_string("SD Init Success");

  loadAreaNamesFromSD();
  loadPhoneNumberFromSD();
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.send_string("All Good");

  for (int i = 0; i < numPins; i++) {
    pinMode(sensorPins[i], INPUT_PULLUP);
  }

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    lcd.setCursor(0, 1);
    lcd.send_string("RTC Failed");
    while (1);
  }

  audio.speakerPin = audioPin;  // Set audio playback pin
  loadShiftTimesFromSD();  // Load the shift change times from SD card
  updateRTCFromSD();
}


// void sendSMS(const String& message) {
//   Serial.print("Sending SMS: ");
//   Serial.println(message);
//   gsmSerial.print("AT+CMGF=1\r");
//   delay(100);
//   gsmSerial.print("AT+CMGS=\"");
//   gsmSerial.print(phoneNumber);
//   gsmSerial.print("\"\r");
//   delay(100);
//   gsmSerial.print(message);
//   delay(100);
//   gsmSerial.write(26); // Ctrl+Z to send the SMS
// }




void loop() {

  bool newFireDetected = false;
  checkForShiftChange();  // Check for shift change time
  checkForMidnight();
  
  for (int i = 0; i < numPins; i++) {
    if (digitalRead(sensorPins[i]) == LOW) {
      newFireDetected = true;
      
      if (!fireDetected || firePinIndex != i) {
        fireDetected = true;
        firePinIndex = i;
        
        lcd.setCursor(0, 0);
        lcd.send_string("Fire in ");
        lcd.send_string(areaNames[i].c_str());

        String smsMessage = "Fire detected in area " + String(areaNumber[i]);

        digitalWrite(buzzerPin, LOW);
        delay(1000);
        digitalWrite(buzzerPin, HIGH);
        lastBuzzerTime = millis();
        sendSMS(smsMessage);

        String filename = "area" + String(areaNumber[i]) + ".wav";
        if (SD.exists(filename)) {
          Serial.print("Playing audio file: ");
          Serial.println(filename);
          audio.play(filename.c_str());
        } else {
          Serial.print("Audio file not found: ");
          Serial.println(filename);
        }
      }
      break;
    }
  }
  
  if (!newFireDetected && fireDetected) {
    fireDetected = false;
    firePinIndex = -1;
    audio.stopPlayback();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.send_string("All Good");
    Serial.println("Fire cleared, system reset to normal.");
  }

  if (fireDetected && !audio.isPlaying()) {
    String filename = "area" +String(areaNumber[firePinIndex]) + ".wav";
    audio.play(filename.c_str());
    Serial.print("Looping audio file: ");
    Serial.println(filename);
  }

  DateTime now = rtc.now();
  String timeString = now.timestamp(DateTime::TIMESTAMP_TIME);
  if (timeString != previousTimeString) {
    previousTimeString = timeString;
    lcd.setCursor(0, 1);
    lcd.send_string(timeString.c_str());
  }

  if (fireDetected && millis() - lastBuzzerTime >= buzzerInterval) {
    digitalWrite(buzzerPin, LOW);
    delay(1000);
    digitalWrite(buzzerPin, HIGH);
    lastBuzzerTime = millis();
  }

  // Check for new Serial input to update RTC
  if (Serial.available() > 0) {
    String serialInput = Serial.readStringUntil('\n');
    serialInput.trim();  // Remove any extra spaces or newline characters
    
    // Check if the input starts with "TIME:"
    if (serialInput.startsWith("TIME:")) {
      // Extract and parse the date and time from the input
      String timeStr = serialInput.substring(5); // Skip "TIME:"
      timeStr.trim();
      
      // Parse the timestamp format "YYYY-MM-DD HH:MM:SS"
      int year = timeStr.substring(0, 4).toInt();
      int month = timeStr.substring(5, 7).toInt();
      int day = timeStr.substring(8, 10).toInt();
      int hour = timeStr.substring(11, 13).toInt();
      int minute = timeStr.substring(14, 16).toInt();
      int second = timeStr.substring(17, 19).toInt();
      
      // Check if the parsed values are valid
      if (year > 2000 && month > 0 && month <= 12 && day > 0 && day <= 31 &&
          hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60) {
        
        // Update the RTC with parsed time
        rtc.adjust(DateTime(year, month, day, hour, minute, second));
        Serial.println("RTC updated via Serial input.");
      } else {
        Serial.println("Invalid date/time format.");
      }
    } else {
      Serial.println("Please use the format: TIME:YYYY-MM-DD HH:MM:SS");
    }
  }
}

void updateRTCFromSD() {
  File timeFile = SD.open("time.txt");  // Open the file with the time data
  
  if (timeFile) {
    String timeData = timeFile.readStringUntil('\n');
    timeData.trim();  // Remove any leading/trailing whitespace
    timeFile.close(); // Close the file after reading
    
    // Check if timeData has content and correct length
    if (timeData.length() == 19) {  // Expected format "YYYY-MM-DD HH:MM:SS"
      int year = timeData.substring(0, 4).toInt();
      int month = timeData.substring(5, 7).toInt();
      int day = timeData.substring(8, 10).toInt();
      int hour = timeData.substring(11, 13).toInt();
      int minute = timeData.substring(14, 16).toInt();
      int second = timeData.substring(17, 19).toInt();

      // Validate parsed date and time
      if (year > 2000 && month > 0 && month <= 12 && day > 0 && day <= 31 &&
          hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60) {
        rtc.adjust(DateTime(year, month, day, hour, minute, second));
        Serial.println("RTC updated from SD card.");
        
        // Delete time.txt after successful RTC update
        if (SD.remove("time.txt")) {
          Serial.println("time.txt erased from SD card after update.");
        } else {
          Serial.println("Failed to delete time.txt.");
        }
      } else {
        Serial.println("Invalid date/time format in time.txt.");
      }
    } else {
      Serial.println("time.txt is empty or has incorrect format.");
    }
  } else {
    Serial.println("time.txt not found on SD card.");
  }
}

void loadShiftTimesFromSD() {
  File shiftFile = SD.open("shift.txt");
  if (shiftFile) {
    Serial.println("Reading shift change times from SD card...");
    
    int index = 0;
    while (shiftFile.available()) {
      String timeLine = shiftFile.readStringUntil('\n');
      timeLine.trim();
      
      if (timeLine.length() > 0) {
        shiftChangeTimes[index] = timeLine;
        Serial.print("Shift time detected: ");
        Serial.println(shiftChangeTimes[index]);  // Print each shift time
        index++;
      }
    }
    numShiftTimes = index;
    shiftFile.close();
    Serial.print("Total shift times loaded: ");
    Serial.println(numShiftTimes);  // Print total number of shift times loaded
  } else {
    Serial.println("Error reading shift times from SD card.");
  }
}


void checkForShiftChange() {
  DateTime now = rtc.now();
  String currentTime = now.timestamp(DateTime::TIMESTAMP_TIME).substring(0, 8);  // Get HH:MM:SS format
  
  // Check if the current time matches any of the shift change times
  for (int i = 0; i < numShiftTimes; i++) {
    if (currentTime == shiftChangeTimes[i] && !playedForShift[i]) {
      // Play the shift change sound
      audio.play("shift.wav");
      Serial.print("Shift change sound played for shift ");
      Serial.println(i + 1);

      // Mark the sound as played for this shift
      playedForShift[i] = true;
      break;
    }
  }
}

void checkForMidnight() {
  DateTime now = rtc.now();
  String currentTime = now.timestamp(DateTime::TIMESTAMP_TIME).substring(0, 5);  // Get HH:MM format
  
  // Reset all shifts at midnight
  if (currentTime == "00:00") {
    for (int i = 0; i < numShiftTimes; i++) {
      playedForShift[i] = false; // Reset the played flag for all shifts
    }
    Serial.println("Shift played flags reset at midnight.");
  }
}


void loadAreaNamesFromSD() {
  File dataFile = SD.open("areas.txt");
  if (dataFile) {
    Serial.println("Reading area names from SD card...");
    
    int index = 0;
    while (dataFile.available() && index < numPins) {
      String line = dataFile.readStringUntil('\n');
      line.trim();
      
      int colonIndex = line.indexOf(':');
      if (colonIndex > 0) {
        String pinStr = line.substring(0, colonIndex);
        String area = line.substring(colonIndex + 1);
        area.trim();
        
        areaNames[index] = area;
        Serial.print("Loaded area ");
        Serial.print(areaNames[index]);
        Serial.print(" for sensor pin ");
        Serial.println(sensorPins[index]);
        index++;
      }
    }
    dataFile.close();
  } else {
    Serial.println("Error reading area names from SD card.");
    lcd.setCursor(0, 1);
    lcd.send_string("Error reading SD");
  }
}

void loadPhoneNumberFromSD() {
  File phoneFile = SD.open("phone.txt");
  if (phoneFile) {
    // phoneFile.readBytesUntil('\n', phoneNumber, maxPhoneNumberLength);
    // phoneNumber[maxPhoneNumberLength - 1] = '\0';
    // Serial.print("Phone number loaded: ");
    // Serial.println(phoneNumber);
    while (phoneFile.available()) {
      String line=phoneFile.readStringUntil('\n');
      line.trim();
      lines[linecount++]=line;

      if(linecount>=maxno){
        break;
      }
    }
    phoneFile.close();
    Serial.println(lines[0]);
    Serial.println(lines[1]);
  } else {
    Serial.println("Error reading phone number from SD card.");
    lcd.setCursor(0, 1);
    lcd.send_string("Error reading phone");
  }
}
void sendSMS(const String& message) {
  for(int j=0;j<maxno;j++){
  Serial.println(message);
  gsmSerial.print("AT+CMGF=1\r"); // Set SMS mode to text
  delay(500);
  gsmSerial.write("AT+CMGS=\"");
  gsmSerial.print(lines[j]);
  Serial.println(lines[j]);
  gsmSerial.print("\"\r");
  //gsmSerial.write(+"\"");
   // Replace with recipient's phone number
  delay(500);
  gsmSerial.print(message);
  delay(500);
  gsmSerial.write(26); // End of message
  delay(5000);

  Serial.println("Message sent!");
  }
}