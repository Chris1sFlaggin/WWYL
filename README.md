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

Il sistema utilizza la crittografia per rendere **economicamente svantaggioso mentire**.

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

```md
[   STRUCT BLOCK (RAM)  ]
+-----------------------+
| Index:   42           |
| Time:    1735689600   |
| Type:    POST (1)     |
| Sender:  045A...B2    |
| Payload: "Hello World"|
+-----------+-----------+
            |
            v
[        1. SERIALIZZAZIONE DETERMINISTICA      ]
["42:1735689600:045A...B2:1:HelloWorld"         ]
[(Conversione in stringa raw unica e immutabile)]
            |
            v
[ 2. HASHING SHA-256    ]
+-----------------------+
|     SHA256 Digest     |
|    (32 bytes raw)     |
+-----------+-----------+
            |
            v
[3. FIRMA ECDSA (secp256k1)]
+--------------------------+
| Chiave Privata (Wallet)  | 
|[ OpenSSL EVP_DigestSign ]|
+--------------------------+    
             |          
             v
[  4. FIRMA DIGITALE (DER) ]
[Blob binario ASN.1 (R + S)]
             |
             v
[     5. NORMALIZZAZIONE     ]
[Estrazione R (32b) + S (32b)]
[Padding Hex a 64 char l'uno ]
             |
             v
[      6. BLOCCO FIRMATO E VALIDO           ]      
+-------------------------------------------+
| Signature: 7c3b8a... (128 char hex string)|
+-------------------------------------------+

```

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

### Implementazione delle HashMap per la Gestione dello Stato in RAM

Una blockchain pura è inefficiente per le query: leggere il balance di un utente richiederebbe di scorrere **tutti** i blocchi dalla genesi. Per risolvere questo problema, ho implementato un sistema di **State Management in RAM** basato su **Hash Tables custom** per tenere conto dei dati in tempo reale.

#### Architettura delle Mappe di Stato

Il sistema mantiene **due hash map separate** per ottimizzare l'accesso ai dati:

1. **`StateMap`** (Mappa Utenti): Indicizza gli utenti per chiave pubblica
2. **`PostMap`** (Mappa Post): Indicizza i post per Block ID

Entrambe utilizzano la strategia di **Chaining** per gestire le collisioni.

#### Gestione Dinamica della Memoria (Resize Automatico)

Per mantenere basse le collisioni, la hash map si **ridimensiona automaticamente** quando il **load factor** supera il 75%:

```c
void state_resize() {
    int old_size = world_state.size;
    int new_size = old_size * 2;  // Raddoppio
    
    StateNode **new_buckets = (StateNode **)safe_zalloc(
        new_size * sizeof(StateNode *)
    );

    // Rehashing di tutti gli elementi
    for (int i = 0; i < old_size; i++) {
        StateNode *curr = world_state.buckets[i];
        while (curr != NULL) {
            StateNode *next = curr->next;
            unsigned long new_index = hash_djb2(
                curr->wallet_address, new_size
            );
            // Inserimento in testa nel nuovo bucket
            curr->next = new_buckets[new_index];
            new_buckets[new_index] = curr;
            curr = next;
        }
    }

    free(world_state.buckets);
    world_state.buckets = new_buckets;
    world_state.size = new_size;
}
```

```C
void post_index_resize() {
    int old_size = global_post_index.size;
    PostStateNode **old_buckets = global_post_index.buckets;
    
    // Double the size
    global_post_index.size = old_size * 2;
    global_post_index.buckets = (PostStateNode**)safe_zalloc(global_post_index.size * sizeof(PostStateNode*));
    global_post_index.count = 0; // Will be recounted during rehashing
    
    printf("[INDEX] Resizing hashmap from %d to %d buckets...\n", old_size, global_post_index.size);
    
    // Rehash all existing entries
    for (int i = 0; i < old_size; i++) {
        PostStateNode *curr = old_buckets[i];
        while (curr) {
            PostStateNode *next = curr->next;
            
            // Recalculate hash with new size
            unsigned long new_idx = hash_int(curr->post_id, global_post_index.size);
            
            // Insert at new position
            curr->next = global_post_index.buckets[new_idx];
            global_post_index.buckets[new_idx] = curr;
            global_post_index.count++;
            
            curr = next;
        }
    }
    
    // Free old bucket array (nodes were moved, not freed)
    free(old_buckets);
    printf("[INDEX] Resize complete. %d posts rehashed.\n", global_post_index.count);
}
```

#### Ricostruzione dello Stato dal Ledger (Event Sourcing)

All'avvio del nodo, le hash map vengono **ricostruite** scorrendo l'intera blockchain. Questo pattern è noto come **Event Sourcing** (`rebuild_state_from_chain()`)

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

### Fase 2: Il Ledger Core (✅ Completata)
- [x] Implementazione del Blocco Genesi.
- [x] Logica di concatenazione blocchi (Hash Chain).
- [x] Validazione dell'integrità della catena.
- [x] Serializzazione dei dati per lo storage su file.

### Fase 3: Logica di Gioco (🚧 In Corso)
- [x] Motore di calcolo dello Stato (Replay del Ledger in RAM).
- [ ] Gestione delle Streak esponenziali.
- [ ] Implementazione del timer 24h per il Commit-Reveal.

### Fase 4: Networking (Prossimamente)
- [ ] Architettura Client-Server su Socket TCP.
- [ ] Protocollo di scambio pacchetti firmati.
- [ ] CLI (Command Line Interface) interattiva per gli utenti.
