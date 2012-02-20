/*
Gutenbird demo sketch: monitors one or more Twitter accounts
for changes, displaying updates on attached thermal printer.
Written by Adafruit Industries.  MIT license.

REQUIRES ARDUINO IDE 1.0 OR LATER -- Back-porting is not likely to
occur, as the code is deeply dependent on the Stream class, etc.

Required hardware includes an Ethernet-connected Arduino board such
as the Arduino Ethernet or other Arduino-compatible board with an
Arduino Ethernet Shield, plus an Adafruit Mini Thermal Receipt
printer and all related power supplies and cabling.

Resources:
http://www.adafruit.com/products/418 Arduino Ethernet
http://www.adafruit.com/products/284 FTDI Friend
http://www.adafruit.com/products/201 Arduino Uno
http://www.adafruit.com/products/201 Ethernet Shield
http://www.adafruit.com/products/597 Mini Thermal Receipt Printer
http://www.adafruit.com/products/600 Printer starter pack
*/

#include <SPI.h>
#include <Ethernet.h>
#include <Adafruit_Thermal.h>
#include <SoftwareSerial.h>

// Global stuff --------------------------------------------------------------

const int
  led_pin         = 3,           // To status LED (hardware PWM pin)
  // Pin 4 is skipped -- this is the Card Select line for Arduino Ethernet!
  printer_RX_Pin  = 5,           // Printer connection: green wire
  printer_TX_Pin  = 6,           // Printer connection: yellow wire
  printer_Ground  = 7,           // Printer connection: black wire
  maxTweets       = 5;           // Limit tweets printed; avoid runaway output
const unsigned long              // Time limits, expressed in milliseconds:
  pollingInterval = 60L * 1000L, // Note: Twitter server will allow 150/hr max
  connectTimeout  = 15L * 1000L, // Max time to retry server link
  responseTimeout = 15L * 1000L; // Max time to wait for data from server
Adafruit_Thermal
  printer(printer_RX_Pin, printer_TX_Pin);
byte
  sleepPos = 0, // Current "sleep throb" table position
  resultsDepth, // Used in JSON parsing
  // Ethernet MAC address is found on sticker on Ethernet shield or board:
  mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x76, 0x09 };
IPAddress
  ip(192,168,0,118); // Fallback address -- code will try DHCP first
EthernetClient
  client;
char
  *serverName  = "search.twitter.com",
  // queryString can be any valid Twitter API search string, including
  // boolean operators.  See https://dev.twitter.com/docs/using-search
  // for options and syntax.  Funny characters do NOT need to be URL
  // encoded here -- the sketch takes care of that.
  *queryString = "from:Adafruit",
  lastId[21],    // 18446744073709551615\0 (64-bit maxint as string)
  timeStamp[32], // WWW, DD MMM YYYY HH:MM:SS +XXXX\0
  fromUser[16],  // Max username length (15) + \0
  msgText[141],  // Max tweet length (140) + \0
  name[11],      // Temp space for name:value parsing
  value[141];    // Temp space for name:value parsing
PROGMEM byte
  sleepTab[] = { // "Sleep throb" brightness table (reverse for second half)
      0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
      1,   1,   2,   3,   4,   5,   6,   8,  10,  13,
     15,  19,  22,  26,  31,  36,  41,  47,  54,  61,
     68,  76,  84,  92, 101, 110, 120, 129, 139, 148,
    158, 167, 177, 186, 194, 203, 211, 218, 225, 232,
    237, 242, 246, 250, 252, 254, 255 };

// Function prototypes -------------------------------------------------------

boolean
  jsonParse(int, byte),
  readString(char *, int);
int
  unidecode(byte),
  timedRead(void);

// ---------------------------------------------------------------------------

void setup() {

  // Set up LED "sleep throb" ASAP, using Timer1 interrupt:
  TCCR1A  = _BV(WGM11); // Mode 14 (fast PWM), 64:1 prescale, OC1A off
  TCCR1B  = _BV(WGM13) | _BV(WGM12) | _BV(CS11) | _BV(CS10);
  ICR1    = 8333;       // ~30 Hz between sleep throb updates
  TIMSK1 |= _BV(TOIE1); // Enable Timer1 interrupt
  sei();                // Enable global interrupts

  Serial.begin(57600);
  pinMode(printer_Ground, OUTPUT);
  digitalWrite(printer_Ground, LOW);  // Just a reference ground, not power
  printer.begin();
  printer.sleep();

  // Initialize Ethernet connection.  Request dynamic
  // IP address, fall back on fixed IP if that fails:
  Serial.print("Initializing Ethernet...");
  if(Ethernet.begin(mac)) {
    Serial.println("OK");
  } else {
    Serial.print("\r\nno DHCP response, using static IP address.");
    Ethernet.begin(mac, ip);
  }

  // Clear all string data
  memset(lastId   , 0, sizeof(lastId));
  memset(timeStamp, 0, sizeof(timeStamp));
  memset(fromUser , 0, sizeof(fromUser));
  memset(msgText  , 0, sizeof(msgText));
  memset(name     , 0, sizeof(name));
  memset(value    , 0, sizeof(value));
}

