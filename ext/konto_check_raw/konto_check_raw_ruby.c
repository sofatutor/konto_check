/*==================================================================
 *
 * KontoCheck Module, C Ruby Extension
 *
 * Copyright (c) 2010 Provideal Systems GmbH
 *
 * Peter Horn, peter.horn@provideal.net
 *
 * many extensions by Michael Plugge, m.plugge@hs-mannheim.de
 *
 * ------------------------------------------------------------------
 *
 * ACKNOWLEDGEMENT
 *
 * This module is entirely based on the C library konto_check
 * http://www.informatik.hs-mannheim.de/konto_check/
 * http://sourceforge.net/projects/kontocheck/
 * by Michael Plugge.
 *
 * The first Ruby port was written by Peter Horn; Jan Martin introduced the
 * split into konto_check_raw (low-level interface to the C library) and
 * konto_check (high-level interface); last Michael Plugge added the Ruby 1.9
 * compatibility definitions, a new initialization function and many more
 * functions of the C library to the Ruby interface.
 *
 * ------------------------------------------------------------------
 *
 * LICENCE
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Include the Ruby headers and goodies
#include "ruby.h"
#include <stdio.h>
#include "konto_check.h"

   /* Ruby 1.8/1.9 compatibility definitions */
#ifndef RSTRING_PTR
#define RSTRING_LEN(x) (RSTRING(x)->len)
#define RSTRING_PTR(x) (RSTRING(x)->ptr)
#endif

#ifndef RUBY_T_STRING
#define RUBY_T_STRING T_STRING
#define RUBY_T_FLOAT  T_FLOAT
#define RUBY_T_FIXNUM T_FIXNUM
#define RUBY_T_BIGNUM T_BIGNUM
#define RUBY_T_ARRAY  T_ARRAY
#endif

#define RUNTIME_ERROR(error) do{ \
   snprintf(error_msg,511,"KontoCheck::%s, %s",kto_check_retval2txt_short(error),kto_check_retval2txt(error)); \
   rb_raise(rb_eRuntimeError,error_msg); \
}while(0)

// Defining a space for information and references about the module to be stored internally
VALUE KontoCheck = Qnil;

/**
 * get_params_file()
 *
 * extract params from argc/argv (filename and optional integer parameters)
 */
static void get_params_file(int argc,VALUE* argv,char *arg1s,int *arg1i,int *arg2i,int opts)
{
   int len,typ1,typ2;
   VALUE v1_rb=0,v2_rb,v3_rb;

   switch(opts){
      case 1:
            /* ein Dateiname, zwei Integer; alle optional (für lut_init) Die
             * Reihenfolge der beiden ersten Parameter ist nicht festgelegt. In
             * der ersten API-Version war als erster Parameter der Dateiname,
             * an zweiter Stelle der Level. Allerdings kommt es oft vor, daß
             * man nur den Level angeben will, da der Dateiname sich aus den
             * Defaultwerten ergibt.
             *
             * Daher wird die Reihenfolge der Parameter level und dateiname on
             * the fly ermittelt. Der Level ist immer ein Integerwert, der
             * Dateiname immer ein String; so ist es eindeutig zu entscheiden,
             * welcher Parameter welchem Wert zuzuordnen ist. Werden zwei
             * Integerwerte oder zwei Strings übergeben, wird eine TypeError
             * Exception ausgelöst.
             */
         rb_scan_args(argc,argv,"03",&v1_rb,&v2_rb,&v3_rb);

         if(NIL_P(v1_rb)){  /* kein Parameter angegeben, nur Defaultwerte benutzen */
            *arg1s=0;
            *arg1i=DEFAULT_INIT_LEVEL;
            *arg2i=0;
            return;
         }

         switch(TYPE(v1_rb)){
            case RUBY_T_FLOAT:
            case RUBY_T_FIXNUM:
            case RUBY_T_BIGNUM:
               typ1=1;
               break;
            case RUBY_T_STRING:
               typ1=2;
               break;
            default:
               typ1=0;
               break;
         }
         switch(TYPE(v2_rb)){
            case RUBY_T_FLOAT:
            case RUBY_T_FIXNUM:
            case RUBY_T_BIGNUM:
               typ2=1;
               break;
            case RUBY_T_STRING:
               typ2=2;
               break;
            default:
               typ2=0;
               break;
         }
            /* es muß genau ein Integer und ein String vorkommen: die Summe ist dann 3 */
         if(typ1+typ2!=3)rb_raise(rb_eTypeError,"wrong type for filename or init level");

         if(typ1==1){   /* Argumente: Level, Dateiname; Level setzen und die Argumente tauschen */
            *arg1i=NUM2INT(v1_rb);
            v1_rb=v2_rb;   /* der weitere Code erwartet den Dateinamen in v1_rb */
         }
         else{   /* Argumente: Dateiname, Level. Die Reihenfolge ist ok, nur Level auslesen */
            if(NIL_P(v2_rb))  /* Level nicht angegeben, Defaultwerte benutzen */
               *arg1i=DEFAULT_INIT_LEVEL;
            else
               *arg1i=NUM2INT(v2_rb);
         }

         if(NIL_P(v3_rb))  /* Set; letzter Parameter */
            *arg2i=0;
         else
            *arg2i=NUM2INT(v3_rb);
         break;

      case 2:  /* ein Dateiname (für dump_lutfile) */
         rb_scan_args(argc,argv,"10",&v1_rb);
         break;

      case 3:  /* ein optionaler Dateiname (für lut_info) */
         rb_scan_args(argc,argv,"01",&v1_rb);
         break;

      default:
         break;
   }
   if(NIL_P(v1_rb)){    /* Leerstring zurückgeben => Defaultwerte probieren */
      *arg1s=0;
   }
   else if(TYPE(v1_rb)==RUBY_T_STRING){
      strncpy(arg1s,RSTRING_PTR(v1_rb),FILENAME_MAX);
         /* der Ruby-String ist nicht notwendig null-terminiert; manuell erledigen */
      if((len=RSTRING_LEN(v1_rb))>FILENAME_MAX)len=FILENAME_MAX;
      *(arg1s+len)=0;
   }
   else
      rb_raise(rb_eTypeError,"wrong type for filename");
}

/**
 * get_params_int()
 *
 * extract two numeric params from argc/argv (for integer search functions)
 */
static void get_params_int(int argc,VALUE* argv,int *arg1,int *arg2)
{
   char buffer[16];
   int len,cnt;
   VALUE arg1_rb,arg2_rb,*arr_ptr;

   rb_scan_args(argc,argv,"11",&arg1_rb,&arg2_rb);

         /* Falls als erster Parameter ein Array übergeben wird, wird der erste
          * Wert werden als Minimal- und der zweite als Maximalwert genommen.
          */
   if(TYPE(arg1_rb)==RUBY_T_ARRAY){
      cnt=RARRAY_LEN(arg1_rb);      /* Anzahl Werte */
      arr_ptr=RARRAY_PTR(arg1_rb);  /* Pointer auf die Array-Daten */
      switch(cnt){
         case 0:
            arg1_rb=arg2_rb=Qnil;
            break;
         case 1:
            arg1_rb=*arr_ptr;
            arg2_rb=Qnil;
            break;
         default:
            arg1_rb=arr_ptr[0];
            arg2_rb=arr_ptr[1];
            break;
      }
   }

   if(NIL_P(arg1_rb))
      *arg1=0;
   else if(TYPE(arg1_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(arg1_rb),15);
         /* der Ruby-String ist nicht notwendig null-terminiert; manuell erledigen */
      if((len=RSTRING_LEN(arg1_rb))>15)len=15;
      *(buffer+len)=0;
      *arg1=atoi(buffer);
   }
   else
      *arg1=NUM2INT(arg1_rb);

   if(NIL_P(arg2_rb))
      *arg2=*arg1;
   else if(TYPE(arg2_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(arg2_rb),15);
      if((len=RSTRING_LEN(arg2_rb))>15)len=15;
      *(buffer+len)=0;
      *arg2=atoi(buffer);
   }
   else
      *arg2=NUM2INT(arg2_rb);
}

/**
 * get_params()
 *
 * extract params from argc/argv, convert&check results
 */
static void get_params(int argc,VALUE* argv,char *arg1s,char *arg2s,char *arg3s,int *argi,int arg_mode)
{
   int len,maxlen=9;
   VALUE arg1_rb,arg2_rb,arg3_rb;

   switch(arg_mode){
      case 0:  /* ein notwendiger Parameter (für lut_filialen und lut_pz) */
         rb_scan_args(argc,argv,"10",&arg1_rb);
         break;

      case 1:  /* ein notwendiger, ein optionaler Parameter (für viele lut-Funktionen) */
         rb_scan_args(argc,argv,"11",&arg1_rb,&arg2_rb);
         if(NIL_P(arg2_rb))   /* Filiale; Hauptstelle ist 0 */
            *argi=0;
         else
            *argi=NUM2INT(arg2_rb);
         break;

      case 2:  /* zwei notwendige Parameter (für konto_check und iban_gen) */
         rb_scan_args(argc,argv,"20",&arg1_rb,&arg2_rb);
         break;

      case 3:  /* ein notwendiger Parameter (für iban_check) */
         rb_scan_args(argc,argv,"10",&arg1_rb);
         maxlen=128;
         break;

      case 4:  /* ein notwendiger Parameter (für ipi_gen) */
         rb_scan_args(argc,argv,"10",&arg1_rb);
         maxlen=24;
         break;

      case 5:  /* zwei notwendige, ein optionaler Parameter (für konto_check_pz) */
         rb_scan_args(argc,argv,"21",&arg1_rb,&arg2_rb,&arg3_rb);
         break;

      default:
         break;
   }

   switch(TYPE(arg1_rb)){
      case RUBY_T_STRING:
         strncpy(arg1s,RSTRING_PTR(arg1_rb),maxlen);
         if((len=RSTRING_LEN(arg1_rb))>maxlen)len=maxlen;
         *(arg1s+len)=0;
         break;
      case RUBY_T_FLOAT:
      case RUBY_T_FIXNUM:
      case RUBY_T_BIGNUM:
            /* Zahlwerte werden zunächst nach double umgewandelt, da der
             * Zahlenbereich von 32 Bit Integers nicht groß genug ist für
             * z.B. Kontonummern (10 Stellen). Mit snprintf wird dann eine
             * Stringversion erzeugt - nicht schnell aber einfach :-).
             */
         if(arg_mode==2)
            snprintf(arg1s,16,"%08.0f",NUM2DBL(arg1_rb));
         else
            snprintf(arg1s,16,"%02.0f",NUM2DBL(arg1_rb));
         break;
      default:
         if(!arg_mode)
            rb_raise(rb_eTypeError,"Unable to convert given blz.");
         else
            rb_raise(rb_eTypeError,"Unable to convert given value.");
         break;
   }
   if(arg_mode==2 || arg_mode==5)switch(TYPE(arg2_rb)){  /* für konto_check() und iban_gen(): kto holen */
      case RUBY_T_STRING:
         strncpy(arg2s,RSTRING_PTR(arg2_rb),15);
         if((len=RSTRING_LEN(arg2_rb))>15)len=15;
         *(arg2s+len)=0;
         break;
      case RUBY_T_FLOAT:
      case RUBY_T_FIXNUM:
      case RUBY_T_BIGNUM:
         snprintf(arg2s,16,"%010.0f",NUM2DBL(arg2_rb));
         break;
      default:
         rb_raise(rb_eTypeError,"Unable to convert given kto.");
         break;
   }
   if(arg_mode==5){  /* für konto_check_pz(): BLZ (optional) holen */
      if(NIL_P(arg3_rb))   /* keine BLZ angegeben */
         *arg3s=0;
      else
         switch(TYPE(arg3_rb)){
            case RUBY_T_STRING:
               strncpy(arg3s,RSTRING_PTR(arg3_rb),15);
               if((len=RSTRING_LEN(arg3_rb))>15)len=15;
               *(arg3s+len)=0;
               break;
            case RUBY_T_FLOAT:
            case RUBY_T_FIXNUM:
            case RUBY_T_BIGNUM:
               snprintf(arg3s,16,"%5.0f",NUM2DBL(arg3_rb));
               break;
            default:
               rb_raise(rb_eTypeError,"Unable to convert given blz.");
               break;
         }
   }
}

/**
 * ===<tt>KontoCheckRaw::konto_check_pz(pz,kto)</tt>
 * =====<tt>KontoCheck::konto_check_pz(pz,kto)</tt>
 *
 * Diese Funktion testet, ob eine gegebene Prüfziffer/Kontonummer-Kombination gültig ist.
 * 
 * Der zusätzliche Parameter blz ist nur für die Verfahren 52, 53, B6 und C0 notwendig; bei
 * diesen Verfahren geht die BLZ in die Berechnung der Prüfziffer ein. Bei allen anderen
 * Prüfzifferverfahren wird er ignoriert. Wird er bei einem dieser Verfahren nicht angegeben,
 * wird stattdessen eine Test-BLZ eingesetzt.
 * 
 * Die Funktion gibt einen skalaren Statuswert zurück, der das Ergebnis der
 * Prüfung enthält. Mögliche Rückgabewerte sind:
 *
 *     -77  (BAV_FALSE)               "BAV denkt, das Konto ist falsch (konto_check hält es für richtig)"
 *     -69  (MISSING_PARAMETER)       "bei der Kontoprüfung fehlt ein notwendiger Parameter (BLZ oder Konto)"
 *     -40  (LUT2_NOT_INITIALIZED)    "die Programmbibliothek wurde noch nicht initialisiert"
 *     -29  (UNDEFINED_SUBMETHOD)     "die (Unter)Methode ist nicht definiert"
 *     -12  (INVALID_KTO_LENGTH)      "ein Konto muß zwischen 1 und 10 Stellen haben"
 *      -3  (INVALID_KTO)             "das Konto ist ungültig"
 *      -2  (NOT_IMPLEMENTED)         "die Methode wurde noch nicht implementiert"
 *      -1  (NOT_DEFINED)             "die Methode ist nicht definiert"
 *       0  (FALSE)                   "falsch"
 *       1  (OK)                      "ok"
 *       2  (OK_NO_CHK)               "ok, ohne Prüfung"
 */
static VALUE konto_check_pz(int argc,VALUE* argv,VALUE self)
{
   char pz[16],blz[16],kto[16],error_msg[512];
   int retval;

   get_params(argc,argv,pz,kto,blz,NULL,5);
   if((retval=kto_check_pz(pz,kto,blz))==LUT2_NOT_INITIALIZED || retval==MISSING_PARAMETER)RUNTIME_ERROR(retval);
   return INT2FIX(retval);
}

/**
 * ===<tt>KontoCheckRaw::konto_check(blz,kto)</tt>
 * =====<tt>KontoCheck::konto_check(blz,kto)</tt>
 * Test, ob eine BLZ/Konto-Kombination eine gültige Prüfziffer enthält. Die Funktion gibt einen skalaren
 * Statuswert zurück, der das Ergebnis der Prüfung enthält. Mögliche Rückgabewerte sind:
 * 
 *    -77  BAV_FALSE               "BAV denkt, das Konto ist falsch (konto_check hält es für richtig)"
 *    -69  MISSING_PARAMETER       "Bei der Kontoprüfung fehlt ein notwendiger Parameter (BLZ oder Konto)"
 *    -40  LUT2_NOT_INITIALIZED    "die Programmbibliothek wurde noch nicht initialisiert"
 *    -29  UNDEFINED_SUBMETHOD     "Die (Unter)Methode ist nicht definiert"
 *    -12  INVALID_KTO_LENGTH      "ein Konto muß zwischen 1 und 10 Stellen haben"
 *     -5  INVALID_BLZ_LENGTH      "die Bankleitzahl ist nicht achtstellig"
 *     -4  INVALID_BLZ             "die Bankleitzahl ist ungültig"
 *     -3  INVALID_KTO             "das Konto ist ungültig"
 *     -2  NOT_IMPLEMENTED         "die Methode wurde noch nicht implementiert"
 *     -1  NOT_DEFINED             "die Methode ist nicht definiert"
 *      0  FALSE                   "falsch"
 *      1  OK                      "ok"
 *      2  OK_NO_CHK               "ok, ohne Prüfung"
 */
static VALUE konto_check(int argc,VALUE* argv,VALUE self)
{
   char kto[16],blz[16],error_msg[512];
   int retval;

   get_params(argc,argv,blz,kto,NULL,NULL,2);
   if((*blz=='0' || strlen(blz)!=8) && lut_blz(kto+2,0)==OK)   /* BLZ/Kto vertauscht, altes Interface */
      rb_raise(rb_eRuntimeError, "It seems that you use the old interface of konto_check?\n"
            "Please check the order of function arguments for konto_test(); should be (blz,kto)");
   if((retval=kto_check_blz(blz,kto))==LUT2_NOT_INITIALIZED || retval==MISSING_PARAMETER)RUNTIME_ERROR(retval);
   return INT2FIX(retval);
}

