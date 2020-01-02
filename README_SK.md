# CellBalancer_Attiny
Jednoduchý balancér Life článkov batérie, používahúci ttiny 85 v konfigurácii s interným 1MHz oscilátorom, so sériovou kommunikáciou.
Navrhnuté pre použitie LiFePo4 kde max. napätie kedz sa spustí balancovanie je 3,5V (konštanta MAX_BALANCE_VOLTAGE). 
Rozsah pracovných napätí modulu je 2.8V - 5V, balancovací prúd max. 4A (osadené oba tranzistory Q1 a Q2 -AO 3420). 
Pri 4A je úbytok na nich cca 50mV - ztrata cca 200mW na oba ciže jeden 100mW. Balancovací prúd je obmedzený odporom R4, 
pre 4A výsledná hodnota 0,87Ohm poskladaná zo 4  5W odporov zapojených paralelne. 
Snažil som sa aby mali čo najväčšiu plochu kôli otepleniu.

Serial communication uses 4800 baud, but with INVERTED levels, so false 0V, true is Vcc.
To communicate with standard UART simple level converter with optoisolation is used

Namerané npätie sa filtruje exponencialnym priemerom s 3 hodnot. Výhoda je jednoduchý výpočet a malý nérok na pamäť.

![Alt text](Pictures/ModuleTest.JPG?raw=true "Module")

Použitý je knižnica AttinyCore od SpenceKonde. BOD je nastavené na 1,8V - napaľuje sa to cez napalovanie bootloaderu (Arduino). 
(v skutocnosti to len nastavi configuracne fuses). 2,7V sa mi zdalo vysoko, keďže horná hranica môže byt až 2,9V. 
EEPROM je nastavené nechat bez resetu (default z vyroby), v EEPROM su z vyroby hodnoty FF... 
Ak napatie modulu spadne pod BOD ( je tam hysterézia okolo 50mV) a nízka úroveň trvá dlhšie ako 2uS tak sa zastavi. 
Modul nabieha spoľahlivo bez použitia reset pinu aj pri pomalom náraste napätia, skôr je problém, 
že pri 1,9V ešte nebezi 100% referencia (potrebuje aspon 2,1V), ale vačší problém je že pri tomto napäti vypadáva sériova komunikacia. 
Odchýľka oscilátora, budenie optočlenov ??). Vzhladom na to že od 2,1V modul beží opäť OK, tak to neriešim.

###### Spotreba modulu
Samotny modul bez processora berie pri 3,3V 125 uA, z toho referencia (REF3020) 40uA a zvyšok odporový delič napatia 20kO+20kO. 
Modul s osadenou ATTiny 85 bez komunikacie a uspávanim má odbercca 130uA, čiže samotny procesor 5uA. 
Použitý je sleep, s meranim každú sekundu, ale je aktivny watchdog a BOD a teda aj vnútorná referencia. 
Bez uspávania by bola spotreba cca 1,4 mA (bez komunikacie).
Externa referencia REF3020 (2048mV) je použitá z dôvodou nestability vnútornej referencie v Attiny. R
EF3020 je 3 svorková referencia s malým úbytkom napätia.
Modul sa zobúdza na časovač a na prichádzajúcu komunikáciu.

###### Sériová komunikácia
Moduly komunikujú navzájom cez sériovú linku, modul môže buď prijímať alebo vysielať, nie súčasne (SW Serial).
Príkaz sa pošle do prvého modulu, ten podľa adresy buď príkaz spracuje alebo prepošle ďalej. Ak prijatý paket má chybný CRC,
tak je zahodený. Po spracovaní sa odpoveď posiela na modul s vyššou adresou.
Master jednotka (balancer manager) posiela prikazy na prvý modul a očakáva odpoved z posledného modulu. 
V masteri je nastavený timeout komunikácie po ktorom sa príkaz zopakuje (2x) alebo sa prejde na ďaľší modul.
Každý modul má jedinečnú adresu 1, 2, ...G . (Dá sa upraviť). Adresa sa nastavuje konfiguračným príkazom.
Na testovanie som použil program pre sériouvú komunikáciu RealTerm s nastavením 4800 8 N, USB to serial modul a už spomínaný 
prevodník (negácia úrovní, budenie optočlena). Na výpočet CRC som použil program ktorý mám pre testovanie komunikácie 
z meničom PIP4048 (Axpert).
Modul umožnuje tzv. dynamické balancovanie,t.j. spustenie/zastavenie balancovania príkazom bez ohľadu na napätie. 
Pokial požiadavka na spustenie nie je zopakovaná automaticky sa vypne po 60sekundách. 
Hardcoded balancovanie nabieha automaticky a nie je možné ho príkazom vypnúť.  
Testovaci balancer manager
![Alt text](Pictures/BalManager.PNG?raw=true "Master - manager")

