#include <c8051F040.h>						// declaratii SFR
#include <osc.h>
#include <Protocol.h>
#include <uart0.h>
#include <lcd.h>
#include <keyb.h>
#include <UserIO.h>

void Afisare_meniu(void);					// afisare meniu initial
void Afisare_mesaj(void);					// afisare mesaj receptionat
void Error(char *ptr);						// afisare mesaj de eroare

unsigned char TERM_Input(void);
unsigned char AFISARE = 1;

extern unsigned char LCD_line,LCD_col;

//***********************************************************************************************************
extern unsigned char ADR_MASTER;
extern unsigned char TIP_NOD;
extern unsigned char STARE_IO;
extern nod retea[];

//***********************************************************************************************************
void UserIO(void){					// interfata cu utilizatorul
	static unsigned char tasta, cmd, dest, lng;	// variabile locale statice
	
	if(0 == (tasta = TERM_Input())){
		tasta = KEYB_Input();
		if(tasta) LCD_Putch(tasta);
	}
	if(tasta){
		
		switch(STARE_IO){
			
			case 0:	switch(tasta){									
				
							case '1': 											// s-a dat comanda de transmisie mesaj								
									// afiseaza Tx Msg:> Nod =
									LCD_Home();
									LCD_Clear();
									LCD_PutStr(0,0,"Tx MSG:> Nod =");
									// blocheaza afisarea (AFISARE = 0)
									AFISARE = 0;
									// trece in starea 1
									STARE_IO = 1;
									// comanda este '1'
									cmd = '1';
									break;
							
							case '2': 											// s-a dat comanda de afisare Stare Nod:
								// blocheaza afisarea (AFISARE = 0)
								AFISARE = 0;
								// trece in starea 1
								STARE_IO = 1;
								// comanda este '2'
								cmd = '2';
								break;															
							default: break;
						}
						break;
									
			case 1:													// s-a selectat nodul								
																			
				// extrage dest din tasta
				dest = tasta - '0';
				// daca comanda e '1' si adresa e intre '0' - '4', mai putin adresa proprie
				if((cmd == '1') && (dest != ADR_NOD) && (dest <= 4 && dest >= 0)){
					// Daca este deja un mesaj in buffer ...
					if(retea[dest].full){
						// afiseaza Buffer plin
						Error("\n\rBuffer plin");
					}
					// trece in starea 0, s-a terminat tratarea comenzii '1'
					STARE_IO = 0;
					// afisare meniu
					Afisare_meniu();
				}	
				// altfel ...
				else {
					// pune in bufferul dest adresa hw dest egala cu dest
					retea[dest].bufbin.adresa_hw_dest = dest;		
					// pune in bufferul dest adresa hw sursa  egala cu ADR_NOD
					retea[dest].bufbin.adresa_hw_src = ADR_NOD;
					// pune in bufferul dest adresa nodului sursa ADR_NOD
					retea[dest].bufbin.src = ADR_NOD;
					// pune in bufferul dest adresa nodului destinatie (dest)
					retea[dest].bufbin.dest = dest;
					// cere introducerea mesajului
					LCD_PutStr(1,0,"Mesaj:");

					// initializeaza lng = 0 
					lng = 0;
					// trece in starea 2, sa astepte caracterele mesajului
					STARE_IO = 2;
				}
				// daca comanda e '2' si adresa e intre '0'-'4'
				if((cmd == '2') && (dest <= 4 && dest >= 0)){
					// extrage dest din tasta
					dest = tasta - '0';
					// Daca este deja un mesaj in buffer ...
					if(retea[ADR_NOD].full){
						// Afiseaza Buffer plin
						Error("\n\rBuffer plin");
					}
					// altfel
					else{
						// Afiseaza Buffer gol
						Error("\n\rBuffer gol");
					}
					// trece in starea 0, s-a terminat tratarea comenzii
					STARE_IO = 0;
					// afisare meniu
					Afisare_meniu();
				}
				break;
		
			case 2:													
				// daca tasta e diferita de CR ('\r'), de NL ('\n') si de '*' si nu s-a ajuns la limita maxima a bufferului de caractere
				if((tasta != '\r') && (tasta != '\n') && (tasta != '*') && (lng < NR_CHAR_MAX)){
					// stocheaza codul tastei apasate in bufferul de date si incrementeaza lng
					retea[dest].bufbin.date[lng++] = tasta;
				}
				// daca s-a atins nr maxim de caractere sau s-a apasat Enter ('\r') sau ('\n') sau '*'
				else {
					// stocheaza lng
					retea[dest].bufbin.lng = lng;
					// pune in bufbin tipul mesajului (USER_MES)
					retea[dest].bufbin.tipmes = USER_MES;
					// marcheaza buffer plin
					retea[dest].full = 1;
					// trece in starea 0, s-a terminat tratarea comenzii
					STARE_IO = 0;
					// afisare meniu
				}
	
						break;	
	
		}
	}
}

