# WWYL
## Why Would You Lie

![Language](https://img.shields.io/badge/language-C-00599C?style=for-the-badge&logo=c&logoColor=white)
![License](https://img.shields.io/badge/license-MIT-green?style=for-the-badge)
![Status](https://img.shields.io/badge/status-Thesis_Prototype-orange?style=for-the-badge)
![Security](https://img.shields.io/badge/crypto-OpenSSL-red?style=for-the-badge&logo=openssl&logoColor=white)

> "Freedom of speech is not freedom from consequences."
> 
📜 Il Manifesto
In un'era digitale dominata da bot, fake news e like inflazionati, il valore della verità è crollato. I social network attuali sono gratuiti, e per questo motivo, dire bugie non costa nulla.
WWYL è un esperimento di SocialFi e Sicurezza Informatica che inverte questo paradigma.
È un protocollo di social betting decentralizzato dove ogni post è una scommessa finanziaria contro la community.
 * Se il post sopravvive alla critica (Like > Dislike): L'autore guadagna reputazione (Streak) e il protocollo conia nuovi token per lui. I votanti vincono il piatto dei dissenzienti.
 * Se il post viene rifiutato: L'autore perde la puntata e la streak viene azzerata.
Il sistema utilizza la crittografia per rendere economicamente svantaggioso mentire.
🛠 Architettura Tecnica: The Hybrid Ledger
Questo progetto non è un semplice social. È una Blockchain Custom scritta da zero in C.
La scelta del linguaggio C è intenzionale per mostrare la gestione di basso livello della memoria, l'ottimizzazione delle risorse e l'implementazione manuale delle primitive crittografiche.
Il sistema adotta un'architettura ibrida Disk/RAM:
 * Disk (Il Ledger): Una lista concatenata di blocchi immutabili (wwyl_chain.dat) garantisce la persistenza e la storia.
 * RAM (Lo Stato): All'avvio, il nodo rilegge la storia (Event Sourcing) e costruisce in memoria delle Hashmap ottimizzate per l'accesso istantaneo a bilanci, stato dei post e voti.
Core Features
 * Event-Sourcing: Non salviamo lo stato degli utenti su disco, salviamo le transazioni. Lo stato è una proiezione deterministica della storia.
 * Identity ECDSA: Autenticazione basata su crittografia asimmetrica secp256k1. Non esistono password, solo chiavi private.
 * Anti-Replay Protection: Il login utilizza un sistema Challenge-Response basato su timestamp per impedire il riutilizzo di firme intercettate.
 * Memory Safety: Allocatori custom (safe_zalloc) e pulizia rigorosa con Valgrind (0 memory leaks).
🔐 Protocollo Commit-Reveal & Hashing
Il cuore della sicurezza contro il Front-Running (o "sniping" dei voti) è il meccanismo di voto a due fasi. Senza di questo, un utente potrebbe attendere l'ultimo secondo, vedere cosa vota la maggioranza e votare uguale per vincere il piatto senza rischio.
La Matematica del Voto Segreto
In WWYL, un voto non è mai salvato in chiaro durante la fase di apertura. Viene salvato un Hash crittografico.
La formula dell'hash implementata è:
Hash = SHA256( PostID + VoteValue + Salt + PubKey )

 * PostID: Lega il voto a uno specifico contenuto.
 * VoteValue: La scelta dell'utente (1 o -1).
 * Salt: Una stringa segreta ("password") nota solo all'utente.
 * PubKey: Lega il voto all'identità del votante (non ripudiabilità).
Fase 1: COMMIT (Il Voto)
L'utente calcola l'hash locale e lo invia alla blockchain.
 * Stato: Il voto è registrato. I token vengono spesi e messi nel piatto.
 * Visibilità: Nessuno sa cosa hai votato, perché l'hash è irreversibile senza il Salt.
Fase 2: REVEAL (La Rivelazione)
Dopo 24 ore (o al termine del periodo di voto), l'utente invia una transazione ACT_VOTE_REVEAL contenente il Voto in chiaro e il Salt.
Il nodo ricalcola l'hash:
if (SHA256(input) == Stored_Commit_Hash) -> Voto Valido
else -> Tentativo di frode (Hash Mismatch)

Se l'hash coincide, il voto viene conteggiato. Se l'utente prova a cambiare voto o mente sul Salt, la verifica matematica fallisce e il voto è scartato.
🛡️ Security Core: Firma e Blocchi
La sicurezza dell'integrità dei dati si basa sulla firma digitale di ogni blocco.
Block Signing Flow
La sfida principale in C è garantire che i dati firmati siano identici byte-per-byte su ogni macchina, evitando problemi di struct padding.
Per questo, ho implementato una Serializzazione Deterministica:
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
[ 1. SERIALIZZAZIONE ]
"42:1735689600:045A...B2:1:HelloWorld"
(Conversione in stringa raw unica e immutabile)
            |
            v
[ 2. HASHING SHA-256 ] -> Digest (32 bytes)
            |
            v
[ 3. FIRMA ECDSA ] -> OpenSSL 3.0 EVP Interface
            |
            v
[ 4. NORMALIZZAZIONE DER ]
Estrazione manuale dei vettori R e S dalla struttura ASN.1 
per ottenere una firma fissa a 128 byte esadecimali.

Sfide Implementative Risolte
 * OpenSSL 3.0 Moderno: Abbandono delle vecchie API EC_KEY in favore dell'interfaccia EVP (Envelope), gestendo manualmente la costruzione delle chiavi via OSSL_PARAM_BLD.
 * Gestione DER vs Raw: OpenSSL restituisce firme in formato binario ASN.1 a lunghezza variabile. Ho scritto un parser (d2i_ECDSA_SIG) per estrarre i numeri puri R e S e convertirli in una stringa hex a lunghezza fissa, essenziale per la struttura del blocco.
🎮 Game Theory & Economia (Tokenomics)
Il sistema economico è progettato per essere a somma zero tra i partecipanti, con un meccanismo inflattivo controllato solo per i creatori di valore.
1. Pay-to-Act (Skin in the Game)
Ogni azione ha un costo per prevenire lo spam:
 * Postare: Costa 5 Token. Questi token finiscono nel "Piatto" del post.
 * Votare: Costa 2 Token. Anche questi finiscono nel "Piatto".
2. Il Piatto (The Pool)
Il PostState mantiene un accumulatore pull (il piatto).
Piatto = (CostoPost) + (N_Voti * CostoVoto)
3. La Redistribuzione (The Payout)
Alla chiusura del post (Finalize):
 * Si determina la maggioranza (Like vs Dislike).
 * I Vincitori: Chi ha votato con la maggioranza si spartisce l'intero piatto.
 * Gli Sconfitti: Chi ha votato contro perde la puntata.
 * L'Autore: Non vince soldi dal piatto (quelli sono per i curatori), ma...
4. La Streak (Reputazione Esponenziale)
L'incentivo per l'autore è la Streak.
Se il post viene approvato (Like > Dislike):
 * Current_Streak aumenta di 1.
 * Il protocollo conia (minta) nuovi token pari al valore della streak.
   * Esempio: Al 5° post consecutivo di successo, l'autore riceve 5 token appena coniati.
     Se il post viene rifiutato:
 * La Streak torna a 0. Nessun premio.
Nota: Esiste un Global Token Limit (Cap) per prevenire l'inflazione infinita.
💾 Strutture Dati: Hashmap Custom
Per gestire lo stato in RAM senza rallentamenti, ho implementato due Hashmap con gestione delle collisioni via concatenamento (Chaining).
 * StateMap (Utenti): Chiave PubKey -> Valore UserState (Balance, Streak).
 * PostMap (Post): Chiave PostID -> Valore PostState (Likes, Dislikes, Pool, Liste Commit/Reveal).
Le mappe supportano il Resizing Dinamico: quando il carico supera il 75%, la dimensione dei bucket raddoppia e tutti i nodi vengono riallocati (Rehashing) per mantenere l'accesso a O(1).
🚀 Guida all'Uso (CLI)
Il progetto compila un nodo completo interattivo.
Compilazione
make

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
- [x] Gestione delle Streak esponenziali.
- [x] Implementazione del timer 24h per il Commit-Reveal.

### Fase 4: Networking (Prossimamente)
- [ ] Architettura Client-Server su Socket TCP.
- [ ] Protocollo di scambio pacchetti firmati.
- [ ] CLI (Command Line Interface) interattiva per gli utenti.