/**
 * ===<tt>KontoCheckRaw::init([<p1>[,<p2>[,<set>]]])</tt>
 * =====<tt>KontoCheck::init([<p1>[,<p2>[,<set>]]])</tt>
 * Die Variablen p1 und p2 stehen für level und lutfile (in beliebiger
 * Reihenfolge); die Zuordnung der beiden Parameter erfolgt on the fly durch
 * eine Typüberprüfung.
 *
 * Diese Funktion initialisiert die Bibliothek und lädt die gewünschten
 * Datenblocks in den Speicher. Alle Argumente sind optional; in konto_check.h
 * werden die Defaultwerte definiert.
 *
 * Die beiden ersten Parameter sind der Dateiname und der
 * Initialisierungslevel. Der Dateiname ist immer als String anzugeben, der
 * Initialisierungslevel immer als Zahl, ansonsten wird eine TypeError
 * Exception geworfen. Auf diese Weise ist es problemlos möglich festzustellen,
 * wie die Parameter p1 und p2 den Variablen lutfile und level zuzuordnen
 * sind.
 *
 * Für die LUT-Datei ist als Defaultwert sowohl für den Pfad als auch den
 * Dateinamen eine Liste möglich, die sequenziell abgearbeitet wird; diese wird
 * in konto_check.h spezifiziert (Compilerzeit-Konstante der C-Bibliothek). Die
 * folgenden Werte sind in der aktuellen konto_check.h definiert:
 *
 *    DEFAULT_LUT_NAME blz.lut, blz.lut2f, blz.lut2
 *    DEFAULT_LUT_PATH ., /usr/local/etc/, /etc/, /usr/local/bin/, /opt/konto_check/ (für nicht-Windows-Systeme)
 *    DEFAULT_LUT_PATH ., C:, C:\\Programme\\konto_check  (für Windows-Systeme)
 *
 * Der Defaultwert für level ist ebenfalls in konto_check.h definiert; in der
 * aktuellen Version ist er 5. Bei diesem Level werden die Blocks BLZ,
 * Prüfziffer, Name, Kurzname, PLZ, Ort und BIC geladen.
 *
 * Falls der Parameter set nicht angegeben ist, wird versucht, das aktuell
 * gültige Set aus dem Systemdatum und dem Gültigkeitszeitraum der in der
 * LUT-Datei gespeicherten Sets zu bestimmen.
 * 
 * Hier noch einmal ein Überblick über die Parameter:
 *
 * * lutfile: die LUT-Datei, mit der initialisiert werden soll
 * * level: (0-9) Welche Daten geladen werden sollen (PZ steht in der folgenden Tabelle für Prüfziffer, NAME_NAME_KURZ ist ein Block, der sowohl den Namen als auch den Kurznamen der Bank enthält; dieser läßt sich besser komprimieren als wenn beide Blocks getrennt sind):
 *    0. BLZ,PZ
 *    1. BLZ,PZ,NAME_KURZ
 *    2. BLZ,PZ,NAME_KURZ,BIC
 *    3. BLZ,PZ,NAME,PLZ,ORT
 *    4. BLZ,PZ,NAME,PLZ,ORT,BIC
 *    5. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC
 *    6. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ
 *    7. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG
 *    8. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG,LOESCHUNG
 *    9. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG,LOESCHUNG,PAN,NR
 * * set (1 oder 2): Datensatz
 * 
 * Mögliche Rückgabewerte:
 * 
 *    -112  (KTO_CHECK_UNSUPPORTED_COMPRESSION) "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
 *     -64  (INIT_FATAL_ERROR)        "Initialisierung fehlgeschlagen (init_wait geblockt)"
 *     -63  (INCREMENTAL_INIT_NEEDS_INFO)            "Ein inkrementelles Initialisieren benötigt einen Info-Block in der LUT-Datei"
 *     -62  (INCREMENTAL_INIT_FROM_DIFFERENT_FILE)   "Ein inkrementelles Initialisieren mit einer anderen LUT-Datei ist nicht möglich"
 *     -38  (LUT2_PARTIAL_OK)         "es wurden nicht alle Blocks geladen"
 *     -36  (LUT2_Z_MEM_ERROR)        "Memory error in den ZLIB-Routinen"
 *     -35  (LUT2_Z_DATA_ERROR)       "Datenfehler im komprimierten LUT-Block"
 *     -34  (LUT2_BLOCK_NOT_IN_FILE)  "Der Block ist nicht in der LUT-Datei enthalten"
 *     -33  (LUT2_DECOMPRESS_ERROR)   "Fehler beim Dekomprimieren eines LUT-Blocks"
 *     -31  (LUT2_FILE_CORRUPTED)     "Die LUT-Datei ist korrumpiert"
 *     -20  (LUT_CRC_ERROR)           "Prüfsummenfehler in der blz.lut Datei"
 *     -10  (FILE_READ_ERROR)         "kann Datei nicht lesen"
 *      -9  (ERROR_MALLOC)            "kann keinen Speicher allokieren"
 *      -7  (INVALID_LUT_FILE)        "die blz.lut Datei ist inkosistent/ungültig"
 *      -6  (NO_LUT_FILE)             "die blz.lut Datei wurde nicht gefunden"
 * 
 *       1  (OK)                      "ok"
 *       6  (LUT1_SET_LOADED)         "Die Datei ist im alten LUT-Format (1.0/1.1)"
 *
 * Anmerkung: Falls der Statuscode LUT2_PARTIAL_OK ist, waren bei der
 * Initialisierung nicht alle Blocks in der LUT-Datei enthalten.
 */
static VALUE init(int argc,VALUE* argv,VALUE self)
{
   char lut_name[FILENAME_MAX+1],error_msg[512];
   int retval,level,set;

   get_params_file(argc,argv,lut_name,&level,&set,1);
   retval=lut_init(lut_name,level,set);
   switch(retval){
      case OK:
      case LUT1_SET_LOADED:
      case LUT2_PARTIAL_OK:
         break;
      default:
         RUNTIME_ERROR(retval);
   }
   return INT2FIX(retval);
}

/**
 * ===<tt>KontoCheckRaw::current_lutfile_name()</tt>
 * =====<tt>KontoCheck::current_lutfile_name()</tt>
 * =====<tt>KontoCheck::current_lutfile_set()</tt>
 * =====<tt>KontoCheck::current_init_level()</tt>
 *
 * Diese Funktion bestimmt den Dateinamen der zur Initialisierung benutzen
 * LUT-Datei, das benutzte Set und den Initialisierungslevel der aktuellen
 * Initialisierung. Rückgabewert ist ein Array mit vier Elementen: das erste
 * ist der Dateiname der LUT-Datei, das zweite  das benutzte Set (0, falls
 * nicht initialisiert wurde, sonst 1 oder 2), das dritte der
 * Initialisierungslevel und das vierte der Statuscode.
 * 
 * Mögliche Statuscodes:
 * 
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *       1  (OK)                         "ok"
 */
static VALUE current_lutfile_name_rb(VALUE self)
{
   char *lut_filename;
   int set,level,retval;
   VALUE lut_filename_rb;

   if(!(lut_filename=current_lutfile_name(&set,&level,&retval)))
      lut_filename_rb=Qnil;
   else
      lut_filename_rb=rb_str_new2(lut_filename);
   return rb_ary_new3(4,lut_filename_rb,INT2FIX(set),INT2FIX(level),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::free()</tt>
 * =====<tt>KontoCheck::free()</tt>
 *
 * Diese Funktion gibt allen allokierten Speicher wieder frei. Der Rückgabewert ist immer true.
 */
static VALUE free_rb(VALUE self)
{
   lut_cleanup();
   return Qtrue;
}

/**
 * ===<tt>KontoCheckRaw::generate_lutfile(inputfile,outputfile [,user_info [,gueltigkeit [,felder [,filialen [,set [,iban_file]]]]]])</tt>
 * =====<tt>KontoCheck::generate_lutfile(inputfile,outputfile [,user_info [,gueltigkeit [,felder [,filialen [,set [,iban_file]]]]]])</tt>
 *
 * Diese Funktion generiert eine neue LUT-Datei aus der BLZ-Datei der Deutschen Bundesbank. Die folgenden
 * Parameter werden unterstützt:
 * * inputfile: Eingabedatei (Textdatei) der Bundesbank
 * * outputfile: Name der Ausgabedatei
 * * user_info: Info-String der in die LUT-Datei geschrieben wird (frei wählbar; wird in den Info-Block aufgenommen)
 * * gueltigkeit: Gültigkeit des Datensatzes im Format JJJJMMTT-JJJJMMTT. Diese Angabe wird benutzt, um festzustellen, ob ein Datensatz aktuell noch gültig ist.
 * * felder: (0-9) Welche Daten aufgenommmen werden sollen (PZ steht in der folgenden Tabelle für Prüfziffer, NAME_NAME_KURZ ist ein Block, der sowohl den Namen als auch den Kurznamen der Bank enthält; dieser läßt sich besser komprimieren als wenn beide Blocks getrennt sind):
 *    0. BLZ,PZ
 *    1. BLZ,PZ,NAME_KURZ
 *    2. BLZ,PZ,NAME_KURZ,BIC
 *    3. BLZ,PZ,NAME,PLZ,ORT
 *    4. BLZ,PZ,NAME,PLZ,ORT,BIC
 *    5. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC
 *    6. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ
 *    7. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG
 *    8. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG,LOESCHUNG
 *    9. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG,LOESCHUNG,PAN,NR
 * * filialen: (0 oder 1) Flag, ob nur die Daten der Hauptstellen (0) oder auch die der Filialen aufgenommen werden sollen
 * * set (0, 1 oder 2): Datensatz-Nummer. Jede LUT-Datei kann zwei Datensätze enthalten. Falls bei der Initialisierung nicht ein bestimmter Datensatz ausgewählt wird, wird derjenige genommen, der (laut Gültigkeitsstring) aktuell gültig ist.
 * * iban_file: Datei der Banken, die einer Selbstberechnung des IBAN nicht zugestimmt haben. Näheres dazu (inklusive Weblink) findet sich bei der Funktion KontoCheckRaw::iban_gen(blz,kto).
 * 
 * Mögliche Rückgabewerte:
 * 
 *    -112  (KTO_CHECK_UNSUPPORTED_COMPRESSION) "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
 *     -57  (LUT2_GUELTIGKEIT_SWAPPED)   "Im Gültigkeitsdatum sind Anfangs- und Enddatum vertauscht"
 *     -56  (LUT2_INVALID_GUELTIGKEIT)   "Das angegebene Gültigkeitsdatum ist ungültig (Soll: JJJJMMTT-JJJJMMTT)"
 *     -32  (LUT2_COMPRESS_ERROR)        "Fehler beim Komprimieren eines LUT-Blocks"
 *     -31  (LUT2_FILE_CORRUPTED)        "Die LUT-Datei ist korrumpiert"
 *     -30  (LUT2_NO_SLOT_FREE)          "Im Inhaltsverzeichnis der LUT-Datei ist kein Slot mehr frei"
 *     -15  (INVALID_BLZ_FILE)           "Fehler in der blz.txt Datei (falsche Zeilenlänge)"
 *     -11  (FILE_WRITE_ERROR)           "kann Datei nicht schreiben"
 *     -10  (FILE_READ_ERROR)            "kann Datei nicht lesen"
 *      -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
 *      -7  (INVALID_LUT_FILE)           "die blz.lut Datei ist inkosistent/ungültig"
 *       1  (OK)                         "ok"
 *       7  (LUT1_FILE_GENERATED)        "ok; es wurde allerdings eine LUT-Datei im alten Format (1.0/1.1) generiert"
 */
static VALUE generate_lutfile_rb(int argc,VALUE* argv,VALUE self)
{
   char input_name[FILENAME_MAX+1],output_name[FILENAME_MAX+1],iban_file[FILENAME_MAX+1];
   char user_info[256],gueltigkeit[20],buffer[16],error_msg[512];
   int retval,felder,filialen,set,len;
   VALUE input_name_rb,output_name_rb,user_info_rb,
         gueltigkeit_rb,felder_rb,filialen_rb,set_rb,iban_file_rb;

   rb_scan_args(argc,argv,"26",&input_name_rb,&output_name_rb,
         &user_info_rb,&gueltigkeit_rb,&felder_rb,&filialen_rb,&set_rb,&iban_file_rb);

   if(TYPE(input_name_rb)==RUBY_T_STRING){
      strncpy(input_name,RSTRING_PTR(input_name_rb),FILENAME_MAX);
      if((len=RSTRING_LEN(input_name_rb))>FILENAME_MAX)len=FILENAME_MAX;
      *(input_name+len)=0;
   }
   else
      rb_raise(rb_eTypeError,"Unable to convert given input filename.");

   if(TYPE(output_name_rb)==RUBY_T_STRING){
      strncpy(output_name,RSTRING_PTR(output_name_rb),FILENAME_MAX);
      if((len=RSTRING_LEN(output_name_rb))>FILENAME_MAX)len=FILENAME_MAX;
      *(output_name+len)=0;
   }
   else
      rb_raise(rb_eTypeError,"Unable to convert given output filename.");

   if(NIL_P(user_info_rb)){
      *user_info=0;
   }
   else if(TYPE(user_info_rb)==RUBY_T_STRING){
      strncpy(user_info,RSTRING_PTR(user_info_rb),255);
      if((len=RSTRING_LEN(user_info_rb))>255)len=255;
      *(user_info+len)=0;
   }
   else
      rb_raise(rb_eTypeError,"Unable to convert given user_info string.");

   if(NIL_P(gueltigkeit_rb)){
      *gueltigkeit=0;
   }
   else if(TYPE(gueltigkeit_rb)==RUBY_T_STRING){
      strncpy(gueltigkeit,RSTRING_PTR(gueltigkeit_rb),19);
      if((len=RSTRING_LEN(gueltigkeit_rb))>19)len=19;
      *(gueltigkeit+len)=0;
   }
   else
      rb_raise(rb_eTypeError,"Unable to convert given gueltigkeit string.");

   if(NIL_P(felder_rb))
      felder=DEFAULT_LUT_FIELDS_NUM;
   else if(TYPE(felder_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(felder_rb),15);
      if((len=RSTRING_LEN(felder_rb))>15)len=15;
      *(buffer+len)=0;
      felder=atoi(buffer);
   }
   else
      felder=NUM2INT(felder_rb);

   if(NIL_P(filialen_rb))
      filialen=0;
   else if(TYPE(filialen_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(filialen_rb),15);
      if((len=RSTRING_LEN(felder_rb))>15)len=15;
      *(buffer+len)=0;
      filialen=atoi(buffer);
   }
   else
      filialen=NUM2INT(filialen_rb);

   if(NIL_P(set_rb))
      set=0;
   else if(TYPE(set_rb)==RUBY_T_STRING){
      strncpy(buffer,RSTRING_PTR(set_rb),15);
      if((len=RSTRING_LEN(set_rb))>15)len=15;
      *(buffer+len)=0;
      set=atoi(buffer);
   }
   else
      set=NUM2INT(set_rb);

   if(NIL_P(iban_file_rb)){
      *iban_file=0;
   }
   else if(TYPE(iban_file_rb)==RUBY_T_STRING){
      strncpy(iban_file,RSTRING_PTR(iban_file_rb),FILENAME_MAX);
      if((len=RSTRING_LEN(iban_file_rb))>FILENAME_MAX)len=FILENAME_MAX;
      *(iban_file+len)=0;
   }
   else
      rb_raise(rb_eTypeError,"Unable to convert given iban file name to string.");

   retval=generate_lut2_p(input_name,output_name,user_info,gueltigkeit,felder,filialen,0,0,set);
   if(retval<0)RUNTIME_ERROR(retval);
   if(*iban_file)lut_keine_iban_berechnung(iban_file,output_name,0);
   return INT2FIX(retval);
}

static int enc_mode(int argc,VALUE *argv)
{
   int mode,t;
   VALUE mode_rb=Qnil;

   rb_scan_args(argc,argv,"01",&mode_rb);
   if(mode_rb==Qnil)
      return 0;
   else switch(TYPE(mode_rb)){
      case RUBY_T_STRING:
         if((mode=atoi(RSTRING_PTR(mode_rb)))>0)return mode;
         mode=*(RSTRING_PTR(mode_rb));
         if(mode=='m' || mode=='M')switch(*(RSTRING_PTR(mode_rb)+1)){
            case 'i':
            case 'I': return 51;
            case 'u':
            case 'U': return 52;
            case 'h':
            case 'H': return 53;
            case 'd':
            case 'D': return 54;
            default: return DEFAULT_ENCODING%10+50;   /* nicht recht spezifiziert, Makro+Default-Encoding nehmen */
         }
         else
            return mode;

         break;
      case RUBY_T_FLOAT:
      case RUBY_T_FIXNUM:
      case RUBY_T_BIGNUM:
         return NUM2INT(mode_rb);
         break;
      default:
         rb_raise(rb_eTypeError,"Unable to convert given value to int");
         break;
   }
}

/**
 * ===<tt>KontoCheckRaw::encoding([mode])</tt>
 * =====<tt>KontoCheck::encoding([mode])</tt>
 * =====<tt>KontoCheckRaw::encoding_str([mode])</tt>
 * =====<tt>KontoCheckRaw::keep_raw_data(mode)</tt>
 *
 * Diese Funktion legt den benutzten Zeichensatz für Fehlermeldungen durch die
 * Funktion KontoCheckRaw::retval2txt() und einige Felder der LUT-Datei (Name,
 * Kurzname, Ort) fest. Wenn die Funktion nicht aufgerufen wird, wird der Wert
 * DEFAULT_ENCODING aus konto_check.h benutzt.
 *
 * _Achtung_: Das Verhalten der Funktion hängt von dem Flag keep_raw_data der
 * C-Bibliothek ab. Falls das Flag gesetzt ist, werden die Rohdaten der Blocks
 * Name, Kurzname und Ort im Speicher gehalten; bei einem Wechsel der Kodierung
 * wird auch für diese Blocks die Kodierung umgesetzt. Falls das Flag nicht
 * gesetzt ist, sollte die Funktion *vor* der Initialisierung aufgerufen werden,
 * da in dem Fall die Daten der LUT-Datei nur bei der Initialisierung konvertiert
 * werden. Mit der Funktion KontoCheckRaw::keep_raw_data() kann das Flag gesetzt
 * oder gelöscht werden.
 *
 * Für den Parameter mode werden die folgenden Werte akzeptiert (die Strings
 * sind nicht case sensitiv; Mi oder mI oder MI ist z.B. auch möglich. Wird der
 * Parameter nicht angegeben, wird die aktuelle Kodierung ausgegeben):
 *
 *    0:            aktuelle Kodierung ausgeben
 *    1,  'i', 'I': ISO-8859-1
 *    2,  'u', 'U': UTF-8
 *    3,  'h', 'H': HTML
 *    4,  'd', 'D': DOS CP 850
 *    51, 'mi':     ISO-8859-1, Makro für Fehlermeldungen
 *    52, 'mu':     UTF-8, Makro für Fehlermeldungen
 *    53, 'mh':     HTML, Makro für Fehlermeldungen
 *    54, 'md':     DOS CP 850, Makro für Fehlermeldungen
 *
 * Rückgabewert ist die aktuelle Kodierung als Integer (falls zwei Kodierungen
 * angegeben sind, ist die erste die der Statusmeldungen, die zweite die der
 * LUT-Blocks):
 * 
 *     0:  "noch nicht spezifiziert" (vor der Initialisierung)
 *     1:  "ISO-8859-1";
 *     2:  "UTF-8";
 *     3:  "HTML entities";
 *     4:  "DOS CP 850";
 *     12: "ISO-8859-1/UTF-8";
 *     13: "ISO-8859-1/HTML";
 *     14: "ISO-8859-1/DOS CP 850";
 *     21: "UTF-8/ISO-8859-1";
 *     23: "UTF-8/HTML";
 *     24: "UTF-8/DOS CP-850";
 *     31: "HTML entities/ISO-8859-1";
 *     32: "HTML entities/UTF-8";
 *     34: "HTML entities/DOS CP-850";
 *     41: "DOS CP-850/ISO-8859-1";
 *     42: "DOS CP-850/UTF-8";
 *     43: "DOS CP-850/HTML";
 *     51: "Makro/ISO-8859-1";
 *     52: "Makro/UTF-8";
 *     53: "Makro/HTML";
 *     54: "Makro/DOS CP 850";
 */
static VALUE encoding_rb(int argc,VALUE* argv,VALUE self)
{
   return INT2FIX(kto_check_encoding(enc_mode(argc,argv)));
}

/**
 * ===<tt>KontoCheckRaw::encoding_str(mode)</tt>
 * =====<tt>KontoCheck::encoding_str(mode)</tt>
 * =====<tt>KontoCheckRaw::encoding(mode)</tt>
 * =====<tt>KontoCheckRaw::keep_raw_data(mode)</tt>
 *
 * Diese Funktion entspricht der Funktion KontoCheck::encoding(). Allerdings
 * ist der Rückgabewert nicht numerisch, sondern ein String, der die aktuelle
 * Kodierung angibt. Die folgenden Rückgabewerte sind möglich (falls zwei
 * Kodierungen angegeben sind, ist die erste die der Statusmeldungen, die
 * zweite die der LUT-Blocks):
 *      
 *     "noch nicht spezifiziert" (vor der Initialisierung)
 *     "ISO-8859-1";
 *     "UTF-8";
 *     "HTML entities";
 *     "DOS CP 850";
 *     "ISO-8859-1/UTF-8";
 *     "ISO-8859-1/HTML";
 *     "ISO-8859-1/DOS CP 850";
 *     "UTF-8/ISO-8859-1";
 *     "UTF-8/HTML";
 *     "UTF-8/DOS CP-850";
 *     "HTML entities/ISO-8859-1";
 *     "HTML entities/UTF-8";
 *     "HTML entities/DOS CP-850";
 *     "DOS CP-850/ISO-8859-1";
 *     "DOS CP-850/UTF-8";
 *     "DOS CP-850/HTML";
 *     "Makro/ISO-8859-1";
 *     "Makro/UTF-8";
 *     "Makro/HTML";
 *     "Makro/DOS CP 850";
 */
static VALUE encoding_str_rb(int argc,VALUE* argv,VALUE self)
{
   return rb_str_new2((kto_check_encoding_str(enc_mode(argc,argv))));
}

/**
 * ===<tt>KontoCheckRaw::keep_raw_data(mode)</tt>
 * =====<tt>KontoCheckRaw::encoding(mode)</tt>
 * =====<tt>KontoCheck::encoding(mode)</tt>
 *
 * Diese Funktion setzt bzw. liest das Flag keep_raw_data in der C-Bibliothek.
 * Falls es gesetzt ist, werden werden die Rohdaten der Blocks Name, Kurzname
 * und Ort im Speicher gehalten; bei einem Wechsel der Kodierung werden diese
 * Blocks dann auch auf die neue Kodierung umgesetzt. Für die Speicherung der
 * Blocks werden allerdings etwa 900 KB Hauptspeicher benötigt, die andernfalls
 * wieder freigegeben würden.
 *
 * Da diese Funktion etwas exotisch ist, ist sie nur in der KontoCheckRaw
 * Bibliothek enthalten, nicht in KontoCheck..
 * 
 * Parameter:
 *    -1: Flag keep_raw_data ausschalten
 *     1: Flag keep_raw_data einschalten
 *     0: Flag lesen
 *
 * Der Rückgabewert ist true oder false.
 */
static VALUE keep_raw_data_rb(VALUE self, VALUE mode_rb)
{
   int t,mode;

   if(NIL_P(mode_rb))
      mode=0;
   else if((t=TYPE(mode_rb))==RUBY_T_STRING)
      mode=*(RSTRING_PTR(mode_rb))-'0';
   else if(t==RUBY_T_FLOAT || t==RUBY_T_FIXNUM || t==RUBY_T_BIGNUM)
      mode=FIX2INT(mode_rb);
   else  /* nicht unterstützter Typ */
      mode=0;
   return keep_raw_data(mode)?Qtrue:Qfalse;
}

/**
 * ===<tt>KontoCheckRaw::retval2txt(retval)</tt>
 * =====<tt>KontoCheck::retval2txt(retval)</tt>
 *
 * Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
 * Der benutzte Zeichensatz wird über die Funktion KontoCheckRaw::encoding()
 * festgelegt. Falls diese Funktion nicht aufgerufen wurde, wird der Wert des
 * Makros DEFAULT_ENCODING aus konto_check.h benutzt.
 */
static VALUE retval2txt_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2txt(FIX2INT(retval)));
}

