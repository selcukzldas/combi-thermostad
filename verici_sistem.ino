#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

LiquidCrystal_I2C lcd(0x3F,20,4);  // set the LCD address to 0x27 0X3F PROTESU for a 16 chars and 2 line display
#define NTC_SENSOR A0
#define BTN_MENU_OK 2
#define BTN_UP      3
#define BTN_DOWN    4
#define BTN_EXIT    5

// NRF24L01
RF24 radio(6, 7);  // CE, CSN
const byte address[8] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x08};

// EEPROM adresleri
#define TEMP_ADDR 0  // Hedef sıcaklık için EEPROM adresi
#define STATE_ADDR 1 // Sistem durumu için EEPROM adresi
#define MODE_ADDR 2 // Running mode
#define UP_HISTER_ADDR 3
#define DOWN_HISTER_ADDR 4

//degiskenler
  bool editingHisteresis = false;  // Histerizasyon ayarı açık mı?
  int runningMode = 1;
  bool nestedAdjust =false;
  float upHisteresis = 0.5;  // Başlangıç değeri
  float downHisteresis = 0.5;
  float histeresis =0.5;
  float targetTemperature = 22.0; // Varsayılan hedef sıcaklık
  bool systemActive = false;      // Sistem durumu (başlat/durdur)
  bool isBoilerOn = false;        // Kombi durumu (açık/kapalı)
  bool adjustMode = false;        // Ayar modu
  int delayCounter = 2000; // sıcalık ölçüm süresi sayacı.

// Alev animasyonlarının karakter desenleri
  byte alevAnimasyon[3][8] = {
    {
      0b00000, 0b00000, 0b00100,
      0b01000, 0b00100, 0b01110, 0b00100,0b00000
    },
    {
      0b00100, 0b01000, 0b00000,
      0b01000, 0b01100, 0b10110, 0b01100,0b00000
    },
    {
      0b00000, 0b01000, 0b00100,
      0b01100, 0b01010, 0b11101, 0b01110, 0b00000
    }
  };

  byte checkIcon[8] = {
    0b00000,
    0b00000,
    0b00001,
    0b00010,
    0b10100,
    0b01000,
    0b00000,
    0b00000
  };

  byte radiatorIcon[8] = {
    0b00000,
    0b10101,
    0b10101,
    0b10101,
    0b10101,
    0b10101,
    0b10101,
    0b00000
  };

  byte upArrow[8] = {
    0b00000,
    0b00100,
    0b01110,
    0b11111,
    0b00100,
    0b00100,
    0b00100,
    0b00000
  };

  byte downArrow[8] = {
    0b00000,
    0b00100,
    0b00100,
    0b00100,
    0b11111,
    0b01110,
    0b00100,
    0b00000
  };

  byte pointerArrow[8] = {
    0b00000,
    0b00100,
    0b00010,
    0b11111,
    0b11111,
    0b00010,
    0b00100,
    0b00000
  };



//Menu İle İlgili Bolum--------

  // Histerizasyon Değişkeni

  // Menü Yapısı
  struct MenuItem {
      const char* name;      // Menü öğesinin adı
      MenuItem* subMenu;     // Bu öğenin alt menüsüne (varsa) işaretçi
      int subMenuSize;       // Alt menüde kaç öğe olduğunu belirten sayi
      int id;                // Menü öğesine özgü ID
  };

  //Hister. Alt Menu
  MenuItem subAyarlarHisteresis[] = {
      {"Ust Hister", nullptr, 0, 321},
      {"Alt Hister", nullptr, 0, 322},
  };

  // Ayarlar Alt Menu
  MenuItem subMenuAyarlar[] = {
      {"Saat Ayar", nullptr, 0, 31},
      {"Ekran Ayar", nullptr, 0, 32},
      {"Histerizasyon", subAyarlarHisteresis, sizeof(subAyarlarHisteresis) / sizeof(MenuItem), 33}, // ID: 3
      {"NTC Set", nullptr, 0, 34}        // ID: 4
  };

  // Bilgi Alt Menu
  MenuItem subMenuBilgi[] = {
      {"Versiyon", nullptr, 0, 41},
      {"Hakkında", nullptr, 0, 42}
  };

  // Mod Alt Menu
  MenuItem subMenuMode[]{
    {"Tasarruf Mod",nullptr,0,21},
    {"Eko Mod(Default)",nullptr,0,22},
    {"Konfor Mod", nullptr,0,23}
  };

  // Ana Menü
  MenuItem mainMenu[] = {
      {"Ana Sayfa", nullptr,0, 1}, 
      {"Mod Sec", subMenuMode,sizeof(subMenuMode)/sizeof(MenuItem), 2}, 
      {"Ayarlar", subMenuAyarlar, sizeof(subMenuAyarlar) / sizeof(MenuItem), 3},
      {"Bilgi", subMenuBilgi, sizeof(subMenuBilgi) / sizeof(MenuItem), 4}
  }; 


  // Menü Durumları
  MenuItem* currentMenu = mainMenu;
  int currentMenuSize = sizeof(mainMenu) / sizeof(MenuItem);
  int selectedIndex = 0;
  MenuItem* menuHistory[5]; 
  int menuSizeHistory[5];
  int menuDepth = 0;

