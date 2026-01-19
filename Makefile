# ==========================================
# WWYL - Thesis Project Makefile
# ==========================================

CC = gcc

# Directory
INC_DIR = lib
SRC_DIR = src

# --- 1. Flags di Compilazione Base ---
# -I$(INC_DIR): Dice al compilatore di cercare gli header (.h) nella cartella 'lib'
# -Wall -Wextra: Attiva tutti i warning (fondamentale per C sicuro)
# -std=c11: Usa lo standard C11
# -O2: Ottimizzazione livello 2 (necessaria per _FORTIFY_SOURCE)
CFLAGS = -I$(INC_DIR) -Wall -Wextra -std=c11 -O2 -Wl,-z,relro,-z,now -s

# --- 2. Security Hardening Flags (IL FLEX PER LA TESI) ---
# -fstack-protector-all: Attiva i 'Canaries' nello stack per prevenire buffer overflow
# -fPIE -pie: Position Independent Executable (Randomizza indirizzi memoria - ASLR)
# -z noexecstack: Segna lo stack come non eseguibile (Previene Shellcode Injection)
# -D_FORTIFY_SOURCE=2: Attiva controlli a compile-time su buffer overflow (es. strcpy/sprintf)
SEC_FLAGS = -fstack-protector-all -fPIE -pie -z noexecstack -D_FORTIFY_SOURCE=2

# --- 3. Librerie Esterne ---
# Linkiamo OpenSSL (libssl e libcrypto)
LIBS = -lssl -lcrypto

# --- 4. Target Files ---
# Main Node
TARGET = wwyl_node
SRCS = $(SRC_DIR)/wwyl.c $(SRC_DIR)/utils.c $(SRC_DIR)/wwyl_crypto.c $(SRC_DIR)/user.c $(SRC_DIR)/post_state.c $(SRC_DIR)/map.c 

DATA = wwyl_chain.dat

# ==========================================
# Rules
# ==========================================

.PHONY: all clean info

all: info $(TARGET) 

# Compilazione del Nodo Principale
$(TARGET): $(SRCS)
	@echo "[BUILD] Compilazione Nodo Blockchain Sicuro..."
	$(CC) $(CFLAGS) $(SEC_FLAGS) -o $(TARGET) $(SRCS) $(LIBS)
	@echo "✅ $(TARGET) compilato con successo."


# Pulizia
clean:
	@echo "[CLEAN] Rimozione file binari..."
	rm -f $(TARGET) *.o $(DATA)

# Info utile per debug
info:
	@echo "========================================"
	@echo "Compilatore: $(CC)"
	@echo "Security Flags: $(SEC_FLAGS)"
	@echo "OpenSSL Libs: $(LIBS)"
	@echo "========================================"