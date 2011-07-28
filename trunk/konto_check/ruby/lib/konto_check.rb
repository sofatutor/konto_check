# load the c extension
require 'konto_check_raw'

#This is a C/Ruby library to check the validity of German Bank Account
#Numbers. All currently defined test methods by Deutsche Bundesbank (April
#2011: 00 to D8) are implemented. 
#
#<b>ATTENTION:</b> There are a few important changes in the API between version 0.0.2 (version
#by Peter Horn/Provideal), version 0.0.6 (jeanmartin) and this version:
#
#* The function KontoCheck::load_bank_data() is no longer used; it is replaced by KontoCheck::init() and KontoCheck::generate_lutfile().
#* The function KontoCheck::konto_check(blz,kto) changed the order of parameters from (kto,blz) to (blz,kto)
#
#Another change affects only the version 0.0.6 by jeanmartin:
#
#* In KontoCheck::init(level,name,set) the order of the two first parameters is now free; the order is determined by the type of the variable (level is integer, filename string). 
#
#Because this class is inteded for german bank accounts, the rest of the
#documentation is in german too.
#
#Diese Bibliothek implementiert die Prüfziffertests für deutsche Bankkonten.
#Die meisten Konten enthalten eine Prüfziffer; mit dieser kann getestet
#werden, ob eine Bankleitzahl plausibel ist oder nicht. Auf diese Weise
#können Zahlendreher oder Zifferverdopplungen oft festgestellt werden. Es ist
#natürlich nicht möglich, zu bestimmen, ob ein Konto wirklich existiert; dazu
#müßte jeweils eine Anfrage bei der Bank gemacht werden ;-).
#
#Die Bibliothek ist in zwei Teile gesplittet: KontoCheckRaw bildet die direkte
#Schnittstelle zur C-Bibliothek und ist daher manchmal etwas sperrig;
#KontoCheck ist dagegen mehr Ruby-artig ausgelegt. KontoCheck gibt meist nur
#eine Teilmenge von KontoCheckRaw zurück, aber (hoffentlich) die Teile, die man
#unmittelbar von den jeweiligen Funktionen erwartet. Eine Reihe einfacher
#Funktionen sind auch in beiden Versionen identisch.
#
#Die Bankleitzahldaten werden in einem eigenen (komprimierten) Format in einer
#sogenannten LUT-Datei gespeichert. Diese Datei läßt sich mit der Funktion
#KontoCheck::generate_lutfile bzw. KontoCheckRaw::generate_lutfile aus der
#Datei der Deutschen Bundesbank (online erhältlich unter
#http://www.bundesbank.de/zahlungsverkehr/zahlungsverkehr_bankleitzahlen_download.php)
#erzeugen. Die LUT-Datei hat den großen Vorteil, daß die Datenblocks (BLZ,
#Prüfziffer, Bankname, Ort, ...) unabhängig voneinander gespeichert sind; jeder
#Block kann für sich geladen werden. In einer Datei können zwei Datensätze der
#Bundesbank mit unterschiedlichem Gültigkeitsdatum enthalten sein. Wenn bei der
#Initialisierung kein bestimmter Datensatz ausgewählt wird, prüft die
#Bibliothek aufgrund des mit jedem Datensatz gespeicherten Gültigkeitszeitraums
#welcher Satz aktuell gültig ist und lädt diesen dann in den Speicher.
#
#Numerische Werte (z.B. Bankleitzahlen, Kontonummern, PLZ,...) können als
#Zahlwerte oder als String angegeben werden; sie werden automatisch
#konvertiert. Prüfziffermethoden können ebenfalls als Zahl oder String
#angegeben werden; die Angabe als Zahl ist allerdings nicht immer eindeutig. So
#kann z.B. 131 sowohl für D1 als auch 13a stehen; daher ist es besser, die
#Prüfziffermethode als String anzugeben (in diesem Beispiel würde 131 als 13a
#interpretiert).

module KontoCheck

#mögliche Suchschlüssel für die Funktion KontoCheck::suche()
#
#:ort, :plz, :pz, :bic, :blz, :namen, :namen_kurz
  SEARCH_KEYS = [:ort, :plz, :pz, :bic, :blz, :namen, :namen_kurz]
#Aliasnamen für einige Suchschlüssel der Funktion KontoCheck::suche()
#
#:bankleitzahl, :city, :zip, :name, :kurzname, :shortname, :pruefziffer
  SEARCH_KEY_MAPPINGS = {
    :bankleitzahl => :blz,
    :city         => :ort,
    :zip          => :plz,
    :name         => :namen,
    :kurzname     => :namen_kurz,
    :shortname    => :namen_kurz,
    :pruefziffer  => :pz
  }

  class << self

#===<tt>KontoCheck::lut_info()</tt>
#=====<tt>KontoCheckRaw::lut_info([lutfile])</tt>
#=====<tt>KontoCheck::lut_info1(lutfile)</tt>
#=====<tt>KontoCheck::lut_info2(lutfile)</tt>
#
#Diese Funktion liefert den Infoblock des Datensatzes zurück, der mittels
#init() in den Speichergeladen wurde. Weitere Infos über die LUT-Datei
#lassen sich mit der Funktion KontoCheckRaw::lut_info() sowie
#KontoCheck::dump_lutfile() erhalten.

    def lut_info()
      KontoCheckRaw::lut_info()[3]
    end

#===<tt>KontoCheck::lut_info1(<lutfile>)</tt>
#=====<tt>KontoCheck::lut_info()</tt>
#=====<tt>KontoCheck::lut_info2()</tt>
#=====<tt>KontoCheckRaw::lut_info()</tt>
#
#Diese Funktion liefert den Infoblock des ersten Datensatzes der angegebenen
#LUT-Datei zurück. Weitere Infos über die LUT-Datei lassen sich mit der
#Funktion KontoCheckRaw::lut_info() sowie KontoCheck::dump_lutfile() erhalten.

    def lut_info1(filename)
      KontoCheckRaw::lut_info(filename)[3]
    end

#===<tt>KontoCheck::lut_info2(lutfile)</tt>
#=====<tt>KontoCheck::lut_info()</tt>
#=====<tt>KontoCheck::lut_info1(lutfile)</tt>
#=====<tt>KontoCheckRaw::lut_info([lutfile])</tt>
#
#Diese Funktion liefert den Infoblock des zweiten Datensatzes der angegebenen
#LUT-Datei zurück. Weitere Infos über die LUT-Datei lassen sich mit der
#Funktion KontoCheckRaw::lut_info() sowie KontoCheck::dump_lutfile() erhalten.

    def lut_info2(filename)
      KontoCheckRaw::lut_info(filename)[4]
    end


