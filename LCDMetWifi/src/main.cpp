#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <Adafruit_MCP23017.h>
#include "WiFi.h"
#include "PubSubClient.h" //pio lib install "knolleary/PubSubClient"
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
//LCD constants
const int LCD_NB_ROWS = 2;
const int LCD_NB_COLUMNS = 16;
//Mqtt constants
#define SSID "NETGEAR68"
#define PWD "excitedtuba713"
#define MQTT_SERVER "192.168.1.2" // could change if the setup is moved
#define MQTT_PORT 1883

WiFiClient espClient;
PubSubClient client(espClient);
char msg[50];
unsigned long lastMillis = 0;
unsigned long currentMillis = 0;
static int oefeningNmr = 0;
static int cijferSlotnummer = esp_random() % 9;
static byte percent = 0; // percentage van de batterij
static int stap = 3;     //stapgrootte waarme batterij stijgt
static boolean oefeningJuist = false;
static boolean pauze = false; //boolean voor Afstand en ontsmetting.(false=> alles kan door gaan/True batterij wordt niet opgeladen)
static boolean toestandVerzonden = false;
static String boodschapLCD = "....."; //Geeft weer of oefening juist of fout is.
static boolean telefoonGebeld = false;
static boolean cijfergestuurd; //zo dat we maar 1 keer cijfer doorsturen

//Blokjes op LCD vastleggen
byte DIV_0_OF_5[8] = {
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000}; // 0 / 5
byte DIV_1_OF_5[8] = {
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000}; // 1 / 5
byte DIV_2_OF_5[8] = {
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000}; // 2 / 5
byte DIV_3_OF_5[8] = {
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100}; // 3 / 5
byte DIV_4_OF_5[8] = {
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110}; // 4 / 5
byte DIV_5_OF_5[8] = {
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111}; // 5 / 5
void setup_progressbar()
{
  lcd.createChar(0, DIV_0_OF_5);
  lcd.createChar(1, DIV_1_OF_5);
  lcd.createChar(2, DIV_2_OF_5);
  lcd.createChar(3, DIV_3_OF_5);
  lcd.createChar(4, DIV_4_OF_5);
  lcd.createChar(5, DIV_5_OF_5);
}
void draw_progressbar(byte percent) //batterij, reeks.. en boodschap printen
{
  //Oef en boodschap weergeven
  lcd.setCursor(0, 0);
  lcd.print("reeks");
  lcd.print(oefeningNmr);
  lcd.setCursor(7, 0);
  lcd.print(boodschapLCD);
  lcd.setCursor(0, 1);
  byte nb_columns = map(percent, 0, 100, 0, LCD_NB_COLUMNS * 5);
  //Elke kararkter tekenen voor elke kollom
  for (byte i = 0; i < LCD_NB_COLUMNS; ++i)
  {
    if (nb_columns == 0)
    { // Leeg geval
      lcd.write((byte)0);
    }
    else if (nb_columns >= 5)
    { // vol geval
      lcd.write(5);
      nb_columns -= 5;
    }
    else
    { //tussenin
      lcd.write(nb_columns);
      nb_columns = 0;
    }
  }
}
//methodes voor wifi
void callback(char *topic, byte *message, unsigned int length);

