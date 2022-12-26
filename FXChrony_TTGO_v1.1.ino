#include <TFT_eSPI.h>
#include <OpenFontRender.h>
#include "ttfbin.h"
#include "BLEDevice.h"
#include "OneButton.h"
#include <EEPROM.h>
#include <esp_sleep.h>
#include "esp_adc_cal.h"
#include "pellet.h"

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
OpenFontRender render;

#define EEPROM_SIZE 6

static esp_adc_cal_characteristics_t adc_chars;

#define PIN_INPUT 0
OneButton button(PIN_INPUT, true);

#define UNITS_IMPERIAL  0
#define UNITS_METRIC    1
#define UNITS_MAX       1

#define PROFILE_BOW_AIRSOFT 0
#define PROFILE_CO2_PISTOL  1
#define PROFILE_AIR_PISTOL  2
#define PROFILE_AIR_GUN_UK  3
#define PROFILE_AIR_GUN_FAC 4
#define PROFILE_MAX         4

#define DISPLAY_FLIP_OFF    0
#define DISPLAY_FLIP_ON     1
#define DISPLAY_FLIP_MAX    1

#define SENSITIVITY_MAX     100

#define STATE_IDLE          0
#define STATE_CONNECTING    1
#define STATE_CONNECTED     2

#define seconds() (millis()/1000)

static uint8_t state = STATE_IDLE;

static float chronyVBattery;
static unsigned long chronyVBattLastRead = 0;

// The remote service we wish to connect to.
static BLEUUID deviceUUID("0000180a-0000-1000-8000-00805f9b34fb");

static BLEUUID serviceUUID("00001623-88EC-688C-644B-3FA706C0BB75");

// The characteristic of the remote service we are interested in.
static const char * charUUIDs[] = {
  "00001624-88EC-688C-644B-3FA706C0BB75",
  "00001625-88EC-688C-644B-3FA706C0BB75",
  "00001626-88EC-688C-644B-3FA706C0BB75",
  "00001627-88EC-688C-644B-3FA706C0BB75",
  "00001628-88EC-688C-644B-3FA706C0BB75",
  "00001629-88EC-688C-644B-3FA706C0BB75",
  "0000162A-88EC-688C-644B-3FA706C0BB75"
};

static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
static BLEClient*  pClient;
static BLERemoteService* pRemoteService;

static bool renderMenu = false;
static bool dirty = false;
static bool profile_changed = false;
static bool power_saving = false;
static unsigned long display_on_at = 0;
static uint8_t searching_ctr = 0;

static uint8_t sensitivity = 50;
static uint8_t units = UNITS_IMPERIAL;
static uint8_t profile = PROFILE_AIR_GUN_FAC;
static uint8_t display_flip = 0;
static uint8_t power_save_duration = 0;
static uint8_t pellet_index = 0;

static uint32_t shot_count = 0;

static uint8_t nc_counter = 0;

typedef  void (* menuItemCallback_t)(uint8_t);
typedef  void (* menuItemGenString_t)(uint8_t, char *);

typedef struct menuItem {
    const char * menuString;
    menuItemGenString_t menuItemGenString;
    struct menuItem * nextMenuItem;
    struct menuItem * currentSubMenu;
    struct menuItem * subMenu;
    menuItemCallback_t menuItemCallback;
    uint8_t param;
    menuItemGenString_t menuItemGenCurSelString;
} menuItem_t;

static menuItem_t * menu_pellet;
static menuItem_t * menuStack[4];
static int menuStackIndex;
static menuItem_t * pCurrentMenuItem;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Open Display (TTGO T Display v1.1) V1.1...");

  button.attachDoubleClick(doubleClick);
  button.attachLongPressStop(longPressStop);
  button.attachClick(singleClick);
  
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN_DB_11);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  pinMode(14, OUTPUT);
  digitalWrite(14, HIGH);

