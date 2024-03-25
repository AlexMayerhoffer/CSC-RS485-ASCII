#include <c8051F040.h>	// declaratii SFR
#include <wdt.h>
#include <osc.h>
#include <port.h>
#include <uart0.h>
#include <uart1.h>
#include <lcd.h>
#include <keyb.h>
#include <Protocol.h>
#include <UserIO.h>

nod retea[NR_NODURI];					// retea cu 5 noduri

unsigned char STARE_COM = 0;		// starea initiala a FSA COM
unsigned char TIP_NOD	= 0;		// tip nod initial: Slave sau No JET
unsigned char STARE_IO 	= 0;		// stare interfata IO - asteptare comenzi
unsigned char ADR_MASTER;				// adresa nod master - numai MS

extern unsigned char AFISARE;		// flag permitere afisare

//***********************************************************************************************************
void TxMesaj(unsigned char i);	// transmisie mesaj destinat nodului i
unsigned char RxMesaj(unsigned char i);		// primire mesaj de la nodul i

//***********************************************************************************************************
void main (void) {
	unsigned char i, found;	// variabile locale
	
	WDT_Disable();												// dezactiveaza WDT
	SYSCLK_Init();											// initializeaza si selecteaza oscilatorul ales in osc.h
	UART1_Init(NINE_BIT, BAUDRATE_COM);		// initilizare UART1 - port comunicatie (TxD la P1.0 si RxD la P1.1)
	UART1_TxRxEN (1,1);									// validare Tx si Rx UART1
	
 	PORT_Init ();													// conecteaza perifericele la pini (UART0, UART1) si stabileste tipul pinilor

	LCD_Init();    												// 2 linii, display ON, cursor OFF, pozitia initiala (0,0)
	KEYB_Init();													// initializare driver tastatura matriciala locala
	UART0_Init(EIGHT_BIT, BAUDRATE_IO);		// initializare UART0  - conectata la USB-UART (P0.0 si P0.1)

	Timer0_Init();  								// initializare Timer 0

 	EA = 1;                         			// valideaza intreruperile
 	SFRPAGE = LEGACY_PAGE;          			// selecteaza pagina 0 SFR
	
	for(i = 0; i < NR_NODURI; i++){		// initializare structuri de date
		retea[i].full = 0;						// initializeaza buffer gol pentru toate nodurile
		retea[i].bufasc[0] = ':';				// pune primul caracter in buffer-ele ASCII ":"
	}

	Afisare_meniu();			   				// Afiseaza meniul de comenzi
	
 	while(1){												// bucla infinita
																
		switch(STARE_COM){
			case 0:			
				switch(RxMesaj(ADR_NOD)){					// asteapta jetonul de la master
					case TMO:
							// anunta ca nodul a regenerat jetonul
							LCD_Home();
							LCD_Clear();
							LCD_PutStr(0,0,"Regenerare jeton");
							UART1_Putstr("Regenerare jeton");
							// nodul curent detine jetonul
							TIP_NOD = JETON;
							// Daca e permisa afisarea, afiseaza meniul de comenzi
							if(AFISARE)
								Afisare_meniu();
							// trece in starea 1
							STARE_COM = 1;
						break;

					case ROK: // a primit un mesaj USER_MES, il afiseaza
						Afisare_mesaj();
						break;
					case JOK:										// a primit un jetonul
						Delay(WAIT/2);						// asteapta WAIT/2 ms
					
						// adresa HW dest este adr_hw_src
						retea[ADR_NOD].bufbin.adresa_hw_dest = retea[ADR_NOD].bufbin.adresa_hw_src;
						// adresa HW src este ADR_NOD
						retea[ADR_NOD].bufbin.adresa_hw_src = ADR_NOD;
						// tip mesaj = JET_MES
						retea[ADR_NOD].bufbin.tipmes = JET_MES;
						// transmite mesajul JET_MES din bufferul ADR_NOD
						TxMesaj(ADR_NOD);
						// nodul curent detine jetonul
						TIP_NOD = JETON;
						// Daca e permisa afisarea, afiseaza meniul de comenzi		
						if(AFISARE)
							Afisare_meniu(); 										
						// trece in starea 1
						STARE_COM = 1;
						break; // nodul detine jetonul, poate trece sa transmita un mesaj de date

					case CAN:												// afiseaza eroare Mesaj incomplet
						Error("Mesaj incomplet!");
						break;
					case CAN_adresa_hw_src:	 								// afiseaza eroare Mesaj incomplet (adresa_hw_src)
						Error("Mesaj incomplet (adresa_hw_src)!");
						break;
					case CAN_tipmes:										// afiseaza eroare Mesaj incomplet (tip mesaj)
						Error("Mesaj incomplet (tip mesaj)!");
						break;
					case CAN_sc:											// afiseaza eroare Mesaj incomplet (sc)
						Error("Mesaj incomplet (sc)!");
						break;
					case TIP:												// afiseaza eroare Tip mesaj necunoscut
						Error("Tip mesaj necunoscut!");
						break;
					case ESC:	 											// afiseaza Eroare SC
						Error("Eroare SC!");
						break;
					default:												// afiseaza cod eroare necunoscut, apoi asteapta 1 sec
						Error("Eroare necunoscuta!");
						Delay(1000);
						break;
				}
				break;							

			case 1:													
																			// cauta sa gaseasca un mesaj util de transmis
				for(i = 0, found = 0; i < NR_NODURI; i++){
					if(retea[i].full){										// daca gaseste un mesaj de transmis catre nodul i
						// adresa HW dest este dest
						retea[ADR_NOD].bufbin.adresa_hw_dest = i;
						// transmite mesajul catre nodul i
						TxMesaj(i);
						// asteapta procesarea mesajului la destinatie (WAIT/2 msec)
						Delay(WAIT/2);
					}
				}
				// va incerca sa transmita jetonul nodului urmator 
				// trece in starea 2, sa transmita jetonul urmatorului nod
				STARE_COM = 2;
				
			break;	
				
			case 2:											// tratare stare 2 si comutare stare
				// selecteaza urmatorul slave (incrementeaza i)
				i = (ADR_NOD + 1) % NR_NODURI;
				Delay(WAIT/2);									// asteapta WAIT/2 sec
				
				// adresa HW dest este i
				retea[i].bufbin.adresa_hw_dest = i;
				// adresa HW src este ADR_NOD
				retea[i].bufbin.adresa_hw_src = ADR_NOD;
				// tip mesaj = JET_MES
				retea[i].bufbin.tipmes = JET_MES;
				// transmite mesajul din bufferul i
				TxMesaj(i);
				// trece in starea 3, sa astepte confirmarea de la nodul i ca jetonul a fost primit
				STARE_COM = 3;

			break;

			case 3:				
				switch(RxMesaj(ADR_NOD)){									// asteapta un raspuns de la nod i
					case TMO:	
						// afiseaza eroare Timeout nod i
						Error("\n\rTimeout nod ");		
						// revine in starea 2 (nu a primit raspuns)
						STARE_COM = 2;		
						break;
					case JOK:																
						// a primit confirmarea transferului jetonului, revine in starea 0
						STARE_COM = 0;
						// nodul nu mai detine jetonul
						TIP_NOD = NOJET;
						// daca afisarea e permisa, afiseaza meniul
						if(AFISARE)
							Afisare_meniu();
						break;
					case ERI:																
						// afiseaza Eroare incadrare
						Error("Eroare incadrare!");
						// revine in starea 0
						STARE_COM = 0;
						// nodul nu mai detine jetonul
						TIP_NOD = NOJET;
						// afiseaza meniul
						if(AFISARE)
							Afisare_meniu();
						break;			
					case ERA:																
						// afiseaza Eroare adresa
						Error("Eroare adresa!");
						// revine in starea 0
						STARE_COM = 0;
						// nodul nu mai detine jetonul
						TIP_NOD = NOJET;
						// afiseaza meniul
						if(AFISARE)
							Afisare_meniu();
						break;			
					
					case CAN:																
						// afiseaza Mesaj incomplet
						Error("Mesaj incomplet!");
						// revine in starea 0
						STARE_COM = 0;
						// nodul nu mai detine jetonul
						TIP_NOD = NOJET;
						// afiseaza meniul
						if(AFISARE)
							Afisare_meniu();
						break;			
					
					case TIP:																
						// afiseaza Tip mesaj necunoscut
						Error("Tip mesaj necunoscut!");
						// revine in starea 0
						STARE_COM = 0;
						// nodul nu mai detine jetonul
						TIP_NOD = NOJET;
						// afiseaza meniul
						if(AFISARE)
							Afisare_meniu();
						break;			
					
					case ESC:																
						// afiseaza Eroare SC
						Error("Eroare SC!");
						// revine in starea 0
						STARE_COM = 0;
						// nodul nu mai detine jetonul
						TIP_NOD = NOJET;
						// afiseaza meniul
						if(AFISARE)
							Afisare_meniu();
						break;			

					default:																
						// afiseaza Eroare necunoscuta
						Error("Eroare necunoscuta!");
						// asteapta 1000 ms
						Delay(1000);
						// revine in starea 0
						STARE_COM = 0;
						// nodul nu mai detine jetonul
						TIP_NOD = NOJET;
						// afiseaza meniul
						if(AFISARE)
							Afisare_meniu();
						break;			
			}	
			break;			
		}
		
		UserIO();							// apel functie interfata
	}
}
