#include <stdio.h>
#include <wiringPi.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include "rs232.h"

/*
struct bank{
	char* data;
}*/

char* ROM_FILENAME = "Super Mario World (U) [!].smc";
char* SRAM_FILENAME = "Earthbound (U).sav";
int ROM_SIZE = 0x80200;
int SRAM_SIZE = 0x2000;
int DEBUG = 1;


// these are the values in the header of the ROM that signify which mapping to use
#define LoROM			20
#define HiROM			21
#define SA1ROM			23
#define LoFastROM		30
#define HiFastROM		31
#define ExLoROM			32
#define ExHiROM			35

// this is the port for /dev/ttyACM0
#define cport_nr		24
// We're using 9600 baud
#define bdrate			57600
// A0 - A23 are the 0th - 23rd address bits
#define A0				29
#define A1				28
#define A2				27
#define A3				26
#define A4				31
#define A5				11
#define A6				10
#define A7				6
#define A8				5
#define A9				4
#define A10				1
#define A11				16
#define A12				15
#define A13				8
#define A14				9
#define A15				7
#define A16				0
#define A17				2
#define A18				3
#define A19				12
#define A20				13
#define A21				14
#define A22				30
#define A23				21
// set if we read data
#define RD				24
// set if we write data
#define WR				23
// set if the information on the bus is for the cartridge
#define CART			22

int setup(void){
	printf("Setting up I/O ports ... \n");

	wiringPiSetup();
	pinMode(A0, INPUT);
	pinMode(A1, INPUT);
	pinMode(A2, INPUT);
	pinMode(A3, INPUT);
	pinMode(A4, INPUT);
	pinMode(A5, INPUT);
	pinMode(A6, INPUT);
	pinMode(A7, INPUT);
	pinMode(A8, INPUT);
	pinMode(A9, INPUT);
	pinMode(A10, INPUT);
	pinMode(A11, INPUT);
	pinMode(A12, INPUT);
	pinMode(A13, INPUT);
	pinMode(A14, INPUT);
	pinMode(A15, INPUT);
	pinMode(A16, INPUT);
	pinMode(A17, INPUT);
	pinMode(A18, INPUT);
	pinMode(A19, INPUT);
	pinMode(A20, INPUT);
	pinMode(A21, INPUT);
	pinMode(A22, INPUT);
	pinMode(A23, INPUT);

	pinMode(RD, INPUT);
	pinMode(WR, INPUT);
	pinMode(CART, INPUT);

	printf("done setting up I/O ports!\n");
	printf("setting up serial port with Arduino ... \n");

	// 8 data bits, no parity, 1 stop bit
	char mode[] = {'8','N','1',0};
	if(RS232_OpenComport(cport_nr, bdrate, mode)){
		printf("Cannot open comport\n");
		return 1;
	}
	return 0;
}

int read_address(void){
	int addr = 0;

	addr += digitalRead(A0) << 0;
	addr += digitalRead(A1) << 1;
	addr += digitalRead(A2) << 2;
	addr += digitalRead(A3) << 3;
	addr += digitalRead(A4) << 4;
	addr += digitalRead(A5) << 5;
	addr += digitalRead(A6) << 6;
	addr += digitalRead(A7) << 7;
	addr += digitalRead(A8) << 8;
	addr += digitalRead(A9) << 9;
	addr += digitalRead(A10) << 10;
	addr += digitalRead(A11) << 11;
	addr += digitalRead(A12) << 12;
	addr += digitalRead(A13) << 13;
	addr += digitalRead(A14) << 14;
	addr += digitalRead(A15) << 15;
	addr += digitalRead(A16) << 16;
	addr += digitalRead(A17) << 17;
	addr += digitalRead(A18) << 18;
	addr += digitalRead(A19) << 19;
	addr += digitalRead(A20) << 20;
	addr += digitalRead(A21) << 21;
	addr += digitalRead(A22) << 22;
	addr += digitalRead(A23) << 23;

	return addr;
}


// use this for ROM and SRAM data
unsigned char* get_game_data(char* filename, int size){
	FILE *fp;

	// Our first test will store everything in one array. We may have to split it into chunks, though.
	unsigned char* data = malloc(ROM_SIZE);

	// ensure file opens correctly
	fp = fopen(filename, "r");
	if(fp == NULL){
		printf("could not find data at file %s/n", filename);
		return NULL;
	}

	for(int i = 0; i < size; i++)
		data[i] = fgetc(fp);

	printf("%p, %p\n", data, &data[1]);

	fclose(fp);
	return data;
}

// use this to get a pointer to the ROM header
unsigned char* get_header(unsigned char* ROM, int rom_size){
	// check the checksum at LoROM
	if((ROM[0x7FDC] | ROM[0x7FDE]) == 0xFF && (ROM[0x7FDD] | ROM[0x7FDF]) == 0xFF){
		SRAM_SIZE = 0x400 << ROM[0x7FD8];
		// ROM_SIZE = 0x400 << ROM[0x7FD7];
		return ROM + 0x7FC0;
	}
	
	// if it failed, it must be a HiROM
	SRAM_SIZE = 0x400 << ROM[0xFFD8];
	// ROM_SIZE = 0x400 << ROM[0x7FD7];
	return ROM + 0xFFC0;
}


void teardown(unsigned char* ROM, unsigned char* SRAM){
	free(ROM);
	// save SRAM
	free(SRAM);
	RS232_CloseComport(cport_nr);
}