Uplne vľavo sú príkazy aj s odozvami (napr. prikaz 1QUA - precitanie aktuálneho napätia a stavu balancéra na clanku 1. nasledovany odozvou "(1UA3239 0" - napatie 3239mV, 0 - balancovane vypnute). Je tam vidno aj prikazy pre citanie teploty 1QTA.. teplota v kelvinoch.
V pravej casti je zoznam clankov prvy stlpec adresy, 2. stlpec aktualne napatie a atd...
Aky by bolo treba dat do serie viac clankov ako 16, bude treba doplnit dekodovanie adries.

Spracovanie prijatých znakov vykonáva trieda BufferClass v metóde readFromSerial.
Číta znaky zo streamu (SW Serial) a čaká na znak CR (koniec riadku 0x0d). 
Poznámka: crc by nemalo nikdy obsahovať tento znak, inak dôjde k chybe, aj preto som použil pre výpocet
rovnaký spôsob ako používa Axpert.
Ak je prijatý znak CR, predpokladá sa že to čo je v poli data[] triedy BufferClass je možná komunikácia,
a ďaľším krokom je kontrola crc. (crc by malo byť 2 znaky pred znakom CR).
Ak crc znaky sedia ("if (recievedCrc == calculatedCrc)") s vypočítaným crc, 
nastaví sa flag hasData na true aby hlavná sľučka programu vedela,
že treba spracovať alebo preposlať prijaté znaky.
Ak vypočítané crc nesedí, pole data[] triedy BufferClass sa vyčistí, a prijaté znaky sú ignorované.
Pre testovacie účely je možné upraviť podmienku tak aby bola stále splnená a nebolo kontrolované crc.
"if (recievedCrc == calculatedCrc)" upravit ->  "if (true)".
![Alt text](Pictures/BufferClass.PNG?raw=true "BufferClass - kontrola crc")

Kontrola crc je v Buffer.cpp riadok 73. samotný výpočet crc robí funkcia "calculate_crc", ta sa volá z riadku 65. v Buffer.cpp. 
Funkcia "calculate_crc" je definovana v súbore "bal_crc.cpp" .
Ked sa použije iný výpočet crc, treba zabezpečiť aby crc neobsahovalo znaky 0x28, 0x0d, 0x0a. Toto má implementovaný výpocet crc pre PIP (axpert), ošetrené na riadku 35. v bal_crc.cpp. Znaky 0x0d, 0x0a sa pouzivaju na označenie konca riadku - to by kolidovalo s detekovaním konca prikazu, resp. konca bloku údajov (v zdrojovom kóde konštanta CR). 
V súbore prikazy.txt su priklady prikazov s vypočítaným crc, ktoré som priamo zadával v Realterm (aplikácia pre seriový terminál pod windows) pri testovani.

###### Príkazy 
Príklady aj s vypočítaným crc sú v súbore [prikazy.txt](Source/prikazy.txt):

        aQUA get actual Ubat in mV and balance state (0/1), response <(aUA9999 B><crc><cr>
	aQTA get actual temperature in kelvin, response <(aTA999><crc><cr> - temp is measured only on request, 
                while voltage each second
	aQBU get starting balance voltage in mV, response <(aBS9999><crc><cr>
	aQRC get reference correction in mV (+- 9mV), response for positive value <(aRC 9><crc><cr>, for negative <(aRC-9><crc><cr>
	aQTC get temperature correction in (+- 9), response for positive value <(aTC 9><crc><cr>, for negative <(aTC-9><crc><cr>
	aQOC get OSCCAL correction in (+- 9), response for positive value <(aOC 9><crc><cr>, for negative <(aOC-9><crc><cr>
	aSDB - start dynamic balancing, response (aSDBOK<crc><cr>, it switch off automatically after 60 seconds
	aEDB - end dynamic balancing, response (aEDBOK<crc><cr>
	aBSV9999 - set start balance voltage, response (aBSVOK<crc><cr>
	aSRC - set voltage reference corection (-9..9), response (aRCOK<crc><cr>
	aSTC - set temperature corection (-99..99), response (aRTOK<crc><cr>
	aSOC - set OSCCAL correction (-9..9), response (aOCOK<crc><cr>
	
	DBG - debug - when active it send measured voltage each 3 seconds, it sets to off after 1 minute
        can be used only when module is stand allone 

Brodcasted (re-routed) commands as address is used 'Z':

        SMA0<crc><cr> set all module addres automaticaly modul increments received address(start is 0) set it, 
	and then forwards modified SMAa<crc><cr>.
	QMA get module address,it first sends address of module "(MAa<crc><cr>" followed by ZQMA<crc><cr>
    
command but with adress of module. so it would look like ZSMA0 -> ZSMA1 -> .... last 16th module sends ZSMAG

    a- is address of module
    all commands and responses must end with correct <CRC><cr>
  
