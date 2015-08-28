
OBJECTS = brainflayer.o bloom.o hex2blf.o base58.o timer.o warpwallet.o
BINARIES = brainflayer hex2blf
LIBS = -lssl -lrt -lcrypto -lz -ldl -lgmp

# Options are BRAINWALLET for SHA256, WARPWALLET for scrypt/pbkdf2
ATTACK_MODE = BRAINWALLET

# Use 1 to display, 0 to suppress
DISPLAY_SECRET  = 0
DISPLAY_HASH160 = 1
DISPLAY_WIF     = 1
DISPLAY_ADDRESS = 1
DISPLAY_COMPR   = 1

# Used in address and wallet import format generation
# See base58.h for more information
COIN_VERSION = 0x00
SECRET_KEY   = 0x80

# Allows adding flags at maketime
# Example: make all CFLAGS="-march=native"
override CFLAGS += -D BENCHMARK \
	-D $(ATTACK_MODE) \
	-D DISPLAY_SECRET=$(DISPLAY_SECRET) \
	-D DISPLAY_HASH160=$(DISPLAY_HASH160) \
	-D DISPLAY_WIF=$(DISPLAY_WIF) \
	-D DISPLAY_ADDRESS=$(DISPLAY_ADDRESS) \
	-D DISPLAY_COMPR=$(DISPLAY_COMPR) \
	-D COIN_VERSION=$(COIN_VERSION) \
	-D SECRET_KEY=$(SECRET_KEY) \

COMPILE = gcc -O2 $(CFLAGS) -g -fopenmp -pedantic -std=gnu99 -Wall -Wextra -funsigned-char -Wno-pointer-sign -Wno-sign-compare

all: $(BINARIES)

secp256k1/.libs/libsecp256k1.a:
	git submodule init
	git submodule update
	cd secp256k1; make distclean || true
	cd secp256k1; ./autogen.sh
	cd secp256k1; ./configure
	cd secp256k1; make CPPFLAGS="-march=native"

secp256k1/include/secp256k1.h: secp256k1/.libs/libsecp256k1.a

brainflayer.o: brainflayer.c secp256k1/include/secp256k1.h

%.o: %.c
	$(COMPILE) -c $< -o $@

hex2blf: hex2blf.o bloom.o
	$(COMPILE) -static $^ $(LIBS) -o hex2blf

brainflayer: brainflayer.o bloom.o base58.o timer.o warpwallet.o secp256k1/.libs/libsecp256k1.a
	$(COMPILE) -static $^ $(LIBS) -o brainflayer

clean:
	rm -f $(BINARIES) $(OBJECTS)
