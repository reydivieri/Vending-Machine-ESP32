#include <Wire.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>

// keypad
const byte ROWS = 4; // four rows
const byte COLS = 4; // four columns

#define I2CADDR 0x20

char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

byte rowPins[ROWS] = {7, 6, 5, 4};
byte colPins[COLS] = {3, 2, 1, 0};

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, I2CADDR, PCF8574);

// Length of Keycode
const int KEYCODE_SIZE = 10;

// Length of Keycode + '\0' char
char input_keypad_code[KEYCODE_SIZE + 1];

bool isComplete = false;
byte ctr = 0;

// LCD

int lcdColumns = 16;
int lcdRows = 2;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
const int FIRST_LCD_ROW = 0;
const int SECOND_LCD_ROW = 1;
const int LCD_TIME_DELAY = 1000;

// Wifi
const char *ssid = "Edu_Robotics";
const char *password = "labrobot21";

// motor
#define MOTOR_PIN1 25
#define MOTOR_PIN2 33

bool motorStatus1 = false;
bool motorStatus2 = false;

// declare GPIO sensor
const int input1 = 18;
const int input2 = 19;

char serverAddress[] = "192.168.1.181"; // server address
int port = 80;

// http client
WiFiClient client;
HttpClient http = HttpClient(client, serverAddress, port);

void initWiFi()
{
    // Set WiFi to station mode and disconnect from an AP if it was previously connected.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid, password);
    Serial.print("WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println('.');
        delay(1000);
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWifi Connected");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        printToLCD("Wifi Done", 2, 0, true);
        delay(2000);
        printToLCD("Input OrderID:", 0, 0, true);
    }
    else
    {
        Serial.println("Wifi Failed");
        printToLCD("Wifi Failed", 0, 0, true);
    }

    delay(3000);
}

void printToLCD(const String &message, uint8_t column, uint8_t row, bool isClear)
{
    if (isClear)
    {
        lcd.clear();
    }
    // set cursor to  column,  row
    lcd.setCursor(column, row);
    if (message.length() == 0)
    {
        lcd.setCursor(0, 1);
        for (int n = 0; n < 16; n++)
        {
            lcd.print(" ");
        }
    }
    else
    {
        lcd.print(message);
    }
}

// Clear LCD display
void clearLCDLineDisplay(uint8_t row)
{
    lcd.setCursor(0, row);
    for (int n = 0; n < 16; n++)
    {
        lcd.print(" ");
    }
}

void clearDataArray()
{
    int index = 0;
    while (input_keypad_code[index] != '\0')
    {
        input_keypad_code[index] = 0;
        index++;
    }
}

bool checkKeyCode(String key_code)
{
    Serial.println("Calling the API...");
    printToLCD("Calling the API", 0, 1, false);
    // Membuat objek JSON
    StaticJsonDocument<200> doc;
    doc["key_code"] = key_code;

    // Serialisasi JSON ke string
    String postData;
    serializeJson(doc, postData);

    // Mengirim HTTP POST request
    http.post("/skripsivm/php/api-key-order.php", "application/json", postData);

    // Membaca status kode dan respons dari server
    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);

    // Parsing respons JSON
    StaticJsonDocument<200> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        Serial.println("Input Length too short");
        printToLCD("Input Err", 0, 1, false);
        return false;
    }

    // Mengecek apakah key code valid
    bool isValid = responseDoc["valid"];
    return isValid;
}

void sendStockStatus(int ProductId, int jumlahproduk)
{
    // Mengupdate stok di server
    String contentType = "application/json";

    // Create JSON object
    StaticJsonDocument<200> doc;
    JsonArray array = doc.to<JsonArray>();

    // Tambahkan data produk
    JsonObject data = array.createNestedObject();
    data["id"] = ProductId;
    if (ProductId == 1)
    {
        data["stok"] = jumlahproduk; // Jumlah stok yang akan dikurangi
    }
    else if (ProductId == 2)
    {
        data["stok"] = jumlahproduk; // Jumlah stok yang akan dikurangi
    }
    else
    {
        Serial.println("Unknown product ID");
        return;
    }

    // Serialize JSON to string
    String postData;
    serializeJson(doc, postData);

    http.post("/skripsivm/php/api-stock-kurangi.php", contentType, postData);
    // /skripsivm/php/api-stock.php

    // Baca kode status dan respons dari server
    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
}

void motor()
{
    Serial.println("Validating with the API...");
    String serverName = "/skripsivm/php/api-get-produk.php";
    String serverPath = serverName + "?midtrans_order_id=" + input_keypad_code;
    http.get(serverPath);

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    // Parsing JSON
    const size_t capacity = JSON_ARRAY_SIZE(2) + 2 * JSON_OBJECT_SIZE(2) + 60;
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, response);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        Serial.println("Database Claim Error");
        printToLCD("Db Claim Err", 0, 1, false);
        return;
    }

    for (JsonObject item : doc.as<JsonArray>())
    {
        int idp = item["id_produk"];
        int jumlah = item["jumlah"];

        Serial.print("Id produk: ");
        Serial.println(idp);
        Serial.print("jumlah produk: ");
        Serial.println(jumlah);

        if (idp == 1)
        {
            sendStockStatus(idp, jumlah);
            digitalWrite(MOTOR_PIN1, LOW);

            motorStatus1 = true;
        }
        if (idp == 2)
        {
            sendStockStatus(idp, jumlah);
            digitalWrite(MOTOR_PIN2, LOW);

            motorStatus2 = true;
        }
        Serial.println("Motor Operation Complete");
    }
}

