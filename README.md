## This repository has been archived

They contents were mirrored to https://github.com/adafruit/Adafruit_Learning_System_Guides/tree/master/Adafruit_Tweet_Receipt

The files in this repository accompanied an [Adafruit Learning System](https://learn.adafruit.com) tutorial that can no longer 
be built https://learn.adafruit.com/internet-of-things-printer/download 
This tutorial is out of date. It is highly suggested that other tutorials be consulted for more modern implementations. This code is being hosted for legacy purposes.

All code BSD License, please keep attribution to Adafruit Industries

Please consider buying your parts at [Adafruit.com](https://www.adafruit.com) to support open source code.

## IMPORTANT: THIS SOFTWARE CURRENTLY DOES NOT WORK

***Twitter has changed their API to require
SSL (Secure Sockets Layer) on -all- connections, a complex
operation beyond the Arduino's ability to handle.  The code is
being kept around on the chance that a suitable proxy service
becomes available...but at present we have no such service, no
code for such, nor a schedule or even a firm commitment to
pursue it.***

For projects requiring Twitter we now recommend
using an SSL-capable system such as Raspberry Pi.  For example:
https://github.com/adafruit/Python-Thermal-Printer

Gutenbird demo sketch: monitors one or more Twitter accounts
for changes, displaying updates on attached thermal printer.
Written by Adafruit Industries, distributed under BSD License.

******************************************************
Designed for the Adafruit Internet of Things printer 
http://www.adafruit.com/products/717 (discontinued)
******************************************************

REQUIRES ARDUINO IDE 1.0 OR LATER -- Back-porting is not likely to
occur, as the code is deeply dependent on the Stream class, etc.

Also requires Adafruit Thermal Printer Library:
  https://github.com/adafruit/Adafruit-Thermal-Printer-Library
and Adafruit fork of Peter Knight's Cryptosuite library for Arduino:
  https://github.com/adafruit/Cryptosuite

Required hardware includes an Ethernet-connected Arduino board such
as the Arduino Ethernet or other Arduino-compatible board with an
Arduino Ethernet Shield, plus an Adafruit Mini Thermal Receipt printer
and all related power supplies and cabling.

Resources:
http://www.adafruit.com/products/418 Arduino Ethernet
http://www.adafruit.com/products/284 FTDI Friend
http://www.adafruit.com/products/201 Arduino Uno
http://www.adafruit.com/products/201 Ethernet Shield
http://www.adafruit.com/products/597 Mini Thermal Receipt Printer
http://www.adafruit.com/products/600 Printer starter pack

### Uses Twitter 1.1 API (doesn't work anymore)
This REQUIRES a Twitter account and some account
configuration.  Start at dev.twitter.com, sign in with your Twitter
credentials, select "My Applications" from the avatar drop-down menu at the
top right, then "Create a new application."  Provide a name, description,
placeholder URL and complete the captcha, after which you'll be provided a
"consumer key" and "consumer secret" for your app.  Select "Create access
token" to also generate an "access token" and "access token secret."
ALL FOUR STRINGS must be copied to the correct positions in the globals below,
and configure the search string to your liking.  DO NOT SHARE your keys or
secrets!  If you put code on Github or other public repository, replace them
with dummy strings.