/*
 * Read the Settings, if any setting is out of range
 * give it a sensible default value (cope with first use)
 */
  EEPROM.begin(EEPROM_SIZE);
  sensitivity = EEPROM.read(0);
  if(sensitivity > SENSITIVITY_MAX) {
    sensitivity = 30;
    EEPROM.write(0, sensitivity);    
  }
  units = EEPROM.read(1);
  if(units > UNITS_MAX) {
    units = UNITS_MAX;
    EEPROM.write(1, units);
  }
  profile = EEPROM.read(2);
  if(profile > PROFILE_MAX) {
    profile = PROFILE_MAX;
    EEPROM.write(2, profile);
  }
  display_flip = EEPROM.read(3);
  if(display_flip > DISPLAY_FLIP_MAX) {
    display_flip = 0;
    EEPROM.write(3, display_flip);
  }
  power_save_duration = EEPROM.read(4);
  if(power_save_duration > 10) {
    power_save_duration = 10;
    EEPROM.write(4, power_save_duration);
  }
  pellet_index = EEPROM.read(5);
  if(pellet_index >= NUM_PELLETS) {
    pellet_index = 0;
    EEPROM.write(5, pellet_index);
  }

  EEPROM.commit();
 
  BLEDevice::init("");

  build_pellet_menu();

  tft.init();
  tft.setRotation(display_flip ? 1 : 3);

	render.setSerial(Serial);	  // Need to print render library message
	render.showFreeTypeVersion(); // print FreeType version
	render.showCredit();		  // print FTL credit

	if (render.loadFont(ttfbin, sizeof(ttfbin)))
	{
		Serial.println("Render initialize error");
		return;
	}

	render.setDrawer(tft); // Set drawer object

  tft.fillScreen(TFT_BLACK);
  dirty = true;
}

void doRenderMenu() {
  char genHeader[16];
  tft.fillScreen(TFT_BLACK);

  const char * pHeadertxt;
  if(pCurrentMenuItem->menuItemGenString == NULL) {
    pHeadertxt = pCurrentMenuItem->menuString;
  } else {
      pHeadertxt = genHeader;
      pCurrentMenuItem->menuItemGenString(pCurrentMenuItem->currentSubMenu->param, genHeader);
  }

  render.setFontSize(30);
  render.cdrawString(pHeadertxt,
              tft.width()/2,
              20,
              TFT_WHITE,
              TFT_BLACK,
              Layout::Horizontal);

  const char * psubHeadertxt = pCurrentMenuItem->currentSubMenu->menuString;
  if(psubHeadertxt == NULL) {
      psubHeadertxt = genHeader;
      pCurrentMenuItem->currentSubMenu->menuItemGenString(pCurrentMenuItem->currentSubMenu->param, genHeader);
  }

  render.setFontSize(20);
  render.cdrawString(psubHeadertxt,
              tft.width()/2,
              60,
              TFT_WHITE,
              TFT_BLACK,
              Layout::Horizontal);

  const char * pcurSelHeadertxt;
  if(pCurrentMenuItem->currentSubMenu->menuItemGenCurSelString) {
      pcurSelHeadertxt = genHeader;
      pCurrentMenuItem->currentSubMenu->menuItemGenCurSelString(pCurrentMenuItem->currentSubMenu->param, genHeader);

      render.setFontSize(20);
      render.cdrawString(pcurSelHeadertxt,
                  tft.width()/2,
                  90,
                  TFT_WHITE,
                  TFT_BLACK,
                  Layout::Horizontal);
    }
}


#define STR_SEARCHING "Searching"

