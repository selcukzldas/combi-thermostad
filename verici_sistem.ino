#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x3F,20,4);  // set the LCD address to 0x27 0X3F PROTESU for a 16 chars and 2 line display
#define NTC_SENSOR A0
#define BTN_MENU_OK 2
#define BTN_UP      3
#define BTN_DOWN    4
#define BTN_EXIT    5

//degiskenler
  bool editingHisteresis = false;  // Histerizasyon ayarı açık mı?
  float histeresis = 0.5;  // Başlangıç değeri
  bool menuStatus =false;

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
      {"Alt Hister", nullptr, 0, 321},
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
    pinMode(BTN_MENU_OK, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_EXIT, INPUT_PULLUP);

  lcd.begin(16, 2);
  lcd.setBacklight(1);

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
  if(menuStatus == true){
  }else {
    HomeView();
  }
}

void buttonCheck(){
  if (digitalRead(BTN_UP) == LOW) {
    if(menuStatus==true){
      if (editingHisteresis) {
          histeresis += 0.1;
          if (histeresis > 5.0) histeresis = 5.0;  // Maksimum değer
          displayHisteresis();
      } else {
          navigateUp();
      }
    }else {

    }
      while(digitalRead(BTN_UP) == LOW){}
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if(menuStatus==true){
      if (editingHisteresis) {
          histeresis -= 0.1;
          if (histeresis < 0.1) histeresis = 0.1;  // Minimum değer
          displayHisteresis();
      } else {
          navigateDown();
      }
    }else {

    }
    while(digitalRead(BTN_DOWN) == LOW){}
  }
  if (digitalRead(BTN_MENU_OK) == LOW) {
    if(menuStatus == true){
      if (currentMenu[selectedIndex].id == 9) {  // Histerizasyon ID kontrolü
          editingHisteresis = true;
          displayHisteresis();
      } else if (editingHisteresis) {
          editingHisteresis = false;
          displayMenu();
      } else {
          enterMenu();
      }
    }else {
      menuStatus = true;
      displayMenu();
    }
    while(digitalRead(BTN_MENU_OK) == LOW){}
  }
  if (digitalRead(BTN_EXIT) == LOW) {
    if(menuStatus == true){
      if(currentMenu==mainMenu){
        menuStatus=false;
      }else{
        if (editingHisteresis) {
            editingHisteresis = false;
            displayMenu();
        } else {
            exitMenu();
        }
      }
    }
  }
  while(digitalRead(BTN_EXIT) == LOW){}
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
void displayHisteresis() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Histerizasyon:");
    lcd.setCursor(0, 1);
    lcd.print(histeresis, 1); // Ondalık olarak göster (0.1 hassasiyet)
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