#===<tt>KontoCheck::dump_lutfile(lutfile)</tt>
#=====<tt>KontoCheckRaw::dump_lutfile(lutfile)</tt>
#
#Diese Funktion liefert detaillierte Informationen über alle Blocks, die in der
#LUT-Datei gespeichert sind, sowie noch einige Internas der LUT-Datei. Im
#Fehlerfall wird nil zurückgegeben.

    def dump_lutfile(filename)
      KontoCheckRaw::dump_lutfile(filename).first
    end

#===<tt>KontoCheck::encoding([mode])</tt>
#=====<tt>KontoCheckRaw::encoding([mode])</tt>
#=====<tt>KontoCheck::encoding_str([mode])</tt>
#=====<tt>KontoCheckRaw::keep_raw_data(mode)</tt>
#
#Diese Funktion legt den benutzten Zeichensatz für Fehlermeldungen durch die
#Funktion KontoCheck::retval2txt() und einige Felder der LUT-Datei (Name,
#Kurzname, Ort) fest. Wenn die Funktion nicht aufgerufen wird, wird der Wert
#DEFAULT_ENCODING aus konto_check.h benutzt.
#
#_Achtung_: Das Verhalten der Funktion hängt von dem Flag keep_raw_data der
#C-Bibliothek ab. Falls das Flag gesetzt ist, werden die Rohdaten der Blocks
#Name, Kurzname und Ort im Speicher gehalten; bei einem Wechsel der Kodierung
#wird auch für diese Blocks die Kodierung umgesetzt. Falls das Flag nicht
#gesetzt ist, sollte die Funktion *vor* der Initialisierung aufgerufen werden,
#da in dem Fall die Daten der LUT-Datei nur bei der Initialisierung konvertiert
#werden. Mit der Funktion KontoCheckRaw::keep_raw_data() kann das Flag gesetzt
#oder gelöscht werden.
#
#Für den Parameter mode werden die folgenden Werte akzeptiert (die Strings sind
#nicht case sensitiv; Mi oder mI oder MI ist z.B. auch möglich; wird der
#Parameter nicht angegeben, wird die aktuelle Kodierung ausgegeben):
#
#   0:            aktuelle Kodierung ausgeben
#   1,  'i', 'I': ISO-8859-1
#   2,  'u', 'U': UTF-8
#   3,  'h', 'H': HTML
#   4,  'd', 'D': DOS CP 850
#   51, 'mi':     ISO-8859-1, Makro für Fehlermeldungen
#   52, 'mu':     UTF-8, Makro für Fehlermeldungen
#   53, 'mh':     HTML, Makro für Fehlermeldungen
#   54, 'md':     DOS CP 850, Makro für Fehlermeldungen
#
#Rückgabewert ist die aktuelle Kodierung als Integer (falls zwei Kodierungen
#angegeben sind, ist die erste die der Statusmeldungen, die zweite die der
#LUT-Blocks):
#
#    0:  "noch nicht spezifiziert" (vor der Initialisierung)
#    1:  "ISO-8859-1";
#    2:  "UTF-8";
#    3:  "HTML entities";
#    4:  "DOS CP 850";
#    12: "ISO-8859-1/UTF-8";
#    13: "ISO-8859-1/HTML";
#    14: "ISO-8859-1/DOS CP 850";
#    21: "UTF-8/ISO-8859-1";
#    23: "UTF-8/HTML";
#    24: "UTF-8/DOS CP-850";
#    31: "HTML entities/ISO-8859-1";
#    32: "HTML entities/UTF-8";
#    34: "HTML entities/DOS CP-850";
#    41: "DOS CP-850/ISO-8859-1";
#    42: "DOS CP-850/UTF-8";
#    43: "DOS CP-850/HTML";
#    51: "Makro/ISO-8859-1";
#    52: "Makro/UTF-8";
#    53: "Makro/HTML";
#    54: "Makro/DOS CP 850";

    def encoding(*args)
      KontoCheckRaw::encoding(*args)
    end

#===<tt>KontoCheck::encoding_str([mode])</tt>
#=====<tt>KontoCheckRaw::encoding_str([mode])</tt>
#=====<tt>KontoCheck::encoding([mode])</tt>
#=====<tt>KontoCheckRaw::keep_raw_data(mode)</tt>
#
#Diese Funktion entspricht der Funktion KontoCheck::encoding(). Allerdings
#ist der Rückgabewert nicht numerisch, sondern ein String, der die aktuelle
#Kodierung angibt. Die folgenden Rückgabewerte sind möglich (falls zwei
#Kodierungen angegeben sind, ist die erste die der Statusmeldungen, die
#zweite die der LUT-Blocks):
#     
#    "noch nicht spezifiziert" (vor der Initialisierung)
#    "ISO-8859-1";
#    "UTF-8";
#    "HTML entities";
#    "DOS CP 850";
#    "ISO-8859-1/UTF-8";
#    "ISO-8859-1/HTML";
#    "ISO-8859-1/DOS CP 850";
#    "UTF-8/ISO-8859-1";
#    "UTF-8/HTML";
#    "UTF-8/DOS CP-850";
#    "HTML entities/ISO-8859-1";
#    "HTML entities/UTF-8";
#    "HTML entities/DOS CP-850";
#    "DOS CP-850/ISO-8859-1";
#    "DOS CP-850/UTF-8";
#    "DOS CP-850/HTML";
#    "Makro/ISO-8859-1";
#    "Makro/UTF-8";
#    "Makro/HTML";
#    "Makro/DOS CP 850";

    def encoding_str(*args)
      KontoCheckRaw::encoding_str(*args)
    end

#===<tt>KontoCheck::retval2txt(retval)</tt>
#=====<tt>KontoCheckRaw::retval2txt(retval)</tt>
#
#Diese Funktion konvertiert einen numerischen Rückgabewert in einen String. Der
#benutzte Zeichensatz wird über die Funktion KontoCheck::encoding() festgelegt.
#Falls diese Funktion nicht aufgerufen wurde, wird der Wert des Makros
#DEFAULT_ENCODING aus konto_check.h benutzt.

    def retval2txt(retval)
      KontoCheckRaw::retval2txt(retval)
    end

#===<tt>KontoCheck::retval2iso(retval)</tt>
#=====<tt>KontoCheckRaw::retval2iso(retval)</tt>
#
#Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
#Der benutzte Zeichensatz ist ISO 8859-1. 

    def retval2iso(retval)
      KontoCheckRaw::retval2iso(retval)
    end

#===<tt>KontoCheck::retval2txt_short(retval)</tt>
#=====<tt>KontoCheckRaw::retval2txt_short(retval)</tt>
#=====<tt>KontoCheck::retval2txt_kurz(retval)</tt>
#
#Diese Funktion konvertiert einen numerischen Rückgabewert in einen kurzen
#String. Die Ausgabe ist der Makroname, wie er in C benutzt wird.

    def retval2txt_short(retval)
      KontoCheckRaw::retval2txt_short(retval)
    end
    alias_method :retval2txt_kurz, :retval2txt_short

