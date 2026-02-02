#include <Arduino.h>
#include "HardwareSerial.h"
#include "CoilData.h"
#include "ModbusServerRTU.h"
#include <Preferences.h>

#define GPIO_LED 8 //LED beim supermini
#define GPIO_BUTTON 9
#define GPIO_SSR1 7 //Pin 7 steuert Magnetventil 1
#define GPIO_SSR2 8 //Pin 8 steuert Magnetventil 2

Preferences preferences;

// Input Registers (solo lectura)
uint16_t inputRegs[1] = {1234};   // <---  VARIABLE

ModbusServerRTU MBserver(2000, GPIO_NUM_10);

CoilData myCoils(1);      // Setup einer einzelnen Coil
bool coilTrigger = false; // Trigger, der gesetzt wird, wann immer eine Coil geschrieben wurde
uint16_t serverID = 1;
bool lastButtonState;
bool discoveryMode;

// Handler für Broacast-Nachrichten
//-nutzen wir zum setzen der Server-ID für jenen Server, der sich gerade im Discovery-Mode befindet
void BroadcastWorker(const ModbusMessage &request)
{
  uint16_t address;
  uint16_t words;
  ModbusMessage response;

  request.get(2, address);
  request.get(4, words);

  if (discoveryMode != true || request.getFunctionCode() != WRITE_HOLD_REGISTER || address != 255 || words < 1 // 0 wäre auch falsch - das ist die Broadcast-Adresse
      || words > 255)
  {
    return;
  }

  serverID = words;

  // Speichern der neuen Server-ID im Flash
  preferences.putShort("serverID", serverID);

  Serial.printf("ServerID ist %u\n", serverID);

  // Discovery Mode beenden
  discoveryMode = false;
  Serial.println("Discovery Mode off");

  // digitalWrite(GPIO_LED, HIGH); //LED beim supermini; obsolet
  // digitalWrite(LED_BUILTIN, HIGH); //LED beim devkitc-02; obsolet
}

// Handler für Write Coil Nachrichten
//-nutzen wir zum An- und Ausschalten Coil, und damit letztlich der LED und SSR
ModbusMessage write_coil(ModbusMessage request)
{

  ModbusMessage response;

  uint16_t start = 0;
  uint16_t state = 0;
  request.get(2, start, state);

  if (request.getServerID() == serverID && start <= myCoils.coils())
  { // gültige Coil?
    if (state == 0x0000 || state == 0xFF00)
    { // Parameter hat einen gültigen Wert (0x0000 (AUS) oder 0xFF00 (AN))
      if (myCoils.set(start, state))
      {

        response = ECHO_RESPONSE;

        coilTrigger = true;
      }
      else
      {
        response.setError(request.getServerID(), request.getFunctionCode(), SERVER_DEVICE_FAILURE); // Setzen der Coil fehlgeschlagen
      }
    }
    else
    {
      response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_VALUE); // Parameter hat einen ungültigen Wert
    }
  }
  else
  {
    response.setError(request.getServerID(), request.getFunctionCode(), ILLEGAL_DATA_ADDRESS); // ungültige Coil
  }

  return response;
}

// ---------------------- Handler FC04 ----------------------
ModbusMessage readInput(ModbusMessage request) {
  uint16_t start, count;
  request.get(2, start, count);

  ModbusMessage response;

  response.add(request.getServerID(), READ_INPUT_REGISTER);
  response.add(static_cast<uint8_t>(count * 2));

  for (uint16_t i = 0; i < count; i++) {
    response.add(inputRegs[start + i]);
  }

  return response;
}

void setup()
{

  Serial.begin(9600);
  while (!Serial)
  {
  }

  pinMode(LED_BUILTIN, OUTPUT); // LED beim devkitc-02
  pinMode(GPIO_SSR1, OUTPUT);
  pinMode(GPIO_SSR2, OUTPUT);

  //LED defaultmäßig aus
  digitalWrite(LED_BUILTIN, LOW); // LED beim devkitc-02

  //Magnetventile defaultmäßig aus
  digitalWrite(GPIO_SSR1, LOW);
  digitalWrite(GPIO_SSR2, LOW);

  pinMode(GPIO_BUTTON, INPUT_PULLUP);

  lastButtonState = digitalRead(GPIO_BUTTON);

  // Lesen der Server-ID aus dem Flash, falls bereits vorhanden
  preferences.begin("serverID", false);
  uint16_t tmp = preferences.getShort("serverID");
  if (tmp != 0)
  {
    serverID = tmp;
  }

  Serial.printf("Irrigation Tile. ServerID %u\n", serverID);

  RTUutils::prepareHardwareSerial(Serial1);

  Serial1.begin(115200, SERIAL_8N1, GPIO_NUM_5, GPIO_NUM_4);

  // MBserver.useModbusASCII(); //einkommentieren, wenn ModBus ASCII verwendet werden soll

  MBserver.registerWorker(0x00, WRITE_COIL, write_coil);

    // Registrar handler para FC04
  MBserver.registerWorker(0x00, READ_INPUT_REGISTER, readInput);

  MBserver.registerBroadcastWorker(BroadcastWorker);

  MBserver.begin(Serial1);
}

void loop()
{

  // Serial.println("ja das Programm ist geladen");

  // Ein- und Ausschalten von LED und SSR1 je nach Wert der Coils-Nachricht
  static bool lastValue = false; // TODO: global machen?
  if (coilTrigger)
  {
    if (myCoils[0] != lastValue)
    { // enthält die empfangene Nachricht einen neuen Wert für die Coil?
      myCoils.print("Coil 0 changed to ", Serial);
      lastValue = myCoils[0];

      digitalWrite(LED_BUILTIN, myCoils[0] ? HIGH : LOW); // LED beim devkitc-02

      digitalWrite(GPIO_SSR1, myCoils[0] ? HIGH : LOW);
    }
    coilTrigger = false;
  }

  //TODO: Ein-und Ausschalten von SSR2

  // Ein- und Ausschalten des Discovery Mode bei Drücken des Buttons
  bool actualButtonState = digitalRead(GPIO_BUTTON);
  if (actualButtonState != lastButtonState)
  {
    if (actualButtonState == LOW)
    {
      if (!discoveryMode)
      {
        discoveryMode = true;
        Serial.println("Discovery Mode");
      }
      else
      {
        discoveryMode = false;
        Serial.println("Discovery Mode off");
      }
    }
    lastButtonState = actualButtonState;
  }
}