//---------

void setup() {
  // Pin Initialize
    pinMode(BTN_MENU_OK, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_EXIT, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.setBacklight(1);

  lcd.setCursor(0, 0);
  lcd.print("Termostat Basliyor");


  // EEPROM'dan verileri yükle
  targetTemperature = EEPROMReadFloat(TEMP_ADDR);
  runningMode = EEPROMReadInt(MODE_ADDR);
  upHisteresis = EEPROMReadFloat(UP_HISTER_ADDR);
  downHisteresis = EEPROMReadFloat(DOWN_HISTER_ADDR);

  if (isnan(runningMode) || runningMode < 0 || targetTemperature > 2) {
    runningMode = 1; // Geçersiz bir değer varsa varsayılanı kullan
  }

  if (isnan(upHisteresis) || upHisteresis < 0 || upHisteresis > 5) {
    upHisteresis = 0.5; // Geçersiz bir değer varsa varsayılanı kullan
  }

  if (isnan(downHisteresis) || downHisteresis < 0 || downHisteresis > 5) {
    downHisteresis = 0.5; // Geçersiz bir değer varsa varsayılanı kullan
  }

  if (isnan(targetTemperature) || targetTemperature < 10 || targetTemperature > 30) {
    targetTemperature = 22.0; // Geçersiz bir değer varsa varsayılanı kullan
  }
  systemActive = EEPROM.read(STATE_ADDR) == 1 ? true : false;

  // NRF24L01 başlatma
  radio.begin();
  radio.openWritingPipe(address); // Adres tanımlama
  radio.setPALevel(RF24_PA_HIGH); // İletim gücünü ayarla

  delay(2000); // 2 saniye bekle

  // Alev animasyon karakterlerini kaydet
  for (int i = 0; i < 3; i++) {
    lcd.createChar(i, alevAnimasyon[i]);
    delay(200);
  }
  lcd.createChar(3,checkIcon);
  lcd.createChar(4,radiatorIcon);
  lcd.createChar(5,upArrow);
  lcd.createChar(6,downArrow);
  lcd.createChar(7,pointerArrow);

  displayMenu();
}

void loop() {
 
  buttonCheck();
  if(adjustMode == false){
    HomeView();
  }
}

void buttonCheck(){
  if (digitalRead(BTN_UP) == LOW) {
    if(adjustMode==true){
      btnUpEvents();
    }
    while(digitalRead(BTN_UP) == LOW){}
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if(adjustMode==true){
      btnDownEvents();
    }
    while(digitalRead(BTN_DOWN) == LOW){}
  }
  if (digitalRead(BTN_MENU_OK) == LOW) {
    if(adjustMode == true){
      btnOkEvents();
    }else {
      adjustMode = true;
      displayMenu();
    }
    while(digitalRead(BTN_MENU_OK) == LOW){}
  }
  if (digitalRead(BTN_EXIT) == LOW) {
    if(adjustMode == true){
      if(currentMenu==mainMenu){
        adjustMode=false;
      }else{
        btnExitEvents();
      }
    }
  }
  while(digitalRead(BTN_EXIT) == LOW){}
}

//Buton  Events
void btnOkEvents(){
  // Histeresis Control
  if (currentMenu[selectedIndex].id == 321) {  // Histerizasyon ID kontrolü
      nestedAdjust =true;
      displayHisteresis(321);
  }else if (currentMenu[selectedIndex].id == 322) { 
      nestedAdjust =true;
      displayHisteresis(322);
  } else if (editingHisteresis) {
      displayMenu();
  } else {
      enterMenu();
  }
}

void btnUpEvents(){
  if(nestedAdjust){
    if (currentMenu[selectedIndex].id == 321) { 
        upHisteresis += 0.1;
        if (upHisteresis > 5.0) upHisteresis = 5.0;
        displayHisteresis(321);
    } else if (currentMenu[selectedIndex].id == 322) { 
        downHisteresis += 0.1;
        if (downHisteresis > 5.0) downHisteresis = 5.0;
        displayHisteresis(322);
    }
  } else {
      navigateUp();
  }
}

void btnDownEvents(){
  if(nestedAdjust){
    if (currentMenu[selectedIndex].id == 321) {  // Histerizasyon ID kontrolü
        upHisteresis -= 0.1;
        if (upHisteresis < 0.1) upHisteresis = 0.1;  // Minimum değer
        displayHisteresis(321);
    } else if (currentMenu[selectedIndex].id == 322) { 
        downHisteresis -= 0.1;
        if (downHisteresis < 0.1) downHisteresis = 0.1;  // Minimum değer
        displayHisteresis(322);
    }
  } else {
      navigateDown();
  }
}

void btnExitEvents()
{
  if (nestedAdjust) {
      nestedAdjust = false;
      displayMenu();
  } else {
      exitMenu();
  }
}

// Sistemi aç/kapat
void toggleSystemState() {
  systemActive = !systemActive;
  EEPROM.write(STATE_ADDR, systemActive ? 1 : 0); // EEPROM'a kaydet
  //systemMeasureAndViews();
}



// Ayar modundan çıkış
void exitAdjustMode() {
  adjustMode = false;
  EEPROMWriteFloat(TEMP_ADDR, targetTemperature); // EEPROM'a kaydet

}

// Menü Ekranı Güncelleme
void displayMenu() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(">"); // Seçili olan menü için işaret
    lcd.print(currentMenu[selectedIndex].name);

    if (selectedIndex + 1 < currentMenuSize) {
        lcd.setCursor(1, 1);
        lcd.print(currentMenu[selectedIndex + 1].name);
    }
}