#===<tt>KontoCheck::retval2txt_kurz(retval)</tt>
#=====<tt>KontoCheckRaw::retval2txt_short(retval)</tt>
#=====<tt>KontoCheck::retval2txt_short(retval)</tt>
#
#Diese Funktion konvertiert einen numerischen Rückgabewert in einen kurzen
#String. Die Ausgabe ist der Makroname, wie er in C benutzt wird. Die Funktion
#ist ein Alias zu KontoCheck::retval2txt_short().

    def retval2txt_kurz(retval)
      KontoCheckRaw::retval2txt_short(retval)
    end

#===<tt>KontoCheck::retval2dos(retval)</tt>
#=====<tt>KontoCheckRaw::retval2dos(retval)</tt>
#
#Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
#Der benutzte Zeichensatz ist cp850 (DOS).
#
    def retval2dos(retval)
      KontoCheckRaw::retval2dos(retval)
    end

#===<tt>KontoCheck::retval2html(retval)</tt>
#=====<tt>KontoCheckRaw::retval2html(retval)</tt>
#
#Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
#Für Umlaute werden HTML-Entities benutzt.

    def retval2html(retval)
      KontoCheckRaw::retval2html(retval)
    end

#===<tt>KontoCheck::retval2utf8(retval)</tt>
#=====<tt>KontoCheckRaw::retval2utf8(retval)</tt>
#
#Diese Funktion konvertiert einen numerischen Rückgabewert in einen String.
#Der benutzte Zeichensatz ist UTF-8.

    def retval2utf8(retval)
      KontoCheckRaw::retval2utf8(retval)
    end

#===<tt>KontoCheck::generate_lutfile(inputfile,outputfile [,user_info [,gueltigkeit [,felder [,filialen [,set [,iban_file]]]]]])</tt>
#=====<tt>KontoCheckRaw::generate_lutfile(inputfile,outputfile [,user_info [,gueltigkeit [,felder [,filialen [,set [,iban_file]]]]]])</tt>
#
#Diese Funktion generiert eine neue LUT-Datei aus der BLZ-Datei der Deutschen Bundesbank. Die folgenden
#Parameter werden unterstützt:
#* inputfile: Eingabedatei (Textdatei) der Bundesbank
#* outputfile: Name der Ausgabedatei
#* user_info: Info-String der in die LUT-Datei geschrieben wird (frei wählbar; wird in den Info-Block aufgenommen)
#* gueltigkeit: Gültigkeit des Datensatzes im Format JJJJMMTT-JJJJMMTT. Diese Angabe wird benutzt, um festzustellen, ob ein Datensatz aktuell noch gültig ist.
#* felder: (0-9) Welche Daten aufgenommmen werden sollen (PZ steht in der folgenden Tabelle für Prüfziffer, NAME_NAME_KURZ ist ein Block, der sowohl den Namen als auch den Kurznamen der Bank enthält; dieser läßt sich besser komprimieren als wenn beide Blocks getrennt sind):
#   0. BLZ,PZ
#   1. BLZ,PZ,NAME_KURZ
#   2. BLZ,PZ,NAME_KURZ,BIC
#   3. BLZ,PZ,NAME,PLZ,ORT
#   4. BLZ,PZ,NAME,PLZ,ORT,BIC
#   5. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC
#   6. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ
#   7. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG
#   8. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG,LOESCHUNG
#   9. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG,LOESCHUNG,PAN,NR
#* filialen: (0 oder 1) Flag, ob nur die Daten der Hauptstellen (0) oder auch die der Filialen aufgenommen werden sollen
#* set (0, 1 oder 2): Datensatz-Nummer. Jede LUT-Datei kann zwei Datensätze enthalten. Falls bei der Initialisierung nicht ein bestimmter Datensatz ausgewählt wird, wird derjenige genommen, der (laut Gültigkeitsstring) aktuell gültig ist.
#* iban_file: Datei der Banken, die einer Selbstberechnung des IBAN nicht zugestimmt haben. Näheres dazu (inklusive Weblink) findet sich bei der Funktion KontoCheckRaw::iban_gen(blz,kto).
#
#Mögliche Rückgabewerte:
#
#   -112  (KTO_CHECK_UNSUPPORTED_COMPRESSION) "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
#    -57  (LUT2_GUELTIGKEIT_SWAPPED)   "Im Gültigkeitsdatum sind Anfangs- und Enddatum vertauscht"
#    -56  (LUT2_INVALID_GUELTIGKEIT)   "Das angegebene Gültigkeitsdatum ist ungültig (Soll: JJJJMMTT-JJJJMMTT)"
#    -32  (LUT2_COMPRESS_ERROR)        "Fehler beim Komprimieren eines LUT-Blocks"
#    -31  (LUT2_FILE_CORRUPTED)        "Die LUT-Datei ist korrumpiert"
#    -30  (LUT2_NO_SLOT_FREE)          "Im Inhaltsverzeichnis der LUT-Datei ist kein Slot mehr frei"
#    -15  (INVALID_BLZ_FILE)           "Fehler in der blz.txt Datei (falsche Zeilenlänge)"
#    -11  (FILE_WRITE_ERROR)           "kann Datei nicht schreiben"
#    -10  (FILE_READ_ERROR)            "kann Datei nicht lesen"
#     -9  (ERROR_MALLOC)               "kann keinen Speicher allokieren"
#     -7  (INVALID_LUT_FILE)           "die blz.lut Datei ist inkosistent/ungültig"
#      1  (OK)                         "ok"
#      7  (LUT1_FILE_GENERATED)        "ok; es wurde allerdings eine LUT-Datei im alten Format (1.0/1.1) generiert"

    def generate_lutfile(*args)
      KontoCheckRaw::generate_lutfile(*args)
    end

