 /* Don't mess with this bit unless 
   you know what you're doing */

typedef struct gun {
  const char * gun_name;
  const char * gun_mfr;
  float gun_caliber_inch;
  float gun_caliber_mm;
  uint8_t gun_profile;
} gun_t;

#define NUM_GUNS    (sizeof(my_guns)/sizeof(gun_t))         // Lots!

#define PROFILE_BOW_AIRSOFT 0
#define PROFILE_CO2_PISTOL  1
#define PROFILE_AIR_PISTOL  2
#define PROFILE_AIR_GUN_UK  3
#define PROFILE_AIR_GUN_FAC 4

/************************************************************************************/
/* Add your guns here !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/************************************************************************************/
/* Every row EXCEPT the last one must have a comma at the end. 
   
   Format is:
   
Name, Manufacturer, caliber in inches, caliber in mm, profile (speed range)
*/
gun_t my_guns[] = {
  {"2240",    "Crosman",  0.22,  5.5, PROFILE_CO2_PISTOL},
  {"Pulsar",  "Daystate", 0.177, 4.5, PROFILE_AIR_GUN_FAC},
  {"Leshiy2", "Ed Gun",   0.25,  6.35,PROFILE_AIR_GUN_FAC},
  {"Red Wolf","Daystate", 0.25,  6.35, PROFILE_AIR_GUN_FAC}
};
/************************************************************************************/