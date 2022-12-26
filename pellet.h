/* Don't mess with this bit unless 
   you know what you're doing */
typedef struct pellet {
  const char * pellet_name;
  const char * pellet_mfr;
  float pellet_weight_grains;
  float pellet_calibre_inch;
  float pellet_weight_grams;
  float pellet_calibre_mm;
} pellet_t;

#define NUM_PELLETS (sizeof(my_pellets)/sizeof(pellet_t))
/************************************************************************************/
/* Add your pellets here !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/**************************************************************** ********************/
/* Every row EXCEPT the last one must have a comma at the end. 
   
   Format is:
   
Name, Manufacturere, weight in grains, calibre in inches, weight in grams, calibre in mm
*/
pellet_t my_pellets[] = {
  {"Diabolo",         "JSB",  8.44,  0.177, 0.547, 4.5},
  {"Diabolo Monster", "JSB",  13.43, 0.177, 0.87,  4.5}
};
/************************************************************************************/