#===<tt>KontoCheck::init([<p1>[,<p2>[,<set>]]])</tt>
#=====<tt>KontoCheckRaw::init([<p1>[,<p2>[,<set>]]])</tt>
#Die Variablen p1 und p2 stehen für level und lutfile (in beliebiger
#Reihenfolge); die Zuordnung der beiden Parameter erfolgt on the fly durch eine
#Typüberprüfung.
#
#Diese Funktion initialisiert die Bibliothek und lädt die gewünschten
#Datenblocks in den Speicher. Alle Argumente sind optional; in konto_check.h
#werden die Defaultwerte definiert.
#
#Die beiden ersten Parameter sind der Dateiname und der
#Initialisierungslevel. Der Dateiname ist immer als String anzugeben, der
#Initialisierungslevel immer als Zahl, ansonsten wird eine TypeError
#Exception geworfen. Auf diese Weise ist es problemlos möglich festzustellen,
#wie die Parameter p1 und p2 den Variablen lutfile und level zuzuordnen
#sind.
#
#Für die LUT-Datei ist als Defaultwert sowohl für den Pfad als auch den
#Dateinamen eine Liste möglich, die sequenziell abgearbeitet wird; diese wird
#in konto_check.h spezifiziert (Compilerzeit-Konstante der C-Bibliothek). Die
#folgenden Werte sind in der aktuellen konto_check.h definiert:
#
#   DEFAULT_LUT_NAME blz.lut, blz.lut2f, blz.lut2
#   DEFAULT_LUT_PATH ., /usr/local/etc/, /etc/, /usr/local/bin/, /opt/konto_check/ (für nicht-Windows-Systeme)
#   DEFAULT_LUT_PATH ., C:\\, C:\\Programme\\konto_check  (für Windows-Systeme)
#
#Der Defaultwert für level ist ebenfalls in konto_check.h definiert; in der
#aktuellen Version ist er 5. Bei diesem Level werden die Blocks BLZ,
#Prüfziffer, Name, Kurzname, PLZ, Ort und BIC geladen.
#
#Falls der Parameter set nicht angegeben ist, wird versucht, das aktuell
#gültige Set aus dem Systemdatum und dem Gültigkeitszeitraum der in der
#LUT-Datei gespeicherten Sets zu bestimmen.
#
#Hier noch einmal ein Überblick über die Parameter:
#
#* lutfile: die LUT-Datei, mit der initialisiert werden soll
#* level: (0-9) Welche Daten geladen werden sollen (PZ steht in der folgenden Tabelle für Prüfziffer, NAME_NAME_KURZ ist ein Block, der sowohl den Namen als auch den Kurznamen der Bank enthält; dieser läßt sich besser komprimieren als wenn beide Blocks getrennt sind):
#   0. BLZ,PZ
#   1. BLZ,PZ,NAME_KURZ
#   2. BLZ,PZ,NAME_KURZ,BIC
#   3. BLZ,PZ,NAME,PLZ,ORT
#   4. BLZ,PZ,NAME,PLZ,ORT,BIC
#   5. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC
#   6. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ
#   7. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG
#   8. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG,LOESCHUNG
#   9. BLZ,PZ,NAME_NAME_KURZ,PLZ,ORT,BIC,NACHFOLGE_BLZ,AENDERUNG,LOESCHUNG,PAN,NR
#* set (1 oder 2): Datensatz
#
#Mögliche Rückgabewerte:
#
#   -112  (KTO_CHECK_UNSUPPORTED_COMPRESSION) "die notwendige Kompressions-Bibliothek wurden beim Kompilieren nicht eingebunden"
#    -64  (INIT_FATAL_ERROR)        "Initialisierung fehlgeschlagen (init_wait geblockt)"
#    -63  (INCREMENTAL_INIT_NEEDS_INFO)            "Ein inkrementelles Initialisieren benötigt einen Info-Block in der LUT-Datei"
#    -62  (INCREMENTAL_INIT_FROM_DIFFERENT_FILE)   "Ein inkrementelles Initialisieren mit einer anderen LUT-Datei ist nicht möglich"
#    -38  (LUT2_PARTIAL_OK)         "es wurden nicht alle Blocks geladen"
#    -36  (LUT2_Z_MEM_ERROR)        "Memory error in den ZLIB-Routinen"
#    -35  (LUT2_Z_DATA_ERROR)       "Datenfehler im komprimierten LUT-Block"
#    -34  (LUT2_BLOCK_NOT_IN_FILE)  "Der Block ist nicht in der LUT-Datei enthalten"
#    -33  (LUT2_DECOMPRESS_ERROR)   "Fehler beim Dekomprimieren eines LUT-Blocks"
#    -31  (LUT2_FILE_CORRUPTED)     "Die LUT-Datei ist korrumpiert"
#    -20  (LUT_CRC_ERROR)           "Prüfsummenfehler in der blz.lut Datei"
#    -10  (FILE_READ_ERROR)         "kann Datei nicht lesen"
#     -9  (ERROR_MALLOC)            "kann keinen Speicher allokieren"
#     -7  (INVALID_LUT_FILE)        "die blz.lut Datei ist inkosistent/ungültig"
#     -6  (NO_LUT_FILE)             "die blz.lut Datei wurde nicht gefunden"
#
#      1  (OK)                      "ok"
#      6  (LUT1_SET_LOADED)         "Die Datei ist im alten LUT-Format (1.0/1.1)"
#
#Anmerkung: Falls der Statuscode LUT2_PARTIAL_OK ist, waren bei der
#Initialisierung nicht alle Blocks in der LUT-Datei enthalten.

    def init(*args)
      KontoCheckRaw::init(*args)
    end

#===<tt>KontoCheck::load_bank_data(<datafile>)</tt>
#=====<tt>KontoCheckRaw::load_bank_data(<datafile>)</tt>
#
#Diese Funktion war die alte Initialisierungsroutine für konto_check; es ist
#nun durch die Funktionen KontoCheck::init() und KontoCheck::generate_lutfile()
#ersetzt. Zur Initialisierung  benutzte sie die Textdatei der Deutschen
#Bundesbank und generiertge daraus eine LUT-Datei, die dann von der
#Initialisierungsroutine der C-Bibliothek benutzt wurde.
#
#Die init() Funktion ist wesentlich schneller (7..20 mal so schnell) und hat
#eine Reihe weiterer Vorteile. So ist es z.B. möglich, zwwei Datensätze mit
#unterschiedlichem Gültigkeitszeitraum in einer Datei zu halten und den jeweils
#gültigen Satz automatisch (nach der Systemzeit) auswählen zu lassen. Die
#einzelnen Datenblocks (Bankleitzahlen, Prüfziffermethoden, PLZ, Ort...) sind
#in der LUT-Datei in jeweils unabhängigen Blocks gespeichert und können einzeln
#geladen werden; die Bankdatei von der Deutschen Bundesbank enthält alle Felder
#in einem linearen Format, so daß einzelne Blocks nicht unabhängig von anderen
#geladen werden können.
#
#Die Funktion load_bank_data() wird nur noch als ein schibbolet benutzt, um
#zu testen, ob jemand das alte Interface benutzt. Bei der Routine
#KontoCheck::konto_check() wurde die Reihenfolge der Parameter getauscht, so
#daß man in dem Falle den alten Code umstellen muß.

   def load_bank_data(*args)
      KontoCheckRaw::load_bank_data(*args)
   end

#===<tt>KontoCheck::current_lutfile_name()</tt>
#=====<tt>KontoCheckRaw::current_lutfile_name()</tt>
#=====<tt>KontoCheck::current_lutfile_set()</tt>
#=====<tt>KontoCheck::current_init_level()</tt>
#
#Diese Funktion bestimmt den Dateinamen der zur Initialisierung benutzten LUT-Datei.

    def current_lutfile_name()
      KontoCheckRaw::current_lutfile_name().first
    end

