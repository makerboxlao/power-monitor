# power-monitor
Collection of scripts for monitor power usage


config.h
```C++
#ifndef config_h
#define config_h

// Configuration to be used by pzem monitoring

// WiFi SSID Name
#define WIFI_SSID ""
// WiFi password
#define WIFI_PASS ""

// MySQL Server address
#define MYSQL_ADDR IPAddress( 127, 0, 0, 1 )
// MySQL port
#define MYSQL_PORT 3306
// MySQL username
#define MYSQL_USER ""
// MySQL password
#define MYSQL_PASS ""

#endif
```