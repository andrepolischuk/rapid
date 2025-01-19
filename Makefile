CC = gcc
SRC = src/app.c src/rapid.c
TARGET = app
LDFLAGS = -L /opt/homebrew/lib/ -l cjson

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $@ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) $(arg)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