#===<tt>KontoCheck::current_lutfile_set()</tt>
#=====<tt>KontoCheckRaw::current_lutfile_name()</tt>
#=====<tt>KontoCheck::current_lutfile_name()</tt>
#=====<tt>KontoCheck::current_init_level()</tt>
#
#Diese Funktion bestimmt das Set der LUT-Datei, das bei der Initialisierung benutzt wurde.

    def current_lutfile_set()
      raw_results = KontoCheckRaw::current_lutfile_name()
      raw_results[1]
    end

#===<tt>KontoCheck::current_init_level()</tt>
#=====<tt>KontoCheckRaw::current_lutfile_name()</tt>
#=====<tt>KontoCheckRaw::current_lutfile_name()</tt>
#=====<tt>KontoCheck::current_lutfile_set()</tt>
#
#Diese Funktion bestimmt den aktuell benutzten Initialisierungslevel

    def current_init_level()
      raw_results = KontoCheckRaw::current_lutfile_name()
      raw_results[2]
    end

#===<tt>KontoCheck::free()</tt>
#=====<tt>KontoCheckRaw::free()</tt>
#Diese Funktion gibt allen allokierten Speicher wieder frei.

    def free()
      KontoCheckRaw::free()
    end

#===<tt>KontoCheck::konto_check(blz,kto)</tt>
#=====<tt>KontoCheckRaw::konto_check(blz,kto)</tt>
#=====<tt>KontoCheck::valid(blz,kto)</tt>
#Test, ob eine BLZ/Konto-Kombination eine gültige Prüfziffer enthält. Die Funktion gibt einen skalaren
#Statuswert zurück, der das Ergebnis der Prüfung enthält. Mögliche Rückgabewerte sind:
#
#   -69  MISSING_PARAMETER       "Bei der Kontoprüfung fehlt ein notwendiger Parameter (BLZ oder Konto)"
#   -40  LUT2_NOT_INITIALIZED    "die Programmbibliothek wurde noch nicht initialisiert"
#        
#   -77  BAV_FALSE               "BAV denkt, das Konto ist falsch (konto_check hält es für richtig)"
#   -29  UNDEFINED_SUBMETHOD     "Die (Unter)Methode ist nicht definiert"
#   -12  INVALID_KTO_LENGTH      "ein Konto muß zwischen 1 und 10 Stellen haben"
#    -5  INVALID_BLZ_LENGTH      "die Bankleitzahl ist nicht achtstellig"
#    -4  INVALID_BLZ             "die Bankleitzahl ist ungültig"
#    -3  INVALID_KTO             "das Konto ist ungültig"
#    -2  NOT_IMPLEMENTED         "die Methode wurde noch nicht implementiert"
#    -1  NOT_DEFINED             "die Methode ist nicht definiert"
#     0  FALSE                   "falsch"
#     1  OK                      "ok"
#     2  OK_NO_CHK               "ok, ohne Prüfung"

    def konto_check(blz,kto)
      KontoCheckRaw::konto_check(blz,kto)
    end

#===<tt>KontoCheck::valid(blz,kto)</tt>
#=====<tt>KontoCheck::konto_check(blz,kto)</tt>
#=====<tt>KontoCheckRaw::konto_check(blz,kto)</tt>
#Dies ist ein Alias für die Funktion KontoCheck::konto_check()

    def valid(blz,kto)
      KontoCheckRaw::konto_check(blz,kto)
    end

#===<tt>KontoCheck::konto_check?(blz, kto)</tt>
#=====<tt>KontoCheck::konto_check(blz,kto)</tt>
#=====<tt>KontoCheckRaw::konto_check(blz,kto)</tt>
#Test, ob eine BLZ/Konto-Kombination eine gültige Prüfziffer enthält. Die
#Funktion gibt einen skalaren Statuswert zurück, der das Ergebnis der Prüfung
#enthält. Mögliche Rückgabewerte sind einfach true und false (convenience
#function für konto_check()).

    def konto_check?(blz,kto)
      KontoCheckRaw::konto_check(blz,kto)>0?true:false
    end

#===<tt>KontoCheck::valid?(blz, kto)</tt>
#=====<tt>KontoCheck::valid(blz, kto)</tt>
#=====<tt>KontoCheckRaw::konto_check(blz, kto)</tt>
#Dies ist einn Alias für die Funktion KontoCheck::konto_check?. Mögliche Rückgabewerte sind true oder false.

    def valid?(blz,kto)
      KontoCheckRaw::konto_check(blz, kto)>0?true:false
    end

#===<tt>KontoCheck::konto_check_pz(pz,kto[,blz])</tt>
#=====<tt>KontoCheckRaw::konto_check_pz(pz,kto[,blz])</tt>
#=====<tt>KontoCheck::valid_pz(pz,kto[,blz])</tt>
#Diese Funktion testet, ob eine gegebene Prüfziffer/Kontonummer-Kombination gültig ist.
#
#Der zusätzliche Parameter blz ist nur für die Verfahren 52, 53, B6 und C0 notwendig; bei
#diesen Verfahren geht die BLZ in die Berechnung der Prüfziffer ein. Bei allen anderen
#Prüfzifferverfahren wird er ignoriert. Wird er bei einem dieser Verfahren nicht angegeben,
#wird stattdessen eine Test-BLZ eingesetzt.
#
#Die Funktion gibt einen skalaren Statuswert zurück, der das Ergebnis der
#Prüfung enthält. Mögliche Rückgabewerte sind:
#
#   -69  (MISSING_PARAMETER)       "bei der Kontoprüfung fehlt ein notwendiger Parameter (BLZ oder Konto)"
#   -40  (LUT2_NOT_INITIALIZED)    "die Programmbibliothek wurde noch nicht initialisiert"
#
#   -77  (BAV_FALSE)               "BAV denkt, das Konto ist falsch (konto_check hält es für richtig)"
#   -29  (UNDEFINED_SUBMETHOD)     "die (Unter)Methode ist nicht definiert"
#   -12  (INVALID_KTO_LENGTH)      "ein Konto muß zwischen 1 und 10 Stellen haben"
#    -3  (INVALID_KTO)             "das Konto ist ungültig"
#    -2  (NOT_IMPLEMENTED)         "die Methode wurde noch nicht implementiert"
#    -1  (NOT_DEFINED)             "die Methode ist nicht definiert"
#     0  (FALSE)                   "falsch"
#     1  (OK)                      "ok"
#     2  (OK_NO_CHK)               "ok, ohne Prüfung"

    def konto_check_pz(*args)
      KontoCheckRaw::konto_check_pz(*args)
    end