int main(void){

	//////////////////////////////////////////////////////
	//  BEGIN SETUP SECTION                             //
	//////////////////////////////////////////////////////



	// setup
	if(setup()){
		printf("something went wrong in setup\n");
		return 1;
	}

	// load ROM into memory
	printf("reading ROM...\n");
	
	unsigned char* RAW_ROM = get_game_data(ROM_FILENAME, ROM_SIZE);
	if(RAW_ROM == NULL)	{
		printf("could not read ROM\n");
		return 1;
	}

	// remove SMC header if there is one:

	// if there isn't an SMC header, the ROM image will be a multiple of 0x8000 or 0x10000 bytes,
	//  so (rom_size % 0x400) == 0.
	// if there is an SMC header, it will be 0x200 bytes long, so the ROM image will be an extra
	//  0x200 bytes long, meaning (rom_size % 0x400) == 0x200
	int SMC_size = ROM_SIZE % 0x400;
	// offset ROM to remove SMC header, but keep the original ROM to avoid memory leaks
	unsigned char* ROM = RAW_ROM + SMC_size;
	// we don't want to keep track of the smc header
	ROM_SIZE -= SMC_size;

	unsigned char* hdr = get_header(ROM, ROM_SIZE);

	// determines romtype from header (LoROM, HiROM, ExLoROM, ExHiROM)
	unsigned char ROM_TYPE = LoROM;//hdr[0x15];

	// TODO will need to deal with fast/slow rom

	printf("finished reading ROM!\n");

	// load SRAM into memory or create a new instance
	printf("reading save data ... \n");
	unsigned char* SRAM = get_game_data(SRAM_FILENAME, SRAM_SIZE);
	if(SRAM == NULL){
		printf("no save found.\n");
		SRAM = malloc(SRAM_SIZE);
	}
	printf("finished reading save data!\n");

	
	
	//////////////////////////////////////////////////////
	//  END SETUP SECTION                               //
	//////////////////////////////////////////////////////



	// prints a ROM dump of a certain region to ensure it has been read correctly
	if(DEBUG){
		int START_BYTE = 0x7FC0;
		int END_BYTE = 0x7FFF;

		for(int line = START_BYTE; line < END_BYTE; line += 16){
			printf("%#x: ", line);

			for(int j = 0; j < 16; j++){
				if(ROM[line + j] < 16)
					printf("0");

				printf("%x ", ROM[line + j]);
				
				if(j % 4 == 3)
					printf(" ");
			}
			printf("\n");
		}
	}
	
	
	// this is the real cart loop
	for(;;){
		while(digitalRead(CART)){
			int addr = read_address();
			int bank = addr >> 16;
			if(DEBUG)
				printf("address read: %p, bank %#x\n", (void*)addr, bank);

			unsigned char* data_ptr = NULL;

			// https://en.wikibooks.org/wiki/Super_NES_Programming/SNES_memory_map#LoROM
			if(ROM_TYPE == LoROM){
				if(DEBUG)
					printf("LoROM\n");
				int cart_bank = (addr & 0x110000) >> 1;
				int cart_addr = cart_bank + (addr & 0x007FFF);

				// top half of address space mirrors lower half
				if(bank > 0x80){
					addr -= 0x800000;
					bank -= 0x80;
				}
				
				if(bank < 0x40 && (addr & 0xFFFF) > 0x7FFF)
					data_ptr = ROM + cart_addr;
				else if(bank < 0x70) {
					// lower banks are mirrors of upper banks, if used
					if((addr & 0xFFFF) < 0x8000) // mirror of banks 40-6F
							bank++;
					
					data_ptr = ROM + cart_addr;

				} else {
					// lower banks access SRAM
					if((addr & 0xFFFF) < 0x8000)
						// TODO how do I tell which bank I'm using
						//  answer: it is mapped continuously
						data_ptr = SRAM; /* + bank +  (addr & 0x7FFF);*/
					else
						data_ptr = ROM + cart_addr;
				}


			// https://en.wikibooks.org/wiki/Super_NES_Programming/SNES_memory_map#HiROM
			} else if(ROM_TYPE == HiROM){
				
				if(DEBUG)
					printf("HiROM\n");
				// top half of address space mirrors lower half
				if(bank > 0x80){
					addr -= 0x800000;
					bank -= 0x80;
				}	
				if(DEBUG)
					printf("new addr/bank: %p, %#x\n", addr, bank);	

				if(bank < 0x20){
					if(DEBUG)
						printf("bank < 0x20\n");
					data_ptr = ROM + addr;
				}
				else if (bank < 0x40){
					if(DEBUG)
						printf("bank < 0x40\n");
					if((addr & 0xFFFF) < 0x8000){
						// look at sram (0x6000 - 0x8000)
						// TODO how do I tell which bank I'm using
						data_ptr = SRAM; /*+ addr ?*/
					} else {
						data_ptr = ROM + addr;
					}
				} else {
					data_ptr = ROM + addr - 0x400000;
					if(DEBUG)
						printf("bank >= 0x40\n");
				}

			} else if(ROM_TYPE == ExLoROM){
				// TODO   
			} else if(ROM_TYPE == ExHiROM){
				// TODO
			}
			
			if(DEBUG)
				printf("final location read: %p\n", data_ptr - ROM);
			if(digitalRead(RD)){
				if(DEBUG)
					printf("data at addr: %c", *data_ptr);
				if(data_ptr - ROM < ROM_SIZE)
					RS232_SendByte(cport_nr, *data_ptr);
			} /*else if(digitalRead(WR)){
				if(DEBUG)
					printf("need to write to SRAM");
				RS232_PollComport(cport_nr, data_ptr, 1);
			}*/
		}
	}
	return 0;
}