// ---------------------------------------------------------------------------

void loop() {
  unsigned long startTime, t;
  int           i;
  char          c;

  startTime = millis();

  // Disable Timer1 interrupt during network access, else there's trouble.
  // Just show LED at steady 100% while working.  :T
  TIMSK1 &= ~_BV(TOIE1);
  analogWrite(led_pin, 255);

  // Attempt server connection, with timeout...
  Serial.print("Connecting to server...");
  while((client.connect(serverName, 80) == false) &&
    ((millis() - startTime) < connectTimeout));

  if(client.connected()) { // Success!
    Serial.print("OK\r\nIssuing HTTP request...");
    // URL-encode queryString to client stream:
    client.print("GET /search.json?q=");
    for(i=0; c=queryString[i]; i++) {
      if(((c >= 'a') && (c <= 'z')) ||
         ((c >= 'A') && (c <= 'Z')) ||
         ((c >= '0') && (c <= '9')) ||
          (c == '-') || (c == '_')  ||
          (c == '.') || (c == '~')) {
        // Unreserved char: output directly
        client.write(c);
      } else {
        // Reserved/other: percent encode
        client.write('%');
        client.print(c, HEX);
      }
    }
    client.print("&result_type=recent&include_entities=false&rpp=");
    if(lastId[0]) {
      client.print(maxTweets);    // Limit to avoid runaway printing
      client.print("&since_id="); // Display tweets since prior query
      client.print(lastId);
    } else {
      client.print('1'); // First run; show single latest tweet
    }
    client.print(" HTTP/1.1\r\nHost: ");
    client.println(serverName);
    client.println("Connection: close\r\n");

    Serial.print("OK\r\nAwaiting results (if any)...");
    t = millis();
    while((!client.available()) && ((millis() - t) < responseTimeout));
    if(client.available()) { // Response received?
      // Could add HTTP response header parsing here (400, etc.)
      if(client.find("\r\n\r\n")) { // Skip HTTP response header
        Serial.println("OK\r\nProcessing results...");
        resultsDepth = 0;
        jsonParse(0, 0);
      } else Serial.println("response not recognized.");
    } else   Serial.println("connection timed out.");
    client.stop();
  } else { // Couldn't contact server
    Serial.println("failed");
  }

  // Sometimes network access & printing occurrs so quickly, the steady-on
  // LED wouldn't even be apparent, instead resembling a discontinuity in
  // the otherwise smooth sleep throb.  Keep it on at least 4 seconds.
  t = millis() - startTime;
  if(t < 4000L) delay(4000L - t);

  // Pause between queries, factoring in time already spent on network
  // access, parsing, printing and LED pause above.
  t = millis() - startTime;
  if(t < pollingInterval) {
    Serial.print("Pausing...");
    sleepPos = sizeof(sleepTab); // Resume following brightest position
    TIMSK1 |= _BV(TOIE1); // Re-enable Timer1 interrupt for sleep throb
    delay(pollingInterval - t);
    Serial.println("done");
  }
}

// ---------------------------------------------------------------------------

boolean jsonParse(int depth, byte endChar) {
  int     c, i;
  boolean readName = true;

  for(;;) {
    while(isspace(c = timedRead())); // Scan past whitespace
    if(c < 0)        return false;   // Timeout
    if(c == endChar) return true;    // EOD

    if(c == '{') { // Object follows
      if(!jsonParse(depth + 1, '}')) return false;
      if(!depth)                     return true; // End of file
      if(depth == resultsDepth) { // End of object in results list

        // Output to printer
        printer.wake();
        printer.inverseOn();
        printer.write(' ');
        printer.print(fromUser);
        for(i=strlen(fromUser); i<31; i++) printer.write(' ');
        printer.inverseOff();
        printer.underlineOn();
        printer.print(timeStamp);
        for(i=strlen(timeStamp); i<32; i++) printer.write(' ');
        printer.underlineOff();
        printer.println(msgText);
        printer.feed(3);
        printer.sleep();

        // Dump to serial console as well
        Serial.print("User: ");
        Serial.println(fromUser);
        Serial.print("Text: ");
        Serial.println(msgText);
        Serial.print("Time: ");
        Serial.println(timeStamp);

        // Clear strings for next object
        timeStamp[0] = fromUser[0] = msgText[0] = 0;
      }
    } else if(c == '[') { // Array follows
      if((!resultsDepth) && (!strcasecmp(name, "results")))
        resultsDepth = depth + 1;
      if(!jsonParse(depth + 1,']')) return false;
    } else if(c == '"') { // String follows
      if(readName) { // Name-reading mode
        if(!readString(name, sizeof(name)-1)) return false;
      } else { // Value-reading mode
        if(!readString(value, sizeof(value)-1)) return false;
        // Process name and value strings:
        if       (!strcasecmp(name, "max_id_str")) {
          strncpy(lastId, value, sizeof(lastId)-1);
        } else if(!strcasecmp(name, "created_at")) {
          strncpy(timeStamp, value, sizeof(timeStamp)-1);
        } else if(!strcasecmp(name, "from_user")) {
          strncpy(fromUser, value, sizeof(fromUser)-1);
        } else if(!strcasecmp(name, "text")) {
          strncpy(msgText, value, sizeof(msgText)-1);
        }
      }
    } else if(c == ':') { // Separator between name:value
      readName = false; // Now in value-reading mode
      value[0] = 0;     // Clear existing value data
    } else if(c == ',') {
      // Separator between name:value pairs.
      readName = true; // Now in name-reading mode
      name[0]  = 0;    // Clear existing name data
    } // Else true/false/null or a number follows.  These values aren't
      // used or expected by this program, so just ignore...either a comma
      // or endChar will come along eventually, these are handled above.
  }
}