#===<tt>KontoCheck::konto_check_pz?(pz,kto[,blz])</tt>
#=====<tt>KontoCheckRaw::konto_check_pz(pz,kto[,blz])</tt>
#=====<tt>KontoCheck::valid_pz?(pz,kto[,blz])</tt>
#Diese Funktion testet, ob eine gegebene Prüfziffer/Kontonummer-Kombination
#gültig ist. Der Rückgabewert dieser Funktion ist true oder false (convenience
#function für KontoCheck::konto_check_pz()).
#
#Der zusätzliche Parameter blz ist nur für die Verfahren 52, 53, B6 und C0 notwendig; bei
#diesen Verfahren geht die BLZ in die Berechnung der Prüfziffer ein. Bei allen anderen
#Prüfzifferverfahren wird er ignoriert. Wird er bei einem dieser Verfahren nicht angegeben,
#wird stattdessen eine Test-BLZ eingesetzt.

    def konto_check_pz?(*args)
      KontoCheckRaw::konto_check_pz(*args)>0?true:false
    end

#===<tt>KontoCheck::valid_pz(pz,kto[,blz])</tt>
#=====<tt>KontoCheck::konto_check_pz(pz,kto[,blz])</tt>
#=====<tt>KontoCheckRaw::konto_check_pz(pz,kto[,blz])</tt>
#Diese Funktion ist ein Alias für KontoCheck::konto_check_pz

    def valid_pz(*args)
      KontoCheckRaw::konto_check_pz(*args)
    end

#===<tt>KontoCheck::valid_pz?(pz,kto[,blz])</tt>
#=====<tt>KontoCheck::valid_pz(pz,kto[,blz])</tt>
#=====<tt>KontoCheckRaw::konto_check_pz(pz,kto[,blz])</tt>
#Diese Funktion ist ein Alias für KontoCheck::konto_check_pz?()

    def valid_pz?(*args)
      KontoCheckRaw::konto_check_pz(*args)>0?true:false
    end

#==== <tt>KontoCheck::bank_valid(blz [,filiale])</tt>
#======<tt>KontoCheckRaw::bank_valid(blz [,filiale])</tt>
#======<tt>KontoCheck::bank_valid?(blz [,filiale])</tt>
#Diese Funktion testet, ob eine gegebene BLZ gültig ist. Der Rückgabewert ist ein
#Statuscode mit den unten angegebenen Werten. Falls das Argument filiale auch
#angegeben ist, wird zusätzlich noch getestet, ob eine Filiale mit dem gegebenen
#Index existiert.
#
#Mögliche Rückgabewerte sind:
#
#    -55  (LUT2_INDEX_OUT_OF_RANGE)    "Der Index für die Filiale ist ungültig"
#    -53  (LUT2_BLZ_NOT_INITIALIZED)   "Das Feld BLZ wurde nicht initialisiert"
#     -5  (INVALID_BLZ_LENGTH)         "die Bankleitzahl ist nicht achtstellig"
#     -4  (INVALID_BLZ)                "die Bankleitzahl ist ungültig"
#      1  (OK)                         "ok"

    def bank_valid(*args)
      KontoCheckRaw::bank_valid(*args)
    end

#====<tt>KontoCheck::bank_valid?(blz [,filiale])</tt>
#======<tt>KontoCheckRaw::bank_valid(blz [,filiale])</tt>
#======<tt>KontoCheck::bank_valid(blz [,filiale])</tt>
#Dies ist eine convenience function zu KontoCheck::bank_valid(). Es wird getestet, ob
#die gegebene BLZ (und evl. noch der Filialindex) gültig ist. Der Rückgabewert ist
#nur true oder false.

    def bank_valid?(*args)
      KontoCheckRaw::bank_valid(*args)>0?true:false
    end

#===<tt>KontoCheck::bank_filialen(blz)</tt>
#=====<tt>KontoCheckRaw::bank_filialen(blz)</tt>
#
#Diese Funktion liefert die Anzahl Filialen einer Bank (inklusive Hauptstelle).
#Die LUT-Datei muß dazu natürlich mit den Filialdaten generiert sein, sonst
#wird für alle Banken nur 1 zurückgegeben.

    def bank_filialen(*args)
      KontoCheckRaw::bank_filialen(*args).first
    end

#===<tt>KontoCheck::bank_name(blz[,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_name(blz[,filiale])</tt>
#
#Diese Funktion liefert den Namen einer Bank, oder nil im Fehlerfall.

    def bank_name(*args)
      KontoCheckRaw::bank_name(*args).first
    end

#===<tt>KontoCheck::bank_name_kurz(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_name_kurz(blz [,filiale])</tt>
#
#Diese Funktion liefert den Kurznamen einer Bank, oder nil im Fehlerfall.

    def bank_name_kurz(*args)
      KontoCheckRaw::bank_name_kurz(*args).first
    end

#===<tt>KontoCheck::bank_ort(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_ort(blz [,filiale])</tt>
#
#Diese Funktion liefert den Ort einer Bank. Falls der Parameter filiale nicht
#angegeben ist, wird der Sitz der Hauptstelle ausgegeben. Im Fehlerfall wird
#für den Ort nil zurückgegeben.

    def bank_ort(*args)
      KontoCheckRaw::bank_ort(*args).first
    end

#===<tt>KontoCheck::bank_plz(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_plz(blz [,filiale])</tt>
#
#Diese Funktion liefert die Postleitzahl einer Bank. Falls der Parameter
#filiale nicht angegeben ist, wird die PLZ der Hauptstelle ausgegeben. Im
#Fehlerfall wird für die PLZ nil zurückgegeben.

    def bank_plz(*args)
      KontoCheckRaw::bank_plz(*args).first
    end

#===<tt>KontoCheck::bank_pz(blz)</tt>
#=====<tt>KontoCheckRaw::bank_pz(blz)</tt>
#
#Diese Funktion liefert die Prüfziffer einer Bank. Die Funktion unterstützt
#keine Filialen; zu jeder BLZ kann es in der LUT-Datei nur eine
#Prüfziffermethode geben.

    def bank_pz(blz)
      KontoCheckRaw::bank_pz(blz).first
    end

#===<tt>KontoCheck::bank_bic(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_bic(blz [,filiale])</tt>
#
#Diese Funktion liefert den BIC (Bank Identifier Code) einer Bank. Im
#Fehlerfall wird nil zurückgegeben.

    def bank_bic(*args)
      KontoCheckRaw::bank_bic(*args).first
    end

#===<tt>KontoCheck::bank_aenderung(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_aenderung(blz [,filiale])</tt>
#
#Diese Funktion liefert das  'Änderung' Flag einer Bank (als string). Mögliche
#Werte sind: A (Addition), M (Modified), U (Unchanged), D (Deletion).

    def bank_aenderung(*args)
      KontoCheckRaw::bank_aenderung(*args).first
    end

