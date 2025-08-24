CROSS_COMPILE	?=
CC		:= $(CROSS_COMPILE)gcc
OBJ		:= writer
PATH		:= $(PATH):$(PWD)/../compiler/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin

# CROSS_COMPILE=aarch64-none-linux-gnu- make

$(OBJ): $(OBJ).c clean
	$(CC) -Wall $< -o $@

clean:
	rm -f $(OBJ)