/**
 * ===<tt>KontoCheckRaw::retval2iso(retval)</tt>
 * =====<tt>KontoCheck::retval2iso(retval)</tt>
 *
 * Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
 * Der benutzte Zeichensatz ist ISO 8859-1. 
 */
static VALUE retval2iso_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2iso(FIX2INT(retval)));
}

/**
 * ===<tt>KontoCheckRaw::retval2txt_short(retval)</tt>
 * =====<tt>KontoCheck::retval2txt_short(retval)</tt>
 *
 * Diese Funktion konvertiert einen numerischen Rückgabewert in einen kurzen
 * String. Die Ausgabe ist der Makroname, wie er in C benutzt wird.
 */
static VALUE retval2txt_short_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2txt_short(FIX2INT(retval)));
}

/**
 * ===<tt>KontoCheckRaw::retval2dos(<retval>)</tt>
 * =====<tt>KontoCheck::retval2dos(<retval>)</tt>
 * 
 * Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
 * Der benutzte Zeichensatz ist cp850 (DOS).
 */
static VALUE retval2dos_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2dos(FIX2INT(retval)));
}

/**
 * ===<tt>KontoCheckRaw::retval2html(<retval>)</tt>
 * =====<tt>KontoCheck::retval2html(<retval>)</tt>
 *
 * Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
 * Für Umlaute werden HTML-Entities benutzt.
 */
static VALUE retval2html_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2html(FIX2INT(retval)));
}

/**
 * ===<tt>KontoCheckRaw::retval2utf8(<retval>)</tt>
 * =====<tt>KontoCheck::retval2utf8(<retval>)</tt>
 *
 * Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
 * Der benutzte Zeichensatz ist UTF-8.
 */
static VALUE retval2utf8_rb(VALUE self, VALUE retval)
{
  return rb_str_new2(kto_check_retval2utf8(FIX2INT(retval)));
}

/**
 * ===<tt>KontoCheckRaw::dump_lutfile(lutfile)</tt>
 * =====<tt>KontoCheck::dump_lutfile(lutfile)</tt>
 *
 * Diese Funktion liefert detaillierte Informationen über alle Blocks, die in
 * der LUT-Datei gespeichert sind, sowie noch einige Internas der LUT-Datei.
 * Der Rückgabewert ist ein Array mit zwei Elementen: im ersten steht ein
 * String mit den Infos, im zweiten ein Statuscode. Im Fehlerfall wird für den
 * Infostring nil zurückgegeben.
 *
 * Mögliche Werte für den Statuscode sind:
 *
 *    -112  (KTO_CHECK_UNSUPPORTED_COMPRESSION) "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
 *     -70  (LUT1_FILE_USED)             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *     -36  (LUT2_Z_MEM_ERROR)           "Memory error in den ZLIB-Routinen"
 *     -35  (LUT2_Z_DATA_ERROR)          "Datenfehler im komprimierten LUT-Block"
 *     -33  (LUT2_DECOMPRESS_ERROR)      "Fehler beim Dekomprimieren eines LUT-Blocks"
 *     -31  (LUT2_FILE_CORRUPTED)        "Die LUT-Datei ist korrumpiert"
 *     -20  (LUT_CRC_ERROR)              "Prüfsummenfehler in der blz.lut Datei"
 *     -10  (FILE_READ_ERROR)            "kann Datei nicht lesen"
 *      -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
 *      -7  (INVALID_LUT_FILE)           "die blz.lut Datei ist inkosistent/ungültig"
 *       1  (OK)                         "ok"
 */