#===<tt>KontoCheck::bank_loeschung(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_loeschung(blz [,filiale])</tt>
#
#Diese Funktion liefert das Lösch-Flag für eine Bank zurück (als Integer;
#mögliche Werte sind 0 und 1); im Fehlerfall wird nil zurückgegeben.

    def bank_loeschung(*args)
      KontoCheckRaw::bank_loeschung(*args).first
    end

#===<tt>KontoCheck::bank_nachfolge_blz(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_nachfolge_blz(blz [,filiale])</tt>
#Diese Funktion liefert die Nachfolge-BLZ für eine Bank, die gelöscht werden
#soll (bei der das 'Löschung' Flag 1 ist).

    def bank_nachfolge_blz(*args)
      KontoCheckRaw::bank_nachfolge_blz(*args).first
    end

#===<tt>KontoCheck::bank_pan(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_pan(blz [,filiale])</tt>
#
#Diese Funktion liefert den PAN (Primary Account Number) einer Bank.

    def bank_pan(*args)
      KontoCheckRaw::bank_pan(*args).first
    end

#===<tt>KontoCheck::bank_nr(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_nr(blz [,filiale])</tt>
#
#Diese Funktion liefert die laufende Nummer einer Bank (internes Feld der BLZ-Datei). Der Wert
#wird wahrscheinlich nicht oft benötigt, ist aber der Vollständigkeit halber enthalten.

    def bank_nr(*args)
      KontoCheckRaw::bank_nr(*args).first
    end

#===<tt>KontoCheck::bank_alles(blz [,filiale])</tt>
#=====<tt>KontoCheckRaw::bank_alles(blz [,filiale])</tt>
#
#Dies ist eine Mammutfunktion, die alle vorhandenen Informationen über eine
#Bank zurückliefert. Das Ergebnis ist ein Array mit den folgenden Komponenten:
#   0:  Statuscode
#   1:  Anzahl Filialen
#   2:  Name
#   3:  Kurzname
#   4:  PLZ
#   5:  Ort
#   6:  PAN
#   7:  BIC
#   8:  Prüfziffer
#   9:  Laufende Nr.
#   10: Änderungs-Flag
#   11: Löeschung-Flag
#   12: Nachfolge-BLZ
#
#Der Statuscode (Element 0) kann folgende Werte annehmen:
#
#    -40  (LUT2_NOT_INITIALIZED)       "die Programmbibliothek wurde noch nicht initialisiert"
#    -38  (LUT2_PARTIAL_OK)            "es wurden nicht alle Blocks geladen"
#     -5  (INVALID_BLZ_LENGTH)         "die Bankleitzahl ist nicht achtstellig"
#     -4  (INVALID_BLZ)                "die Bankleitzahl ist ungültig"
#      1  (OK)                         "ok"
#
#Anmerkung: Falls der Statuscode LUT2_PARTIAL_OK ist, wurden bei der
#Initialisierung nicht alle Blocks geladen (oder es sind nicht alle verfügbar);
#die entsprechenden Elemente haben dann den Wert nil.

    def bank_alles(*args)
      KontoCheckRaw::bank_alles(*args)
    end

#===<tt>KontoCheck::iban2bic(iban)</tt>
#=====<tt>KontoCheckRaw::iban2bic(iban)</tt>
#
#Diese Funktion bestimmt zu einer (deutschen!) IBAN den zugehörigen BIC (Bank
#Identifier Code). Der BIC wird für eine EU-Standard-Überweisung im
#SEPA-Verfahren (Single Euro Payments Area) benötigt; für die deutschen Banken
#ist er in der BLZ-Datei enthalten. Nähere Infos gibt es z.B. unter
#http://www.bic-code.de/ .

    def iban2bic(*args)
      KontoCheckRaw::iban2bic(*args).first
    end

#===<tt>KontoCheck::iban_check(iban)</tt>
#=====<tt>KontoCheckRaw::iban_check(iban)</tt>
#
#Diese Funktion testet einen IBAN. Dabei wird sowohl die Prüfziffer des IBAN
#getestet als auch (bei deutschen Konten) die Prüfziffer der Bankverbindung
#(aus der Kontonummer). Diese Funktion gibt nur den globalen Status zurück;
#bei der Funktion KontoCheckRaw::iban_check() kann man zusätzlich noch den
#Status der Kontenprüfung abfragen.
#
#Mögliche Rückgabewerte sind:
#
#    -67  (IBAN_OK_KTO_NOT)            "Die Prüfziffer der IBAN stimmt, die der Kontonummer nicht"
#    -66  (KTO_OK_IBAN_NOT)            "Die Prüfziffer der Kontonummer stimmt, die der IBAN nicht"
#      0  (FALSE)                      "falsch"
#      1  (OK)                         "ok"

    def iban_check(*args)
      KontoCheckRaw::iban_check(*args).first
    end

#===<tt>KontoCheck::iban_gen(kto,blz)</tt>
#=====<tt>KontoCheckRaw::iban_gen(kto,blz)</tt>
#Diese Funktion generiert aus (deutscher) BLZ und Konto einen IBAN. Hierbei
#ist zu beachten, daß nicht alle Banken der Selbstberechnung zugestimmt
#haben. Es gibt von den Sparkassen eine Liste dieser Institute; sie ist auch
#in der LUT-Datei mit der ID 22 (1. Eigene IBAN) enthalten. Bei diesen Banken
#wird - falls der Block in der LUT-Datei enthalten ist - keine Berechnung
#durchgeführt, sondern die Status-Variable auf NO_OWN_IBAN_CALCULATION
#gesetzt. 
#
#Alle Banken der Liste erzeugen eine Statusmeldung mit dem Wert
#OK_UNTERKONTO_ATTACHED, OK_UNTERKONTO_POSSIBLE oder OK_UNTERKONTO_GIVEN. Falls
#einer dieser Stauswerte zurückgegeben wird, ist somit immer Vorsicht geboten;
#der generierte IBAN sollte direkt bei dem zugehörigen Institut überprüft
#werden.
#
#Hier ein Auszug aus der Anleitung des SEPA Account Converters:
#
#Der SEPA Account Converter ist so eingestellt, dass nur Kontoverbindungen in IBAN und
#BIC umgerechnet werden, bei denen das ausgebende Kreditinstitut der Umrechnung
#zugestimmt hat. Kreditinstitute, welche einer Umrechnung nicht zugestimmt haben und
#welche zum Teil spezielle, dem SEPA Account Converter nicht bekannte Umrechnungs-
#methoden verwenden, sind in der Datei "CONFIG.INI" hinterlegt. Durch Löschen der Datei
#"CONFIG.INI" aus dem Programmverzeichnis haben Sie die Möglichkeit, eine Umrechnung
#für alle Konten durchzuführen. Bitte beachten Sie dabei, dass die so erhaltenen IBAN und
#BIC fehlerhaft sein können und deshalb mit ihren Kunden zu überprüfen sind.
#
#Weblinks:
#
#     https://www.sparkasse-rhein-neckar-nord.de/pdf/content/sepa/kurzanleitung.pdf
#     https://www.sparkasse-rhein-neckar-nord.de/firmenkunden/internationales_geschaeft/sepa/vorteile/index.php
#     https://www.sparkasse-rhein-neckar-nord.de/firmenkunden/internationales_geschaeft/sepa/vorteile/sepa_account_converter.msi
#
#Es ist möglich, sowohl die Prüfung auf Stimmigkeit der Kontonummer als auch
#auf Zulässigkeit der Selbstberechnung zu deaktivieren. Falls die IBAN trotz
#fehlender Zustimmung der Bank berechnet werden, ist vor die BLZ ein @ zu
#setzen; falls auch bei falscher Bankverbindung ein IBAN berechnet werden
#soll, ist vor die BLZ ein + zu setzen. Um beide Prüfungen zu deaktiviern,
#kann @+ (oder +@) vor die BLZ gesetzt werden. Die so erhaltenen IBANs sind
#dann i.A. allerdings wohl nicht gültig.
#
#Rückgabewert ist der generierte IBAN oder nil, falls ein Fehler aufgetreten ist. Die genauere
#Fehlerursache läßt sich mit der Funktion KontoCheckRaw::iban_gen() feststellen.

    def iban_gen(*args)
      KontoCheckRaw::iban_gen(*args).first
    end

