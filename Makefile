CC ?= cc

ifeq ($(OS),Windows_NT)
	BFC_TGT ?= bfc.exe
	BFI_TGT ?= bfi.exe
	BFM_TGT ?= bfm.exe
else
	BFC_TGT ?= bfc
	BFI_TGT ?= bfi
	BFM_TGT ?= bfm
endif

.PHONY: debug clean

all: $(BFC_TGT) $(BFI_TGT) $(BFM_TGT)

$(BFC_TGT): bfc.c
	$(CC) bfc.c -O2 -Wall -Wextra -pedantic -o $(BFC_TGT)

$(BFI_TGT): bfi.c
	$(CC) bfi.c -O2 -Wall -Wextra -pedantic -o $(BFI_TGT)

$(BFM_TGT): bfm.c
	$(CC) bfm.c -O2 -Wall -Wextra -pedantic -o $(BFM_TGT)

clean:
	rm -f $(BFC_TGT) $(BFI_TGT) $(BFM_TGT)
