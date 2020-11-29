INPUT = Asst2
OUTPUT=detector
CFLAGS=-g -pthread -Wall
LFLAGS=-lm

%: %.c %.h
	gcc $(CFLAGS) -o $@ $< $(LFLAGS)

%: %.c
	gcc $(CFLAGS) -o $(OUTPUT) $< $(LFLAGS)

all: $(INPUT)

clean:
	rm -f *.o $(OUTPUT)