void renderSearching() {
  char temp_str[16];
  int16_t w;
  tft.fillScreen(TFT_BLACK);

  strcpy(temp_str,STR_SEARCHING);
  for(uint8_t i = 0; i < searching_ctr; i++) {
    strcat(temp_str,".");
  }      
  render.setFontSize(36);
  render.cdrawString(temp_str,
              tft.width()/2,
              (tft.height()/2) - 18,
              TFT_WHITE,
              TFT_BLACK,
              Layout::Horizontal);

  renderDeviceVBatt();
  searching_ctr++;
  searching_ctr %= 4;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(deviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      state = STATE_CONNECTING;
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

void do_scan() {
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

bool writeChar(BLERemoteService* pRemoteService, int idx, uint8_t value)
{
  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUIDs[idx]);
  if (pRemoteCharacteristic == nullptr) {
    return false;
  }

  // Read the value of the characteristic.
  if(pRemoteCharacteristic->canWrite()) {
    pRemoteCharacteristic->writeValue(&value, 1);
  }
  return true;
}

bool readChar(BLERemoteService* pRemoteService, int idx, uint8_t * value)
{
  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUIDs[idx]);
  if (pRemoteCharacteristic == nullptr) {
    return false;
  }

  // Read the value of the characteristic.
  if(pRemoteCharacteristic->canRead()) {
    std::string v = pRemoteCharacteristic->readValue();
    *value = v[0];
  }
  return true;
}

/* 
 4.5V = 100%
 3.6V = 0%

  50-100% Green     > 4.05V
  25-50% Yellow   
  0-25% Red       < 3.85V
 */ 
void renderChronyVBatt(){
  uint8_t vb;
  uint16_t font_color = TFT_YELLOW;
  char temp_str[16];
  if(chronyVBattery >= 4.05) {
    font_color = TFT_GREEN;    
  }
  if(chronyVBattery < 3.85) {
    font_color = TFT_RED;    
  }
  sprintf (temp_str, "C %.1fV", chronyVBattery);
  render.setFontSize(15);
  render.rdrawString(temp_str,
              tft.width(),
              0,
              font_color,
              TFT_BLACK,
              Layout::Horizontal);
}

void renderDeviceVBatt() {
  uint16_t font_color = TFT_YELLOW;
  char temp_str[16];
  uint32_t vbat = esp_adc_cal_raw_to_voltage(analogRead(34), &adc_chars);
  float fbat = ((float)vbat / 4095.0) * 2.0 * 3.3 * (1100 / 1000.0);

  if(fbat >= 4.00) {
    font_color = TFT_GREEN;    
  }
  if(fbat < 3.80) {
    font_color = TFT_RED;    
  }

  sprintf (temp_str, "D %.1fV", fbat);
  render.setFontSize(15);
  render.drawString(temp_str,
                0,
                0,
                font_color,
                TFT_BLACK,
                Layout::Horizontal);
}

bool readBattery(){
  uint8_t vb;
  if(!readChar(pRemoteService, 3, &vb)) {
    return false;
  }
  chronyVBattery = (vb * 20.0)/1000;
  return true;  
}


class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    state = STATE_IDLE;
    dirty = true;
    renderMenu = false;
  }
};


static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  char sbuffer[16];
  // Third byte is return in steps of 5%, anything better than 10% we display  
  uint8_t r = ((char*)pData)[2] * 5;
  uint16_t speed;    
  if((r >= sensitivity) && !renderMenu) {
    display_on_at = seconds();
    power_saving = false;

    renderMenu = false;
    shot_count++;    

    speed = ((char*)pData)[0];
    speed <<= 8;
    speed |= ((char*)pData)[1];

    float energy;
    float fspeed = speed;

    if(units == UNITS_IMPERIAL) {
      fspeed *= 0.0475111859;
      sprintf (sbuffer, "%d FPS", int(fspeed));
    } else {
      fspeed *= 0.014481409;
      sprintf (sbuffer, "%d M/S", int(fspeed));
    }
    tft.fillScreen(TFT_BLACK);
    render.setFontSize(40);
    render.cdrawString(sbuffer,
                tft.width()/2,
                15,
                TFT_WHITE,
                TFT_BLACK,
                Layout::Horizontal);

    /* Draw the Pellet energy */
    if(units == UNITS_IMPERIAL) {
      energy = (my_pellets[pellet_index].pellet_weight_grains * powf(fspeed, 2))/450240;
      sprintf (sbuffer, "%.2f FPE", energy);
    } else {
      energy = 0.5 * (my_pellets[pellet_index].pellet_weight_grams / 1000) * powf(fspeed, 2);
      sprintf (sbuffer, "%.2f J", energy);
    }
    render.setFontSize(30);
    render.cdrawString(sbuffer,
                tft.width()/2,
                57,
                TFT_WHITE,
                TFT_BLACK,
                Layout::Horizontal);


    render.setFontSize(20);

    render.cdrawString(my_pellets[pellet_index].pellet_name,
                tft.width()/2,
                95,
                TFT_WHITE,
                TFT_BLACK,
                Layout::Horizontal);

    render.setFontSize(15);
    
    sprintf (sbuffer, "Shot# %d", shot_count);
    render.drawString(sbuffer,
                0,
                120,
                TFT_WHITE,
                TFT_BLACK,
                Layout::Horizontal);

    sprintf (sbuffer, "Return %d%%", r);
    render.rdrawString(sbuffer,
                tft.width(),
                120,
                TFT_WHITE,
                TFT_BLACK,
                Layout::Horizontal);

    renderDeviceVBatt();
    renderChronyVBatt();
  }
  nc_counter++;
  nc_counter %= 5;
}

