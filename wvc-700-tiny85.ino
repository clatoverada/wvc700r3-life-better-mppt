#include <avr/io.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define Mittel_aus 16 //gleitendens Mittel aus X Werten (Maximal 63 sonst Überlauf)

//LUT mit modifiziertem Sinus mit größeren Pausen in den Nulldurchgängen und etwas phasenverschoben
const uint8_t sinus2[] ={0,0,0,0,5,12,19,26,33,40,47,55,62,69,75,82,89,95,102,108,114,121,127,133,
	139,145,151,155,161,166,172,177,181,186,191,196,199,204,208,212,216,219,223,
	227,229,232,235,237,240,242,244,246,248,249,250,251,253,254,254,255,255,255,
	255,255,254,254,253,251,250,249,248,246,244,242,240,237,235,232,229,227,223,
	219,216,212,208,204,199,196,191,186,181,177,172,166,161,155,151,145,139,133,
127,121,114,108,102,95,89,82,75,69,62,55,47,40,33,26,19,12,5,0,0,0,0,0,0,0,0,0,0};
//LUT mit standard Sinus
const uint8_t sinus1[] = {0,6,13,19,25,31,37,44,50,56,62,68,74,80,86,92,98,103,109,115,120,126,131,136,
	142,147,152,157,162,167,171,176,180,185,189,193,197,201,205,208,212,215,219,
	222,225,228,231,233,236,238,240,242,244,246,247,249,250,251,252,253,254,254,
	255,255,255,255,255,254,254,253,252,251,250,249,247,246,244,242,240,238,236,
	233,231,228,225,222,219,215,212,208,205,201,197,193,189,185,180,176,171,167,
	162,157,152,147,142,136,131,126,120,115,109,103,98,92,86,80,74,68,62,56,50,
44,37,31,25,19,13,6};

const uint16_t minimalspannung_abs = 410 * Mittel_aus;  //= etwa 26V darunter kann der Wandler wegen zu geringer Ausgangsspannung an den Scheitelpunkten nicht ins Netz einspeisen
const uint16_t maximalstrom = 700 * Mittel_aus; //= etwa 15A darüber wird der WR wohl verglühen

volatile uint8_t U_out, counter, cnt200ms;
volatile bool flag200ms;

void ADC_Init();
uint16_t ADC_Read(uint8_t channel);

int main(void) {
	uint8_t Schritt=1,cnt_a=0;
	bool enable,teilbereichsuche=false;
	uint16_t spannung_MPP=0, Langzeitzaehler=0,minimalspannung=0,maximalspannung=0;
	uint32_t leistung_MPP=0;
	uint16_t spannung=0, strom=0, spannung_a[Mittel_aus], strom_a[Mittel_aus], temperatur=0;
	uint32_t leistung;

	DDRB = 0b00000001;    //PB0 als PWM Ausgang
	TCCR0A = 0b10000011;  //Timer0 für PWM Ausgabe einstellen
	TCCR0B = 0b00000001;
	TCCR1 = 0b10000100;  //Timer1 auf einen Zyklus einstellen
	OCR1C = 157;         //um mit den Werten aus der Sinus-LUT
	OCR1A = 0;           //Sinushalbwellen mit 50Hz auszugeben
	TIMSK = 0b01000000;  //Freigabe Interupt Timer1
	GIMSK = 0b00100000;  //Freigabe Pin Change Interrupt
	PCMSK = 0b00010000;  //auf PortB 4 für ZCD
	ADC_Init();
	for (cnt_a=0;cnt_a < Mittel_aus;cnt_a++)
	{
		spannung_a[cnt_a]=0;
		strom_a[cnt_a]=0;
	}
	cnt_a=0;
	sei();
	
	while (1) {
		//sobald die Reglerfreigabe weg geht WandlersStom abschalten und in den Warteschritt gehen
		enable = (PINB & (1 << PINB1)) == 0;
		if (!enable) {
			Schritt = 1;
			U_out = 0;
		}
		//wenn nix los dann messen und gleitendes Mittel aus "Mittel_aus" Werten bilden, aktuelle Leistung ausrechnen und im Suchmodus die Maximalwerte speichern
		if (flag200ms==false) {
			if (cnt_a > Mittel_aus-1) cnt_a=0;
			spannung-=spannung_a[cnt_a];
			spannung_a[cnt_a]= ADC_Read(1);//1V=0,05V am ADC
			spannung+=spannung_a[cnt_a];
			strom-=strom_a[cnt_a];
			strom_a[cnt_a] = ADC_Read(3);//1A=0,14V am ADC
			strom+=strom_a[cnt_a];
			cnt_a++;
			if (Schritt==2){ //im Suchschritt suchen wir hier den Punkt mit der grösten Leistung und merken uns die Spannung dort. Auf die regeln wir später.
				leistung = (uint32_t) spannung * strom;
				if (leistung > leistung_MPP) {
					spannung_MPP = spannung; 
					leistung_MPP = leistung;
				}
			}
		}
		//Haupttakt alle 200ms
		else{
			flag200ms = false;
			ADMUX=0b10001111;
			temperatur=ADC_Read(15); //Temperatur im Gehäuse messen
			ADMUX=0;

			switch (Schritt) {
				case 1:
				//auf die Reglerfreigabe vom Hauptcontroller warten
				{

					if (enable) {
						minimalspannung=minimalspannung_abs;
						maximalspannung=minimalspannung_abs;
						Langzeitzaehler = 0;
						leistung_MPP = 0;
						U_out = 0;
						Schritt = 2;
					}
					break;
				}
				case 2:
				//Wandlerstom kontinuirlich hoch fahren bis die Zellspannung auf die Minimalspannung sinkt oder der Strom auf den Maximalstrom steigt
				//dabei den Punkt mit der grösten Leistung suchen und sich die Spannung dort merken = MPP Spannung
				{
					if (maximalspannung<spannung)maximalspannung=spannung;
					if(teilbereichsuche){
						if((maximalspannung-(maximalspannung>>2))>minimalspannung)minimalspannung=(maximalspannung-(maximalspannung>>2));
					}
					else minimalspannung=minimalspannung_abs;
					
					if ((spannung > minimalspannung) && (strom < maximalstrom)) {
						if (U_out < 255) {
							U_out++;
						}
						else {
							teilbereichsuche=false;
							Schritt = 3;
						}
					}
					else {
						teilbereichsuche=false;
						Schritt = 3;
					}
					break;
				}
				case 3:
				{
					//bei plötzlicher Verschattung leistung schnell reduzieren (Spannung fällt unter 24V)
					if (spannung < (minimalspannung_abs - 38 * Mittel_aus)) {
						U_out = U_out >> 1;
					}
					//ansonsten Wandlerstrom kontinuirlich so einstellen dass die Zellen mit der in Schritt 2 gefundenen MPP Spannung laufen
					else {
						if ((spannung < spannung_MPP)||(strom>maximalstrom)||(temperatur>355)) { //72°C
							if (U_out > 0) U_out--; //Ausgangsstrom reduzieren
						}
						if ((spannung > spannung_MPP)&&(strom<maximalstrom)&&(temperatur<350)) { //68°C
							if (U_out < 255) U_out++; //Augangsstrom erhöhen
						}
					}
					Langzeitzaehler++;
					//alle 10 min gucken ob sich die MPP Spannung durch änderung der Zellentemperatur verschoben hat
					//Wandlerstom dazu um 1/4 senken und Suche starten
					if ((Langzeitzaehler == 3000) || (Langzeitzaehler == 6000)) {
						leistung_MPP = 0;
						U_out -= U_out >> 2;
						teilbereichsuche=true;
						Schritt = 2;
					}
					//alle 30min gucken ob sich durch verschattung ein neues globales maximum gebildet hat
					//Wandlerstom dazu auf 1/8 absenken und Suche starten
					if (Langzeitzaehler == 9000) {
						leistung_MPP = 0;
						Langzeitzaehler = 0;
						U_out = U_out >> 3;
						Schritt = 2;
					}
					break;
				}
			}
		}
	}
}

