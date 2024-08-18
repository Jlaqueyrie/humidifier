#include <Arduino.h>
#include <LiquidMenu.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <DFRobot_SHT20.h>
#include <Adafruit_PCF8574.h>
#include <Adafruit_PCF8575.h>
#include <string.h>
#include "Button.h"

#define ADDRESS_LCD 0x38
#define ADDRESS_PCF 0x20
#define PIN_BRUMISATEUR 0
#define PIN_VENTILATEUR 1

#define PIN_ROT_DT 2
#define PIN_ROT_CLK 4
#define PIN_ROT_SW 3
//RotaryEncoder *pEncoder = nullptr;

enum ERREUR {ERREUR_OK, ERREUR_NOK, HUMI_NOK, TEMP_NOK, TYPE_REGUL_NOK};
enum ETAT {ON , OFF};
enum TYPE_REGUL {TOR, PID};
enum COM { RS232, WIFI};

struct limiteCapteur{
  float limiteBasse;
  float limiteHaute;
};
struct mesure{
  float temperature;
  float humiditeRelative;
};
struct paramRegulTor{
  float Seuil;
  float SeuilInf;
  float SeuilSup;
};
struct paramRegulPid{
  float P;
  float I;
  float D;
};
struct paramControleur{
  enum TYPE_REGUL typeRegulation;
  enum COM typeComControleur;
  struct paramRegulPid paramPID;
  struct paramRegulTor paramTOR;
  struct limiteCapteur limiteCapteurTemperature;
  struct limiteCapteur limiteCapteurHumidite;
};
struct paramControleur parametres{
  TOR, 
  RS232,
  {
    1.0,
    1.0,
    1.0
  },
   {
    90.0,
    5.0,
    5.0
  },
  {
    -20.0,
    +85
  },
  {
    0,
    95
  }

};
struct mesure mesureControleur{
  0.0,
  0.0
};
volatile ETAT gEtat = OFF;
ERREUR gErreur = ERREUR_OK;
struct mesure *pPmesure = &mesureControleur;
paramControleur *pParametres = &parametres;
LiquidCrystal_I2C lcd(ADDRESS_LCD, 16, 2);
DFRobot_SHT20 sht20(&Wire, SHT20_I2C_ADDR);

Adafruit_PCF8574 pcf;


const bool pullup = true;
Button left(9, pullup);
Button right(7, pullup);
Button up(8, pullup);
Button down(10, pullup);
Button enter(3, pullup);
unsigned short sample_period = 2;

// Text used for indication for the save lines.
char input_saved[3];
char output_saved[3];

char string_saved[] = " *";
char string_notSaved[] = "  ";


enum FunctionTypes {
  increase = 1,
  decrease = 2,
};

// Declaration prototype
void HumidifierInitCapteurs(DFRobot_SHT20 *pSht20);
void HumidifierInitAffichage(LiquidCrystal_I2C *pLcd);
void HumidifierInitPCF(Adafruit_PCF8574 *pPcf);
void HumidifierSauveParam(struct paramControleur *pParam, int addresseParam);
void HumidifierRappelParam(struct paramControleur *pParamControleur, int addresseParam);
void HumidifierOnOff(void);
void HumidifierReglage(void);
void HumidifierAfficheDemarage(LiquidMenu *pMenu);
void HumidifierAfficheMesure(LiquidMenu *pMenu, mesure *pMesures);
void HumidifierGestionAffichage(LiquidMenu *pMenu);
void HumidifierAcquisitionMesure(ETAT *pEtat, DFRobot_SHT20 *pCapteur, struct mesure *pMesure, ERREUR *pErreur);
void HumidifierIsMesureOk(ETAT *pEtat, struct mesure *pMesure, struct paramControleur *pParam, ERREUR *pErreur);
void HumidifierRegul(ETAT *pEtat, struct paramControleur *pParam, struct mesure *pMesure, Adafruit_PCF8574 *pPcf, ERREUR *pErreur);
void HumidifierEnvoiMesure(ETAT *pEtat, struct mesure *pMesure,struct paramControleur *pParam, ERREUR *pErreur);
void HumidifierEnvoiErreur(ETAT *pEtat, struct paramControleur *pParam, ERREUR *pErreur);
void buttonsCheck();
void go_back();
void goto_menu_reglage();
void goto_menu_mesures();
void increase_Seuil();
void decrease_Seuil();
void increase_Seuil_Sup();
void decrease_Seuil_Sup();
void increase_Seuil_Inf();
void decrease_Seuil_Inf();
void save_output();
void SerialPrintLn(String id, String valeur);
// A LiquidLine object can be used more that once.
LiquidLine ligne_retour(8, 1, "Retour");

