<div id="top">

<div align="center">

<img src="images/Gemini_Generated_Image_zh7u03zh7u03zh7u.png" width="30%" style="position: relative; top: 0; right: 0;" alt="Project Logo"/>

# WWYL

<em>Why Would You Lie: A Game-Theoretic Anarchist Decentralized Social Protocol.</em>

<img src="https://img.shields.io/badge/language-C11-00599C?style=for-the-badge&logo=c&logoColor=white" alt="Language">
<img src="https://img.shields.io/github/license/chris1sflaggin/wwyl?style=flat-square&logo=opensourceinitiative&logoColor=white&color=FF4B4B" alt="license">
<img src="https://img.shields.io/badge/status-Thesis_Prototype-orange?style=for-the-badge" alt="Status">
<img src="https://img.shields.io/badge/crypto-OpenSSL_3.0-red?style=for-the-badge&logo=openssl&logoColor=white" alt="Security">

<em>Built with the tools and technologies:</em>

<img src="https://img.shields.io/badge/C-A8B9CC.svg?style=flat-square&logo=C&logoColor=black" alt="C">
<img src="https://img.shields.io/badge/OpenSSL-7213EA.svg?style=flat-square&logo=openssl&logoColor=white" alt="OpenSSL">
<img src="https://img.shields.io/badge/GNU_Make-3D6117.svg?style=flat-square&logo=gnu&logoColor=white" alt="Make">

</div>
<br>

