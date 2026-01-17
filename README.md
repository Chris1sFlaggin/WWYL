# WWYL
## Why Would You Lie

![Language](https://img.shields.io/badge/language-C-00599C?style=for-the-badge&logo=c&logoColor=white)
![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)
![Status](https://img.shields.io/badge/status-Thesis_Prototype-orange?style=for-the-badge)
![Security](https://img.shields.io/badge/crypto-OpenSSL-red?style=for-the-badge&logo=openssl&logoColor=white)

### A Game-Theoretic Anarchist Decentralized Social Protocol 
#### Progetto sviluppato per conseguite la mia tesi universitaria dal possibile titolo "Sicurezza delle transazioni in ambito Blockchain: implementazione in C del protocollo Commit-Reveal contro il Front-Runnin".

> *"Freedom of speech is not freedom from consequences."*

## 📜 Il Manifesto
In un'era digitale dominata da bot, fake news e like inflazionati, il valore della verità è crollato. I social network attuali sono gratuiti, e per questo motivo, dire bugie non costa nulla.

**WWYL** è un esperimento di **SocialFi** e **Sicurezza Informatica** che inverte questo paradigma.
È un protocollo di *social betting* decentralizzato dove ogni post è una scommessa finanziaria contro la community.
* Se il post sopravvive alla critica (Like > Dislike): Entri in una streak in cui guadagni sempre più token.
* Se il post viene rifiutato: Perdi la scommessa e la tua puntata viene distribuita a chi ti ha criticato.

Il sistema utilizza la crittografia per rendere **matematicamente svantaggioso mentire**.

## 🛠 Architettura Tecnica
Questo progetto non è un semplice social. È una **Blockchain Custom scritta da zero in C**.
La scelta del linguaggio C è intenzionale al fine di mostrare le mitigazioni imparate in C e sfruttare al massimo la memoria.

### Core Features
* **Blockchain Event-Sourcing:** Non salviamo lo stato, salviamo la storia. Ogni azione (Post, Voto, Follow) è un blocco immutabile crittograficamente legato al precedente.
* **Identity senza Password:** Autenticazione basata su crittografia asimmetrica **ECDSA (secp256k1)**. La tua chiave privata è il tuo account.
* **Anti-Front-Running (Commit-Reveal):** Per impedire lo "sniping" dei voti all'ultimo secondo, utilizziamo uno schema a due fasi:
    1.  **Commit:** L'utente invia l'hash del voto + un segreto (Salt). Il voto è registrato ma illeggibile.
    2.  **Reveal:** A fine timer, l'utente svela la chiave.
* **Memory Safety:** Gestione manuale della memoria e mitigazione delle eventuali vulnerabilità del codice in C.

### 🛡️ Architettura Crittografica & Security Core
La sicurezza e l'integrità di WWYL non si basano sulla fiducia, ma su prove crittografiche verificabili. Ho progettato il modulo wwyl_crypto (basato su OpenSSL 3.0+) per aderire agli standard più moderni.
La scelta dell'algoritmo è ricaduta su ECDSA (Elliptic Curve Digital Signature Algorithm) sulla curva secp256k1, lo stesso standard industriale utilizzato da Bitcoin ed Ethereum per la sua efficienza e robustezza.
Il Flusso di Firma del Blocco (Block Signing Flow)
La sfida principale in una blockchain C non è solo firmare, ma garantire che ciò che viene firmato sia identico per ogni nodo della rete. Un solo byte di differenza (come il padding di una struct) cambierebbe l'hash e invaliderebbe la firma.
Ho implementato un processo di Serializzazione Deterministica e un flusso di firma a più stadi per garantire la consistenza.
%%
    A[Struct Block in RAM] -->|Serializzazione Deterministica| B(Raw String Buffer);
    B -->|SHA-256 Hashing| C(Digest Hash 32-byte);
    C -->|Input per Firma| D{OpenSSL EVP Signing};
    Key[PrivateKey secp256k1] --> D;
    D -->|Output| E(Firma DER ASN.1 Binaria);
    E -->|Decodifica & Estrazione| F[Valori R e S grezzi];
    F -->|Padding Hex a 64 byte| G[Stringa Firma Finale 128-char];
    G -->|Inserimento| H[Block struct field: signature];

    style B fill:#f9f,stroke:#333,stroke-width:2px,color:black
    style E fill:#ff9,stroke:#333,stroke-width:2px,color:black
    style G fill:#9f9,stroke:#333,stroke-width:2px,color:black

Visualizzazione ASCII del Flusso Dati
[ STRUCT BLOCK (RAM) ]
+--------------------+
| Index: 0           |
| Time:  1735689600  |  
| Type:  POST (1)    |
| Prev:  0000...00   |
| Sender: 045A...B2  |
| Payload: "Hello"   |
+---------+----------+
          |
          v
   [ 1. SERIALIZZAZIONE DETERMINISTICA ]
   "0:1735689600:0000...00:045A...B2:1:Hello"
          |
          | (SHA-256)
          v
   [ 2. HASH DIGEST (32 bytes) ] 
   [ a8fd3c...9z ]
          |
          | (Firma ECDSA con PrivateKey via EVP)
          v
   [ 3. FIRMA DER BINARIA (Variabile ~70 bytes) ]
   [ 0x30 0x44 0x02 0x20 (R...) 0x02 0x20 (S...) ] <--- Formato ASN.1 complesso
          |
          | (Decodifica, Estrazione R/S, Hex Padding)
          v
   [ 4. FIRMA FINALE HEX (Fissa 128 chars) ]
   R: [7c3b...0a] (64 chars)
   S: [1f9d...4b] (64 chars)
   --> "7c3b...0a1f9d...4b"

#### Sfide Implementative
Durante lo sviluppo del core crittografico, ho affrontato due sfide tecniche principali che distinguono questa implementazione da esempi didattici standard.
1. Adozione di OpenSSL 3.0 EVP (Modernizzazione)
La maggior parte della documentazione online utilizza le funzioni di basso livello EC_KEY, ora deprecate in OpenSSL 3.0. Ho scelto di utilizzare l'interfaccia moderna ad alto livello EVP (Envelope).
 * La Sfida: Le API EVP sono astratte e verbose. Non si manipolano direttamente i parametri della curva.
 * La Soluzione: Ho utilizzato OSSL_PARAM_BLD per costruire programmaticamente le chiavi dai dati grezzi esadecimali.
2. Gestione del Formato DER vs R/S Raw
OpenSSL, per standard, restituisce le firme nel formato binario ASN.1/DER. Questo formato è a lunghezza variabile e complesso da parsare, inadatto per essere memorizzato in un campo di testo a lunghezza fissa in una blockchain.
 * La Sfida: Estrarre i valori matematici puri della firma (i componenti crittografici R e S) dal blob binario DER.
 * La Soluzione: Ho implementato un flusso di decodifica manuale (d2i_ECDSA_SIG) post-firma per estrarre i Big Number R e S. Successivamente, li converto in esadecimale e applico un padding rigoroso per garantire che la firma finale sia sempre una stringa fissa di 128 caratteri (64 per R + 64 per S), essenziale per la prevedibilità della struttura dati.

#### Creazione del blocco genesi con chiavi hardcodate da `keygen.c`
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wwyl_crypto.h" 

int main() {
    char my_priv_key[128];
    char my_pub_key[256];

    printf("[*] Generazione chiavi Genesi usando la lib wwyl_crypto\n");
    generate_keypair(my_priv_key, my_pub_key);

    printf("\n=== COPIA E INCOLLA IN wwyl_config.h ===\n\n");
    
    printf("// Chiavi Genesi generate il %s\n", __DATE__);
    printf("#define GENESIS_PRIV_KEY \"%s\"\n", my_priv_key);
    printf("#define GENESIS_PUB_KEY  \"%s\"\n", my_pub_key);

    return 0;
}
```

## 🗺 Roadmap di Sviluppo

### Fase 1: Primitive Crittografiche (✅ Completata)
- [x] Implementazione SHA256 per l'hashing dei blocchi.
- [x] Implementazione firme digitali ECDSA (curva Bitcoin).
- [x] Gestione sicura delle stringhe e padding esadecimale.
- [x] Definizione delle Struct di base (`wwyl.h`).

### Fase 2: Il Ledger Core (🚧 In Corso)
- [x] Implementazione del Blocco Genesi.
- [ ] Logica di concatenazione blocchi (Hash Chain).
- [ ] Validazione dell'integrità della catena.
- [x] Serializzazione dei dati per lo storage su file.

### Fase 3: Logica di Gioco (Prossimamente)
- [ ] Motore di calcolo dello Stato (Replay del Ledger in RAM).
- [ ] Gestione delle Streak esponenziali.
- [ ] Implementazione del timer 24h per il Commit-Reveal.

### Fase 4: Networking (Futuro)
- [ ] Architettura Client-Server su Socket TCP.
- [ ] Protocollo di scambio pacchetti firmati.
- [ ] CLI (Command Line Interface) interattiva per gli utenti.
