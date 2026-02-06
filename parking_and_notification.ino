#include <SPI.h>            // SPI communication for RFID
#include <MFRC522.h>        // RFID reader library
#include <LiquidCrystal.h>  // LCD display library
#include <Servo.h>          // Servo motor library
#include <SoftwareSerial.h> // For GSM module communication

// RFID pins
#define SS_PIN 10
#define RST_PIN 9

// LCD pins
#define RS_PIN 2
#define EN_PIN 3
#define D4_PIN 4
#define D5_PIN 5
#define D6_PIN 6
#define D7_PIN 7

// Servo pin
#define SERVO_PIN 8

// IR sensors for parking slot detection and vehicle entry/exit detection
#define ENTRY_SENSOR_PIN A0
#define EXIT_SENSOR_PIN A1
#define SLOT1_SENSOR_PIN A2
#define SLOT2_SENSOR_PIN A3
#define SLOT3_SENSOR_PIN A4
#define SLOT4_SENSOR_PIN A5

// GSM module pins
#define GSM_TX 11
#define GSM_RX 12

// Authorized RFID card UIDs (hexadecimal format)
String authorizedCards[] = {
  "A1 B2 C3 D4",  // Example card 1 
  "E5 F6 G7 H8",  // Example card 2
  "11 22 33 44"   // Example card 3
};
const int NUM_AUTHORIZED_CARDS = 3;

// User phone numbers (one for each card)
String userPhoneNumbers[] = {
  "+1234567890",
  "+1234567891",
  "+1234567892"
};

// Global variables
int totalSlots = 4;
int availableSlots = 4;
bool gateOpen = false;
int lastCardIndex = -1;
bool vehicleEntering = false;

// Object initialization
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);
Servo gateServo;
SoftwareSerial gsmModule(GSM_TX, GSM_RX);

void setup() {
  // Initialize serial communications
  Serial.begin(9600);
  gsmModule.begin(9600);
  
  // Initialize SPI bus and RFID
  SPI.begin();
  rfid.PCD_Init();
  
  // Initialize LCD
  lcd.begin(16, 2);
  
  // Initialize servo
  gateServo.attach(SERVO_PIN);
  closeGate(); // Start with gate closed
  
  // Initialize IR sensors
  pinMode(ENTRY_SENSOR_PIN, INPUT);
  pinMode(EXIT_SENSOR_PIN, INPUT);
  pinMode(SLOT1_SENSOR_PIN, INPUT);
  pinMode(SLOT2_SENSOR_PIN, INPUT);
  pinMode(SLOT3_SENSOR_PIN, INPUT);
  pinMode(SLOT4_SENSOR_PIN, INPUT);
  
  // Initialize GSM module
  initGSM();
  
  // Display welcome message and available slots
  updateLCDDisplay();
  
  Serial.println("Parking System Ready");
}

void loop() {
  // Update available slots based on IR sensors
  updateAvailableSlots();
  
  // Update LCD display
  updateLCDDisplay();
  
  // Check for RFID card
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String cardUID = getCardUID();
    Serial.print("Card detected: ");
    Serial.println(cardUID);
    
    // Check if card is authorized
    int cardIndex = checkAuthorization(cardUID);
    if (cardIndex >= 0) {
      Serial.println("Access granted");
      lastCardIndex = cardIndex;
      vehicleEntering = true;
      openGate();
    } else {
      Serial.println("Access denied");
      lcd.setCursor(0, 1);
      lcd.print("Access Denied!  ");
      delay(2000);
    }
    
    // Halt PICC
    rfid.PICC_HaltA();
    // Stop encryption on PCD
    rfid.PCD_StopCrypto1();
  }
  
  // Check entry sensor for entering vehicle
  if (gateOpen && vehicleEntering && digitalRead(ENTRY_SENSOR_PIN) == LOW) {
    // Vehicle detected passing through entry
    delay(1000); // Wait for vehicle to fully enter
    
    if (availableSlots > 0) {
      availableSlots--;
      
      // Send SMS notification for entry
      if (lastCardIndex >= 0) {
        sendSMS(userPhoneNumbers[lastCardIndex], "Your vehicle has entered the parking. Available slots: " + String(availableSlots));
      }
    }
    
    closeGate();
    vehicleEntering = false;
  }
  
  // Check exit sensor for exiting vehicle
  if (digitalRead(EXIT_SENSOR_PIN) == LOW) {
    // Vehicle detected at exit
    Serial.println("Vehicle exiting");
    openGate();
    
    delay(1000); // Wait for vehicle to pass
    
    if (availableSlots < totalSlots) {
      availableSlots++;
      
      // Try to determine which vehicle is leaving based on parking slots
      int leavingCardIndex = determineExitingVehicle();
      if (leavingCardIndex >= 0) {
        sendSMS(userPhoneNumbers[leavingCardIndex], "Your vehicle has exited the parking. Thank you!");
      }
    }
    
    closeGate();
  }
  
  delay(100); // Small delay for stability
}