LiquidLine ligne_accueil_1(1, 0, "Humidificateur ");
LiquidLine ligne_accueil_2(6, 1, "TPO");
LiquidScreen ecran_accueil(ligne_accueil_1, ligne_accueil_2);

// These lines direct to other menus.
LiquidLine ligne_mesure(0, 0, "Mesure");
LiquidLine ligne_reglage(0, 1, "Reglage");
LiquidScreen ecran_choix_principale(ligne_mesure, ligne_reglage);

// This is the first menu.
LiquidMenu menu_principale(lcd, ecran_accueil, ecran_choix_principale, 1);


LiquidLine ligne_reg_sup(0, 0, "Seuil sup : ", pParametres->paramTOR.SeuilSup);
LiquidLine ligne_reg_sp(0, 1, "Seuil : ", pParametres->paramTOR.Seuil);
LiquidLine ligne_reg_inf(0, 2, "Seuil inf : ", pParametres->paramTOR.SeuilInf);
LiquidScreen ecran_reglage(ligne_reg_sup, ligne_reg_sp, ligne_reg_inf);

LiquidLine ligne_sauvegarde(0, 0, "Enreg.", output_saved);
LiquidScreen ecran_sauv_retour(ligne_sauvegarde, ligne_retour);

// This is the second menu.
LiquidMenu menu_reglage(lcd, ecran_reglage, ecran_sauv_retour);


LiquidLine ligne_hummidite(0, 0, "Temp : ", pPmesure->temperature, " C");
LiquidLine ligne_temperature(0, 1, "RH : ", pPmesure->humiditeRelative, " %");
LiquidScreen ecran_mesure(ligne_temperature, ligne_hummidite);

// And this is the final third menu.
LiquidMenu menu_mesures(lcd, ecran_mesure);

/*
 * LiquidSystem object combines the LiquidMenu objects to form
 * a menu system. It provides the same functions as LiquidMenu
 * with the addition of add_menu() and change_menu().
 */
LiquidSystem menu_system(menu_principale, menu_reglage, menu_mesures);



void setup() {
  pinMode(PIN_ROT_SW, INPUT_PULLUP);

  Serial.begin(9600);
  while(!Serial)
  ;
  HumidifierSauveParam(&parametres, 0);
  
  //pEncoder = new RotaryEncoder(PIN_ROT_CLK, PIN_ROT_DT, RotaryEncoder::LatchMode::TWO03);
  attachInterrupt(digitalPinToInterrupt(PIN_ROT_SW), HumidifierOnOff, FALLING);
  //attachInterrupt(digitalPinToInterrupt(PIN_ROT_SW), sw_on, FALLING);
  HumidifierInitCapteurs(&sht20);
  HumidifierInitAffichage(&lcd);
  delay(500);
  HumidifierInitPCF(&pcf);

  HumidifierRappelParam(&parametres, 0);
  ligne_retour.set_focusPosition(Position::LEFT);

  ligne_retour.attach_function(1, go_back);
  ligne_retour.attach_function(2, go_back);

  //appuyer sur heut ou bas pour rentrer dans le menu
  ligne_mesure.attach_function(1, goto_menu_mesures);
  ligne_mesure.attach_function(2, goto_menu_mesures);
  ligne_reglage.attach_function(1, goto_menu_reglage);
  ligne_reglage.attach_function(2, goto_menu_reglage);

  ligne_reg_sup.attach_function(increase, increase_Seuil_Sup);
  ligne_reg_sup.attach_function(decrease, decrease_Seuil_Sup);
  ligne_reg_sp.attach_function(increase, increase_Seuil);
  ligne_reg_sp.attach_function(decrease, decrease_Seuil);
  ligne_reg_inf.attach_function(increase, increase_Seuil_Inf);
  ligne_reg_inf.attach_function(decrease, decrease_Seuil_Inf);

  ligne_sauvegarde.attach_function(1, save_output);
  ligne_sauvegarde.attach_function(2, save_output);

  menu_system.update();


  delay(5000);
  
}

