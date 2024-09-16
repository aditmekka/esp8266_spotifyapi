#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>  // For OLED communication
#include <Adafruit_SSD1306.h>  // OLED library

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // No reset pin on OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid     = "your_ssid";
const char* password = "ssid_password";

const char* host = "api.spotify.com";
const int httpsPort = 443;  // HTTPS port
String accessToken = "your_access_token";

String trackName = "";
String artistName = "";
int scrollPos = 0;
int textWidth = 0;
unsigned long lastScrollTime = 0;
const unsigned long scrollDelay = 100;  // Adjust delay for scrolling speed

unsigned long lastFetchTime = 0; // Time since the last fetch
const unsigned long fetchInterval = 10000; // Fetch every 10 seconds

bool isFetching = false;

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  
  // Initially fetch Spotify track info
  getSpotifyTrack();
}

void getSpotifyTrack() {
  isFetching = true; // Set fetching flag to true
  
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed");
    isFetching = false; // Mark fetching as done
    return;
  }

  String url = "/v1/me/player/currently-playing";
  Serial.print("Requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Authorization: Bearer " + accessToken + "\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }

  String payload = client.readString();
  Serial.println("Response: ");
  Serial.println(payload);

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);

  const char* newTrackName = doc["item"]["name"];
  const char* newArtistName = doc["item"]["artists"][0]["name"];
  
  if (newTrackName && strcmp(newTrackName, trackName.c_str()) != 0) {
    // If the track name has changed, update
    trackName = String(newTrackName);
    artistName = String(newArtistName);
    
    // Calculate text width based on track name length and font size (Text size 2)
    textWidth = trackName.length() * 6;  // Approx. 12 pixels per character, adjust as needed
    scrollPos = 24;  // Reset scroll position for new track
    displayTrackAndArtist();  // Refresh display with new track and artist
  }
  
  isFetching = false; // Mark fetching as done
}

void displayTrackAndArtist() {
  // Clear only the display area that will be refreshed (track and artist)
  display.clearDisplay();
  
  // Display track name (Scrolling logic is handled in loop)
  display.setTextSize(1);  // Set text size to 2
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 16);
  display.print("Currently Playing: ");
  display.setCursor(0, 24);
  if (textWidth <= SCREEN_WIDTH) {
    // If the text fits within the screen, display it statically
    display.print(trackName);
  }
  
  // Display artist name statically below the track name
  display.setTextSize(1);  // Set text size to 1 for artist
  display.setCursor(0, 8);  // Y position slightly below the track name
  display.print("By: ");
  display.print(artistName);
  
  display.display();
}

void scrollTrackName() {
  // Clear only the track name area
  display.fillRect(0, 24, SCREEN_WIDTH, 12, SSD1306_BLACK);  // 20 pixels height for the track name area
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(-scrollPos, 24);  // Scroll position offset
  display.print(trackName);
  
  display.display();

  if (millis() - lastScrollTime > scrollDelay) {
    scrollPos += 2;  // Adjust scroll speed as necessary
    if (scrollPos > textWidth) {
      scrollPos = 0;  // Reset scroll when the text has fully scrolled
    }
    lastScrollTime = millis();  // Update the last scroll time
  }
}

void loop() {
  // Scroll the track name if it's too long to fit in one line
  if (textWidth > SCREEN_WIDTH) {
    scrollTrackName();  // Scroll track name in loop
  }

  // Check if it's time to fetch new data without blocking scrolling
  unsigned long currentTime = millis();
  if (currentTime - lastFetchTime >= fetchInterval && !isFetching) {
    lastFetchTime = currentTime;
    getSpotifyTrack();  // Fetch fresh data without blocking scrolling
  }
}