// ---------------------------------------------------------------------------

// Read string from client stream into destination buffer, up to a maximum
// requested length.  Buffer should be at least 1 byte larger than this to
// accommodate NUL terminator.  Opening quote is assumed already read,
// closing quote will be discarded, and stream will be positioned
// immediately following the closing quote (regardless whether max length
// is reached -- excess chars are discarded).  Returns true on success
// (including zero-length string), false on timeout/read error.
boolean readString(char *dest, int maxLen) {
  int c, len = 0;

  while((c = timedRead()) != '\"') { // Read until closing quote
    if(c == '\\') {    // Escaped char follows
      c = timedRead(); // Read it
      // Certain escaped values are for cursor control --
      // there might be more suitable printer codes for each.
      if     (c == 'b') c = '\b'; // Backspace
      else if(c == 'f') c = '\f'; // Form feed
      else if(c == 'n') c = '\n'; // Newline
      else if(c == 'r') c = '\r'; // Carriage return
      else if(c == 't') c = '\t'; // Tab
      else if(c == 'u') c = unidecode(4);
      else if(c == 'U') c = unidecode(8);
      // else c is unaltered -- an escaped char such as \ or "
    } // else c is a normal unescaped char

    if(c < 0) return false; // Timeout

    // In order to properly position the client stream at the end of
    // the string, characters are read to the end quote, even if the max
    // string length is reached...the extra chars are simply discarded.
    if(len < maxLen) dest[len++] = c;
  }

  dest[len] = 0;
  return true; // Success (even if empty string)
}

// ---------------------------------------------------------------------------

// Read a given number of hexadecimal characters from client stream,
// representing a Unicode symbol.  Return -1 on error, else return nearest
// equivalent glyph in printer's charset.  (See notes below -- for now,
// always returns '-' or -1.)
int unidecode(byte len) {
  int c, v, result = 0;
  while(len--) {
    if((c = timedRead()) < 0) return -1; // Stream timeout
    if     ((c >= '0') && (c <= '9')) v =      c - '0';
    else if((c >= 'A') && (c <= 'F')) v = 10 + c - 'A';
    else if((c >= 'a') && (c <= 'f')) v = 10 + c - 'a';
    else return '-'; // garbage
    result = (result << 4) | v;
  }

  // To do: some Unicode symbols may have equivalents in the printer's
  // native character set.  Remap any such result values to corresponding
  // printer codes.  Until then, all Unicode symbols are returned as '-'.
  // (This function still serves an interim purpose in skipping a given
  // number of hex chars while watching for timeouts or malformed input.)

  return '-';
}

// ---------------------------------------------------------------------------

// Read from client stream with a 5 second timeout.  Although an
// essentially identical method already exists in the Stream() class,
// it's declared private there...so this is a local copy.
int timedRead(void) {
  int           c;
  unsigned long start = millis();

  while((!client.available()) && ((millis() - start) < 5000L));

  return client.read();  // -1 on timeout
}

// ---------------------------------------------------------------------------

// Timer1 interrupt handler for sleep throb
ISR(TIMER1_OVF_vect, ISR_NOBLOCK) {
  // Sine table contains only first half...reflect for second half...
  analogWrite(led_pin, pgm_read_byte(&sleepTab[
    (sleepPos >= sizeof(sleepTab)) ?
    ((sizeof(sleepTab) - 1) * 2 - sleepPos) : sleepPos]));
  if(++sleepPos >= ((sizeof(sleepTab) - 1) * 2)) sleepPos = 0; // Roll over
  TIFR1 |= TOV1; // Clear Timer1 interrupt flag
}

