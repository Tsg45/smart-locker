#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define LED_PIN 13 // Pin for the LED
#define SS_PIN 10  // SDA pin for the RFID reader
#define RST_PIN 9  // Reset pin for the RFID reader

// Relay pins
#define RELAY1_PIN 2
#define RELAY2_PIN 3
#define RELAY3_PIN 4

MFRC522 rfid(SS_PIN, RST_PIN); // Create an object for the RFID reader

const char* ssid = "LAPTOP0000"; // Change to your WiFi SSID
const char* password = "12345678"; // Change to your WiFi Password
const char* mqtt_server = "broker.hivemq.com"; // Change to your MQTT broker
const int mqtt_port = 1883;
const char* mqtt_Client = "fa54c3d5-205c-4eaa-8695-sddnatee";  // Change to your MQTT client ID
const char* mqtt_username = "";  // Token if needed
const char* mqtt_password = "";  // Secret if needed

WiFiClient espClient; 
PubSubClient client(espClient);

long lastMsg = 0;
int value = 0;
char msg[100];
String DataString;
bool checkedIn = false; // Variable to track check-in/out state
byte lastUID[10]; // To store the last scanned UID
bool firstScan = true; // To check if it's the first scan of the card

// Arrays for storing cards and their associated UID sizes
#define MAX_UID_SIZE 10
#define MAX_CARDS 3 // Maximum number of cards stored
byte storedUIDs[MAX_CARDS][MAX_UID_SIZE];
byte storedSizes[MAX_CARDS]; 
int cardCount = 0; // Card counter

// Assign each relay to a specific card
int relayAssignments[MAX_CARDS] = {-1, -1, -1}; // Stores relay assigned to each card (-1 means no assignment)

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_Client, mqtt_username, mqtt_password)) { // Connect to the MQTT broker
      Serial.println("connected");
      client.subscribe("@msg/operator");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    message = message + char(payload[i]);
  }
  Serial.println(message);
  if(String(topic) == "@msg/operator") {
    if (message == "ON") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED ON");
    } else if (message == "OFF") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED OFF");
    }
  } 
}

bool isSameCard() {
  // Compare the current UID with the last UID scanned
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] != lastUID[i]) {
      return false; // If any byte is different, it's a different card
    }
  }
  return true; // All bytes match, it's the same card
}

void updateLastUID() {
  // Update the lastUID with the current card's UID
  for (byte i = 0; i < rfid.uid.size; i++) {
    lastUID[i] = rfid.uid.uidByte[i];
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  SPI.begin();
  rfid.PCD_Init();
  
  // Setup relay pins
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  
  // Turn off all relays initially
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(RELAY3_PIN, LOW);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password); // Connect to WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); // Print WiFi IP address
  client.setServer(mqtt_server, mqtt_port); // Set the MQTT broker and port
  client.setCallback(callback); // Set the callback function for incoming messages
  client.subscribe("@msg/operator");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    Serial.print("UID tag: ");
    DataString = "{\"data\":{\"rfid\":\""; // Prepare the JSON string for the RFID UID
    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(rfid.uid.uidByte[i], HEX);
      DataString += String(rfid.uid.uidByte[i], HEX);
      if (i < rfid.uid.size - 1) DataString += ":";
    }

    // Check if it's the same card as last time
    if (firstScan || !isSameCard()) {
      // If it's the first scan or a new card, treat as check-in
      DataString += "\",\"status\":\"check-in\"}}";
      checkedIn = true; // Mark as checked in
      updateLastUID(); // Save the UID of this card
      firstScan = false; // First scan is now complete
    } else {
      // If it's the same card as last time, treat as check-out
      DataString += "\",\"status\":\"check-out\"}}";
      checkedIn = false; // Mark as checked out
      firstScan = true; // Reset to first scan for the next card
    }

    Serial.println();
    
    // Publish the RFID UID and check-in/check-out status to the MQTT broker
    DataString.toCharArray(msg, 100);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("test/rfid", msg);
    
    // Check if the card is already in the system
    int cardIndex = -1;
    for (int i = 0; i < cardCount; i++) {
      bool isSameCard = (rfid.uid.size == storedSizes[i]);
      for (byte j = 0; j < rfid.uid.size && isSameCard; j++) {
        if (rfid.uid.uidByte[j] != storedUIDs[i][j]) {
          isSameCard = false;
        }
      }
      if (isSameCard) {
        cardIndex = i; // Card found
        break;
      }
    }

    if (cardIndex >= 0) {
      // Only trigger the relay on check-out
      if (!checkedIn) {
        // If currently checked out, trigger the associated relay
        triggerRelay(relayAssignments[cardIndex]); 
      }
    } else {
      // New card detected
      if (cardCount < MAX_CARDS) {
        // Store the new card's UID and assign it to a relay
        storedSizes[cardCount] = rfid.uid.size;
        for (byte i = 0; i < rfid.uid.size; i++) {
          storedUIDs[cardCount][i] = rfid.uid.uidByte[i];
        }
        
        assignRelay(cardCount); // Assign a relay to the card
        cardCount++;
      }

      // Turn on LED for a second for a new card
      digitalWrite(LED_PIN, HIGH); 
      delay(1000);                 
      digitalWrite(LED_PIN, LOW);
    }

    // Display the card's UID
    Serial.print("UID tag: ");
    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();

    // List all stored cards
    Serial.println("Stored cards:");
    for (int i = 0; i < cardCount; i++) {
      Serial.print("Card ");
      Serial.print(i + 1);
      Serial.print(": ");
      for (byte j = 0; j < storedSizes[i]; j++) {
        Serial.print(storedUIDs[i][j] < 0x10 ? " 0" : " ");
        Serial.print(storedUIDs[i][j], HEX);
      }
      Serial.println();
    }

    rfid.PICC_HaltA(); // Halt RFID communication
  }
  delay(1);
}

// Function to trigger the relay associated with a card
void triggerRelay(int relay) {
  int relayPin = (relay == 1) ? RELAY1_PIN : (relay == 2) ? RELAY2_PIN : RELAY3_PIN;
  digitalWrite(relayPin, LOW); // Turn on relay
  delay(2000); // Keep relay on for 2 seconds
  digitalWrite(relayPin, HIGH); // Turn off relay
}

// Function to assign a relay to the card
void assignRelay(int cardIndex) {
  if (cardIndex == 0) {
    relayAssignments[cardIndex] = 1;
  } else if (cardIndex == 1) {
    relayAssignments[cardIndex] = 2;
  } else if (cardIndex == 2) {
    relayAssignments[cardIndex] = 3;
  }
  Serial.print("Card ");
  Serial.print(cardIndex + 1);
  Serial.print(" assigned to relay ");
  Serial.println(relayAssignments[cardIndex]);
}