// Histerizasyon Ekranını Gösterme
void displayHisteresis(int type) {
    float histeresisValue = 0;
    String  histeresisType = "";
  
    if(type == 321){
      histeresisType= "Ust Hister:";
      histeresisValue = upHisteresis;
    }else if(type== 322){
      histeresisType= "Alt Hister:";
      histeresisValue = downHisteresis;
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(histeresisType);
    lcd.print(histeresisValue, 1); // Ondalık olarak göster (0.1 hassasiyet)
}

// Yukarı Gitme
void navigateUp() {
    if (selectedIndex > 0) {
        selectedIndex--;
        displayMenu();
    }
}

// Aşağı Gitme
void navigateDown() {
    if (selectedIndex < currentMenuSize - 1) {
        selectedIndex++;
        displayMenu();
    }
}

// Alt Menüye Girme
void enterMenu() {
    if (currentMenu[selectedIndex].subMenu != nullptr) {
        menuHistory[menuDepth] = currentMenu; 
        menuSizeHistory[menuDepth] = currentMenuSize;
        menuDepth++;

        currentMenu = currentMenu[selectedIndex].subMenu;
        currentMenuSize = menuHistory[menuDepth - 1][selectedIndex].subMenuSize;
        selectedIndex = 0;
        displayMenu();
    }
}

// Üst Menüye Dönme
void exitMenu() {
    if (menuDepth > 0) {
        menuDepth--;
        currentMenu = menuHistory[menuDepth]; 
        currentMenuSize = menuSizeHistory[menuDepth];
        selectedIndex = 0;
        displayMenu();
    }
}

void HomeView(){
  float currentTemperature = NTC(NTC_SENSOR);

  // Sistem aktifse kombiyi kontrol et
  if (systemActive) {
    if (currentTemperature < (targetTemperature - histeresis)) {
      sendBoilerCommand(true); // Kombi Aç
      isBoilerOn = true;
    } else if (currentTemperature > (targetTemperature + histeresis)) {
      sendBoilerCommand(false); // Kombi Kapandı
      isBoilerOn = false;
    }
  } else {
    sendBoilerCommand(false); // Sistem kapalıysa kombiyi kapalı tut
    isBoilerOn = false;
  }

  lcd.setCursor(0,0);
  lcd.print("Oda :");
  lcd.print(NTC(analogRead(NTC_SENSOR)));

  lcd.setCursor(0,1);
  lcd.print("Term:");
  lcd.print("21.5");

    
  lcd.setCursor(12,0);
  lcd.write(4);
  lcd.write(4);
  lcd.setCursor(15,0);
  lcd.write(3);
    
  lcd.setCursor(12,1);
  lcd.write(5);
  lcd.write(6);
    
  fire();
}

double NTC(int ADC1) { // Bu altyordamda sensörden okunan veri hesaplanıyor
  double Temp;
  Temp = log(10000.0*((1024.0/ADC1-1))); 
  Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
  Temp = Temp - 273.15;
  return Temp;
} 

void fire(){
  lcd.setCursor(15, 1);
  lcd.write(2);

  //  for (int i = 0; i < 3; i++) {
  //   lcd.setCursor(15, 1);
  //   lcd.write(i);
  //   if (digitalRead(BTN_MENU_OK)==HIGH) {lcd.clear(); break; }
  //   delay(250);
  // }
}

// Kombi komutlarını gönder
void sendBoilerCommand(bool state) {
  const char text = state ? '1' : '0'; // '1' veya '0' karakteri
  radio.write(&text, sizeof(text));    // Tek bir karakter gönder
}


// EEPROM yazma ve okuma fonksiyonları
void EEPROMWriteFloat(int address, float value) {
  byte *data = (byte *)&value;
  for (int i = 0; i < sizeof(value); i++) {
    EEPROM.write(address + i, data[i]);
  }
}

void EEPROMWriteInt(int address, int value) {
  byte *data = (byte *)&value;  // int'i byte dizisine çevir
  for (int i = 0; i < sizeof(value); i++) {
    EEPROM.write(address + i, data[i]);  // Her byte'ı EEPROM'a yaz
  }
}

float EEPROMReadFloat(int address) {
  float value = 0.0;
  byte *data = (byte *)&value;
  for (int i = 0; i < sizeof(value); i++) {
    data[i] = EEPROM.read(address + i);
  }
  return value;
}

int EEPROMReadInt(int address) {
  int value = 0;
  EEPROM.get(address, value);  // EEPROM'dan int olarak oku
  return value;
}
