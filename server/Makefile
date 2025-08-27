CROSS_COMPILE	?=
CC		:= $(CROSS_COMPILE)gcc
OBJ		:= aesdsocket
PATH		:= $(PATH):$(PWD)/../compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin
CGLAGS		:= -g -Wall -Werror

# CROSS_COMPILE=aarch64-none-linux-gnu- make

all: $(OBJ)

$(OBJ): $(OBJ).c clean
	$(CC) $(CGLAGS) $< -o $@

clean:
	rm -f $(OBJ)