uint8_t profile_bytes[2][5] = {{0x32, 0x32, 0x32, 0x32, 0x64},{0x17, 0x1E, 0x2D, 0x43, 0x5A}};

void connectToChrony() {
  pClient  = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  // Connect to the remove BLE Server.
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)

  // Obtain a reference to the service we are after in the remote BLE server.
  pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    pClient->disconnect();
    dirty = true;
    state = STATE_IDLE;
    return;
  }

  if(!writeChar(pRemoteService, 2, profile_bytes[0][profile]))
  {
    pClient->disconnect();
    dirty = true;
    state = STATE_IDLE;
    return;
  }
  if(!writeChar(pRemoteService, 4, profile_bytes[1][profile]))
  {
    pClient->disconnect();
    dirty = true;
    state = STATE_IDLE;
    return;
  }
  
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUIDs[0]);
  if (pRemoteCharacteristic == nullptr) {
    pClient->disconnect();
    dirty = true;
    state = STATE_IDLE;
    return;
  }

  if(pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  } else {
    pClient->disconnect();
    dirty = true;
    state = STATE_IDLE;
    return;      
  }

  readBattery();

  state = STATE_CONNECTED;    
  tft.fillScreen(TFT_BLACK); 
  power_saving = true;
}

void loop() {
  unsigned long now = seconds();  

  if( !power_saving && 
      !renderMenu && 
      state != STATE_IDLE && 
      (power_save_duration != 0) &&
      (display_on_at + power_save_duration) < now) 
  {
    display_on_at = seconds();
    power_saving = true;
  }


  button.tick();
  if(dirty) {
    if(power_saving) {
      display_on_at = seconds();
      //u8g2.setPowerSave(0);
      power_saving = false;
    }
    if(renderMenu){
      doRenderMenu();
    }
    dirty = false;
  }

  switch(state)
  {
    case STATE_IDLE:
      if(!dirty) { 
        renderSearching();
        do_scan(); 
      }
      break;
    case STATE_CONNECTING:
      connectToChrony();
      break;
    case STATE_CONNECTED:
      if(now - chronyVBattLastRead > 5) {
        readBattery();
        chronyVBattLastRead = now;
      }        
      break;
  }
}


/* 
  Menu system
*/


static void sleepCallback(uint8_t param)
{
  //u8g2.setPowerSave(1);
  esp_deep_sleep_start();
}

static menuItem_t menu_sleep[] = {
  { "Zzzz",  NULL, menu_sleep, NULL, NULL, sleepCallback, 0, NULL},
};

void menuItemGenStringCurSleep(uint8_t, char * buffer)
{
  sprintf(buffer, "[%s]", menu_sleep[0].menuString);
}

static const uint8_t power_save_duration_lut[] = {0,2,5,10};

static void powerSaveCallback(uint8_t param)
{
  if(power_save_duration != power_save_duration_lut[param]) {
    power_save_duration = power_save_duration_lut[param];
    EEPROM.write(4, power_save_duration);
    EEPROM.commit();
  }  
  pCurrentMenuItem = menuStack[--menuStackIndex];
}

static menuItem_t menu_power_save[] = {
  { "Off",  NULL, &menu_power_save[1], NULL, NULL, powerSaveCallback, 0, NULL},
  { "2s",   NULL, &menu_power_save[2], NULL, NULL, powerSaveCallback, 1, NULL},
  { "5s",   NULL, &menu_power_save[3], NULL, NULL, powerSaveCallback, 2, NULL},
  { "10s",  NULL, &menu_power_save[0], NULL, NULL, powerSaveCallback, 3, NULL}
};

void menuItemGenStringCurPowerSaving(uint8_t, char * buffer)
{
  uint8_t idx = 0;
  switch(power_save_duration)
  {
    case 2:  idx = 1;  break;
    case 5:  idx = 2;  break;
    case 10: idx = 3;  break;
  }  
  sprintf(buffer, "[%s]", menu_power_save[idx].menuString);
}

