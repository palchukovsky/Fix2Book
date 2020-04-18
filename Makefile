CC = g++
CFLAGS  = -g -Wall -Wfatal-errors -std=c++17
SRC = src/Main.cpp
OBJ = Main.o
TARGET = fix2book

$(TARGET):
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)
