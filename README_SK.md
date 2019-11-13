# CellBalancer_Attiny
Jednoduchý balancér Life článkov batérie, používahúci ttiny 85 v konfigurácii s interným 1MHz oscilátorom, so sériovou kommunikáciou.
Navrhnuté pre použitie LiFePo4 kde max. napätie kedz sa spustí balancovanie je 3,5V (konštanta MAX_BALANCE_VOLTAGE). 
Rozsah pracovných napätí modulu je 2.8V - 5V, balancovací prúd max. 4A (osadené oba tranzistory Q1 a Q2 -AO 3420). 
Pri 4A je úbytok na nich cca 50mV - ztrata cca 200mW na oba ciže jeden 100mW. Balancovací prúd je obmedzený odporom R4, 
pre 4A výsledná hodnota 0,87Ohm poskladaná zo 4  5W odporov zapojených paralelne. Snažil som sa aby mali čo najväčšiu plochu kôli otepleniu.

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

Spotreba modulu
Samotny modul bez processora berie pri 3,3V 125 uA, z toho referencia (REF3020) 40uA a zvyšok odporový delič napatia 20kO+20kO. 
Modul s osadenou ATTiny 85 bez komunikacie a uspávanim má odbercca 130uA, čiže samotny procesor 5uA. 
Použitý je sleep, s meranim každú sekundu, ale je aktivny watchdog a BOD a teda aj vnútorná referencia. 
Bez uspávania by bola spotreba cca 1,4 mA (bez komunikacie).
Externa referencia REF3020 (2048mV) je použitá z dôvodou nestability vnútornej referencie v Attiny. R
EF3020 je 3 svorková referencia s malým úbytkom napätia.