static void displayFlipCallback(uint8_t param)
{
  display_flip = param;
  EEPROM.write(3, display_flip);
  EEPROM.commit();
  tft.setRotation(display_flip ? 1 : 3);
  pCurrentMenuItem = menuStack[--menuStackIndex];
}

static menuItem_t menu_display_flip[] = {
  { "Off",         NULL, &menu_display_flip[1], NULL, NULL, displayFlipCallback, DISPLAY_FLIP_OFF, NULL},
  { "On",          NULL, &menu_display_flip[0], NULL, NULL, displayFlipCallback, DISPLAY_FLIP_ON, NULL}
};

void menuItemGenStringCurDisplayFlip(uint8_t, char * buffer)
{
  sprintf(buffer, "[%s]", menu_display_flip[display_flip].menuString);
}

static void unitsCallback(uint8_t param)
{
  units = param;
  EEPROM.write(1, units);
  EEPROM.commit();
  pCurrentMenuItem = menuStack[--menuStackIndex];
}

static menuItem_t menu_units[] = {
  { "FPS",          NULL, &menu_units[1], NULL, NULL, unitsCallback, UNITS_IMPERIAL, NULL},
  { "M/S",          NULL, &menu_units[0], NULL, NULL, unitsCallback, UNITS_METRIC, NULL}
};

void menuItemGenStringCurSelUnits(uint8_t, char * buffer)
{
  sprintf(buffer, "[%s]", menu_units[units].menuString);
}

static void profileCallback(uint8_t param)
{
  if(profile != param) {
    profile = param;
    EEPROM.write(2, profile);
    EEPROM.commit();
    profile_changed = true;
  }  
  pCurrentMenuItem = menuStack[--menuStackIndex];
}

static menuItem_t menu_profile[] = {
  { "Bow/Airsoft",  NULL, &menu_profile[1], NULL, NULL, profileCallback, PROFILE_BOW_AIRSOFT, NULL},
  { "CO2 Pistol",   NULL, &menu_profile[2], NULL, NULL, profileCallback, PROFILE_CO2_PISTOL, NULL},
  { "Air Pistol",   NULL, &menu_profile[3], NULL, NULL, profileCallback, PROFILE_AIR_PISTOL, NULL},
  { "Air Gun UK",   NULL, &menu_profile[4], NULL, NULL, profileCallback, PROFILE_AIR_GUN_UK, NULL},
  { "Air Gun FAC",  NULL, &menu_profile[0], NULL, NULL, profileCallback, PROFILE_AIR_GUN_FAC, NULL}
};

void menuItemGenStringCurSelProfile(uint8_t, char * buffer)
{
  sprintf(buffer, "[%s]", menu_profile[profile].menuString);
}

static void sensitivityIncCallback(uint8_t param)
{
  if(sensitivity <= 95){ 
    sensitivity += 5;
    EEPROM.write(0, sensitivity);
    EEPROM.commit();
  }
}
static void sensitivityDecCallback(uint8_t param)
{ 
  if(sensitivity >= 5){
    sensitivity -= 5;
    EEPROM.write(0, sensitivity);
    EEPROM.commit();
  }
}

static menuItem_t menu_sensitivity[] = {
  { "Increase",     NULL, &menu_sensitivity[1], NULL, NULL, sensitivityIncCallback, 0, NULL},
  { "Decrease",     NULL, &menu_sensitivity[0], NULL, NULL, sensitivityDecCallback, 0, NULL}
};

void menuItemGenStringSensitivity(uint8_t, char * buffer)
{
  sprintf(buffer, "%d %%", sensitivity);
}

void menuItemGenStringCurSelSensitivity(uint8_t, char * buffer)
{
  sprintf(buffer, "[%d %%]", sensitivity);
}

static void selectPelletCallback(uint8_t param)
{
  Serial.printf("selectPelletCallback %d\n",param);  
  pellet_index = param;
  EEPROM.write(5, pellet_index);
  EEPROM.commit();
  pCurrentMenuItem = menuStack[--menuStackIndex];
}

