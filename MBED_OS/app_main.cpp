// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license.

#define MQTT_DEBUG 1
#include <stdlib.h>

#include "mbed.h"
#include "NetworkInterface.h"
#include <string.h>

#include "src/iotc/iotc.h"
#include "src/iotc/common/string_buffer.h"

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

const char *scopeId = "0ne00028D69";
const char *deviceId = "ublox";
const char *deviceKey = "Z0gDJBpRekNOiyPK4MOMrTwPyD7k/gjoKO2mOodXG9k=";

static IOTContext context = NULL;
static bool isConnected = false;

void onEvent(IOTContext ctx, IOTCallbackInfo *callbackInfo)
{
    if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0)
    {
        LOG_VERBOSE("Is connected ? %s (%d)", callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO", callbackInfo->statusCode);
        isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    }

    AzureIOT::StringBuffer buffer;
    if (callbackInfo->payloadLength > 0)
    {
        buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
    }
    LOG_VERBOSE("- [%s] event was received. Payload => %s", callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

    if (strcmp(callbackInfo->eventName, "Command") == 0)
    {
        LOG_VERBOSE("- Command name was => %s\r\n", callbackInfo->tag);
    }
}

static unsigned prevMillis = 0, loopId = 0;
void loop()
{
    if (isConnected)
    {
        unsigned long ms = us_ticker_read() / 1000;
        if (ms - prevMillis > 15000)
        { // send telemetry every 15 seconds
            char msg[128] = {0};
            int pos = 0, errorCode = 0;

            prevMillis = ms;
            if (loopId++ % 2 == 0)
            { // send telemetry
                pos = snprintf(msg, sizeof(msg) - 1, "{\"accelerometerX\": %d,\"accelerometerY\": %d,\"accelerometerZ\": %d,\"temp\": %d}", 10 + (rand() % 20), 6 + (rand() % 20), 18 + (rand() % 20), 10 + (rand() % 20));
                errorCode = iotc_send_telemetry(context, msg, pos);
            }
            else
            { // send property
                pos = snprintf(msg, sizeof(msg) - 1, "{\"sold\":%d}", 1 + (rand() % 5));
                errorCode = iotc_send_property(context, msg, pos);
            }
            msg[pos] = 0;

            if (errorCode != 0)
            {
                LOG_ERROR("Sending message has failed with error code %d", errorCode);
            }
        }

        iotc_do_work(context); // do background work for iotc
    }
}

int main(int argc, char *argv[])
{
    printf("Welcome to IoTCentral Ublox Environment");
    printf("Mbed OS version: %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
    NetworkInterface *network = NetworkInterface::get_default_instance();
    if (!network)
    {
        LOG_VERBOSE("Unable to open network interface.");
        return -1;
    }

    // if this sample is unable to pass NTP phase. enable the logic below to fix DNS issue
    // it is a silly hack to set dns server as a router assuming router is sitting at .1

    LOG_VERBOSE("Network interface opened successfully.");
    WiFiInterface *wifi = network->wifiInterface();
    EthInterface *eth = network->ethInterface();
    CellularBase *cell = network->cellularBase();
    printf("Available interfaces...");
    if (wifi)
    {
        printf("WIFI,");
    }
    if (eth)
    {
        printf("ETHERNET,");
    }
    if (cell)
    {
        printf("CELLULAR");
    }
    network->connect();
    LOG_VERBOSE("Ip address: %s", network->get_ip_address());

    // call WiFi-specific methods

AzureIOT::StringBuffer routerAddress(network->get_ip_address(), strlen(network->get_ip_address()));
{
    int dotIndex = 0, foundCount = 0;
    for (int i = 0; i < 3; i++)
    {
        int pre = dotIndex;
        dotIndex = routerAddress.indexOf(".", 1, dotIndex + 1);
        if (dotIndex > pre)
        {
            foundCount++;
        }
    }

    if (foundCount != 3 || dotIndex + 1 > routerAddress.getLength())
    {
        LOG_ERROR("IPv4 address is expected."
                  "(retval: %s foundCount:%d dotIndex:%d)",
                  *routerAddress, foundCount, dotIndex);
        return 1;
    }

    routerAddress.set(dotIndex + 1, '1');
    if (routerAddress.getLength() > dotIndex + 1)
    {
        routerAddress.setLength(dotIndex + 2);
    }
    LOG_VERBOSE("routerAddress is assumed at %s", *routerAddress);
}
SocketAddress socketAddress(*routerAddress);
network->add_dns_server(socketAddress);
iotc_set_logging(IOTC_LOGGING_ALL);
iotc_set_network_interface(network);

int errorCode = iotc_init_context(&context);
if (errorCode != 0)
{
    LOG_ERROR("Error initializing IOTC. Code %d", errorCode);
    return 1;
}

//iotc_set_logging(IOTC_LOGGING_API_ONLY);

// for the simplicity of this sample, used same callback for all the events below
iotc_on(context, "MessageSent", onEvent, NULL);
iotc_on(context, "Command", onEvent, NULL);
iotc_on(context, "ConnectionStatus", onEvent, NULL);
iotc_on(context, "SettingsUpdated", onEvent, NULL);
iotc_on(context, "Error", onEvent, NULL);
LOG_VERBOSE("Connecting to IoTCentral");
//errorCode = iotc_connect(context, scopeId, connstring, deviceId, IOTC_CONNECT_CONNECTION_STRING);
errorCode = iotc_connect(context, scopeId, deviceKey, deviceId, IOTC_CONNECT_SYMM_KEY);
if (errorCode != 0)
{
    LOG_ERROR("Error @ iotc_connect. Code %d", errorCode);
    return 1;
}
LOG_VERBOSE("----------------Client is connected --------------------------");
prevMillis = us_ticker_read() / 1000;

while (true)
    loop();

return 1;
}
