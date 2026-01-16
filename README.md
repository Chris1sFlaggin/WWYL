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
* Se il post sopravvive alla critica (Like > Dislike): Guadagni reputazione esponenziale (Streak) e token.
* Se il post viene rifiutato: Perdi la scommessa e la tua puntata viene distribuita a chi ti ha criticato.

Il sistema utilizza la crittografia per rendere **matematicamente svantaggioso mentire**.

## 🛠 Architettura Tecnica
Questo progetto non è un semplice social. È una **Blockchain Custom scritta da zero in C**.
La scelta del linguaggio C è intenzionale: per garantire la massima sicurezza e performance, è necessario avere il controllo totale sulla memoria e sulla gestione crittografica, senza astrazioni di alto livello.

### Core Features
* **Blockchain Event-Sourcing:** Non salviamo lo stato, salviamo la storia. Ogni azione (Post, Voto, Follow) è un blocco immutabile crittograficamente legato al precedente.
* **Identity senza Password:** Autenticazione basata su crittografia asimmetrica **ECDSA (secp256k1)**. La tua chiave privata è il tuo account.
* **Anti-Front-Running (Commit-Reveal):** Per impedire lo "sniping" dei voti all'ultimo secondo, utilizziamo uno schema a due fasi:
    1.  **Commit:** L'utente invia l'hash del voto + un segreto (Salt). Il voto è registrato ma illeggibile.
    2.  **Reveal:** A fine timer, l'utente svela la chiave.
* **Memory Safety:** Gestione manuale della memoria e mitigazione delle eventuali vulnerabilità del codice in C.

## 🗺 Roadmap di Sviluppo

### Fase 1: Primitive Crittografiche (✅ Completata)
- [x] Implementazione SHA256 per l'hashing dei blocchi.
- [x] Implementazione firme digitali ECDSA (curva Bitcoin).
- [x] Gestione sicura delle stringhe e padding esadecimale.
- [x] Definizione delle Struct di base (`wwyl.h`).

### Fase 2: Il Ledger Core (🚧 In Corso)
- [ ] Implementazione del Blocco Genesi.
- [ ] Logica di concatenazione blocchi (Hash Chain).
- [ ] Validazione dell'integrità della catena.
- [ ] Serializzazione dei dati per lo storage su file.

### Fase 3: Logica di Gioco (Prossimamente)
- [ ] Motore di calcolo dello Stato (Replay del Ledger in RAM).
- [ ] Gestione delle Streak esponenziali.
- [ ] Implementazione del timer 24h per il Commit-Reveal.

### Fase 4: Networking (Futuro)
- [ ] Architettura Client-Server su Socket TCP.
- [ ] Protocollo di scambio pacchetti firmati.
- [ ] CLI (Command Line Interface) interattiva per gli utenti.