// Function to get card UID as a String
String getCardUID() {
  String cardUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    cardUID.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
    cardUID.concat(String(rfid.uid.uidByte[i], HEX));
  }
  cardUID.toUpperCase();
  return cardUID.substring(1); // Remove leading space
}

// Function to check if card is authorized
int checkAuthorization(String cardUID) {
  for (int i = 0; i < NUM_AUTHORIZED_CARDS; i++) {
    if (cardUID.equals(authorizedCards[i])) {
      return i; // Return index of authorized card
    }
  }
  return -1; // Card not authorized
}

// Function to update available slots based on IR sensors
void updateAvailableSlots() {
  int occupiedSlots = 0;
  
  // Count occupied slots
  if (digitalRead(SLOT1_SENSOR_PIN) == LOW) occupiedSlots++;
  if (digitalRead(SLOT2_SENSOR_PIN) == LOW) occupiedSlots++;
  if (digitalRead(SLOT3_SENSOR_PIN) == LOW) occupiedSlots++;
  if (digitalRead(SLOT4_SENSOR_PIN) == LOW) occupiedSlots++;
  
  availableSlots = totalSlots - occupiedSlots;
}

// Function to update LCD display
void updateLCDDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Parking System");
  lcd.setCursor(0, 1);
  lcd.print("Free Slots: ");
  lcd.print(availableSlots);
}

// Function to open gate
void openGate() {
  gateServo.write(90); // Adjust angle as needed
  gateOpen = true;
  Serial.println("Gate opened");
  delay(500);
}

// Function to close gate
void closeGate() {
  gateServo.write(0); // Adjust angle as needed
  gateOpen = false;
  Serial.println("Gate closed");
  delay(500);
}

// Function to initialize GSM module
void initGSM() {
  // Power up GSM module and wait for it to register to network
  Serial.println("Initializing GSM module...");
  
  gsmModule.println("AT"); // Test AT command
  delay(1000);
  
  gsmModule.println("AT+CMGF=1"); // Set SMS text mode
  delay(1000);
  
  gsmModule.println("AT+CNMI=1,2,0,0,0"); // Set notification for new messages
  delay(1000);
  
  Serial.println("GSM module initialized");
}

// Function to send SMS
void sendSMS(String phoneNumber, String message) {
  Serial.println("Sending SMS to: " + phoneNumber);
  Serial.println("Message: " + message);
  
  gsmModule.println("AT+CMGF=1"); // Set SMS text mode
  delay(500);
  
  gsmModule.println("AT+CMGS=\"" + phoneNumber + "\""); // Set recipient phone number
  delay(500);
  
  gsmModule.print(message); // Message content
  delay(500);
  
  gsmModule.write(26); // Ctrl+Z to send
  delay(3000);
  
  Serial.println("SMS sent");
}

// Function to determine which vehicle is exiting
int determineExitingVehicle() {
  // This is a simplified approach - in a real system you would need
  // to track which card holder parked in which slot
  // For now, we'll just return the last card used to enter
  return lastCardIndex;
}