void loop() 
{
  buttonsCheck();

  if(gEtat == ON){
    //HumidifierAfficheMesure(&menu, &mesureControleur);
  }else{
    //HumidifierAfficheDemarage(&menu);
  }
  HumidifierAcquisitionMesure(&gEtat, &sht20, &mesureControleur, &gErreur);
  HumidifierIsMesureOk(&gEtat, &mesureControleur, &parametres, &gErreur);
  if(gErreur != HUMI_NOK | gErreur != TEMP_NOK){
    HumidifierRegul(&gEtat, &parametres, &mesureControleur, &pcf, &gErreur);
  };
  HumidifierEnvoiMesure(&gEtat, &mesureControleur,&parametres, &gErreur);
  HumidifierEnvoiErreur(&gEtat,&parametres, &gErreur);
  delay(500);
}
void HumidifierInitCapteurs(DFRobot_SHT20 *pSht20){
  pSht20->initSHT20();
  pSht20->checkSHT20();
  Serial.println(F("Init Sht20 ok"));

}
void HumidifierInitAffichage(LiquidCrystal_I2C *pLcd){
  pLcd->init();
  pLcd->backlight();
  Serial.println(F("Initialisation affichage lcd ok"));

}
void HumidifierInitPCF(Adafruit_PCF8574 *pPcf){
  if (!pPcf->begin(ADDRESS_PCF, &Wire)) {
    Serial.println(F("Erreur Init pcf"));
    while (1);
  }
  pPcf->pinMode(PIN_VENTILATEUR, OUTPUT);
  pPcf->pinMode(PIN_BRUMISATEUR, OUTPUT);
  Serial.println(F("Pcf initialisé ok"));
}
void HumidifierSauveParam(struct paramControleur *pParam, int addresseParam){
    EEPROM.put(addresseParam, *pParam);
    Serial.println(F("EEPROM - param sauvegarde ok"));
}
void SerialPrintLn(String id, String valeur){
  Serial.print(id);
  Serial.println(valeur);
}
void HumidifierRappelParam(struct paramControleur *pParamControleur, int addresseParam){
  EEPROM.get(addresseParam, *pParamControleur);
  delay(100);
  Serial.println(F("EEPROM - param rappel ok"));
  Serial.print(F("Type de regulation : "));
  String strTypeRegul;
  pParamControleur->typeRegulation == TOR ? strTypeRegul = "TOR" : strTypeRegul = "PID";
  Serial.println(strTypeRegul);
  Serial.print(F("Param PID : "));
  SerialPrintLn(F("P = "), (String)pParamControleur->paramPID.P);
  SerialPrintLn(F("I = "), (String)pParamControleur->paramPID.I);
  SerialPrintLn(F("D = "), (String)pParamControleur->paramPID.D);
  Serial.print(F("Param Tor : "));
  SerialPrintLn(F("Seuil = "), (String) pParamControleur->paramTOR.Seuil);
  SerialPrintLn(F("Seuil Inf = "), (String) pParamControleur->paramTOR.SeuilInf);
  SerialPrintLn(F("Seuil Sup = "), (String) pParamControleur->paramTOR.SeuilSup);
  Serial.print(F("lim capteur temp : "));
  SerialPrintLn(F("lim basse = "), (String)pParamControleur->limiteCapteurTemperature.limiteBasse);
  SerialPrintLn(F(" lim haute = "), (String) pParamControleur->limiteCapteurTemperature.limiteHaute);
  Serial.print(F("lim capteur humi : "));
  SerialPrintLn(F("lim basse = "), (String)pParamControleur->limiteCapteurHumidite.limiteBasse);
  SerialPrintLn(F("lim haute = "), (String) pParamControleur->limiteCapteurHumidite.limiteHaute);
  Serial.println(F("--------------- fin rappel param -----------------"));
  delay(10000);

}
void HumidifierOnOff(void){
  
  if(gEtat == ON){
    gEtat = OFF;
  }else{
    gEtat = ON;
  };
}
void HumidifierReglage(void){
}
/*void HumidifierAfficheDemarage(LiquidMenu *pMenu){
    pMenu->change_screen(&ecran_demarrage);
    static int pos = 0;

    pEncoder->tick(); // just call tick() to check the state.

    int newPos = pEncoder->getPosition();
    if (pos != newPos) {      
      if(pEncoder->getDirection() == RotaryEncoder::Direction::CLOCKWISE){
        pMenu->switch_focus(true);
      }
      if(pEncoder->getDirection() == RotaryEncoder::Direction::COUNTERCLOCKWISE){
        pMenu->switch_focus(false);
      }
      pos = newPos;
    } // if
};*/
void HumidifierAfficheMesure(LiquidMenu *pMenu, mesure *pMesures){
    static float mesurePrecedenteTemp = 0.0;
    static float mesurePrecedenteHumi = 0.0;

    if(pMesures->temperature != mesurePrecedenteTemp){
      mesurePrecedenteTemp = pMesures->temperature;
      pMenu->change_screen(&ecran_mesure);
    };
    if(pMesures->humiditeRelative != mesurePrecedenteHumi){
      mesurePrecedenteHumi = pMesures->humiditeRelative;
      pMenu->change_screen(&ecran_mesure);
    };
};
// void HumidifierGestionAffichage(LiquidMenu &menu, mesure){
//   if(gEtat == ON){
//     HumidifierAfficheMesure(&menu, &mesures);
//   }else{
//     HumidifierAfficheDemarage(&menu);
//   }
// }
void HumidifierAcquisitionMesure(ETAT *pEtat, DFRobot_SHT20 *pCapteur, struct mesure *pMesure, ERREUR *pErreur){
  if(*pEtat == ON){
    pMesure->temperature = pCapteur->readTemperature();
    pMesure->humiditeRelative = pCapteur->readHumidity();
    Serial.print("Mesure temperature : ");
    Serial.print(pMesure->temperature);
    Serial.print(" Mesure humidite : ");
     Serial.println(pMesure->humiditeRelative);   
  }
}
void HumidifierIsMesureOk(ETAT *pEtat, struct mesure *pMesure, struct paramControleur *pParam, ERREUR *pErreur){
  if(*pEtat == ON){
    if(pMesure->temperature > pParam->limiteCapteurTemperature.limiteHaute | pMesure->temperature < pParam->limiteCapteurTemperature.limiteBasse){
      *pErreur = TEMP_NOK;
    }
    if(pMesure->humiditeRelative > pParam->limiteCapteurHumidite.limiteHaute | pMesure->humiditeRelative < pParam->limiteCapteurHumidite.limiteBasse){
      *pErreur = HUMI_NOK;
    }
  }
}
void HumidifierRegul(ETAT *pEtat, struct paramControleur *pParam, struct mesure *pMesure, Adafruit_PCF8574 *pPcf, ERREUR *pErreur){
  if(*pEtat == ON){
    switch(pParam->typeRegulation ){
      case TOR:
        if(pMesure->humiditeRelative < pParam->paramTOR.Seuil - pParam->paramTOR.SeuilInf){
          pPcf->digitalWrite(PIN_BRUMISATEUR, ON);
          Serial.println("Regul on ");
          pPcf->digitalWrite(PIN_VENTILATEUR, ON);
        }else{
          Serial.println("Regul off ");
          pPcf->digitalWrite(PIN_BRUMISATEUR, OFF);
          pPcf->digitalWrite(PIN_VENTILATEUR, OFF);
          //ajouter controle sur ventilation extraction si nécessaire
        }
      case PID:
        break;
      default:
        *pErreur = TYPE_REGUL_NOK;
    }
  }
}
void HumidifierEnvoiMesure(ETAT *pEtat, struct mesure *pMesure,struct paramControleur *pParam, ERREUR *pErreur){
  if(*pEtat == ON){
    if(pParam->typeComControleur == RS232){
      String chaineEnvoiMesure = String("Temps = ") + String(millis()) + String(" - Temp = ") + String(pMesure->temperature) + String(" - RH = ") + String(pMesure-> humiditeRelative);
      Serial.println(chaineEnvoiMesure);
    }
  }
}
void HumidifierEnvoiErreur(ETAT *pEtat, struct paramControleur *pParam, ERREUR *pErreur){
  if(!*pErreur == ERREUR_OK){
    String chaineEnvoiErreur = String("Temps = ") + String(millis()) + String(" Erreur = ")+ String(*pErreur) + String(" Etat systeme = ")+ String(*pEtat);
    Serial.println(chaineEnvoiErreur);
  }
}
void buttonsCheck() {
  if (right.check() == LOW) {
    menu_system.next_screen();
    Serial.println("boutton droit enfoncé");
  }
  if (left.check() == LOW) {
    menu_system.previous_screen();
    Serial.println("boutton gauche enfoncé");
  }
  if (up.check() == LOW) {
    menu_system.call_function(increase);
    Serial.println("boutton heaut enfoncé");
  }
  if (down.check() == LOW) {
    menu_system.call_function(decrease);
    Serial.println("boutton bas enfoncé");
  }
  if (enter.check() == LOW) {
    menu_system.switch_focus();
    Serial.println("boutton entré enfoncé");
  }
}
// Callback function that will be attached to back_line.
void go_back() {
  // This function takes reference to the wanted menu.
  menu_system.change_menu(menu_principale);
}
void goto_menu_reglage() {
  menu_system.change_menu(menu_reglage);
}
void goto_menu_mesures() {
  menu_system.change_menu(menu_mesures);
}
void increase_Seuil() {
  if (pParametres->paramTOR.Seuil< 225) {
    pParametres->paramTOR.Seuil += 25;
  } else {
    pParametres->paramTOR.Seuil = 250;
  }
}
void decrease_Seuil() {
  if (pParametres->paramTOR.Seuil > 25) {
    pParametres->paramTOR.Seuil -= 25;
  } else {
    pParametres->paramTOR.Seuil = 0;
  }
}
void increase_Seuil_Sup() {
  if (pParametres->paramTOR.SeuilSup< 225) {
    pParametres->paramTOR.SeuilSup += 25;
  } else {
    pParametres->paramTOR.SeuilSup = 250;
  }
}
void decrease_Seuil_Sup() {
  if (pParametres->paramTOR.SeuilSup > 25) {
    pParametres->paramTOR.SeuilSup -= 25;
  } else {
    pParametres->paramTOR.SeuilSup = 0;
  }
}
void increase_Seuil_Inf() {
  if (pParametres->paramTOR.SeuilInf< 225) {
    pParametres->paramTOR.SeuilInf += 25;
  } else {
    pParametres->paramTOR.SeuilInf = 250;
  }
}
void decrease_Seuil_Inf() {
  if (pParametres->paramTOR.SeuilInf > 25) {
    pParametres->paramTOR.SeuilInf -= 25;
  } else {
    pParametres->paramTOR.SeuilInf = 0;
  }
}
void save_output() {
  EEPROM.put(9, pParametres);
}