//Sinushalbwellen mit variabler Amplitude für den Strommodulator und den 200ms Takt für die Hauptschleife erzeugen
ISR(TIMER1_COMPA_vect) {
	OCR0A = ((sinus2[counter & 0b01111111] * U_out) >> 8);
	counter++;
	if (counter == 128) { //alle 20ms
		if (cnt200ms < 10) {
			cnt200ms++;
		}
		else {                //10 x 20ms = 200ms
			cnt200ms = 0;
			flag200ms = true;
		}
	}
}



//auf Netzfrequenz und Phase synchronisieren

ISR(PCINT0_vect) {
	if ((PINB & (1 << PINB4))) {
		if (counter > 128) OCR1C--;
		else OCR1C++;
		counter = 0;
	}
}


void ADC_Init(void) {
	// die Versorgungsspannung AVcc als Referenz wählen:
	ADMUX = 0;
	// oder interne Referenzspannung als Referenz für den ADC wählen:
	// ADMUX = (1<<REFS1) | (1<<REFS0);
	// Bit ADFR ("free running") in ADCSRA steht beim Einschalten
	// schon auf 0, also single conversion
	ADCSRA = (1 << ADPS2) | (1 << ADPS1);  // Frequenzvorteiler
	ADCSRA |= (1 << ADEN);                 // ADC aktivieren
	DIDR0 = 0b00001100;
	/* nach Aktivieren des ADC wird ein "Dummy-Readout" empfohlen, man liest
	also einen Wert und verwirft diesen, um den ADC "warmlaufen zu lassen" */
	ADCSRA |= (1 << ADSC);          // eine ADC-Wandlung
	while (ADCSRA & (1 << ADSC)) {  // auf Abschluss der Konvertierung warten
	}
	/* ADCW muss einmal gelesen werden, sonst wird Ergebnis der nächsten
	Wandlung nicht übernommen. */
	(void)ADCW;
}

/* ADC Einzelmessung */
uint16_t ADC_Read(uint8_t channel) {
	// Kanal waehlen, ohne andere Bits zu beeinflußen
	ADMUX = (ADMUX & ~(0x1F)) | (channel & 0x1F);
	ADCSRA |= (1 << ADSC);          // eine Wandlung "single conversion"
	while (ADCSRA & (1 << ADSC)) {  // auf Abschluss der Konvertierung warten
	}
	return ADCW;  // ADC auslesen und zurückgeben
}
