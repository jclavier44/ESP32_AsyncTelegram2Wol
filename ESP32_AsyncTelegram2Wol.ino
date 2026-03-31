/*
  ESP32 Wake-on-LAN Telegram Bot
  Libraries required:
  - WiFi (built-in)
  - WiFiClientSecure (built-in)
  - ArduinoJson by Benoit Blanchon
  - AsyncTelegram2 by cotestatnt
  - WakeOnLan by a7md0
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <AsyncTelegram2.h>
#include <WakeOnLan.h>
#include "config.h"

// ==================== CONFIGURATION ====================
// WiFi credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

// Telegram Bot Token (get from @BotFather)
const char* botToken = BOT_TOKEN;
int64_t userid = TELEGRAM_USER_ID;

// Target PCs configuration
struct TargetPC {
  const char* ip;
  const char* mac;
  const char* name;
};

// Add your PCs' MAC addresses here (format: "XX:XX:XX:XX:XX:XX")
TargetPC pcs[] = {
  {"192.168.1.60", MAC_PC_1, NAME_PC_1},
  {"192.168.1.61", MAC_PC_2, NAME_PC_2}
};
const int numPCs = sizeof(pcs) / sizeof(pcs[0]);

// UDP ports
const int WOL_PORT = 9;           // Standard Wake-on-LAN port
const int SHUTDOWN_PORT = 7777;   // Custom shutdown message port

// ==================== GLOBAL OBJECTS ====================
WiFiClientSecure client;
AsyncTelegram2 myBot(client);

// FIX: WakeOnLan needs WiFiUDP, not WiFiClientSecure
WiFiUDP udp;
WakeOnLan WOL(udp);

WiFiUDP shutdownUDP;

// ==================== FUNCTION DECLARATIONS ====================
bool sendWOL(const char* mac);
// FIX: Removed default parameter to avoid ambiguity
bool pingHost(const char* ip);
bool sendShutdownUDP(const char* ip);
void showMenu(const TBMessage &msg);

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32 Wake-on-LAN Telegram Bot");
  Serial.println("=================================\n");

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize WakeOnLan settings
  WOL.setRepeat(3, 100); // Send 3 magic packets with 100ms delay
  
  // Calculate broadcast address automatically
  WOL.calculateBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
  
  Serial.print("Broadcast address: ");
  IPAddress localIP = WiFi.localIP();
  IPAddress subnet = WiFi.subnetMask();
  IPAddress broadcast(
    (localIP[0] & subnet[0]) | (~subnet[0] & 0xFF),
    (localIP[1] & subnet[1]) | (~subnet[1] & 0xFF),
    (localIP[2] & subnet[2]) | (~subnet[2] & 0xFF),
    (localIP[3] & subnet[3]) | (~subnet[3] & 0xFF)
  );
  Serial.println(broadcast);

  // Initialize UDP for shutdown messages
  shutdownUDP.begin(SHUTDOWN_PORT);
  
  // Setup Telegram bot
  client.setInsecure(); // For simplicity (production: use certificate)
  myBot.setUpdateTime(1000);
  myBot.setTelegramToken(botToken);
  
  // Test Telegram connection
  Serial.print("Testing Telegram connection...");
  if (myBot.begin()) {
    Serial.println(" ✅ OK");
  } else {
    Serial.println(" ❌ FAILED - check your bot token!");
    while(true) delay(1000); // Halt if can't connect
  }

  Serial.println("\n🤖 Bot is ready!");
  Serial.println("Available commands:");
  Serial.println("  /start or /help - Show menu");
  Serial.println("  /wakeup 192.168.1.X - Wake up PC");
  Serial.println("  /status 192.168.1.X - Check if PC is online");
  Serial.println("  /status all - Check all PCs");
  Serial.println("  /shutdown 192.168.1.X - Send UDP shutdown message");
  Serial.println("  /list - Show configured PCs");
  Serial.println("=================================\n");
}

// ==================== LOOP ====================
void loop() {
  TBMessage msg;
  
  // Check for new messages
  if (myBot.getNewMessage(msg)) {
    String response = "";
    String text = msg.text;
    text.toLowerCase();
    
    Serial.print("📩 Received from ");
    Serial.print(msg.sender.username);
    Serial.print(": ");
    Serial.println(msg.text);
    
    // Handle commands
    if (text == "/start" || text == "/help") {
      showMenu(msg);
      return;
    }
    else if (text == "/list") {
      response = "📋 *Configured PCs:*\n\n";
      for (int i = 0; i < numPCs; i++) {
        response += String(i+1) + ". *" + pcs[i].name + "*\n";
        response += "   IP: `" + String(pcs[i].ip) + "`\n";
        response += "   MAC: `" + String(pcs[i].mac) + "`\n\n";
      }
    }
    else if (text.startsWith("/wakeup ")) {
      String ip = msg.text.substring(8); // Get original case to match
      ip.trim();
      
      bool found = false;
      for (int i = 0; i < numPCs; i++) {
        if (ip.equals(pcs[i].ip)) {
          found = true;
          response = "⏰ *Waking up " + String(pcs[i].name) + "*\n";
          response += "IP: `" + ip + "`\n";
          response += "MAC: `" + String(pcs[i].mac) + "`\n\n";
          
          if (sendWOL(pcs[i].mac)) {
            response += "✅ *Magic packet sent successfully!*\n";
            response += "The PC should wake up in a few seconds.\n\n";
            response += "_Note: Ensure Wake-on-LAN is enabled in BIOS and Network Adapter settings._";
          } else {
            response += "❌ *Failed to send magic packet!*";
          }
          break;
        }
      }
      if (!found) {
        response = "❌ *Unknown IP: " + ip + "*\n";
        response += "Use /list to see configured PCs.";
      }
    }
    else if (text.startsWith("/status ")) {
      String ip = msg.text.substring(8);
      ip.trim();
      
      if (ip == "all" || ip == "ALL") {
        response = "📊 *Status of all PCs:*\n\n";
        for (int i = 0; i < numPCs; i++) {
          bool online = pingHost(pcs[i].ip);
          response += "*" + String(pcs[i].name) + "* (`" + String(pcs[i].ip) + "`)\n";
          response += online ? "🟢 *ONLINE*\n\n" : "🔴 *OFFLINE*\n\n";
          delay(100); // Small delay between pings
        }
      } else {
        bool found = false;
        for (int i = 0; i < numPCs; i++) {
          if (ip.equals(pcs[i].ip)) {
            found = true;
            response = "📊 *Checking " + String(pcs[i].name) + "*\n";
            response += "IP: `" + ip + "`\n\n";
            
            bool online = pingHost(pcs[i].ip);
            if (online) {
              response += "🟢 *PC is ONLINE* ✅\n";
              response += "Responding to network requests.";
            } else {
              response += "🔴 *PC is OFFLINE* ❌\n";
              response += "Not responding (might be sleeping, powered off, or firewall blocking).";
            }
            break;
          }
        }
        if (!found) {
          response = "❌ *Unknown IP: " + ip + "*\n";
          response += "Use /list to see configured PCs.";
        }
      }
    }
    else if (text.startsWith("/shutdown ")) {
      String ip = msg.text.substring(10);
      ip.trim();
      
      bool found = false;
      for (int i = 0; i < numPCs; i++) {
        if (ip.equals(pcs[i].ip)) {
          found = true;
          response = "🛑 *Sending shutdown to " + String(pcs[i].name) + "*\n";
          response += "IP: `" + ip + "`\n";
          response += "Port: " + String(SHUTDOWN_PORT) + "\n\n";
          
          if (sendShutdownUDP(ip.c_str())) {
            response += "✅ *Shutdown message sent!*\n\n";
            response += "Make sure your PC is running a listener on UDP port " + String(SHUTDOWN_PORT) + ".\n\n";
            response += "_Example Python listener included in project files._";
          } else {
            response += "❌ *Failed to send shutdown message!*";
          }
          break;
        }
      }
      if (!found) {
        response = "❌ *Unknown IP: " + ip + "*\n";
        response += "Use /list to see configured PCs.";
      }
    }
    else {
      response = "❓ *Unknown command*\n\n";
      response += "Use /help to see available commands.";
    }
    
    // Send response
    if (response.length() > 0) {
      //myBot.sendMessage(msg, response, "Markdown");
      myBot.sendTo(userid, response);
      Serial.println("✅ Response sent: ");
      Serial.println(response);
    }
  }
  
  delay(10); // Small delay to prevent watchdog issues
}

// ==================== HELPER FUNCTIONS ====================

void showMenu(const TBMessage &msg) {
  String menu = "🖥️ *ESP32 Wake-on-LAN Controller*\n\n";
  menu += "*Available Commands:*\n\n";
  menu += "📥 `/wakeup 192.168.1.X`\n";
  menu += "   Send Wake-on-LAN magic packet\n\n";
  menu += "📊 `/status 192.168.1.X`\n";
  menu += "   Check if PC responds to ping\n\n";
  menu += "📊 `/status all`\n";
  menu += "   Check all configured PCs\n\n";
  menu += "🛑 `/shutdown 192.168.1.X`\n";
  menu += "   Send UDP shutdown message\n\n";
  menu += "📋 `/list`\n";
  menu += "   Show all configured PCs\n\n";
  menu += "❓ `/help`\n";
  menu += "   Show this menu\n\n";
  menu += "─────────────────────\n";
  menu += "*Configured IPs:*\n";
  for (int i = 0; i < numPCs; i++) {
    menu += "• `" + String(pcs[i].ip) + "` (" + pcs[i].name + ")\n";
  }
  
  //myBot.sendMessage(msg, menu, "Markdown");
  myBot.sendTo(userid, menu);
}

bool sendWOL(const char* mac) {
  Serial.print("Sending WOL to MAC: ");
  Serial.println(mac);
  
  // The WakeOnLan library handles the magic packet generation and sending
  // It uses the WiFiUDP object we passed to the constructor
  WOL.sendMagicPacket(mac, WOL_PORT);
  
  Serial.println("✅ Magic packet sent via UDP");
  return true;
}

// FIX: Removed default parameter, hardcoded timeout inside function
bool pingHost(const char* ip) {
  const int timeout = 800; // Hardcoded timeout value
  
  Serial.print("Pinging ");
  Serial.print(ip);
  Serial.print("... ");
  
  // Simple TCP connection test on common ports
  WiFiClient testClient;
  bool result = false;
  
  // Try common ports: 445 (SMB), 22 (SSH), 80 (HTTP), 3389 (RDP), 5900 (VNC)
  int ports[] = {445, 22, 80, 3389, 5900};
  
  for (int i = 0; i < 5; i++) {
    if (testClient.connect(ip, ports[i], timeout)) {
      result = true;
      testClient.stop();
      Serial.print("Port ");
      Serial.print(ports[i]);
      Serial.println(" open - ONLINE");
      return true;
    }
  }
  
  Serial.println("No response - OFFLINE");
  return false;
}

bool sendShutdownUDP(const char* ip) {
  Serial.print("Sending shutdown UDP to ");
  Serial.print(ip);
  Serial.print(":");
  Serial.println(SHUTDOWN_PORT);
  
  // Send a simple shutdown command
  const char* shutdownCmd = "SHUTDOWN_NOW";
  
  int result = shutdownUDP.beginPacket(ip, SHUTDOWN_PORT);
  if (result == 0) {
    Serial.println("❌ Failed to begin packet");
    return false;
  }
  
  shutdownUDP.write((const uint8_t*)shutdownCmd, strlen(shutdownCmd));
  
  if (shutdownUDP.endPacket()) {
    Serial.println("✅ Shutdown packet sent successfully");
    return true;
  } else {
    Serial.println("❌ Failed to send packet");
    return false;
  }
}