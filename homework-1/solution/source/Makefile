target: driver programA programB

driver: driver.o
	gcc -o driver driver.c

programA: programA.c utils.c
	gcc -o programA programA.c utils.c

programB: programB.c utils.c fft.c
	gcc -o programB programB.c utils.c fft.c -lm