> **"Freedom of speech is not freedom from consequences."**

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Project Structure](#project-structure)
    - [Project Index](#project-index)
- [Getting Started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Installation](#installation)
    - [Usage](#usage)
    - [Testing](#testing)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

In un mondo dove il valore della verità nei social network è crollato serve un restauro della nostra percezione di essi.

**WWYL** è un esperimento di **SocialFi** e **Sicurezza Informatica** che ci prova invertendo questo paradigma odierno rendenndo **eonomicamente svantaggioso mentire** attraverso delle meccaniche di tokenizzazione delle azioni social di consuetudine.

Sarà la community a scegliere quali post rimarranno on-line e questo è garantito dalla totale decentralizzazione permessa dell'architettura **blockchain**. 

### Più tecnicamente

È un protocollo di *social betting* decentralizzato basato su una **Blockchain scritta da zero in C**, dove ogni post è una scommessa finanziaria contro la community.

---

## Features

* **Blockchain Ibrida (Disk/RAM)**: Ledger immutabile su disco per la persistenza (`wwyl_chain.dat`) e Stato mutabile in RAM (Hashmap custom) per l'accesso O(1) ai dati.
* **🔐 Crittografia Reale**: Autenticazione identity-based tramite firme digitali **ECDSA (secp256k1)** e integrità dei blocchi via **SHA256**. Nessuna password salvata.
* **🛑 Anti-Front-Running**: Sistema di voto a due fasi (**Commit-Reveal Scheme**) per impedire lo "sniping" dei voti e l'effetto gregge. I voti sono criptati con un *Salt* per 24h.
* **💰 Tokenomics**:
    * **Welcome-Bonus**: 20 token garantiti.
    * **Pay-to-Act**: Postare costa 4 token, Votare costa 2 token.
    * **The Pool**: I token spesi formano un piatto che viene ridistribuito alla maggioranza vincente.
    * **Streak Minting**: L'autore guadagna reputazione e conia sempre più token se mantiene una streak di post approvati.
* **🧠 Memory Safety**: Gestione rigorosa della memoria con allocatori custom (`safe_zalloc`), validata con **Valgrind** (0 memory leaks).
* **⚡ Anti-Replay Attack**: Login protetto da Challenge-Response temporale.

---

## Project Structure

```sh
└── wwyl/
    ├── LICENSE
    ├── Makefile
    ├── README.md
    ├── lib
    │   ├── map.h
    │   ├── post_state.h
    │   ├── user.h
    │   ├── utils.h
    │   ├── wwyl.h
    │   ├── wwyl_config.template.h
    │   └── wwyl_crypto.h
    ├── src
    │   ├── map.c
    │   ├── post_state.c
    │   ├── user.c
    │   ├── utils.c
    │   ├── wwyl.c
    │   └── wwyl_crypto.c
    └── wwyl.wallet

```

### Project Index

<details open>
<summary><b><code>WWYL/</code></b></summary>
<details>
<summary><b>**root**</b></summary>
<blockquote>
<div class='directory-path' style='padding: 8px 0; color: #666;'>
<code><b>⦿ **root**</b></code>
<table style='width: 100%; border-collapse: collapse;'>
<thead>
<tr style='background-color: #f8f9fa;'>
<th style='width: 30%; text-align: left; padding: 8px;'>File Name</th>
<th style='text-align: left; padding: 8px;'>Summary</th>
</tr>
</thead>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./LICENSE'>LICENSE</a></b></td>
<td style='padding: 8px;'>licenza MIT dell'autore del progetto.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./Makefile'>Makefile</a></b></td>
<td style='padding: 8px;'>Script di build ottimizzato con flag di sicurezza (Stack Protector, PIE, Fortify Source).</td>
</tr>
</table>
</blockquote>
</details>
<details>
<summary><b>src</b></summary>
<blockquote>
<div class='directory-path' style='padding: 8px 0; color: #666;'>
<code><b>⦿ src</b></code>
<table style='width: 100%; border-collapse: collapse;'>
<thead>
<tr style='background-color: #f8f9fa;'>
<th style='width: 30%; text-align: left; padding: 8px;'>File Name</th>
<th style='text-align: left; padding: 8px;'>Summary</th>
</tr>
</thead>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./src/wwyl_crypto.c'>wwyl_crypto.c</a></b></td>
<td style='padding: 8px;'>Wrapper per OpenSSL 3.0. Gestisce SHA256, generazione chiavi ECDSA, firme e verifiche. Implementa il parsing manuale DER.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./src/wwyl.c'>wwyl.c</a></b></td>
<td style='padding: 8px;'>Entry point. Gestisce la CLI interattiva, l'I/O della blockchain su file e il loop principale.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./src/utils.c'>utils.c</a></b></td>
<td style='padding: 8px;'>Funzioni di utilità, incluso l'allocatore sicuro <code>safe_zalloc</code> e la gestione errori.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./src/user.c'>user.c</a></b></td>
<td style='padding: 8px;'>Core logic per gli Utenti. Gestisce mining dei blocchi, economia (token), login sicuro e calcolo delle ricompense (Streak).</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./src/post_state.c'>post_state.c</a></b></td>
<td style='padding: 8px;'>Gestisce lo stato dei Post in RAM. Mantiene le liste di Commit/Reveal e verifica la validità dei voti.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./src/map.c'>map.c</a></b></td>
<td style='padding: 8px;'>Implementazione generica di Hashmap con resizing dinamico e chaining per la gestione delle collisioni.</td>
</tr>
</table>
</blockquote>
</details>
<details>
<summary><b>lib</b></summary>
<blockquote>
<div class='directory-path' style='padding: 8px 0; color: #666;'>
<code><b>⦿ lib</b></code>
<table style='width: 100%; border-collapse: collapse;'>
<thead>
<tr style='background-color: #f8f9fa;'>
<th style='width: 30%; text-align: left; padding: 8px;'>File Name</th>
<th style='text-align: left; padding: 8px;'>Summary</th>
</tr>
</thead>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./lib/wwyl_crypto.h'>wwyl_crypto.h</a></b></td>
<td style='padding: 8px;'>Interfaccia per le librerie crittografiche.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./libsrc/wwyl.h'>wwyl.h</a></b></td>
<td style='padding: 8px;'>Header principale.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./lib/utils.h'>utils.c</a></b></td>
<td style='padding: 8px;'>Interfaccia per funzioni di utilità.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./lib/user.h'>user.c</a></b></td>
<td style='padding: 8px;'>Interfaccia core logic utenti.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./lib/post_state.h'>post_state.c</a></b></td>
<td style='padding: 8px;'>Interfaccia core logic post.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./lib/map.h'>map.c</a></b></td>
<td style='padding: 8px;'>Interfaccia hashmap.</td>
</tr>
<tr style='border-bottom: 1px solid #eee;'>
<td style='padding: 8px;'><b><a href='./lib/wwyl_config.template.h'>.h</a></b></td>
<td style='padding: 8px;'>Prime chiavi create.</td>
</tr>
</table>
</blockquote>
</details>
</details>

---

## Getting Started

### Prerequisites

Questo progetto richiede un ambiente Linux/Unix con le seguenti dipendenze:

* **Compiler:** GCC (con supporto C11)
* **Build Tool:** GNU Make
* **Crypto Library:** OpenSSL Development Headers (`libssl-dev`)

### Installation

1. **Clona la repository:**
```sh
❯ git clone https://github.com/chris1sflaggin/wwyl
❯ cd wwyl

```


2. **Installa le dipendenze (Ubuntu/Debian):**
```sh
❯ sudo apt update && sudo apt install build-essential libssl-dev valgrind

```


3. **Compila il progetto:**
```sh
❯ make

```


*Questo genererà l'eseguibile `wwyl_node`.*

### Usage

Avvia il nodo per entrare nella CLI interattiva:

```sh
❯ ./wwyl_node

```

**Comandi Principali della CLI:**

* `[1] 🔑 Keygen`: Genera una nuova identità locale (Alice, Bob...).
* `[2] 📝 Register`: Registra l'identità sulla blockchain (richiede 0 token, offre Welcome Bonus).
* `[3] 👤 Login`: Cambia utente attivo tramite verifica crittografica.
* `[4] 📢 Post`: Pubblica contenuto (Costo: 5 Token).
* `[5] 🗳️ Vote (Commit)`: Invia un voto segreto (Hash + Salt).
* `[6] 🔓 Reveal`: Svela il voto dopo il periodo di lock.
* `[7] 🏁 Finalize`: Chiude il post e distribuisce il piatto ai vincitori.
* `[9] ⏰ Time Travel`: (Debug) Simula il passaggio del tempo per testare il meccanismo 24h.

### Testing

Per verificare la sicurezza della memoria e l'assenza di leak, esegui il nodo tramite Valgrind:

```sh
❯ valgrind --leak-check=full --show-leak-kinds=all ./wwyl_node

```

---

## Roadmap

* [x] **Core Architecture**: Blockchain ibrida Disk/RAM, Hashing SHA256.
* [x] **Cryptography**: Firme ECDSA (secp256k1), gestione chiavi, serializzazione deterministica.
* [x] **Data Structures**: Hashmap custom in RAM con resizing dinamico.
* [x] **Game Theory Logic**: Protocollo Commit-Reveal, Tokenomics, Streak Minting.
* [x] **Security**: Anti-Replay Login, Sanitizzazione Input, Memory Safety (Valgrind Clean).
* [ ] **Networking**: Architettura P2P Client-Server su Socket TCP.

---

## Contributing

* 🐛 **[Report Issues**](https://github.com/chris1sflaggin/wwyl/issues): Submit bugs found or log feature requests for the `wwyl` project.

<details closed>
<summary>Contributing Guidelines</summary>

1. **Fork the Repository**: Start by forking the project repository to your github account.
2. **Clone Locally**: Clone the forked repository to your local machine using a git client.
```sh
git clone https://github.com/chris1sflaggin/wwyl

```


3. **Create a New Branch**: Always work on a new branch, giving it a descriptive name.
```sh
git checkout -b new-feature-x

```


4. **Make Your Changes**: Develop and test your changes locally.
5. **Commit Your Changes**: Commit with a clear message describing your updates.
```sh
git commit -m 'Implemented new feature x.'

```


6. **Push to github**: Push the changes to your forked repository.
```sh
git push origin new-feature-x

```


7. **Submit a Pull Request**: Create a PR against the original project repository. Clearly describe the changes and their motivations.

</details>

---

## License

Wwyl is protected under the [MIT](https://www.google.com/search?q=https://choosealicense.com/licenses/mit/) License. For more details, refer to the [LICENSE](https://www.google.com/search?q=LICENSE) file.

---

<div align="right">

</div>
