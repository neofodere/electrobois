# SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
# SPDX-License-Identifier: MIT

CXX = g++

CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -O2 -DSDL_MAIN_HANDLED \
	$(shell sdl2-config --cflags)

SRC = src/main.cpp \
	  src/bus.cpp \
	  src/6507.cpp \
	  src/riot.cpp \
	  src/tia.cpp

OBJ = $(SRC:.cpp=.o)

ifeq ($(OS),Windows_NT)
    BIN = electrobois.exe
    LDFLAGS = -lmingw32 -lSDL2main -lSDL2
    CLEAN_CMD = rm -f src/*.o 
    FCLEAN_CMD = rm -f src/*.o $(BIN)
else
    BIN = electrobois
    LDFLAGS = $(shell sdl2-config --libs)
    CLEAN_CMD = rm -f src/*.o
    FCLEAN_CMD = rm -f src/*.o $(BIN)
endif

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(CLEAN_CMD)

fclean:
	$(FCLEAN_CMD)

re: fclean all

.PHONY: all clean fclean re