# Wi-Fi Room Mapper

Wi-Fi Room Mapper uses an ESP32 to show where Wi-Fi signals are stronger or weaker in a room. You collect readings at different spots, then view a coloured heat map in your phone or computer browser.

## How it works

1. The ESP32 creates its own Wi-Fi hotspot and serves a small web page.
2. You connect your phone or laptop to that hotspot and open the page.
3. You move the ESP32 around the room, select its position on the map, and take a scan.
4. Each scan records nearby Wi-Fi networks and their signal strength.
5. The browser combines your readings to draw a heat map for a selected access point.

**The ESP32 takes the measurements.** Move the board itself to each spot, even if you control the page from a phone.

The heat map estimates signal strength between your measurement points. It does not measure internet speed or automatically detect your position or walls.

## What you need

- An ESP32 board supported by the Arduino ESP32 board package.
- A USB cable that supports data transfer.
- A computer with Arduino IDE and the ESP32 board package installed.
- A phone or laptop with Wi-Fi and a web browser.
- A way to keep the ESP32 powered while moving it, such as a USB power bank.

The sketch uses `WiFi.h` and `WebServer.h`, included with the Arduino ESP32 board package, plus the standard C++ `vector` library.

## Set up the ESP32

1. Open `wifi_room_mapper.ino` in Arduino IDE.
2. Near the top of the sketch, change the hotspot settings:

   ```cpp
   static const char *AP_SSID = "WiFi-Room-Mapper";
   static const char *AP_PASSWORD = "change-this-password";
   ```

   Choose your own password with at least 8 characters.

3. Connect the ESP32 to your computer by USB.
4. Select your board and serial port in Arduino IDE. Use **ESP32 Dev Module** if it matches your board. Keep the default upload speed initially.
5. Upload the sketch.
6. Open Serial Monitor at **115200 baud**. The ESP32 prints its web address after starting. Reset the board if you missed the message.

## Make a room map

1. On your phone or laptop, connect to **WiFi-Room-Mapper**, or the hotspot name you chose. Enter your configured password.
2. Stay connected if your device warns that the network has no internet. Internet access is not needed to use the mapper.
3. Open the address printed in Serial Monitor in your browser, usually **http://192.168.4.1**. Use `http://`.
4. Enter the room width and height in metres.
5. Move the powered ESP32 to your first measurement spot.
6. Tap or click the matching position in the room rectangle.
7. Press **Scan at selected point** and wait until the page says the reading was saved. Keep the board still and leave the selected point unchanged during the scan.
8. Repeat at several spots spread across the room.
9. Choose an entry from **Heat map network**. A heat map appears once that access point has been detected at **at least four saved points**.

Green means a stronger signal; red means a weaker signal. Extra readings near corners and weak areas can make the estimate more useful.

Networks are identified by their **BSSID**, the address of an individual access point. Two entries can have the same Wi-Fi name but different BSSIDs, so collect four readings for the same entry.

Room dimensions label the coordinates only. Changing them keeps points in their relative positions and does not resize the rectangle to match the room's proportions.

## Save and manage readings

Readings are saved automatically in that browser's local storage. The ESP32 keeps the latest scan temporarily in memory; it does not permanently store your room survey.

- **Export JSON** downloads `wifi-room-survey.json`. Use this to back up your work or move it to another browser.
- **Import** loads an exported survey and replaces the current one.
- **Delete** removes an individual measurement.
- **Clear selected point** cancels your point selection without deleting saved readings.
- **Clear project** removes the saved room and readings from that browser after confirmation.

Use the same browser and web address to return to your saved survey. Clearing browser data can erase it, so export a backup when finished.

## Common problems

- **The page will not open:** Check that you are connected to the ESP32 hotspot and use the address shown in Serial Monitor.
- **Your phone disconnects:** Choose to stay connected to the network without internet. If needed, temporarily disable automatic network switching.
- **A scan fails:** Rejoin the hotspot, reload the page, and try again.
- **No heat map appears:** Select a network and make sure the same BSSID was detected at four or more saved points.
- **A network is missing:** The ESP32 can only detect networks supported by its Wi-Fi hardware. Check your board's supported bands.