#===<tt>KontoCheck::ipi_gen(zweck)</tt>
#=====<tt>KontoCheckRaw::ipi_gen(zweck)</tt>
#
#Diese Funktion generiert einen "Strukturierten Verwendungszweck" für SEPA-Überweisungen.
#Der Rückgabewert ist der Strukturierte Verwendungszweck als String oder nil, falls ein Fehler
#aufgetreten ist.
#
#Der String für den Strukturierten Verwendungszweck darf maximal 18 Byte lang sein und nur
#alphanumerische Zeichen enthalten (also auch keine Umlaute). Die Funktion KontoCheckRaw::ipi_gen()
#gibt einen Statuscode zurück, der etwas nähere Infos im Fehlerfall enthält.

    def ipi_gen(zweck)
      KontoCheckRaw::ipi_gen(zweck).first
    end

#===<tt>KontoCheck::ipi_check(zweck)</tt>
#=====<tt>KontoCheckRaw::ipi_check(zweck)</tt>
#
#Die Funktion testet, ob ein Strukturierter Verwendungszweck gültig ist (Anzahl Zeichen, Prüfziffer). Der
#Rückgabewert ist true oder false.

    def ipi_check(zweck)
      KontoCheckRaw::ipi_check(zweck)>0?true:false
    end

#===<tt>KontoCheck::suche()</tt>
#=====<tt>KontoCheck::search()</tt>
#=====<tt>KontoCheck::SEARCH_KEYS</tt>
#=====<tt>KontoCheck::SEARCH_KEY_MAPPINGS</tt>
#=====<tt>KontoCheckRaw::bank_suche_bic(search_bic)</tt>
#=====<tt>KontoCheckRaw::bank_suche_blz(blz1[,blz2])</tt>
#=====<tt>KontoCheckRaw::bank_suche_namen(name)</tt>
#=====<tt>KontoCheckRaw::bank_suche_namen_kurz(short_name)</tt>
#=====<tt>KontoCheckRaw::bank_suche_plz(plz1[,plz2])</tt>
#=====<tt>KontoCheckRaw::bank_suche_pz(pz1[,pz2])</tt>
#=====<tt>KontoCheckRaw::bank_suche_ort(suchort)</tt>
#
#Diese Funktion sucht alle Banken, die auf bestimmte Suchmuster passen.
#Momentan sind nur grundlegende Suchfunktionen eingebaut, und es wird auch nur
#die Suche nach einem Kriterium unterstützt; in einer späteren Version werden
#die Möglichkeiten noch erweitert werden.
#
#Eine Suche ist möglich nach den Schlüsseln BIC, Bankleitzahl, Postleitzahl,
#Prüfziffer, Ort, Name oder Kurzname. Bei den alphanumerischen Feldern (BIC,
#Ort, Name, Kurzname) ist der Suchschlüssel der Wortanfang des zu suchenden
#Feldes (ohne Unterscheidung zwischen Groß- und Kleinschreibung); bei den
#numerischen Feldern (BLZ, PLZ, Prüfziffer) ist die Suche nach einem bestimmten
#Wert oder nach einem Wertebereich möglich; dieser wird dann angegeben als
#Array mit zwei Elementen. Der Rückgabewert ist jeweils ein Array mit den
#Bankleitzahlen, oder nil falls die Suche fehlschlug. Die Ursache für eine
#fehlgeschlagene Suche läßt sich nur mit den Funktionen der KontoCheckRaw
#Bibliothek näher lokalisieren.
#
#Die Funktion KontoCheck::search() ist ein Alias für die Funktion
#KontoCheck::suche().
#
#Die möglichen Suchschlüssel sind in den Variablen KontoCheck::SEARCH_KEYS
#definiert; in der Variablen KontoCheck::SEARCH_KEY_MAPPINGS finden sich noch
#einige Aliasdefinitionen zu den Suchschlüsseln.
#
#Hier einige Beispiele möglicher Suchaufrufe:
#
#   s=KontoCheck::suche( :blz => [13051172,13070172] )        BLZ-Bereich
#   s=KontoCheck::suche( :blz => [13051172,13070172,333] )    überzählige Array-Elemente werden ignoriert
#   s=KontoCheck::suche( :bic => 'genodef1wi' )   BIC Teilstring
#   s=KontoCheck::suche( :plz => [68100,68200] )  PLZ-Bereich
#   s=KontoCheck::suche( :pz => 90 )              Prüfziffer numerisch
#   s=KontoCheck::suche( :pz => '90' )            Prüfziffer als String
#   s=KontoCheck::suche( :pz => ['95',98] )       Prüfzifferbereich gemischt String/numerisch auch möglich
#   s=KontoCheck::suche( :name => 'postbank' ) 
#   s=KontoCheck::suche( :ort => 'lingenfeld' )

    def suche(options={})
      key   = options.keys.first.to_sym
      value = options[key]
      key = SEARCH_KEY_MAPPINGS[key] if SEARCH_KEY_MAPPINGS.keys.include?(key)
      raise 'search key not supported' unless SEARCH_KEYS.include?(key)
      raw_results = KontoCheckRaw::send("bank_suche_#{key}", value)
      raw_results[1]
    end
    alias_method :search, :suche

#===<tt>KontoCheck::version()</tt>
#=====<tt>KontoCheckRaw::version()</tt>
#Diese Funktion gibt den Versions-String der C-Bibliothek zurück.

    def version()
      KontoCheckRaw::version()
    end

  end

end
