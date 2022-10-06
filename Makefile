CC = gcc
CFLAGS = -I -g

MASTER=master
SLAVE=slave

OBJ1= master.o 
OBJ2=slave.o

SRC= config.h
OUTPUT=$(MASTER) $(SLAVE)

all: $(OUTPUT) 

%.o: %.c $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(MASTER)): $(OBJ1) 
	$(CC) $(CFLAGS) $(OBJ1) -o $(MASTER)
	
$(SLAVE): $(OBJ2)
	$(CC) $(CFLAGS) $(OBJ2) -o $(SLAVE)

clean:  
	rm -f $(OUTPUT) *.o  cstest logfile* 