static VALUE dump_lutfile_rb(int argc,VALUE* argv,VALUE self)
{
   char lut_name[FILENAME_MAX+1],*ptr;
   int retval;
   VALUE dump;

   get_params_file(argc,argv,lut_name,NULL,NULL,2);
   retval=lut_dir_dump_str(lut_name,&ptr);
   if(retval<=0)
      dump=Qnil;
   else
      dump=rb_str_new2(ptr);
   kc_free(ptr);  /* die C-Funktion allokiert Speicher, der muß wieder freigegeben werden */
   return rb_ary_new3(2,dump,INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::lut_info([lutfile])</tt>
 * =====<tt>KontoCheck::lut_info()</tt>
 * =====<tt>KontoCheck::lut_info1(lutfile)</tt>
 * =====<tt>KontoCheck::lut_info2(lutfile)</tt>
 *
 * Diese Funktion liefert Informationen über die Datensätze sowie die beiden
 * Infoblocks einer LUT-Datei. Falls sie ohne Parameter aufgerufen wird,
 * werden Infos über die geladenen Blocks zurückgegeben.
 * 
 * Der Rückgabewert ist ein Array mit 5 Elementen:
 *
 *    0. retval:  Statuscode
 *    1. valid1:  Gültigkeitscode für den ersten Block
 *    2. valid2:  Gültigkeitscode für den zweiten Block
 *    3. info1:   erster Infoblock, oder nil, falls der Block nicht existiert
 *    4. info2:   zweiter Infoblock, oder nil, falls der Block nicht existiert
 * 
 * Mögliche Statuscodes für valid1 and valid2 sind:
 *
 *    -105  (LUT2_NO_LONGER_VALID_BETTER)   "Beide Datensätze sind nicht mehr gültig; dieser ist  aber jünger als der andere"
 *     -59  (LUT2_NOT_YET_VALID)            "Der Datenblock ist noch nicht gültig"
 *     -58  (LUT2_NO_LONGER_VALID)          "Der Datenblock ist nicht mehr gültig"
 *     -34  (LUT2_BLOCK_NOT_IN_FILE)        "Die LUT-Datei enthält den Infoblock nicht"
 *       4  (LUT2_VALID)                    "Der Datenblock ist aktuell gültig"
 *       5  (LUT2_NO_VALID_DATE)            "Der Datenblock enthält kein Gültigkeitsdatum"
 *
 * Mögliche Werte für den Statuscode retval:
 *
 *    -112  (KTO_CHECK_UNSUPPORTED_COMPRESSION) "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
 *     -10  (FILE_READ_ERROR)        "kann Datei nicht lesen"
 *     -70  (LUT1_FILE_USED)         "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *     -40  (LUT2_NOT_INITIALIZED)   "die Programmbibliothek wurde noch nicht initialisiert"
 *     -37  (LUT2_Z_BUF_ERROR)       "Buffer error in den ZLIB Routinen"
 *     -36  (LUT2_Z_MEM_ERROR)       "Memory error in den ZLIB-Routinen"
 *     -35  (LUT2_Z_DATA_ERROR)      "Datenfehler im komprimierten LUT-Block"
 *     -34  (LUT2_BLOCK_NOT_IN_FILE) "Der Block ist nicht in der LUT-Datei enthalten"
 *     -31  (LUT2_FILE_CORRUPTED)    "Die LUT-Datei ist korrumpiert"
 *     -20  (LUT_CRC_ERROR)          "Prüfsummenfehler in der blz.lut Datei"
 *      -9  (ERROR_MALLOC)           "kann keinen Speicher allokieren"
 *      -7  (INVALID_LUT_FILE)       "die blz.lut Datei ist inkosistent/ungültig"
 *       1  (OK)                     "ok"
 */
static VALUE lut_info_rb(int argc,VALUE* argv,VALUE self)
{
   char lut_name[FILENAME_MAX+1],*info1,*info2,error_msg[512];
   int retval,valid1,valid2;
   VALUE info1_rb,info2_rb;

   get_params_file(argc,argv,lut_name,NULL,NULL,3);
   retval=lut_info(lut_name,&info1,&info2,&valid1,&valid2);
   if(!info1)
      info1_rb=Qnil;
   else
      info1_rb=rb_str_new2(info1);
   if(!info2)
      info2_rb=Qnil;
   else
      info2_rb=rb_str_new2(info2);
   kc_free(info1);  /* die C-Funktion allokiert Speicher, der muß wieder freigegeben werden */
   kc_free(info2);
   if(retval<0)RUNTIME_ERROR(retval);
   return rb_ary_new3(5,INT2FIX(retval),INT2FIX(valid1),INT2FIX(valid2),info1_rb,info2_rb);
}

/**
 * ===<tt>KontoCheckRaw::load_bank_data(<datafile>)</tt>
 * =====<tt>KontoCheck::load_bank_data(<datafile>)</tt>
 *
 * Diese Funktion war die alte Initialisierungsroutine für konto_check; es ist
 * nun durch die Funktionen KontoCheck::init() und KontoCheck::generate_lutfile()
 * ersetzt. Zur Initialisierung  benutzte sie die Textdatei der Deutschen
 * Bundesbank und generiertge daraus eine LUT-Datei, die dann von der
 * Initialisierungsroutine der C-Bibliothek benutzt wurde.
 * 
 * Die init() Funktion ist wesentlich schneller (7..20 mal so schnell ohne
 * Generierung der Indexblocks; mit Indexblocks macht es noch wesentlich mehr
 * aus) und hat eine Reihe weiterer Vorteile. So ist es z.B. möglich, zwwei
 * Datensätze mit unterschiedlichem Gültigkeitszeitraum in einer Datei zu
 * halten und den jeweils gültigen Satz automatisch (nach der Systemzeit)
 * auswählen zu lassen. Die einzelnen Datenblocks (Bankleitzahlen,
 * Prüfziffermethoden, PLZ, Ort...) sind in der LUT-Datei in jeweils
 * unabhängigen Blocks gespeichert und können einzeln geladen werden; die
 * Bankdatei von der Deutschen Bundesbank enthält alle Felder in einem linearen
 * Format, so daß einzelne Blocks nicht unabhängig von anderen geladen werden
 * können.
 * 
 * Die Funktion load_bank_data() wird nur noch als ein schibbolet benutzt, um
 * zu testen, ob jemand das alte Interface benutzt. Bei der Routine
 * KontoCheck::konto_check() wurde die Reihenfolge der Parameter getauscht, so
 * daß man in dem Falle den alten Code umstellen muß.
 */

static VALUE load_bank_data(VALUE self, VALUE path_rb)
{
   rb_raise(rb_eRuntimeError, "Perhaps you used the old interface of konto_check.\n"
         "Use KontoCheck::init() to initialize the library\n"
         "and check the order of function arguments for konto_test(blz,kto)");
}

/**
 * ===<tt>KontoCheckRaw::iban2bic(iban)</tt>
 * =====<tt>KontoCheck::iban2bic(iban)</tt>
 * 
 * Diese Funktion bestimmt zu einer (deutschen!) IBAN den zugehörigen BIC (Bank
 * Identifier Code). Der BIC wird für eine EU-Standard-Überweisung im
 * SEPA-Verfahren (Single Euro Payments Area) benötigt; für die deutschen
 * Banken ist er in der BLZ-Datei enthalten. Nähere Infos gibt es z.B. unter
 * http://www.bic-code.de/
 *
 * Der Rückgabewert ist ein Array mit vier Elementen: im ersten steht der BIC,
 * im zweiten ein Statuscode, im dritten die BLZ und im vierten die Kontonummer
 * (die beiden letzteren werden aus der IBAN extrahiert). Im Fehlerfall wird
 * für BIC, BLZ und Kontonummer nil zurückgegeben. 
 * 
 * Mögliche Statuscodes:
 * 
 *     -68  (IBAN2BIC_ONLY_GERMAN)       "Die Funktion iban2bic() arbeitet nur mit deutschen Bankleitzahlen"
 *     -46  (LUT2_BIC_NOT_INITIALIZED)   "Das Feld BIC wurde nicht initialisiert"
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)         "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                "die Bankleitzahl ist ungültig"
 *       1  (OK)                         "ok"
 */
static VALUE iban2bic_rb(int argc,VALUE* argv,VALUE self)
{
   char iban[128],error_msg[512],blz[10],kto[16];
   const char *bic;
   int retval;

   get_params(argc,argv,iban,NULL,NULL,NULL,3);
   bic=iban2bic(iban,&retval,blz,kto);
   if(retval<0 && retval!=INVALID_BLZ)RUNTIME_ERROR(retval);
   return rb_ary_new3(4,retval<=0?Qnil:rb_str_new2(bic),INT2FIX(retval),
         retval<=0?Qnil:rb_str_new2(blz),retval<=0?Qnil:rb_str_new2(kto));
}

/**
 * ===<tt>KontoCheckRaw::iban_gen(blz,kto)</tt>
 * =====<tt>KontoCheck::iban_gen(kto,blz)</tt>
 * Diese Funktion generiert aus (deutscher) BLZ und Konto einen IBAN. Hierbei
 * ist zu beachten, daß nicht alle Banken der Selbstberechnung zugestimmt
 * haben. Es gibt von den Sparkassen eine Liste dieser Institute; sie ist auch
 * in der LUT-Datei mit der ID 22 (1. Eigene IBAN) enthalten. Bei diesen Banken
 * wird - falls der Block in der LUT-Datei enthalten ist - keine Berechnung
 * durchgeführt, sondern die Status-Variable auf NO_OWN_IBAN_CALCULATION
 * gesetzt. 
 *
 * Alle Banken der Liste erzeugen eine Statusmeldung mit dem Wert
 * OK_UNTERKONTO_ATTACHED, OK_UNTERKONTO_POSSIBLE oder OK_UNTERKONTO_GIVEN.
 * Falls einer dieser Stauswerte zurückgegeben wird, ist somit immer Vorsicht
 * geboten; der generierte IBAN sollte direkt bei dem zugehörigen Institut
 * überprüft werden.
 * 
 * Hier ein Auszug aus der Anleitung des SEPA Account Converters:
 * 
 * Der SEPA Account Converter ist so eingestellt, dass nur Kontoverbindungen in
 * IBAN und BIC umgerechnet werden, bei denen das ausgebende Kreditinstitut der
 * Umrechnung zugestimmt hat. Kreditinstitute, welche einer Umrechnung nicht
 * zugestimmt haben und welche zum Teil spezielle, dem SEPA Account Converter
 * nicht bekannte Umrechnungsmethoden verwenden, sind in der Datei "CONFIG.INI"
 * hinterlegt. Durch Löschen der Datei "CONFIG.INI" aus dem Programmverzeichnis
 * haben Sie die Möglichkeit, eine Umrechnung für alle Konten durchzuführen.
 * Bitte beachten Sie dabei, dass die so erhaltenen IBAN und BIC fehlerhaft
 * sein können und deshalb mit ihren Kunden zu überprüfen sind.
 * 
 * Weblinks:
 * 
 *      https://www.sparkasse-rhein-neckar-nord.de/pdf/content/sepa/kurzanleitung.pdf
 *      https://www.sparkasse-rhein-neckar-nord.de/firmenkunden/internationales_geschaeft/sepa/vorteile/index.php
 *      https://www.sparkasse-rhein-neckar-nord.de/firmenkunden/internationales_geschaeft/sepa/vorteile/sepa_account_converter.msi
 *
 * Es ist möglich, sowohl die Prüfung auf Stimmigkeit der Kontonummer als auch
 * auf Zulässigkeit der Selbstberechnung zu deaktivieren. Falls die IBAN trotz
 * fehlender Zustimmung der Bank berechnet werden, ist vor die BLZ ein @ zu
 * setzen; falls auch bei falscher Bankverbindung ein IBAN berechnet werden
 * soll, ist vor die BLZ ein + zu setzen. Um beide Prüfungen zu deaktiviern,
 * kann @+ (oder +@) vor die BLZ gesetzt werden. Die so erhaltenen IBANs sind
 * dann i.A. allerdings wohl nicht gültig.
 * 
 * Mögliche Statuscodes sind:
 *    -110  OK_UNTERKONTO_ATTACHED    "wahrscheinlich OK; es wurde allerdings ein (weggelassenes) Unterkonto angefügt"
 *     -77  (BAV_FALSE)               "BAV denkt, das Konto ist falsch (konto_check hält es für richtig)"
 *     -74  (NO_GERMAN_BIC)           "Ein Konto kann kann nur für deutsche Banken geprüft werden"
 *     -69  (MISSING_PARAMETER)       "Bei der Kontoprüfung fehlt ein notwendiger Parameter (BLZ oder Konto)"
 *     -40  (LUT2_NOT_INITIALIZED)    "die Programmbibliothek wurde noch nicht initialisiert"
 *     -29  (UNDEFINED_SUBMETHOD)     "Die (Unter)Methode ist nicht definiert"
 *     -12  (INVALID_KTO_LENGTH)      "ein Konto muß zwischen 1 und 10 Stellen haben"
 *      -5  (INVALID_BLZ_LENGTH)      "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)             "Die (Unter)Methode ist nicht definiert"
 *      -3  (INVALID_KTO)             "das Konto ist ungültig"
 *      -2  (NOT_IMPLEMENTED)         "die Methode wurde noch nicht implementiert"
 *      -1  (NOT_DEFINED)             "die Methode ist nicht definiert"
 *       0  (FALSE)                   "falsch"
 *       1  (OK)                      "ok"
 *       2  (OK_NO_CHK)               "ok, ohne Prüfung"
 *      11  (OK_UNTERKONTO_POSSIBLE)  "wahrscheinlich ok; die Kontonummer kann allerdings (nicht angegebene) Unterkonten enthalten"
 *      12  (OK_UNTERKONTO_GIVEN)     "wahrscheinlich ok; die Kontonummer enthält eine Unterkontonummer"
 */
static VALUE iban_gen_rb(int argc,VALUE* argv,VALUE self)
{
   char iban[128],*papier,*ptr,*dptr,blz[20],kto[20];
   int retval;
   VALUE iban_rb,papier_rb;

   get_params(argc,argv,blz,kto,NULL,NULL,2);
   papier=iban_gen(blz,kto,&retval);
   if(retval==OK || retval==OK_UNTERKONTO_ATTACHED || retval==OK_UNTERKONTO_POSSIBLE || retval==OK_UNTERKONTO_GIVEN){
      for(ptr=papier,dptr=iban;*ptr;ptr++)if(*ptr!=' ')*dptr++=*ptr;
      *dptr=0;
      iban_rb=rb_str_new2(iban);
      papier_rb=rb_str_new2(papier);
      kc_free(papier);  /* die C-Funktion allokiert Speicher, der muß wieder freigegeben werden */
   }
   else
      iban_rb=papier_rb=Qnil;
   return rb_ary_new3(3,iban_rb,papier_rb,INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::iban_check(blz)</tt>
 * =====<tt>KontoCheck::iban_check(iban)</tt>
 * Diese Funktion testet eine IBAN; bei deutschen Bankverbindungen wird
 * zusätzlich noch die Plausibilität der Bankverbindung getestet und im
 * Statuswert zurückgegeben. Die Rückgabe ist ein Array mit zwei Elementen: im
 * ersten (retval) wird das Testergebnis für die IBAN zurückgegeben, im zweiten
 * (bei deutschen Bankverbindungen) das Testergebnis des Kontotests.
 *
 * Mögliche Rückgabewerte für den IBAN-Test:
 *
 *     -67  (IBAN_OK_KTO_NOT)         "Die Prüfziffer der IBAN stimmt, die der Kontonummer nicht"
 *     -66  (KTO_OK_IBAN_NOT)         "Die Prüfziffer der Kontonummer stimmt, die der IBAN nicht"
 *       0  (FALSE)                   "falsch"
 *       1  (OK)                      "ok"
 *
 * Mögliche Rückgabewerte für den Kontotest:
 *     -77  (BAV_FALSE)               "BAV denkt, das Konto ist falsch (konto_check hält es für richtig)"
 *     -74  (NO_GERMAN_BIC)           "Ein Konto kann kann nur für deutsche Banken geprüft werden"
 *     -69  (MISSING_PARAMETER)       "Bei der Kontoprüfung fehlt ein notwendiger Parameter (BLZ oder Konto)"
 *     -40  (LUT2_NOT_INITIALIZED)    "die Programmbibliothek wurde noch nicht initialisiert"
 *     -29  (UNDEFINED_SUBMETHOD)     "Die (Unter)Methode ist nicht definiert"
 *     -12  (INVALID_KTO_LENGTH)      "ein Konto muß zwischen 1 und 10 Stellen haben"
 *      -5  (INVALID_BLZ_LENGTH)      "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)             "Die (Unter)Methode ist nicht definiert"
 *      -3  (INVALID_KTO)             "das Konto ist ungültig"
 *      -2  (NOT_IMPLEMENTED)         "die Methode wurde noch nicht implementiert"
 *      -1  (NOT_DEFINED)             "die Methode ist nicht definiert"
 *       0  (FALSE)                   "falsch"
 *       1  (OK)                      "ok"
 *       2  (OK_NO_CHK)               "ok, ohne Prüfung"
 */
static VALUE iban_check_rb(int argc,VALUE* argv,VALUE self)
{
   char iban[128];
   int retval,retval_kc;

   get_params(argc,argv,iban,NULL,NULL,NULL,3);
   retval=iban_check(iban,&retval_kc);
   return rb_ary_new3(2,INT2FIX(retval),INT2FIX(retval_kc));
}

/**
 * ===<tt>KontoCheckRaw::ipi_gen(<zweck>)</tt>
 * =====<tt>KontoCheck::ipi_gen(<zweck>)</tt>
 *
 * Diese Funktion generiert einen "Strukturierten Verwendungszweck" für SEPA-Überweisungen.
 * Der Rückgabewert ist der Strukturierte Verwendungszweck als String oder nil, falls ein Fehler
 * aufgetreten ist.
 * 
 * Der String für den Strukturierten Verwendungszweck darf maximal 18 Byte lang sein und nur
 * alphanumerische Zeichen enthalten (also auch keine Umlaute).
 *
 * Der Rückgabewert ist ein Array mit drei Elementen: im ersten steht der Strukturierte Verwendungszweck,
 * im zweiten ein Statuscode und im dritten die Papierform (mit eingestreuten Blanks).
 * 
 * Mögliche Statuscodes:
 *
 *     -71  (IPI_INVALID_LENGTH)         "Die Länge des IPI-Verwendungszwecks darf maximal 18 Byte sein"
 *     -72  (IPI_INVALID_CHARACTER)      "Im strukturierten Verwendungszweck dürfen nur alphanumerische Zeichen vorkommen"
 *       1  (OK)                         "ok"
 */
static VALUE ipi_gen_rb(int argc,VALUE* argv,VALUE self)
{
   char zweck[24],dst[24],papier[30];
   int retval;

   get_params(argc,argv,zweck,NULL,NULL,NULL,4);
   retval=ipi_gen(zweck,dst,papier);
   if(retval==OK)
      return rb_ary_new3(3,rb_str_new2(dst),INT2FIX(retval),rb_str_new2(papier));
   else
      return rb_ary_new3(3,Qnil,INT2FIX(retval),Qnil);
}

/**
 * ===<tt>KontoCheckRaw::ipi_check(<zweck>)</tt>
 * =====<tt>KontoCheck::ipi_check(<zweck>)</tt>
 * 
 * Die Funktion testet, ob ein Strukturierter Verwendungszweck gültig ist
 * (Anzahl Zeichen, Prüfziffer). Die Rückgabe ist ein Statuscode, der die
 * folgenden Werte annehmen kann:
 * 
 *     -73  (IPI_CHECK_INVALID_LENGTH)   "Der zu validierende strukturierete Verwendungszweck muß genau 20 Zeichen enthalten"
 *       0  (FALSE)                      "falsch"
 *       1  (OK)                         "ok"
 */
static VALUE ipi_check_rb(int argc,VALUE* argv,VALUE self)
{
   char zweck[128];

   get_params(argc,argv,zweck,NULL,NULL,NULL,3);
   return INT2FIX(ipi_check(zweck));
}

/**
 * ====<tt>KontoCheckRaw::bank_valid(blz [,filiale])</tt>
 * ====== <tt>KontoCheck::bank_valid(blz [,filiale])</tt>
 * ====== <tt>KontoCheck::bank_valid?(blz [,filiale])</tt>
 * Diese Funktion testet, ob eine gegebene BLZ gültig ist. Der Rückgabewert ist ein
 * Statuscode mit den unten angegebenen Werten. Falls das Argument filiale auch
 * angegeben ist, wird zusätzlich noch getestet, ob eine Filiale mit dem gegebenen
 * Index existiert.
 * 
 * Mögliche Rückgabewerte sind:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)    "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)   "Das Feld BLZ wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)         "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                "die Bankleitzahl ist ungültig"
 *       1  (OK)                         "ok"
 */
static VALUE bank_valid(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int filiale,retval;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   retval=lut_blz(blz,filiale);
   if(retval==LUT2_BLZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return INT2FIX(retval);
}

/**
 * ===<tt>KontoCheckRaw::bank_filialen(blz)</tt>
 * =====<tt>KontoCheck::bank_filialen(blz)</tt>
 * 
 * Diese Funktion liefert die Anzahl Filialen einer Bank (inklusive Hauptstelle).
 * Die LUT-Datei muß dazu natürlich mit den Filialdaten generiert sein, sonst
 * wird für alle Banken nur 1 zurückgegeben. Der Rückgabewert ist ein Array mit
 * der Anzahl Filialen im ersten Parameter, und einem Statuscode im zwweiten.
 * 
 * Mögliche Statuswerte sind:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -52  (LUT2_FILIALEN_NOT_INITIALIZED) "Das Feld Filialen wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */

static VALUE bank_filialen(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,cnt;

   get_params(argc,argv,blz,NULL,NULL,NULL,0);
   cnt=lut_filialen(blz,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_FILIALEN_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(cnt),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_alles(blz[,filiale])</tt>
 * =====<tt>KontoCheck::bank_alles(blz[,filiale])</tt>
 * 
 * Dies ist eine Mammutfunktion, die alle vorhandenen Informationen über eine
 * Bank zurückliefert. Das Ergebnis ist ein Array mit den folgenden Komponenten:
 *    0:  Statuscode
 *    1:  Anzahl Filialen
 *    2:  Name
 *    3:  Kurzname
 *    4:  PLZ
 *    5:  Ort
 *    6:  PAN
 *    7:  BIC
 *    8:  Prüfziffer
 *    9:  Laufende Nr.
 *    10: Änderungs-Flag
 *    11: Löeschung-Flag
 *    12: Nachfolge-BLZ
 * 
 * Der Statuscode (Element 0) kann folgende Werte annehmen:
 * 
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *     -38  (LUT2_PARTIAL_OK)            "es wurden nicht alle Blocks geladen"
 *      -5  (INVALID_BLZ_LENGTH)         "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                "die Bankleitzahl ist ungültig"
 *       1  (OK)                         "ok"
 *
 * Anmerkung: Falls der Statuscode LUT2_PARTIAL_OK ist, wurden bei der
 * Initialisierung nicht alle Blocks geladen (oder es sind nicht alle verfügbar);
 * die entsprechenden Elemente haben dann den Wert nil.
 */

static VALUE bank_alles(int argc,VALUE* argv,VALUE self)
{
   char blz[16],**p_name,**p_name_kurz,**p_ort,**p_bic,*p_aenderung,*p_loeschung,aenderung[2],loeschung[2],error_msg[512];
   int retval,filiale,cnt,*p_blz,*p_plz,*p_pan,p_pz,*p_nr,*p_nachfolge_blz;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   retval=lut_multiple(blz,&cnt,&p_blz, &p_name,&p_name_kurz,&p_plz,&p_ort,&p_pan,&p_bic,&p_pz,&p_nr,
         &p_aenderung,&p_loeschung,&p_nachfolge_blz,NULL,NULL,NULL);
   if(retval==LUT2_BLZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   if(filiale<0 || (cnt && filiale>=cnt))return(INT2FIX(LUT2_INDEX_OUT_OF_RANGE));  /* ungültige Filiale */

      /* Fehler; die C-Arrays dürfen in diesem Fall nicht dereferenziert werden */
   if(retval<=0 && retval!=LUT2_PARTIAL_OK)return rb_ary_new3(2,INT2FIX(retval),Qnil);

   if(p_aenderung){
      *aenderung=p_aenderung[filiale];
      *(aenderung+1)=0;
   }
   else
      *aenderung=0;

   if(p_loeschung){
      *loeschung=p_loeschung[filiale];
      *(loeschung+1)=0;
   }
   else
      *loeschung=0;

   /* Makros für StringValue und IntegerValue definieren, damit die Rückgabe
    * einigermaßen übersichtlich bleibt
    */
#define SV(x) *x?rb_str_new2(x):Qnil
#define IV(x) x>=0?INT2FIX(x):Qnil
   return rb_ary_new3(13,INT2FIX(retval),IV(cnt),SV(p_name[filiale]),
         SV(p_name_kurz[filiale]),IV(p_plz[filiale]),SV(p_ort[filiale]),
         IV(p_pan[filiale]),SV(p_bic[filiale]),IV(p_pz),IV(p_nr[filiale]),
         SV(aenderung),SV(loeschung),IV(p_nachfolge_blz[filiale]));
}

/**
 * ===<tt>KontoCheckRaw::bank_name(blz[,filiale])</tt>
 * =====<tt>KontoCheck::bank_name(blz[,filiale])</tt>
 * 
 * Diese Funktion liefert den Namen einer Bank. Der Rückgabewert ist ein
 * Array mit zwei Elementen: im ersten steht ein String mit dem Namen, im
 * zweiten ein Statuscode. Im Fehlerfall wird für den Namen nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -51  (LUT2_NAME_NOT_INITIALIZED)     "Das Feld Bankname wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_name(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   const char *name;
   int retval,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   name=lut_name(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_NAME_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(name),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_name_kurz(blz [,filiale])</tt>
 * =====<tt>KontoCheck::bank_name_kurz(blz [,filiale])</tt>
 * 
 * Diese Funktion liefert den Kurznamen einer Bank. Der Rückgabewert ist ein
 * Array mit zwei Elementen: im ersten steht ein String mit dem Namen, im
 * zweiten ein Statuscode. Im Fehlerfall wird für den Namen nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -48  (LUT2_NAME_KURZ_NOT_INITIALIZED) "Das Feld Kurzname wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_name_kurz(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   const char *name;
   int retval,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   name=lut_name_kurz(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_NAME_KURZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(name),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_ort(blz[,filiale])</tt>
 * =====<tt>KontoCheck::bank_ort(blz[,filiale])</tt>
 *
 * Diese Funktion liefert den Ort einer Bank. Falls der Parameter filiale nicht
 * angegeben ist, wird der Sitz der Hauptstelle ausgegeben. Der Rückgabewert
 * ist ein Array mit zwei Elementen: im ersten steht ein String mit dem Namen,
 * im zweiten ein Statuscode. Im Fehlerfall wird für den Ort nil
 * zurückgegeben.
 * 
 * Mögliche Statuscodes:
 *
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)   "Das Feld BLZ wurde nicht initialisiert"
 *     -49  (LUT2_ORT_NOT_INITIALIZED)   "Das Feld Ort wurde nicht initialisiert"
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)    "Der Index für die Filiale ist ungültig"
 *      -5  (INVALID_BLZ_LENGTH)         "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                "die Bankleitzahl ist ungültig"
 *       1  (OK)                         "ok"
 */
static VALUE bank_ort(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   const char *ort;
   int retval,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   ort=lut_ort(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_ORT_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(ort),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_plz(blz [,filiale])</tt>
 * =====<tt>KontoCheck::bank_plz(blz [,filiale])</tt>
 * 
 * Diese Funktion liefert die Postleitzahl einer Bank. Falls der Parameter
 * filiale nicht angegeben ist, wird die PLZ der Hauptstelle ausgegeben. Der
 * Rückgabewert ist ein Array mit zwei Elementen: im ersten steht die PLZ, im
 * zweiten ein Statuscode. Im Fehlerfall wird für die PLZ nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -50  (LUT2_PLZ_NOT_INITIALIZED)      "Das Feld PLZ wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_plz(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,plz,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   plz=lut_plz(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_PLZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(plz),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_pz(blz)</tt>
 * =====<tt>KontoCheck::bank_pz(blz)</tt>
 * 
 * Diese Funktion liefert die Prüfziffer einer Bank. Die Funktion unterstützt
 * keine Filialen; zu jeder BLZ kann es in der LUT-Datei nur eine
 * Prüfziffermethode geben. Der Rückgabewert ist ein Array mit zwei Elementen:
 * im ersten steht die Prüfziffer, im zweiten ein Statuscode. Im Fehlerfall
 * wird für den Prüfziffer nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -45  (LUT2_PZ_NOT_INITIALIZED)       "Das Feld Prüfziffer wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_pz(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,pz;

   get_params(argc,argv,blz,NULL,NULL,NULL,0);
   pz=lut_pz(blz,0,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_PZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(pz),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_bic(blz[,filiale])</tt>
 * =====<tt>KontoCheck::bank_bic(blz[,filiale])</tt>
 * 
 * Diese Funktion liefert den BIC (Bank Identifier Code) einer Bank. Der
 * Rückgabewert ist ein Array mit zwei Elementen: im ersten steht ein String
 * mit dem BIC, im zweiten ein Statuscode. Im Fehlerfall wird für den BIC
 * nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -46  (LUT2_BIC_NOT_INITIALIZED)      "Das Feld BIC wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_bic(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   const char *ptr;
   int retval,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   ptr=lut_bic(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_BIC_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(ptr),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_aenderung(blz [,filiale])</tt>
 * =====<tt>KontoCheck::bank_aenderung(blz [,filiale])</tt>
 * 
 * Diese Funktion liefert das  'Änderung' Flag einer Bank (als string).
 * Mögliche Werte sind: A (Addition), M (Modified), U (Unchanged), D
 * (Deletion). Der Rückgabewert ist ein Array mit zwei Elementen: im ersten
 * steht ein String mit dem Änderungsflag, im zweiten ein Statuscode. Im
 * Fehlerfall wird für das Flag nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -43  (LUT2_AENDERUNG_NOT_INITIALIZED) "Das Feld Änderung wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_aenderung(int argc,VALUE* argv,VALUE self)
{
   char blz[16],aenderung[2],error_msg[512];
   int retval,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   *aenderung=lut_aenderung(blz,filiale,&retval);
   aenderung[1]=0;
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_AENDERUNG_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:rb_str_new2(aenderung),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheck::bank_loeschung(blz [,filiale])</tt>
 * =====<tt>KontoCheckRaw::bank_loeschung(blz [,filiale])</tt>
 * 
 * Diese Funktion liefert das Lösch-Flag für eine Bank zurück (als Integer;
 * mögliche Werte sind 0 und 1). Der Rückgabewert ist ein Array mit zwei
 * Elementen: im ersten steht das Flag, im zweiten ein Statuscode. Im
 * Fehlerfall wird für das Flag nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -42  (LUT2_LOESCHUNG_NOT_INITIALIZED) "Das Feld Löschung wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_loeschung(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,loeschung,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   loeschung=lut_loeschung(blz,filiale,&retval)-'0';
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_LOESCHUNG_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(loeschung),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_nachfolge_blz(blz [,filiale])</tt>
 * =====<tt>KontoCheck::bank_nachfolge_blz(blz [,filiale])</tt>
 * 
 * Diese Funktion liefert die Nachfolge-BLZ für eine Bank, die gelöscht werden
 * soll (bei der das 'Löschung' Flag 1 ist). Der Rückgabewert ist ein Array mit
 * zwei Elementen: im ersten steht die Nachfolge-BLZ, im zweiten ein Statuscode. Im
 * Fehlerfall wird für die Nachfolge-BLZ nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -41  (LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED) "Das Feld Nachfolge-BLZ wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_nachfolge_blz(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,n_blz,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   n_blz=lut_nachfolge_blz(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(n_blz),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheckRaw::bank_pan(blz[,filiale])</tt>
 *
 * ===<tt>KontoCheckRaw::bank_pan(blz [,filiale])</tt>
 * =====<tt>KontoCheck::bank_pan(blz [,filiale])</tt>
 * 
 * Diese Funktion liefert den PAN (Primary Account Number) einer Bank. Der
 * Rückgabewert ist ein Array mit zwei Elementen: im ersten steht der PAN, im
 * zweiten ein Statuscode. Im Fehlerfall wird für den PAN nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -47  (LUT2_PAN_NOT_INITIALIZED)      "Das Feld PAN wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_pan(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,pan,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   pan=lut_pan(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_PAN_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(pan),INT2FIX(retval));
}

/**
 * ===<tt>KontoCheck::bank_nr(blz [,filiale])</tt>
 * =====<tt>KontoCheckRaw::bank_nr(blz[,filiale])</tt>
 * 
 * Diese Funktion liefert die laufende Nummer einer Bank (internes Feld der
 * BLZ-Datei). Der Wert wird wahrscheinlich nicht oft benötigt, ist aber der
 * Vollständigkeit halber enthalten. Der Rückgabewert ist ein Array mit zwei
 * Elementen: im ersten steht die Nummer, im zweiten ein Statuscode. Im Fehlerfall
 * wird für die Nummer nil zurückgegeben.
 * 
 * Mögliche Statuscodes:
 * 
 * Possible return values (and short description):
 * 
 *     -55  (LUT2_INDEX_OUT_OF_RANGE)       "Der Index für die Filiale ist ungültig"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)      "Das Feld BLZ wurde nicht initialisiert"
 *     -44  (LUT2_NR_NOT_INITIALIZED)       "Das Feld NR wurde nicht initialisiert"
 *      -5  (INVALID_BLZ_LENGTH)            "die Bankleitzahl ist nicht achtstellig"
 *      -4  (INVALID_BLZ)                   "die Bankleitzahl ist ungültig"
 *       1  (OK)                            "ok"
 */
static VALUE bank_nr(int argc,VALUE* argv,VALUE self)
{
   char blz[16],error_msg[512];
   int retval,nr,filiale;

   get_params(argc,argv,blz,NULL,NULL,&filiale,1);
   nr=lut_nr(blz,filiale,&retval);
   if(retval==LUT2_BLZ_NOT_INITIALIZED || retval==LUT2_NR_NOT_INITIALIZED)RUNTIME_ERROR(retval);
   return rb_ary_new3(2,retval<=0?Qnil:INT2FIX(nr),INT2FIX(retval));
}

/**
 * Die Suchroutinen der C-Bibliothek sind eher Low-Level Routinen und ohne
 * einen Blick in die Sourcen nicht intuitiv nutzbar ;-), daher hier zunächst
 * eine kurze Einführung.
 *
 * Beim ersten Aufruf einer dieser Routinen wird zunächst getestet, ob das
 * Array mit den Zweigstellen bereits erstellt wurde. Dieses Array
 * korrespondiert mit der Tabelle der Bankleitzahlen; es enthält zu jedem Index
 * die jeweilige Zweigstellennummer, wobei Hauptstellen den Wert 0 erhalten.
 * Das erste Element in diesem Array entspricht der Bundesbank.
 *
 * Als zweites wird getestet, ob das Array mit den sortierten Werten schon
 * vorhanden ist; falls nicht, wird Speicher allokiert, der entsprechende Block
 * sortiert und die Indizes in das Array eingetragen (es wäre zu testen, ob die
 * Initialisierung schneller ist, wenn man das Array in der LUT-Datei
 * speichert).
 *
 * Um einen bestimmten Wert zu finden, wird eine binäre Suche durchgeführt.
 * Wird das gesuchte Element gefunden, werden die folgenden Werte zurückgegeben:
 *
 *    - anzahl:      Anzahl der gefundenen Werte, die den Suchkriterien entsprechen
 *
 *    - start_idx:   Pointer auf den ersten Wert im Sortierarray. Die nächsten
 *                   <anzahl> Elemente ab diesem Element entsprechen den
 *                   Sortierkriterien. Die Werte geben dabei den Index innerhalb
 *                   der BLZ-Datei an, beginnend mit 0 für die Bundesbank.
 *
 *    - zweigstelle: Dieses Array enthält die Zweigstellennummern der
 *                   jeweiligen Banken. Es umfasst jedoch den Gesamtbestand der
 *                   BLZ-Datei, nicht den des Sortierarrays.
 *
 *    - base_name:   Dies ist ein Array das die Werte der Suchfunktion (in der
 *                   Reihenfolge der BLZ-Datei) enthält.
 *
 *    - blz_base:    Dies ist das Array mit den Bankleitzahlen.
 *
 * Beispiel: Der Aufruf
 *
 * retval=lut_suche_ort("aa",&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
 *
 * liefert das folgende Ergebnis (mit der LUT-Datei vom 7.3.2011; bei anderen LUT-Dateien
 * können sich die Indizes natürlich verschieben):
 *
 *    anzahl: 38
 *    i:  0, start_idx[ 0]:  9259, blz_base[ 9259]: 58550130, zweigstelle[ 9259]: 42, base_name[ 9259]: Aach b Trier
 *    i:  1, start_idx[ 1]: 12862, blz_base[12862]: 69251445, zweigstelle[12862]:  1, base_name[12862]: Aach, Hegau
 *    i:  2, start_idx[ 2]: 12892, blz_base[12892]: 69290000, zweigstelle[12892]:  5, base_name[12892]: Aach, Hegau
 *    i:  3, start_idx[ 3]:  3946, blz_base[ 3946]: 31010833, zweigstelle[ 3946]: 13, base_name[ 3946]: Aachen
 *    i:  4, start_idx[ 4]:  4497, blz_base[ 4497]: 37060590, zweigstelle[ 4497]:  3, base_name[ 4497]: Aachen
 *    i:  5, start_idx[ 5]:  4830, blz_base[ 4830]: 39000000, zweigstelle[ 4830]:  0, base_name[ 4830]: Aachen
 *    i:  6, start_idx[ 6]:  4831, blz_base[ 4831]: 39010111, zweigstelle[ 4831]:  0, base_name[ 4831]: Aachen
 *    i:  7, start_idx[ 7]:  4833, blz_base[ 4833]: 39020000, zweigstelle[ 4833]:  0, base_name[ 4833]: Aachen
 *    i:  8, start_idx[ 8]:  4834, blz_base[ 4834]: 39040013, zweigstelle[ 4834]:  0, base_name[ 4834]: Aachen
 *    i:  9, start_idx[ 9]:  4841, blz_base[ 4841]: 39050000, zweigstelle[ 4841]:  0, base_name[ 4841]: Aachen
 *    i: 10, start_idx[10]:  4851, blz_base[ 4851]: 39060180, zweigstelle[ 4851]:  0, base_name[ 4851]: Aachen
 *    i: 11, start_idx[11]:  4857, blz_base[ 4857]: 39060180, zweigstelle[ 4857]:  6, base_name[ 4857]: Aachen
 *    i: 12, start_idx[12]:  4858, blz_base[ 4858]: 39060630, zweigstelle[ 4858]:  0, base_name[ 4858]: Aachen
 *    i: 13, start_idx[13]:  4861, blz_base[ 4861]: 39070020, zweigstelle[ 4861]:  0, base_name[ 4861]: Aachen
 *    i: 14, start_idx[14]:  4872, blz_base[ 4872]: 39070024, zweigstelle[ 4872]:  0, base_name[ 4872]: Aachen
 *    i: 15, start_idx[15]:  4883, blz_base[ 4883]: 39080005, zweigstelle[ 4883]:  0, base_name[ 4883]: Aachen
 *    i: 16, start_idx[16]:  4889, blz_base[ 4889]: 39080098, zweigstelle[ 4889]:  0, base_name[ 4889]: Aachen
 *    i: 17, start_idx[17]:  4890, blz_base[ 4890]: 39080099, zweigstelle[ 4890]:  0, base_name[ 4890]: Aachen
 *    i: 18, start_idx[18]:  4891, blz_base[ 4891]: 39160191, zweigstelle[ 4891]:  0, base_name[ 4891]: Aachen
 *    i: 19, start_idx[19]:  4892, blz_base[ 4892]: 39161490, zweigstelle[ 4892]:  0, base_name[ 4892]: Aachen
 *    i: 20, start_idx[20]:  4896, blz_base[ 4896]: 39162980, zweigstelle[ 4896]:  3, base_name[ 4896]: Aachen
 *    i: 21, start_idx[21]:  4906, blz_base[ 4906]: 39500000, zweigstelle[ 4906]:  0, base_name[ 4906]: Aachen
 *    i: 22, start_idx[22]:  6333, blz_base[ 6333]: 50120383, zweigstelle[ 6333]:  1, base_name[ 6333]: Aachen
 *    i: 23, start_idx[23]: 10348, blz_base[10348]: 60420000, zweigstelle[10348]: 18, base_name[10348]: Aachen
 *    i: 24, start_idx[24]: 16183, blz_base[16183]: 76026000, zweigstelle[16183]:  1, base_name[16183]: Aachen
 *    i: 25, start_idx[25]: 10001, blz_base[10001]: 60069673, zweigstelle[10001]:  1, base_name[10001]: Aalen, Württ
 *    i: 26, start_idx[26]: 10685, blz_base[10685]: 61370024, zweigstelle[10685]:  1, base_name[10685]: Aalen, Württ
 *    i: 27, start_idx[27]: 10695, blz_base[10695]: 61370086, zweigstelle[10695]:  3, base_name[10695]: Aalen, Württ
 *    i: 28, start_idx[28]: 10714, blz_base[10714]: 61420086, zweigstelle[10714]:  0, base_name[10714]: Aalen, Württ
 *    i: 29, start_idx[29]: 10715, blz_base[10715]: 61430000, zweigstelle[10715]:  0, base_name[10715]: Aalen, Württ
 *    i: 30, start_idx[30]: 10716, blz_base[10716]: 61440086, zweigstelle[10716]:  0, base_name[10716]: Aalen, Württ
 *    i: 31, start_idx[31]: 10717, blz_base[10717]: 61450050, zweigstelle[10717]:  0, base_name[10717]: Aalen, Württ
 *    i: 32, start_idx[32]: 10755, blz_base[10755]: 61450191, zweigstelle[10755]:  0, base_name[10755]: Aalen, Württ
 *    i: 33, start_idx[33]: 10756, blz_base[10756]: 61480001, zweigstelle[10756]:  0, base_name[10756]: Aalen, Württ
 *    i: 34, start_idx[34]: 10757, blz_base[10757]: 61490150, zweigstelle[10757]:  0, base_name[10757]: Aalen, Württ
 *    i: 35, start_idx[35]: 10766, blz_base[10766]: 61490150, zweigstelle[10766]:  9, base_name[10766]: Aalen, Württ
 *    i: 36, start_idx[36]:  7002, blz_base[ 7002]: 51050015, zweigstelle[ 7002]: 69, base_name[ 7002]: Aarbergen
 *    i: 37, start_idx[37]:  7055, blz_base[ 7055]: 51091700, zweigstelle[ 7055]:  2, base_name[ 7055]: Aarbergen
 * 
 */

/**
 * ===<tt>KontoCheckRaw::bank_suche_bic(search_bic)</tt>
 * =====<tt>KontoCheck::suche()</tt>
 *
 * Diese Funktion sucht alle Banken, deren BIC mit dem angegebenen Wert <search_bic> beginnen.
 * Die Rückgabe ist ein Array mit fünf Elementen:
 *
 *    0. Das erste Element ist ein Array mit den gefundenen BIC-Werten
 *    1. Das zweite Element ist ein Array mit den Bankleitzahlen, die auf das Suchmuster passen
 *    2. Das dritte Element ist ein Array mit den Zweigstellen-Indizes der gefundenen Bankleitzahlen
 *    3. das vierte Element ist der Statuscode (s.u.)
 *    4. das fünfte Element gibt die Anzahl der gefundenen Banken zurück.
 *
 * Mögliche Statuscodes:
 *
 *     -78  (KEY_NOT_FOUND)              "Die Suche lieferte kein Ergebnis"
 *     -70  (LUT1_FILE_USED)             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *     -46  (LUT2_BIC_NOT_INITIALIZED)   "Das Feld BIC wurde nicht initialisiert"
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *      -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
 *       1  (OK)                         "ok"
 */
static VALUE bank_suche_bic(int argc,VALUE* argv,VALUE self)
{
   char such_name[128],**base_name,error_msg[512];
   int i,j,retval,anzahl,*start_idx,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params(argc,argv,such_name,NULL,NULL,NULL,3);
   retval=lut_suche_bic(such_name,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,rb_str_new2(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * ===<tt>KontoCheckRaw::bank_suche_namen(name)</tt>
 * =====<tt>KontoCheck::suche()</tt>
 *
 * Diese Funktion sucht alle Banken, deren Namen mit dem angegebenen Wert <search_bic> beginnen.
 * Die Rückgabe ist ein Array mit fünf Elementen:
 *
 *    0. Das erste Element ist ein Array mit den gefundenen Namenswerten
 *    1. Das zweite Element ist ein Array mit den Bankleitzahlen, die auf das Suchmuster passen
 *    2. Das dritte Element ist ein Array mit den Zweigstellen-Indizes der gefundenen Bankleitzahlen
 *    3. das vierte Element ist der Statuscode (s.u.)
 *    4. das fünfte Element gibt die Anzahl der gefundenen Banken zurück.
 *
 * Mögliche Statuscodes:
 *
 *     -78  (KEY_NOT_FOUND)              "Die Suche lieferte kein Ergebnis"
 *     -70  (LUT1_FILE_USED)             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *     -51  (LUT2_NAME_NOT_INITIALIZED)  "Das Feld Bankname wurde nicht initialisiert"
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *      -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
 *       1  (OK)                         "ok"
 */
static VALUE bank_suche_namen(int argc,VALUE* argv,VALUE self)
{
   char such_name[128],**base_name,error_msg[512];
   int i,j,retval,anzahl,*start_idx,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params(argc,argv,such_name,NULL,NULL,NULL,3);
   retval=lut_suche_namen(such_name,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,rb_str_new2(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * ===<tt>KontoCheckRaw::bank_suche_namen_kurz(short_name)</tt>
 * =====<tt>KontoCheck::suche()</tt>
 *
 * Diese Funktion sucht alle Banken, deren Kurznamen mit dem angegebenen Wert <search_bic> beginnen.
 * Die Rückgabe ist ein Array mit fünf Elementen:
 *
 *    0. Das erste Element ist ein Array mit den gefundenen Namenswerten
 *    1. Das zweite Element ist ein Array mit den Bankleitzahlen, die auf das Suchmuster passen
 *    2. Das dritte Element ist ein Array mit den Zweigstellen-Indizes der gefundenen Bankleitzahlen
 *    3. das vierte Element ist der Statuscode (s.u.)
 *    4. das fünfte Element gibt die Anzahl der gefundenen Banken zurück.
 *
 * Mögliche Statuscodes:
 *
 *     -78  (KEY_NOT_FOUND)              "Die Suche lieferte kein Ergebnis"
 *     -70  (LUT1_FILE_USED)             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *     -48  (LUT2_NAME_KURZ_NOT_INITIALIZED) "Das Feld Kurzname wurde nicht initialisiert"
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *      -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
 *       1  (OK)                         "ok"
 */
static VALUE bank_suche_namen_kurz(int argc,VALUE* argv,VALUE self)
{
   char such_name[128],**base_name,error_msg[512];
   int i,j,retval,anzahl,*start_idx,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params(argc,argv,such_name,NULL,NULL,NULL,3);
   retval=lut_suche_namen_kurz(such_name,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,rb_str_new2(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * ===<tt>KontoCheckRaw::bank_suche_plz(plz1[,plz2])</tt>
 * =====<tt>KontoCheck::suche()</tt>
 *
 * Diese Funktion sucht alle Banken, deren PLZ gleich <plz1> ist oder (bei
 * Angabe von plz2) die im Bereich zwischen <plz1> und <plz2> liegen.. Die
 * Rückgabe ist ein Array mit fünf Elementen:
 *
 *    0. Das erste Element ist ein Array mit den gefundenen Namenswerten
 *    1. Das zweite Element ist ein Array mit den Bankleitzahlen, die auf das Suchmuster passen
 *    2. Das dritte Element ist ein Array mit den Zweigstellen-Indizes der gefundenen Bankleitzahlen
 *    3. das vierte Element ist der Statuscode (s.u.)
 *    4. das fünfte Element gibt die Anzahl der gefundenen Banken zurück.
 *
 * Mögliche Statuscodes:
 *
 *     -78  (KEY_NOT_FOUND)              "Die Suche lieferte kein Ergebnis"
 *     -70  (LUT1_FILE_USED)             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *     -50  (LUT2_PLZ_NOT_INITIALIZED)   "Das Feld PLZ wurde nicht initialisiert"
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *      -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
 *       1  (OK)                         "ok"
 */
static VALUE bank_suche_plz(int argc,VALUE* argv,VALUE self)
{
   char error_msg[512];
   int i,j,retval,such1,such2,anzahl,*start_idx,*base_name,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params_int(argc,argv,&such1,&such2);
   retval=lut_suche_plz(such1,such2,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,INT2FIX(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * ===<tt>KontoCheckRaw::bank_suche_pz(pz1[,pz2])</tt>
 * =====<tt>KontoCheck::suche()</tt>
 *
 * Diese Funktion sucht alle Banken, deren Prüfziffer gleich <pz1> ist oder (bei
 * Angabe von pz2) die im Bereich zwischen <pz1> und <pz2> liegen.. Die
 * Rückgabe ist ein Array mit fünf Elementen:
 *
 *    0. Das erste Element ist ein Array mit den gefundenen Prüfziffern
 *    1. Das zweite Element ist ein Array mit den Bankleitzahlen, die auf das Suchmuster passen
 *    2. Das dritte Element ist ein Array mit den Zweigstellen-Indizes der gefundenen Bankleitzahlen
 *    3. das vierte Element ist der Statuscode (s.u.)
 *    4. das fünfte Element gibt die Anzahl der gefundenen Banken zurück.
 *
 * Mögliche Statuscodes:
 *
 *     -78  (KEY_NOT_FOUND)              "Die Suche lieferte kein Ergebnis"
 *     -70  (LUT1_FILE_USED)             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *     -45  (LUT2_PZ_NOT_INITIALIZED)    "Das Feld Prüfziffer wurde nicht initialisiert"
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *      -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
 *       1  (OK)                         "ok"
 */
static VALUE bank_suche_pz(int argc,VALUE* argv,VALUE self)
{
   char error_msg[512];
   int i,j,retval,such1,such2,anzahl,*start_idx,*base_name,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params_int(argc,argv,&such1,&such2);
   retval=lut_suche_pz(such1,such2,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,INT2FIX(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * ===<tt>KontoCheckRaw::bank_suche_blz(blz1[,blz2])</tt>
 * =====<tt>KontoCheck::suche()</tt>
 *
 * Diese Funktion sucht alle Banken, deren Bankleitzahl gleich <blz1> ist oder (bei
 * Angabe von blz2) die im Bereich zwischen <blz1> und <blz2> liegen.. Die
 * Rückgabe ist ein Array mit fünf Elementen:
 *
 *    0. Das erste Element ist ein Array mit den gefundenen Bankleitzahlen
 *    1. Das zweite Element ist ein Array mit den Bankleitzahlen, die auf das Suchmuster passen (bei dieser Funktion doppelt gemoppelt :-) )
 *    2. Das dritte Element ist ein Array mit den Zweigstellen-Indizes der gefundenen Bankleitzahlen
 *    3. das vierte Element ist der Statuscode (s.u.)
 *    4. das fünfte Element gibt die Anzahl der gefundenen Banken zurück.
 *
 * Mögliche Statuscodes:
 *
 * find all banks with bic between blz1 and blz2
 *
 * possible return values (and short description):
 *
 *     -78  (KEY_NOT_FOUND)              "Die Suche lieferte kein Ergebnis"
 *     -70  (LUT1_FILE_USED)             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *     -53  (LUT2_BLZ_NOT_INITIALIZED)   "Das Feld BLZ wurde nicht initialisiert"
 *     -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
 *      -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
 *       1  (OK)                         "ok"
 */
static VALUE bank_suche_blz(int argc,VALUE* argv,VALUE self)
{
   char error_msg[512];
   int i,j,retval,such1,such2,anzahl,*start_idx,*base_name,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params_int(argc,argv,&such1,&such2);
   retval=lut_suche_blz(such1,such2,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,INT2FIX(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * ===<tt>KontoCheckRaw::bank_suche_ort(suchort)</tt>
 * =====<tt>KontoCheck::suche()</tt>
 *
 * Diese Funktion sucht alle Banken, deren Sitz mit dem angegebenen Wert <suchort> beginnen.
 * Die Rückgabe ist ein Array mit fünf Elementen:
 *
 *    0. Das erste Element ist ein Array mit den gefundenen Orten
 *    1. Das zweite Element ist ein Array mit den Bankleitzahlen, die auf das Suchmuster passen
 *    2. Das dritte Element ist ein Array mit den Zweigstellen-Indizes der gefundenen Bankleitzahlen
 *    3. das vierte Element ist der Statuscode (s.u.)
 *    4. das fünfte Element gibt die Anzahl der gefundenen Banken zurück.
 *
 * Mögliche Statuscodes:
 *
 *    LUT1_FILE_USED             "Es wurde eine LUT-Datei im Format 1.0/1.1 geladen"
 *    LUT2_NOT_INITIALIZED       "die Programmbibliothek wurde noch nicht initialisiert"
 *    LUT2_ORT_NOT_INITIALIZED   "Das Feld Ort wurde nicht initialisiert"
 *    ERROR_MALLOC               "kann keinen Speicher allokieren"
 *    KEY_NOT_FOUND              "Die Suche lieferte kein Ergebnis"
 *    OK                         "ok"
 */
static VALUE bank_suche_ort(int argc,VALUE* argv,VALUE self)
{
   char such_name[128],**base_name,error_msg[512];
   int i,j,retval,anzahl,*start_idx,*zweigstelle,*blz_base;
   VALUE ret_blz,ret_idx,ret_suche;

   get_params(argc,argv,such_name,NULL,NULL,NULL,3);
   retval=lut_suche_ort(such_name,&anzahl,&start_idx,&zweigstelle,&base_name,&blz_base);
   if(retval==KEY_NOT_FOUND)return rb_ary_new3(5,Qnil,Qnil,Qnil,INT2FIX(retval),INT2FIX(0));
   if(retval<0)RUNTIME_ERROR(retval);
   ret_suche=rb_ary_new2(anzahl);
   ret_blz=rb_ary_new2(anzahl);
   ret_idx=rb_ary_new2(anzahl);
   for(i=0;i<anzahl;i++){
      j=start_idx[i];   /* Index innerhalb der BLZ-Datei */
      rb_ary_store(ret_suche,i,rb_str_new2(base_name[j]));
      rb_ary_store(ret_blz,i,INT2FIX(blz_base[j]));
      rb_ary_store(ret_idx,i,INT2FIX(zweigstelle[j]));
   }
   return rb_ary_new3(5,ret_suche,ret_blz,ret_idx,INT2FIX(retval),INT2FIX(anzahl));
}

/**
 * ===<tt>KontoCheckRaw::version()</tt>
 * =====<tt>KontoCheck::version()</tt>
 *
 * Diese Funktion gibt den Versions-String der C-Bibliothek zurück.
 */
static VALUE version_rb(int argc,VALUE* argv,VALUE self)
{
   return rb_str_new2(get_kto_check_version());
}

/**
 * The initialization method for this module
 */
void Init_konto_check_raw()
{
/* 
 * This is a C/Ruby library to check the validity of German Bank Account
 * Numbers. All currently defined test methods by Deutsche Bundesbank (April
 * 2011: 00 to D8) are implemented. 
 * 
 * <b>ATTENTION:</b> There are a few important changes in the API between version 0.0.2 (version
 * by Peter Horn/Provideal), version 0.0.6 (jeanmartin) and this version:
 * 
 * * The function KontoCheck::load_bank_data() is no longer used; it is replaced by KontoCheck::init() and KontoCheck::generate_lutfile().
 * * The function KontoCheck::konto_check(blz,kto) changed the order of parameters from (kto,blz) to (blz,kto)
 * 
 * Another change affects only the version 0.0.6 by jeanmartin:
 * 
 * * In KontoCheck::init(level,name,set) the order of the two first parameters is now free; the order is determined by the type of the variable (level is integer, filename string). 
 * 
 * Because this class is inteded for german bank accounts, the rest of the
 * documentation is in german too.
 *
 * Diese Bibliothek implementiert die Prüfziffertests für deutsche Bankkonten.
 * Die meisten Konten enthalten eine Prüfziffer; mit dieser kann getestet
 * werden, ob eine Bankleitzahl plausibel ist oder nicht. Auf diese Weise
 * können Zahlendreher oder Zifferverdopplungen oft festgestellt werden. Es ist
 * natürlich nicht möglich, zu bestimmen, ob ein Konto wirklich existiert; dazu
 * müßte jeweils eine Anfrage bei der Bank gemacht werden ;-).
 *
 * Die Bibliothek ist in zwei Teile gesplittet: KontoCheckRaw bildet die
 * direkte Schnittstelle zur C-Bibliothek und ist daher manchmal etwas sperrig;
 * KontoCheck ist dagegen mehr Ruby-artig ausgelegt. KontoCheck gibt meist nur
 * eine Teilmenge von KontoCheckRaw zurück, aber (hoffentlich) die Teile, die
 * man unmittelbar von den jeweiligen Funktionen erwartet. Eine Reihe einfacher
 * Funktionen sind auch in beiden Versionen identisch.
 *
 * Die Bankleitzahldaten werden in einem eigenen (komprimierten) Format in einer
 * sogenannten LUT-Datei gespeichert. Diese Datei läßt sich mit der Funktion
 * KontoCheck::generate_lutfile bzw. KontoCheckRaw::generate_lutfile aus der
 * Datei der Deutschen Bundesbank (online erhältlich unter
 * http://www.bundesbank.de/zahlungsverkehr/zahlungsverkehr_bankleitzahlen_download.php)
 * erzeugen. Die LUT-Datei hat den großen Vorteil, daß die Datenblocks (BLZ,
 * Prüfziffer, Bankname, Ort, ...) unabhängig voneinander gespeichert sind;
 * jeder Block kann für sich geladen werden. In einer Datei können zwei
 * Datensätze der Bundesbank mit unterschiedlichem Gültigkeitsdatum enthalten
 * sein. Wenn bei der Initialisierung kein bestimmter Datensatz ausgewählt
 * wird, prüft die Bibliothek aufgrund des mit jedem Datensatz gespeicherten
 * Gültigkeitszeitraums welcher Satz aktuell gültig ist und lädt diesen dann in
 * den Speicher.
 *
 * Numerische Werte (z.B. Bankleitzahlen, Kontonummern, PLZ,...) können als
 * Zahlwerte oder als String angegeben werden; sie werden automatisch
 * konvertiert. Prüfziffermethoden können ebenfalls als Zahl oder String
 * angegeben werden; die Angabe als Zahl ist allerdings nicht immer eindeutig. So
 * kann z.B. 131 sowohl für D1 als auch 13a stehen; daher ist es besser, die
 * Prüfziffermethode als String anzugeben (in diesem Beispiel würde 131 als 13a
 * interpretiert).
 */
   KontoCheck = rb_define_module("KontoCheckRaw");

      /* die Parameteranzahl -1 bei den folgenden Funktionen meint eine variable
       * Anzahl Parameter; die genaue Anzahl wird bei der Funktion rb_scan_args()
       * als 3. Parameter angegeben.
       *
       * Bei -1 erfolgt die Übergabe als C-Array, bei -2 ist die Übergabe als
       * Ruby-Array (s. http://karottenreibe.github.com/2009/10/28/ruby-c-extension-5/):
       *
       *    A positive number means, your function will take just that many
       *       arguments, none less, none more.
       *    -1 means the function takes a variable number of arguments, passed
       *       as a C array.
       *    -2 means the function takes a variable number of arguments, passed
       *       as a Ruby array.
       *
       * Was in ext_ruby.pdf (S.39) dazu steht, ist einfach falsch (das hat
       * mich einige Zeit gekostet):
       *
       *    In some of the function definitions that follow, the parameter argc
       *    specifies how many arguments a Ruby method takes. It may have the
       *    following values. If the value is not negative, it specifies the
       *    number of arguments the method takes. If negative, it indicates
       *    that the method takes optional arguments. In this case, the
       *    absolute value of argc minus one is the number of required
       *    arguments (so -1 means all arguments are optional, -2 means one
       *    mandatory argument followed by optional arguments, and so on).
       *
       * Es wäre zu testen, ob dieses Verhalten bei älteren Versionen gilt;
       * dann müßte man u.U. eine entsprechende Verzweigung einbauen. Bei den
       * aktuellen Varianten (Ruby 1.8.6 und 1.9.2) funktioniert diese
       * Variante.
       */
   rb_define_module_function(KontoCheck,"init",init,-1);
   rb_define_module_function(KontoCheck,"current_lutfile_name",current_lutfile_name_rb,0);
   rb_define_module_function(KontoCheck,"free",free_rb,0);
   rb_define_module_function(KontoCheck,"konto_check",konto_check,-1);
   rb_define_module_function(KontoCheck,"konto_check_pz",konto_check_pz,-1);
   rb_define_module_function(KontoCheck,"bank_valid",bank_valid,-1);
   rb_define_module_function(KontoCheck,"bank_filialen",bank_filialen,-1);
   rb_define_module_function(KontoCheck,"bank_alles",bank_alles,-1);
   rb_define_module_function(KontoCheck,"bank_name",bank_name,-1);
   rb_define_module_function(KontoCheck,"bank_name_kurz",bank_name_kurz,-1);
   rb_define_module_function(KontoCheck,"bank_plz",bank_plz,-1);
   rb_define_module_function(KontoCheck,"bank_ort",bank_ort,-1);
   rb_define_module_function(KontoCheck,"bank_pan",bank_pan,-1);
   rb_define_module_function(KontoCheck,"bank_bic",bank_bic,-1);
   rb_define_module_function(KontoCheck,"bank_nr",bank_nr,-1);
   rb_define_module_function(KontoCheck,"bank_pz",bank_pz,-1);
   rb_define_module_function(KontoCheck,"bank_aenderung",bank_aenderung,-1);
   rb_define_module_function(KontoCheck,"bank_loeschung",bank_loeschung,-1);
   rb_define_module_function(KontoCheck,"bank_nachfolge_blz",bank_nachfolge_blz,-1);
   rb_define_module_function(KontoCheck,"dump_lutfile",dump_lutfile_rb,-1);
   rb_define_module_function(KontoCheck,"lut_info",lut_info_rb,-1);
   rb_define_module_function(KontoCheck,"keep_raw_data",keep_raw_data_rb,1);
   rb_define_module_function(KontoCheck,"encoding",encoding_rb,-1);
   rb_define_module_function(KontoCheck,"encoding_str",encoding_str_rb,-1);
   rb_define_module_function(KontoCheck,"retval2txt",retval2txt_rb,1);
   rb_define_module_function(KontoCheck,"retval2iso",retval2iso_rb,1);
   rb_define_module_function(KontoCheck,"retval2txt_short",retval2txt_short_rb,1);
   rb_define_module_function(KontoCheck,"retval2dos",retval2dos_rb,1);
   rb_define_module_function(KontoCheck,"retval2html",retval2html_rb,1);
   rb_define_module_function(KontoCheck,"retval2utf8",retval2utf8_rb,1);
   rb_define_module_function(KontoCheck,"generate_lutfile",generate_lutfile_rb,-1);
   rb_define_module_function(KontoCheck,"iban_check",iban_check_rb,-1);
   rb_define_module_function(KontoCheck,"iban2bic",iban2bic_rb,-1);
   rb_define_module_function(KontoCheck,"iban_gen",iban_gen_rb,-1);
   rb_define_module_function(KontoCheck,"ipi_gen",ipi_gen_rb,-1);
   rb_define_module_function(KontoCheck,"ipi_check",ipi_check_rb,-1);
   rb_define_module_function(KontoCheck,"bank_suche_bic",bank_suche_bic,-1);
   rb_define_module_function(KontoCheck,"bank_suche_namen",bank_suche_namen,-1);
   rb_define_module_function(KontoCheck,"bank_suche_namen_kurz",bank_suche_namen_kurz,-1);
   rb_define_module_function(KontoCheck,"bank_suche_ort",bank_suche_ort,-1);
   rb_define_module_function(KontoCheck,"bank_suche_blz",bank_suche_blz,-1);
   rb_define_module_function(KontoCheck,"bank_suche_plz",bank_suche_plz,-1);
   rb_define_module_function(KontoCheck,"bank_suche_pz",bank_suche_pz,-1);
   rb_define_module_function(KontoCheck,"version",version_rb,0);
   rb_define_module_function(KontoCheck,"load_bank_data",load_bank_data,1);

      /* Rückgabewerte der konto_check Bibliothek */
      /* (-113) das Institut erlaubt keine eigene IBAN-Berechnung */
   rb_define_const(KontoCheck,"NO_OWN_IBAN_CALCULATION",INT2FIX(NO_OWN_IBAN_CALCULATION));
      /* (-112) die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden */
   rb_define_const(KontoCheck,"KTO_CHECK_UNSUPPORTED_COMPRESSION",INT2FIX(KTO_CHECK_UNSUPPORTED_COMPRESSION));
      /* (-111) der angegebene Wert für die Default-Kompression ist ungültig */
   rb_define_const(KontoCheck,"KTO_CHECK_INVALID_COMPRESSION_LIB",INT2FIX(KTO_CHECK_INVALID_COMPRESSION_LIB));
      /* (-110) wahrscheinlich OK; es wurde allerdings ein (weggelassenes) Unterkonto angefügt */
   rb_define_const(KontoCheck,"OK_UNTERKONTO_ATTACHED",INT2FIX(OK_UNTERKONTO_ATTACHED));
      /* (-109) Ungültige Signatur im Default-Block */
   rb_define_const(KontoCheck,"KTO_CHECK_DEFAULT_BLOCK_INVALID",INT2FIX(KTO_CHECK_DEFAULT_BLOCK_INVALID));
      /* (-108) Die maximale Anzahl Einträge für den Default-Block wurde erreicht */
   rb_define_const(KontoCheck,"KTO_CHECK_DEFAULT_BLOCK_FULL",INT2FIX(KTO_CHECK_DEFAULT_BLOCK_FULL));
      /* (-107) Es wurde noch kein Default-Block angelegt */
   rb_define_const(KontoCheck,"KTO_CHECK_NO_DEFAULT_BLOCK",INT2FIX(KTO_CHECK_NO_DEFAULT_BLOCK));
      /* (-106) Der angegebene Schlüssel wurde im Default-Block nicht gefunden */
   rb_define_const(KontoCheck,"KTO_CHECK_KEY_NOT_FOUND",INT2FIX(KTO_CHECK_KEY_NOT_FOUND));
      /* (-105) Beide Datensätze sind nicht mehr gültig; dieser ist  aber jünger als der andere */
   rb_define_const(KontoCheck,"LUT2_NO_LONGER_VALID_BETTER",INT2FIX(LUT2_NO_LONGER_VALID_BETTER));
      /* (-104) Die Auftraggeber-Kontonummer des C-Datensatzes unterscheidet sich von der des A-Satzes */
   rb_define_const(KontoCheck,"DTA_SRC_KTO_DIFFERENT",INT2FIX(DTA_SRC_KTO_DIFFERENT));
      /* (-103) Die Auftraggeber-Bankleitzahl des C-Datensatzes unterscheidet sich von der des A-Satzes */
   rb_define_const(KontoCheck,"DTA_SRC_BLZ_DIFFERENT",INT2FIX(DTA_SRC_BLZ_DIFFERENT));
      /* (-102) Die DTA-Datei enthält (unzulässige) Zeilenvorschübe */
   rb_define_const(KontoCheck,"DTA_CR_LF_IN_FILE",INT2FIX(DTA_CR_LF_IN_FILE));
      /* (-101) ungültiger Typ bei einem Erweiterungsblock eines C-Datensatzes */
   rb_define_const(KontoCheck,"DTA_INVALID_C_EXTENSION",INT2FIX(DTA_INVALID_C_EXTENSION));
      /* (-100) Es wurde ein C-Datensatz erwartet, jedoch ein E-Satz gefunden */
   rb_define_const(KontoCheck,"DTA_FOUND_SET_A_NOT_C",INT2FIX(DTA_FOUND_SET_A_NOT_C));
      /* (-99) Es wurde ein C-Datensatz erwartet, jedoch ein E-Satz gefunden */
   rb_define_const(KontoCheck,"DTA_FOUND_SET_E_NOT_C",INT2FIX(DTA_FOUND_SET_E_NOT_C));
      /* (-98) Es wurde ein C-Datensatzerweiterung erwartet, jedoch ein C-Satz gefunden */
   rb_define_const(KontoCheck,"DTA_FOUND_SET_C_NOT_EXTENSION",INT2FIX(DTA_FOUND_SET_C_NOT_EXTENSION));
      /* (-97) Es wurde ein C-Datensatzerweiterung erwartet, jedoch ein E-Satz gefunden */
   rb_define_const(KontoCheck,"DTA_FOUND_SET_E_NOT_EXTENSION",INT2FIX(DTA_FOUND_SET_E_NOT_EXTENSION));
      /* (-96) Die Anzahl Erweiterungen paßt nicht zur Blocklänge */
   rb_define_const(KontoCheck,"DTA_INVALID_EXTENSION_COUNT",INT2FIX(DTA_INVALID_EXTENSION_COUNT));
      /* (-95) Ungültige Zeichen in numerischem Feld */
   rb_define_const(KontoCheck,"DTA_INVALID_NUM",INT2FIX(DTA_INVALID_NUM));
      /* (-94) Ungültige Zeichen im Textfeld */
   rb_define_const(KontoCheck,"DTA_INVALID_CHARS",INT2FIX(DTA_INVALID_CHARS));
      /* (-93) Die Währung des DTA-Datensatzes ist nicht Euro */
   rb_define_const(KontoCheck,"DTA_CURRENCY_NOT_EURO",INT2FIX(DTA_CURRENCY_NOT_EURO));
      /* (-92) In einem DTA-Datensatz wurde kein Betrag angegeben */
   rb_define_const(KontoCheck,"DTA_EMPTY_AMOUNT",INT2FIX(DTA_EMPTY_AMOUNT));
      /* (-91) Ungültiger Textschlüssel in der DTA-Datei */
   rb_define_const(KontoCheck,"DTA_INVALID_TEXT_KEY",INT2FIX(DTA_INVALID_TEXT_KEY));
      /* (-90) Für ein (alphanumerisches) Feld wurde kein Wert angegeben */
   rb_define_const(KontoCheck,"DTA_EMPTY_STRING",INT2FIX(DTA_EMPTY_STRING));
      /* (-89) Die Startmarkierung des A-Datensatzes wurde nicht gefunden */
   rb_define_const(KontoCheck,"DTA_MARKER_A_NOT_FOUND",INT2FIX(DTA_MARKER_A_NOT_FOUND));
      /* (-88) Die Startmarkierung des C-Datensatzes wurde nicht gefunden */
   rb_define_const(KontoCheck,"DTA_MARKER_C_NOT_FOUND",INT2FIX(DTA_MARKER_C_NOT_FOUND));
      /* (-87) Die Startmarkierung des E-Datensatzes wurde nicht gefunden */
   rb_define_const(KontoCheck,"DTA_MARKER_E_NOT_FOUND",INT2FIX(DTA_MARKER_E_NOT_FOUND));
      /* (-86) Die Satzlänge eines C-Datensatzes muß zwischen 187 und 622 Byte betragen */
   rb_define_const(KontoCheck,"DTA_INVALID_SET_C_LEN",INT2FIX(DTA_INVALID_SET_C_LEN));
      /* (-85) Die Satzlänge eines A- bzw. E-Satzes muß 128 Byte betragen */
   rb_define_const(KontoCheck,"DTA_INVALID_SET_LEN",INT2FIX(DTA_INVALID_SET_LEN));
      /* (-84) als Währung in der DTA-Datei ist nicht Euro eingetragen */
   rb_define_const(KontoCheck,"DTA_WAERUNG_NOT_EURO",INT2FIX(DTA_WAERUNG_NOT_EURO));
      /* (-83) das Ausführungsdatum ist zu früh oder zu spät (max. 15 Tage nach Dateierstellung) */
   rb_define_const(KontoCheck,"DTA_INVALID_ISSUE_DATE",INT2FIX(DTA_INVALID_ISSUE_DATE));
      /* (-82) das Datum ist ungültig */
   rb_define_const(KontoCheck,"DTA_INVALID_DATE",INT2FIX(DTA_INVALID_DATE));
      /* (-81) Formatfehler in der DTA-Datei */
   rb_define_const(KontoCheck,"DTA_FORMAT_ERROR",INT2FIX(DTA_FORMAT_ERROR));
      /* (-80) die DTA-Datei enthält Fehler */
   rb_define_const(KontoCheck,"DTA_FILE_WITH_ERRORS",INT2FIX(DTA_FILE_WITH_ERRORS));
      /* (-79) ungültiger Suchbereich angegeben (unten>oben) */
   rb_define_const(KontoCheck,"INVALID_SEARCH_RANGE",INT2FIX(INVALID_SEARCH_RANGE));
      /* (-78) Die Suche lieferte kein Ergebnis */
   rb_define_const(KontoCheck,"KEY_NOT_FOUND",INT2FIX(KEY_NOT_FOUND));
      /* (-77) BAV denkt, das Konto ist falsch (konto_check hält es für richtig) */
   rb_define_const(KontoCheck,"BAV_FALSE",INT2FIX(BAV_FALSE));
      /* (-76) User-Blocks müssen einen Typ > 500 haben */
   rb_define_const(KontoCheck,"LUT2_NO_USER_BLOCK",INT2FIX(LUT2_NO_USER_BLOCK));
      /* (-75) für ein LUT-Set sind nur die Werte 0, 1 oder 2 möglich */
   rb_define_const(KontoCheck,"INVALID_SET",INT2FIX(INVALID_SET));
      /* (-74) Ein Konto kann kann nur für deutsche Banken geprüft werden */
   rb_define_const(KontoCheck,"NO_GERMAN_BIC",INT2FIX(NO_GERMAN_BIC));
      /* (-73) Der zu validierende strukturierete Verwendungszweck muß genau 20 Zeichen enthalten */
   rb_define_const(KontoCheck,"IPI_CHECK_INVALID_LENGTH",INT2FIX(IPI_CHECK_INVALID_LENGTH));
      /* (-72) Im strukturierten Verwendungszweck dürfen nur alphanumerische Zeichen vorkommen */
   rb_define_const(KontoCheck,"IPI_INVALID_CHARACTER",INT2FIX(IPI_INVALID_CHARACTER));
      /* (-71) Die Länge des IPI-Verwendungszwecks darf maximal 18 Byte sein */
   rb_define_const(KontoCheck,"IPI_INVALID_LENGTH",INT2FIX(IPI_INVALID_LENGTH));
      /* (-70) Es wurde eine LUT-Datei im Format 1.0/1.1 geladen */
   rb_define_const(KontoCheck,"LUT1_FILE_USED",INT2FIX(LUT1_FILE_USED));
      /* (-69) Bei der Kontoprüfung fehlt ein notwendiger Parameter (BLZ oder Konto) */
   rb_define_const(KontoCheck,"MISSING_PARAMETER",INT2FIX(MISSING_PARAMETER));
      /* (-68) Die Funktion iban2bic() arbeitet nur mit deutschen Bankleitzahlen */
   rb_define_const(KontoCheck,"IBAN2BIC_ONLY_GERMAN",INT2FIX(IBAN2BIC_ONLY_GERMAN));
      /* (-67) Die Prüfziffer der IBAN stimmt, die der Kontonummer nicht */
   rb_define_const(KontoCheck,"IBAN_OK_KTO_NOT",INT2FIX(IBAN_OK_KTO_NOT));
      /* (-66) Die Prüfziffer der Kontonummer stimmt, die der IBAN nicht */
   rb_define_const(KontoCheck,"KTO_OK_IBAN_NOT",INT2FIX(KTO_OK_IBAN_NOT));
      /* (-65) Es sind nur maximal 500 Slots pro LUT-Datei möglich (Neukompilieren erforderlich) */
   rb_define_const(KontoCheck,"TOO_MANY_SLOTS",INT2FIX(TOO_MANY_SLOTS));
      /* (-64) Initialisierung fehlgeschlagen (init_wait geblockt) */
   rb_define_const(KontoCheck,"INIT_FATAL_ERROR",INT2FIX(INIT_FATAL_ERROR));
      /* (-63) Ein inkrementelles Initialisieren benötigt einen Info-Block in der LUT-Datei */
   rb_define_const(KontoCheck,"INCREMENTAL_INIT_NEEDS_INFO",INT2FIX(INCREMENTAL_INIT_NEEDS_INFO));
      /* (-62) Ein inkrementelles Initialisieren mit einer anderen LUT-Datei ist nicht möglich */
   rb_define_const(KontoCheck,"INCREMENTAL_INIT_FROM_DIFFERENT_FILE",INT2FIX(INCREMENTAL_INIT_FROM_DIFFERENT_FILE));
      /* (-61) Die Funktion ist nur in der Debug-Version vorhanden */
   rb_define_const(KontoCheck,"DEBUG_ONLY_FUNCTION",INT2FIX(DEBUG_ONLY_FUNCTION));
      /* (-60) Kein Datensatz der LUT-Datei ist aktuell gültig */
   rb_define_const(KontoCheck,"LUT2_INVALID",INT2FIX(LUT2_INVALID));
      /* (-59) Der Datensatz ist noch nicht gültig */
   rb_define_const(KontoCheck,"LUT2_NOT_YET_VALID",INT2FIX(LUT2_NOT_YET_VALID));
      /* (-58) Der Datensatz ist nicht mehr gültig */
   rb_define_const(KontoCheck,"LUT2_NO_LONGER_VALID",INT2FIX(LUT2_NO_LONGER_VALID));
      /* (-57) Im Gültigkeitsdatum sind Anfangs- und Enddatum vertauscht */
   rb_define_const(KontoCheck,"LUT2_GUELTIGKEIT_SWAPPED",INT2FIX(LUT2_GUELTIGKEIT_SWAPPED));
      /* (-56) Das angegebene Gültigkeitsdatum ist ungültig (Soll: JJJJMMTT-JJJJMMTT) */
   rb_define_const(KontoCheck,"LUT2_INVALID_GUELTIGKEIT",INT2FIX(LUT2_INVALID_GUELTIGKEIT));
      /* (-55) Der Index für die Filiale ist ungültig */
   rb_define_const(KontoCheck,"LUT2_INDEX_OUT_OF_RANGE",INT2FIX(LUT2_INDEX_OUT_OF_RANGE));
      /* (-54) Die Bibliothek wird gerade neu initialisiert */
   rb_define_const(KontoCheck,"LUT2_INIT_IN_PROGRESS",INT2FIX(LUT2_INIT_IN_PROGRESS));
      /* (-53) Das Feld BLZ wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_BLZ_NOT_INITIALIZED",INT2FIX(LUT2_BLZ_NOT_INITIALIZED));
      /* (-52) Das Feld Filialen wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_FILIALEN_NOT_INITIALIZED",INT2FIX(LUT2_FILIALEN_NOT_INITIALIZED));
      /* (-51) Das Feld Bankname wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_NAME_NOT_INITIALIZED",INT2FIX(LUT2_NAME_NOT_INITIALIZED));
      /* (-50) Das Feld PLZ wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_PLZ_NOT_INITIALIZED",INT2FIX(LUT2_PLZ_NOT_INITIALIZED));
      /* (-49) Das Feld Ort wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_ORT_NOT_INITIALIZED",INT2FIX(LUT2_ORT_NOT_INITIALIZED));
      /* (-48) Das Feld Kurzname wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_NAME_KURZ_NOT_INITIALIZED",INT2FIX(LUT2_NAME_KURZ_NOT_INITIALIZED));
      /* (-47) Das Feld PAN wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_PAN_NOT_INITIALIZED",INT2FIX(LUT2_PAN_NOT_INITIALIZED));
      /* (-46) Das Feld BIC wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_BIC_NOT_INITIALIZED",INT2FIX(LUT2_BIC_NOT_INITIALIZED));
      /* (-45) Das Feld Prüfziffer wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_PZ_NOT_INITIALIZED",INT2FIX(LUT2_PZ_NOT_INITIALIZED));
      /* (-44) Das Feld NR wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_NR_NOT_INITIALIZED",INT2FIX(LUT2_NR_NOT_INITIALIZED));
      /* (-43) Das Feld Änderung wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_AENDERUNG_NOT_INITIALIZED",INT2FIX(LUT2_AENDERUNG_NOT_INITIALIZED));
      /* (-42) Das Feld Löschung wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_LOESCHUNG_NOT_INITIALIZED",INT2FIX(LUT2_LOESCHUNG_NOT_INITIALIZED));
      /* (-41) Das Feld Nachfolge-BLZ wurde nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED",INT2FIX(LUT2_NACHFOLGE_BLZ_NOT_INITIALIZED));
      /* (-40) die Programmbibliothek wurde noch nicht initialisiert */
   rb_define_const(KontoCheck,"LUT2_NOT_INITIALIZED",INT2FIX(LUT2_NOT_INITIALIZED));
      /* (-39) der Block mit der Filialenanzahl fehlt in der LUT-Datei */
   rb_define_const(KontoCheck,"LUT2_FILIALEN_MISSING",INT2FIX(LUT2_FILIALEN_MISSING));
      /* (-38) es wurden nicht alle Blocks geladen */
   rb_define_const(KontoCheck,"LUT2_PARTIAL_OK",INT2FIX(LUT2_PARTIAL_OK));
      /* (-37) Buffer error in den ZLIB Routinen */
   rb_define_const(KontoCheck,"LUT2_Z_BUF_ERROR",INT2FIX(LUT2_Z_BUF_ERROR));
      /* (-36) Memory error in den ZLIB-Routinen */
   rb_define_const(KontoCheck,"LUT2_Z_MEM_ERROR",INT2FIX(LUT2_Z_MEM_ERROR));
      /* (-35) Datenfehler im komprimierten LUT-Block */
   rb_define_const(KontoCheck,"LUT2_Z_DATA_ERROR",INT2FIX(LUT2_Z_DATA_ERROR));
      /* (-34) Der Block ist nicht in der LUT-Datei enthalten */
   rb_define_const(KontoCheck,"LUT2_BLOCK_NOT_IN_FILE",INT2FIX(LUT2_BLOCK_NOT_IN_FILE));
      /* (-33) Fehler beim Dekomprimieren eines LUT-Blocks */
   rb_define_const(KontoCheck,"LUT2_DECOMPRESS_ERROR",INT2FIX(LUT2_DECOMPRESS_ERROR));
      /* (-32) Fehler beim Komprimieren eines LUT-Blocks */
   rb_define_const(KontoCheck,"LUT2_COMPRESS_ERROR",INT2FIX(LUT2_COMPRESS_ERROR));
      /* (-31) Die LUT-Datei ist korrumpiert */
   rb_define_const(KontoCheck,"LUT2_FILE_CORRUPTED",INT2FIX(LUT2_FILE_CORRUPTED));
      /* (-30) Im Inhaltsverzeichnis der LUT-Datei ist kein Slot mehr frei */
   rb_define_const(KontoCheck,"LUT2_NO_SLOT_FREE",INT2FIX(LUT2_NO_SLOT_FREE));
      /* (-29) Die (Unter)Methode ist nicht definiert */
   rb_define_const(KontoCheck,"UNDEFINED_SUBMETHOD",INT2FIX(UNDEFINED_SUBMETHOD));
      /* (-28) Der benötigte Programmteil wurde beim Kompilieren deaktiviert */
   rb_define_const(KontoCheck,"EXCLUDED_AT_COMPILETIME",INT2FIX(EXCLUDED_AT_COMPILETIME));
      /* (-27) Die Versionsnummer für die LUT-Datei ist ungültig */
   rb_define_const(KontoCheck,"INVALID_LUT_VERSION",INT2FIX(INVALID_LUT_VERSION));
      /* (-26) ungültiger Prüfparameter (erste zu prüfende Stelle) */
   rb_define_const(KontoCheck,"INVALID_PARAMETER_STELLE1",INT2FIX(INVALID_PARAMETER_STELLE1));
      /* (-25) ungültiger Prüfparameter (Anzahl zu prüfender Stellen) */
   rb_define_const(KontoCheck,"INVALID_PARAMETER_COUNT",INT2FIX(INVALID_PARAMETER_COUNT));
      /* (-24) ungültiger Prüfparameter (Position der Prüfziffer) */
   rb_define_const(KontoCheck,"INVALID_PARAMETER_PRUEFZIFFER",INT2FIX(INVALID_PARAMETER_PRUEFZIFFER));
      /* (-23) ungültiger Prüfparameter (Wichtung) */
   rb_define_const(KontoCheck,"INVALID_PARAMETER_WICHTUNG",INT2FIX(INVALID_PARAMETER_WICHTUNG));
      /* (-22) ungültiger Prüfparameter (Rechenmethode) */
   rb_define_const(KontoCheck,"INVALID_PARAMETER_METHODE",INT2FIX(INVALID_PARAMETER_METHODE));
      /* (-21) Problem beim Initialisieren der globalen Variablen */
   rb_define_const(KontoCheck,"LIBRARY_INIT_ERROR",INT2FIX(LIBRARY_INIT_ERROR));
      /* (-20) Prüfsummenfehler in der blz.lut Datei */
   rb_define_const(KontoCheck,"LUT_CRC_ERROR",INT2FIX(LUT_CRC_ERROR));
      /* (-19) falsch (die BLZ wurde außerdem gelöscht) */
   rb_define_const(KontoCheck,"FALSE_GELOESCHT",INT2FIX(FALSE_GELOESCHT));
      /* (-18) ok, ohne Prüfung (die BLZ wurde allerdings gelöscht) */
   rb_define_const(KontoCheck,"OK_NO_CHK_GELOESCHT",INT2FIX(OK_NO_CHK_GELOESCHT));
      /* (-17) ok (die BLZ wurde allerdings gelöscht) */
   rb_define_const(KontoCheck,"OK_GELOESCHT",INT2FIX(OK_GELOESCHT));
      /* (-16) die Bankleitzahl wurde gelöscht */
   rb_define_const(KontoCheck,"BLZ_GELOESCHT",INT2FIX(BLZ_GELOESCHT));
      /* (-15) Fehler in der blz.txt Datei (falsche Zeilenlänge) */
   rb_define_const(KontoCheck,"INVALID_BLZ_FILE",INT2FIX(INVALID_BLZ_FILE));
      /* (-14) undefinierte Funktion; die library wurde mit THREAD_SAFE=0 kompiliert */
   rb_define_const(KontoCheck,"LIBRARY_IS_NOT_THREAD_SAFE",INT2FIX(LIBRARY_IS_NOT_THREAD_SAFE));
      /* (-13) schwerer Fehler im Konto_check-Modul */
   rb_define_const(KontoCheck,"FATAL_ERROR",INT2FIX(FATAL_ERROR));
      /* (-12) ein Konto muß zwischen 1 und 10 Stellen haben */
   rb_define_const(KontoCheck,"INVALID_KTO_LENGTH",INT2FIX(INVALID_KTO_LENGTH));
      /* (-11) kann Datei nicht schreiben */
   rb_define_const(KontoCheck,"FILE_WRITE_ERROR",INT2FIX(FILE_WRITE_ERROR));
      /* (-10) kann Datei nicht lesen */
   rb_define_const(KontoCheck,"FILE_READ_ERROR",INT2FIX(FILE_READ_ERROR));
      /* (-9) kann keinen Speicher allokieren */
   rb_define_const(KontoCheck,"ERROR_MALLOC",INT2FIX(ERROR_MALLOC));
      /* (-8) die blz.txt Datei wurde nicht gefunden */
   rb_define_const(KontoCheck,"NO_BLZ_FILE",INT2FIX(NO_BLZ_FILE));
      /* (-7) die blz.lut Datei ist inkosistent/ungültig */
   rb_define_const(KontoCheck,"INVALID_LUT_FILE",INT2FIX(INVALID_LUT_FILE));
      /* (-6) die blz.lut Datei wurde nicht gefunden */
   rb_define_const(KontoCheck,"NO_LUT_FILE",INT2FIX(NO_LUT_FILE));
      /* (-5) die Bankleitzahl ist nicht achtstellig */
   rb_define_const(KontoCheck,"INVALID_BLZ_LENGTH",INT2FIX(INVALID_BLZ_LENGTH));
      /* (-4) die Bankleitzahl ist ungültig */
   rb_define_const(KontoCheck,"INVALID_BLZ",INT2FIX(INVALID_BLZ));
      /* (-3) das Konto ist ungültig */
   rb_define_const(KontoCheck,"INVALID_KTO",INT2FIX(INVALID_KTO));
      /* (-2) die Methode wurde noch nicht implementiert */
   rb_define_const(KontoCheck,"NOT_IMPLEMENTED",INT2FIX(NOT_IMPLEMENTED));
      /* (-1) die Methode ist nicht definiert */
   rb_define_const(KontoCheck,"NOT_DEFINED",INT2FIX(NOT_DEFINED));
      /* (0) falsch */
   rb_define_const(KontoCheck,"FALSE",INT2FIX(FALSE));
      /* (1) ok */
   rb_define_const(KontoCheck,"OK",INT2FIX(OK));
      /* (2) ok, ohne Prüfung */
   rb_define_const(KontoCheck,"OK_NO_CHK",INT2FIX(OK_NO_CHK));
      /* (3) ok; für den Test wurde eine Test-BLZ verwendet */
   rb_define_const(KontoCheck,"OK_TEST_BLZ_USED",INT2FIX(OK_TEST_BLZ_USED));
      /* (4) Der Datensatz ist aktuell gültig */
   rb_define_const(KontoCheck,"LUT2_VALID",INT2FIX(LUT2_VALID));
      /* (5) Der Datensatz enthält kein Gültigkeitsdatum */
   rb_define_const(KontoCheck,"LUT2_NO_VALID_DATE",INT2FIX(LUT2_NO_VALID_DATE));
      /* (6) Die Datei ist im alten LUT-Format (1.0/1.1) */
   rb_define_const(KontoCheck,"LUT1_SET_LOADED",INT2FIX(LUT1_SET_LOADED));
      /* (7) ok; es wurde allerdings eine LUT-Datei im alten Format (1.0/1.1) generiert */
   rb_define_const(KontoCheck,"LUT1_FILE_GENERATED",INT2FIX(LUT1_FILE_GENERATED));
      /* (8) In der DTAUS-Datei wurden kleinere Fehler gefunden */
   rb_define_const(KontoCheck,"DTA_FILE_WITH_WARNINGS",INT2FIX(DTA_FILE_WITH_WARNINGS));
      /* (9) ok; es wurde allerdings eine LUT-Datei im Format 2.0 generiert (Compilerswitch) */
   rb_define_const(KontoCheck,"LUT_V2_FILE_GENERATED",INT2FIX(LUT_V2_FILE_GENERATED));
      /* (10) ok; der Wert für den Schlüssel wurde überschrieben */
   rb_define_const(KontoCheck,"KTO_CHECK_VALUE_REPLACED",INT2FIX(KTO_CHECK_VALUE_REPLACED));
      /* (11) wahrscheinlich ok; die Kontonummer kann allerdings (nicht angegebene) Unterkonten enthalten */
   rb_define_const(KontoCheck,"OK_UNTERKONTO_POSSIBLE",INT2FIX(OK_UNTERKONTO_POSSIBLE));
      /* (12) wahrscheinlich ok; die Kontonummer enthält eine Unterkontonummer */
   rb_define_const(KontoCheck,"OK_UNTERKONTO_GIVEN",INT2FIX(OK_UNTERKONTO_GIVEN));
      /* (13) ok; die Anzahl Slots wurde auf SLOT_CNT_MIN hochgesetzt */
   rb_define_const(KontoCheck,"OK_SLOT_CNT_MIN_USED",INT2FIX(OK_SLOT_CNT_MIN_USED));
}