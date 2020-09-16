make: cartloop.c rs232.h
	gcc -Wall -o cartloop cartloop.c rs232.c -lwiringPi
