#ifndef _MSR_VERSION_H_
#define _MSR_VERSION_H_

// Version der MSR_LIBRARIES 
#define VERSION  6
#define PATCHLEVEL 0
#define SUBLEVEL  10


#define MSR_VERSION (((VERSION) << 16) + ((PATCHLEVEL) << 8) + (SUBLEVEL))

//Liste der Features der aktuellen rtlib-Version, wichtig, mu� aktuell gehalten werden
//da der Testmanager sich auf die Features verl��t

#define MSR_FEATURES "pushparameters,binparameters,maschinehalt,eventchannels,statistics,pmtime,aic,messages"

/* pushparameters: Parameter werden vom Echtzeitprozess an den Userprozess gesendet bei �nderung
   binparameters: Parameter k�nnen Bin�r �bertragen werden
   maschinehalt: Rechner kann durch Haltbefehl heruntergefahren werden
   eventchannels: Kan�le werden nur bei �nderung �bertragen
   statistics: Statistische Informationen �ber die anderen verbundenen Echtzeitprozesses
   pmtime: Die Zeit der �nderungen eines Parameters wird vermerkt und �bertragen
   aic: ansychrone Input Kan�le
   messages: Kan�le mit dem Attribut: "messagetyp" werden von der msr_lib �berwacht und bei �nderung des Wertes als Klartextmeldung verschickt V:6.0.10

*/
#endif