//***********************************************************************************************************
void Afisare_meniu(void){				  			// afisare meniu initial
	AFISARE = 1;
	UART0_Putstr("\n\rTema ");
	LCD_PutStr(0,0,"T");
	UART0_Putch(TEMA + '0');
	LCD_Putch(TEMA + '0');
	

	if(TIP_NOD == JETON){
		UART0_Putstr(" Jeton ");
		LCD_PutStr(LCD_line, LCD_col, " Jeton:");
	}
	else{
		UART0_Putstr(" NoJet ");
		LCD_PutStr(LCD_line, LCD_col, "NoJet:");
	}
	
	UART0_Putch(ADR_NOD + '0');						// afiseaza adresa nodului
	LCD_Putch(ADR_NOD + '0');
	UART0_Putstr(":ASC" );								// afiseaza parametrii specifici temei
	LCD_PutStr(LCD_line, LCD_col, " ASC");
	UART0_Putstr("\n\r> 1-TxM 2-Stare :>");	// meniul de comenzi
	LCD_PutStr(1,0, "1-TxM 2-Stare :>");
}


//***********************************************************************************************************
void Afisare_mesaj(void){          		// afisare mesaj din bufferul de receptie i
	unsigned char j, lng, *ptr;
	if(retea[ADR_NOD].full){						// exista mesaj in bufferul de receptie?
		lng = retea[ADR_NOD].bufbin.lng;
		UART0_Putstr("\n\r>Rx: De la nodul ");
		LCD_DelLine(1);
		LCD_PutStr(1,0, "Rx: ");		
		UART0_Putch(retea[ADR_NOD].bufbin.src + '0');			// afiseaza adresa nodului sursa al mesajului
		LCD_Putch(retea[ADR_NOD].bufbin.src + '0');
		
		UART0_Putstr(": ");
		LCD_PutStr(LCD_line, LCD_col, ">: ");	
		
		for(j = 0, ptr = retea[ADR_NOD].bufbin.date; j < lng; j++) UART0_Putch(*ptr++);	// afiseaza mesajul, caracter cu caracter
		for(j = 0, ptr = retea[ADR_NOD].bufbin.date; j < lng; j++) LCD_Putch(*ptr++);		// afiseaza mesajul, caracter cu caracter

		retea[ADR_NOD].full = 0;					// mesajul a fost afisat, marcheaza buffer gol
	}
}

//***********************************************************************************************************
void Error(char *ptr){
	if(AFISARE){
		UART0_Putstr(ptr);
		LCD_DelLine(1);
		LCD_PutStr(1,0, ptr+2);
	}
}

unsigned char TERM_Input(void){

	unsigned char ch, SFRPAGE_save = SFRPAGE;
	
	SFRPAGE = LEGACY_PAGE;
	
	ch = 0;
	if(RI0) ch = UART0_Getch(1);
	
	SFRPAGE = SFRPAGE_save;
	
	return ch;
}