// functie wifi connectie
void setup_wifi()
{
  delay(10);
  Serial.println("Connecting to WiFi..");
  WiFi.begin(SSID, PWD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
// callback function, only used when receiving messages
void callback(char *topic, byte *message, unsigned int length)
{
  String messageTemp;
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "esp32/fitness/nmrOef")
  {
    oefeningNmr = messageTemp.toInt();
    draw_progressbar(percent);
    //Serial.print(oefeningNmr);
  }

  if (String(topic) == "esp32/fitness/control")
  {
    //'2' START(ontsmetten)
    if (messageTemp == "2")
    {
      pauze = false;
    }
    //'1' STOP (afstand)
    else if (messageTemp == "1")
    {
      pauze = true;
    }
    //'0'=RESET
    else if (messageTemp == "0")
    {
      //setup();
      ESP.restart();
    }
  }
  //Cijfer vrijgeven via broker moesten spelers er niet in slagen om de reeks te vervolledigen.
  if (String(topic) == "esp32/fitness/geforceerdCijfer")
  {
    if (messageTemp == "vrijgeven")
    {
      lcd.print(cijferSlotnummer);
      draw_progressbar(percent);
    }
  }
  if (String(topic) == "esp32/fitness/OKmessage")
  {
    {
      if (messageTemp == "oefOK")
      {
        oefeningJuist = true;
      }
      else
        oefeningJuist = false;
    }
  }

  if (String(topic) == "esp32/fitness/LCDmessage")
  {
    if (messageTemp == "juist") //serie juist
    {
      boodschapLCD = "JUIST:) ";
      //draw_progressbar(percent);
      //cijfer alohomora vrijgeven
      lcd.setCursor(15, 0);
      lcd.print(cijferSlotnummer);
      //delay(1000);
      //morsecode inlichten
      if (telefoonGebeld == false)
      {
        client.publish("esp32/fitness/telefoon", "BEL");
        telefoonGebeld = true;
      }
      draw_progressbar(percent);
    }
    else if (messageTemp == "fout") //serie fout
    {
      boodschapLCD = "FOUT!    ";
      if ((percent - 5) > 0) // een foute beweging zal de batterij 10% laten afnemen
      {
        percent = percent - 5;
      }
      else
      {
        percent = 0;
      }
    }
    else if (messageTemp == "meten")
    {
      boodschapLCD = "METEN    ";
    }

    else if (messageTemp == "klaar")
    {
      boodschapLCD = "ZET KLAAR";
    }
    draw_progressbar(percent);
  }
}

// function to establish MQTT connection
void reconnect()
{
  delay(10);
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32fitness"))
    {
      Serial.println("connected");

      // ... and resubscribe
      client.subscribe("esp32/fitness/#");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
void setup()
{
  Serial.begin(115200);
  //initialiseren van LCD
  lcd.begin(LCD_NB_COLUMNS, LCD_NB_ROWS);
  lcd.clear();
  setup_progressbar();

  cijfergestuurd = false; //boolean zodat we maar 1 keer cijfer doorsturen

  //Wifi
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  draw_progressbar(percent);
}

void loop()
{

  //loop voor wifi
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  //passing the number for alohomara
  Serial.print(cijfergestuurd);
  if (cijfergestuurd == false)
  {
    Serial.print("Test");
    cijfergestuurd = true;
    String nummer = (String)cijferSlotnummer;
    const char *nmr = nummer.c_str();
    client.publish("esp32/alohomora/code1", nmr);
  }
  //zolang geen reeksnummer ontvangen, request messages sturen
  if (oefeningNmr == 0)
  {
    delay(3000);
    client.publish("esp32/fitness/request", "request");
    draw_progressbar(percent);
  }

  //'3' POWEROFF (stop voor fitness)
  if (percent == 0 && toestandVerzonden == false)
  {
    client.publish("esp32/fitness/control", "3");
    client.publish("esp32/vaccin/control", "3");
    client.publish("esp32/morse/control", "3");
    client.publish("esp32/5g/control", "3");

    toestandVerzonden = true;
  }
  //'4' POWERON(start voor fitness)
  if (percent > 0 && toestandVerzonden == true)
  {
    client.publish("esp32/fitness/control", "4");
    client.publish("esp32/vaccin/control", "4");
    client.publish("esp32/morse/control", "4");
    client.publish("esp32/5g/control", "4");

    toestandVerzonden = false;
  }

  //loop LCD
  if (oefeningJuist == true && (percent < 100) && pauze == false)
  {
    lastMillis = millis();
    oefeningJuist = false;
    if ((percent + stap) < 100)
    {
      percent = percent + stap;
    }
    else
    {
      percent = 100;
    }
    draw_progressbar(percent);
  }

  //Als 10 seconden geen oefening juist dan zal batterij 1% dalen
  currentMillis = millis();
  if (currentMillis - lastMillis > 10000 && percent > 0)
  {
    percent = percent - 1;
    lastMillis = millis();
    draw_progressbar(percent);
  }
}