#pragma once

// Copy this file to secrets.h and replace the example values locally.
// secrets.h is ignored by git. Never place production credentials in source control.
#define IAQ_WIFI_SSID "replace-with-wifi-ssid"
#define IAQ_WIFI_PASSWORD "replace-with-wifi-password"
#define IAQ_MQTT_HOST "mqtt.example.net"
#define IAQ_MQTT_PORT 8883
#define IAQ_MQTT_USERNAME "device-user"
#define IAQ_MQTT_PASSWORD "replace-with-device-password"
#define IAQ_DEVICE_ID "iaq-node-001"

// Paste the trusted broker CA certificate here. Do not use setInsecure().
#define IAQ_MQTT_ROOT_CA \
	"-----BEGIN CERTIFICATE-----\n" \
	"replace-with-the-broker-root-ca-certificate\n" \
	"-----END CERTIFICATE-----\n"