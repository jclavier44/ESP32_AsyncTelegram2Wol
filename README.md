# ESP32 Wake-on-LAN Telegram Bot

A Telegram bot running on ESP32 that allows you to remotely wake up, check status, and shutdown computers on your local network via Wake-on-LAN (WoL).

## Features

- **Wake-on-LAN**: Send magic packets to wake up PCs from sleep/shutdown
- **Status Check**: Verify if PCs are online by testing common ports (SMB, SSH, HTTP, RDP, VNC)
- **Remote Shutdown**: Send UDP shutdown commands to running PCs
- **Multi-PC Support**: Configure multiple target PCs
- **User Access Control**: Only authorized Telegram user ID can control the bot

## Hardware

- ESP32 development board (any ESP32 variant works)
- Micro-USB cable for power and programming
- WiFi network

## Required Libraries

Install these from Arduino Library Manager:

- **ArduinoJson** by Benoit Blanchon
- **AsyncTelegram2** by cotestatnt
- **WakeOnLan** by a7md0

## Configuration

Edit `config.h` with your settings:

```c
// WiFi
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASS "YourPassword"

// Telegram
#define BOT_TOKEN "your_bot_token_from_botfather"
#define TELEGRAM_USER_ID 123456789  // Your Telegram user ID

// Target PCs (add as many as needed)
#define MAC_PC_1 "AA:BB:CC:DD:EE:FF"
#define NAME_PC_1 "Office PC"
#define MAC_PC_2 "11:22:33:44:55:66"
#define NAME_PC_2 "Gaming PC"
```

To get your bot token:
1. Open Telegram and search for @BotFather
2. Use /newbot command to create a bot
3. Copy the token

To get your user ID:
1. Search for @userinfobot on Telegram
2. Send any message
3. The bot will display your user ID

## Usage

Send these commands to your Telegram bot:

| Command | Description |
|---------|-------------|
| `/start` or `/help` | Show available commands and configured PCs |
| `/list` | List all configured PCs with their IP and MAC addresses |
| `/wakeup 192.168.1.X` | Send WoL magic packet to wake PC |
| `/status 192.168.1.X` | Check if specific PC is online |
| `/status all` | Check all configured PCs |
| `/shutdown 192.168.1.X` | Send UDP shutdown command |

## PC Requirements

### Wake-on-LAN
- Enable WoL in BIOS
- Enable WoL in Network Adapter settings (Device Manager → Network Adapter → Properties → Wake on LAN)
- Ensure router allows broadcast traffic to port 9

### Shutdown Command
- PC must run a listener script to receive UDP shutdown commands
- Requires administrator/sudo privileges
- Example Python listener: see below

## Python Shutdown Listener (Optional)

To use the shutdown feature, copy `shutdown_listener.py` to your target PC.

### Running Manually

```bash
python3 shutdown_listener.py
```

### Running as a Systemd Service (Linux/Debian)

1. Copy the script to a permanent location:
   ```bash
   sudo cp shutdown_listener.py /usr/local/bin/
   sudo chmod +x /usr/local/bin/shutdown_listener.py
   ```

2. Create a systemd service file:
   ```bash
   sudo nano /etc/systemd/system/shutdown-listener.service
   ```

3. Add the following content:
   ```ini
   [Unit]
   Description=ESP32 Shutdown Listener
   After=network-online.target
   Wants=network-online.target

   [Service]
   Type=simple
   User=root
   WorkingDirectory=/root
   ExecStart=/usr/bin/python3 /usr/local/bin/shutdown_listener.py
   Restart=always
   RestartSec=10
   StandardOutput=journal
   StandardError=journal

   [Install]
   WantedBy=multi-user.target
   ```

4. Enable and start the service:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable shutdown-listener.service
   sudo systemctl start shutdown-listener.service
   ```

5. Check status:
   ```bash
   sudo systemctl status shutdown-listener.service
   ```

6. View logs:
   ```bash
   journalctl -u shutdown-listener.service -f
   ```

**Note**: The service runs as root to allow the shutdown command. Using `network-online.target` ensures the network is ready before the listener starts.

**Security Note**: Anyone on your local network can send shutdown commands if they know the IP and port. Consider adding authentication or restricting access.

## Wiring

No external wiring needed. Just connect ESP32 via USB and ensure it's on the same network as target PCs.

## Troubleshooting

- **Bot won't connect**: Check WiFi credentials and signal strength
- **WoL not working**: Verify WoL is enabled on target PC BIOS and network adapter
- **Status always shows offline**: Check firewall blocking ports 445, 22, 80, 3389, 5900
- **Shutting down wrong PC**: Verify IP addresses in `config.h` match actual network

## License

MIT License