void menuItemGenStringPellet(uint8_t i, char * buffer)
{
  if(units == UNITS_IMPERIAL) {
    sprintf(buffer, "%s %.3f %.3f", my_pellets[i].pellet_mfr, my_pellets[i].pellet_weight_grains, my_pellets[i].pellet_calibre_inch);
  } else {
    sprintf(buffer, "%s %.3f %.3f", my_pellets[i].pellet_mfr, my_pellets[i].pellet_weight_grams, my_pellets[i].pellet_calibre_mm);
  }
  Serial.println(buffer);  
}

void menuItemGenStringCurSelPellet(uint8_t i, char * buffer)
{
  sprintf(buffer, "[%s]",my_pellets[pellet_index].pellet_name);
  Serial.println(buffer);  
}

static menuItem_t menu_top_level[] = {
  { "Profile",      NULL, &menu_top_level[1], menu_profile,     menu_profile,     NULL, 0, menuItemGenStringCurSelProfile},
  { "Pellet",       NULL, &menu_top_level[2], NULL,             NULL,             NULL, 0, menuItemGenStringCurSelPellet},
  { "Min. Return", menuItemGenStringSensitivity, &menu_top_level[3], menu_sensitivity, menu_sensitivity, NULL, 0, menuItemGenStringCurSelSensitivity},
  { "Units",        NULL, &menu_top_level[4], menu_units,       menu_units,       NULL, 0, menuItemGenStringCurSelUnits},
  { "Display Flip", NULL, &menu_top_level[5], menu_display_flip,menu_display_flip,NULL, 0, menuItemGenStringCurDisplayFlip},
  { "Power Save",   NULL, &menu_top_level[6], menu_power_save, menu_power_save,  NULL, 0, menuItemGenStringCurPowerSaving},
  { "Sleep",        NULL, &menu_top_level[0], menu_sleep,      menu_sleep,        NULL, 0, menuItemGenStringCurSleep}
  
};

void build_pellet_menu()
{
  uint8_t i;
  menu_pellet = (menuItem_t *)malloc(NUM_PELLETS * sizeof(menuItem_t));
  for(i=0;i<NUM_PELLETS;i++) {
    menu_pellet[i].menuString = my_pellets[i].pellet_name;
    menu_pellet[i].menuItemGenString = NULL;
    menu_pellet[i].nextMenuItem = ((i == NUM_PELLETS - 1) ? &menu_pellet[0] : &menu_pellet[i+1]);
    menu_pellet[i].currentSubMenu = NULL;
    menu_pellet[i].subMenu = NULL;
    menu_pellet[i].menuItemCallback = selectPelletCallback;
    menu_pellet[i].param = i;
    menu_pellet[i].menuItemGenCurSelString = menuItemGenStringPellet;
  }
  menu_top_level[1].currentSubMenu = menu_pellet;
  menu_top_level[1].subMenu = menu_pellet;
}


static menuItem_t menu_entry = {  "Settings", NULL, menu_top_level, menu_top_level, NULL, NULL, 0, NULL };

void singleClick()
{
  if(renderMenu)
  {
    dirty = true;
    pCurrentMenuItem->currentSubMenu = pCurrentMenuItem->currentSubMenu->nextMenuItem;
  } else {
    if(power_saving) {
      display_on_at = seconds();
      //u8g2.setPowerSave(0);
      power_saving = false;
    }
  }
}

void doubleClick()
{
  if(renderMenu)
  {
      dirty = true;
      if(pCurrentMenuItem->currentSubMenu->menuItemCallback != NULL)
      {
          (*pCurrentMenuItem->currentSubMenu->menuItemCallback)(pCurrentMenuItem->currentSubMenu->param);
      } else {
          menuStack[menuStackIndex++] = pCurrentMenuItem;
          pCurrentMenuItem = pCurrentMenuItem->currentSubMenu;
      }
  }
}

void longPressStop()
{
  if(renderMenu) {
    if(menuStackIndex == 1) {
      renderMenu = false;
      tft.fillScreen(TFT_BLACK);
      if(profile_changed) {
        pClient->disconnect();
        state = STATE_IDLE;
      }
    } else {
      dirty = true;
      pCurrentMenuItem = menuStack[--menuStackIndex];
    }
  } else if(state == STATE_CONNECTED) {
    profile_changed = false;
    dirty = true;
    renderMenu = true;
    pCurrentMenuItem = &menu_entry;
    menuStackIndex = 0;
    menuStack[menuStackIndex++] = pCurrentMenuItem;
  }
}