void keypadEvent(KeypadEvent key)
{

    switch (keypad.getState())
    {
    case PRESSED:
        if (key == '#')
        {
            isComplete = true;
            if (isComplete)
            {

                Serial.println(input_keypad_code);
                bool isValid = checkKeyCode(input_keypad_code);
                if (isValid)
                {
                    printToLCD("Correct OrderID!", 0, 1, false);
                    Serial.println("Correct OrderID");
                    delay(LCD_TIME_DELAY);
                    motor();
                }
                else
                {
                    Serial.println("Invalid OrderID!");
                    printToLCD("Invalid OrderID!", 0, 1, false);
                    delay(LCD_TIME_DELAY);
                }
                delay(1000);
                printToLCD("", 0, 1, false);
                ctr = 0;
                isComplete = false;
                clearDataArray();
            }
        }
        else if (key == '*')
        {
            clearDataArray();
            Serial.print("After clearing...");
            Serial.println(input_keypad_code);
            printToLCD(input_keypad_code, 3, 1, false);
            ctr = 0;
            isComplete = false;
        }
        else
        {
            if (isComplete)
            {
                printToLCD("Max Length Over!", 0, 1, false);
                delay(1000);
                clearLCDLineDisplay(SECOND_LCD_ROW);
                printToLCD(input_keypad_code, 3, 1, false);
                return;
            }
            input_keypad_code[ctr] = key;
            Serial.println(input_keypad_code);
            printToLCD(input_keypad_code, 3, 1, false);
            ctr++;
            if (ctr == KEYCODE_SIZE)
            {
                Serial.println("10 digit keypad entered!");
                isComplete = true;
            }
        }
        break;

    case RELEASED:
    case IDLE:
    case HOLD:
        break;
    }
}

void sensor()
{
    // state read
    int state1 = digitalRead(input1);
    int state2 = digitalRead(input2);

    Serial.print(state1);
    Serial.print("-");
    Serial.println(state2);
    if (state1 == LOW)
    {
        motorStatus1 = false;
        Serial.println("Motor1 Mati");
    }
    // else{
    //   Serial.println("Motorhidup");
    // }
    if (state2 == LOW)
    {
        motorStatus2 = false;
        Serial.println("Motor2 Mati");
    }
    if (!motorStatus1)
    {
        digitalWrite(MOTOR_PIN1, HIGH);
    }
    if (!motorStatus2)
    {
        digitalWrite(MOTOR_PIN2, HIGH);
    }
    delay(10);

    // else
    // {
    //     Serial.println("All Sensor Clear");
    // }
}

void setup()
{
    Serial.begin(115200);
    Wire.begin();
    // Motor Output
    pinMode(MOTOR_PIN1, OUTPUT);
    pinMode(MOTOR_PIN2, OUTPUT);
    digitalWrite(MOTOR_PIN1, HIGH);
    digitalWrite(MOTOR_PIN2, HIGH);

    // Sensor Input
    pinMode(input1, INPUT);
    pinMode(input2, INPUT);

    // LCD
    lcd.init();      // Initialize the LCD
    lcd.backlight(); // Turn on the backlight
    lcd.clear();     // Clear the LCD screen

    printToLCD("Initializing...", 1, 0, true);
    initWiFi();

    // // keypad
    keypad.begin(makeKeymap(keys));
    keypad.addEventListener(keypadEvent);
    // // adjust the debounce accordingly
    keypad.setDebounceTime(50);
}

void loop()
{

    // keypad
    char key = keypad.getKey();

    if (key)
    {
        Serial.println(key);
    }

    /*
    if (Serial.available())
    {
        String keyboard = Serial.readStringUntil('\r\n');
        keyboard.trim();

        String serverName = "/skripsivm/php/api-get-produk.php";
        String serverPath = serverName + "?midtrans_order_id=" + keyboard;
        http.get(serverPath);

        int statusCode = http.responseStatusCode();
        String response = http.responseBody();

        // Parsing JSON
        const size_t capacity = JSON_ARRAY_SIZE(2) + 2 * JSON_OBJECT_SIZE(2) + 60;
        DynamicJsonDocument doc(capacity);

        DeserializationError error = deserializeJson(doc, response);
        if (error)
        {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            Serial.println("Database Claim Error");
            printToLCD("Db Claim Err", 0, 1, false);
            return;
        }
        Serial.println(keyboard);
        Serial.println("Validating with the API...");

        for (JsonObject item : doc.as<JsonArray>())
        {
            int idp = item["id_produk"];
            int jumlah = item["jumlah"];

            Serial.print("Id produk: ");
            Serial.println(idp);
            Serial.print("jumlah produk: ");
            Serial.println(jumlah);

            if (idp == 1)
            {
                sendStockStatus(idp, jumlah);
                digitalWrite(MOTOR_PIN1, LOW);
            }
            if (idp == 2)
            {
                sendStockStatus(idp, jumlah);
                digitalWrite(MOTOR_PIN2, LOW);
            }
            Serial.println("Motor Operation Complete");
        }
    }
    */
    sensor();
}
