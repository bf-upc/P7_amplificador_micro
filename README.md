# P7 — Pass-through Micròfon → Altaveu (ESP32-S3)

> Pràctica 7 · Sistemes Encastats · Universitat Politècnica de Catalunya (UPC)

## Descripció

Aquesta pràctica implementa un **sistema de pass-through d'àudio en temps real**: el micròfon INMP441 captura so via I2S, l'ESP32-S3 el processa i el reenvía immediatament a l'altaveu MAX98357A a través d'un segon port I2S. Tot el pipeline opera a nivell de driver IDF (`driver/i2s.h`), sense cap biblioteca d'alt nivell.

La novetat respecte a la P7 anterior és la **captura activa d'àudio**: aquí hi ha dos perifèrics I2S funcionant simultàniament, un en mode RX (micròfon) i un en mode TX (altaveu).

## Objectius

- Configurar dos perifèrics I2S independents en el mateix ESP32-S3.
- Llegir mostres de 32 bits del micròfon INMP441 i convertir-les a 16 bits per a la sortida.
- Implementar un pipeline de captura→processament→reproducció en temps real.
- Gestionar buffers DMA i evitar overflow amb clamp de senyal.

## Maquinari necessari

| Component | Descripció |
|---|---|
| ESP32-S3-DevKitC-1 | Microcontrolador principal |
| INMP441 | Micròfon digital I2S (32 bits) |
| MAX98357A | Amplificador I2S (16 bits) |
| Altaveu (4–8 Ω) | Transductor de sortida |
| Cables Dupont | Connexions |

## Connexions de hardware

### Micròfon INMP441 — I2S Port 1 (RX)

| ESP32-S3 GPIO | INMP441 Pin | Descripció |
|---|---|---|
| **GPIO 6** | SCK | Rellotge de bits (BCLK) |
| **GPIO 7** | WS | Rellotge de canal (word select) |
| **GPIO 8** | SD | Dades d'àudio (entrada) |
| 3.3 V | VDD | Alimentació |
| GND | GND | Massa |
| GND | L/R | Canal esquerre (forçat a GND) |

### Altaveu MAX98357A — I2S Port 0 (TX)

| ESP32-S3 GPIO | MAX98357A Pin | Descripció |
|---|---|---|
| **GPIO 1** | BCLK | Rellotge de bits |
| **GPIO 2** | LRC | Rellotge de canal (word select) |
| **GPIO 4** | DIN | Dades d'àudio (sortida) |
| 3.3 V | VDD | Alimentació |
| GND | GND | Massa |

## Estructura del projecte

```
P7_amplificador_micro/
├── include/          # Fitxers de capçalera
├── lib/              # Biblioteques locals
├── src/
│   └── main.cpp      # Codi font principal
├── test/             # Tests unitaris
└── platformio.ini    # Configuració de PlatformIO
```

## Configuració — `platformio.ini`

```ini
[env:esp32-s3-devkitc-1]
platform      = espressif32
board         = esp32-s3-devkitc-1
framework     = arduino
monitor_speed = 115200
```

> Aquest projecte no requereix cap biblioteca externa: fa servir directament el driver IDF `driver/i2s.h` inclòs amb el framework Arduino per a ESP32.

## Instal·lació i compilació

```bash
# 1. Clona el repositori
git clone https://github.com/bf-upc/P7_amplificador_micro.git
cd P7_amplificador_micro

# 2. Compila i carrega el firmware
pio run --target upload

# 3. Obre el monitor sèrie
pio device monitor --baud 115200
```

## Com funciona

### Pipeline d'àudio

```
INMP441 (32 bit)
      │  I2S Port 1 (RX) · GPIO 6/7/8
      ▼
  raw32[512]  ←── i2s_read()
      │
      │  Conversió 32→16 bit + clamp
      │  val = raw32[i] >> 11
      ▼
  out16[512]
      │
      │  i2s_write()
      ▼
  I2S Port 0 (TX) · GPIO 1/2/4
      │
      ▼
MAX98357A → Altaveu
```

### 1. Configuració del micròfon (`setupMic`)
- Port I2S 1 en mode **MASTER RX**.
- El INMP441 envia mostres de **32 bits**, canal esquerre (L/R connectat a GND → `I2S_CHANNEL_FMT_ONLY_LEFT`).
- 4 buffers DMA de 512 mostres cadascun.

### 2. Configuració de l'altaveu (`setupSpeaker`)
- Port I2S 0 en mode **MASTER TX**.
- Sortida de **16 bits**, canal esquerre.
- `tx_desc_auto_clear = true` per evitar soroll quan no hi ha dades.

### 3. Bucle de processament (`loop`)
1. `i2s_read()` omple `raw32[]` amb mostres de 32 bits.
2. Cada mostra es desplaça 11 bits a la dreta (`>> 11`) per extreure els bits significatius del INMP441 i ajustar el guany (×4 implícit).
3. S'aplica **clamp** entre −32768 i 32767 per evitar overflow.
4. `i2s_write()` envia el buffer `out16[]` de 16 bits a l'altaveu.

## Paràmetres configurables

| Constant | Valor | Descripció |
|---|---|---|
| `SAMPLE_RATE` | `16000` Hz | Freqüència de mostratge |
| `BUFFER_SIZE` | `512` mostres | Mida dels buffers DMA |
| `I2S_MIC_NUM` | `I2S_NUM_1` | Port I2S del micròfon |
| `I2S_SPK_NUM` | `I2S_NUM_0` | Port I2S de l'altaveu |
| Desplaçament | `>> 11` | Conversió 32→16 bit + guany ×4 |

## Diferències respecte a P7_amplificador

| Característica | P7_amplificador | P7_amplificador_micro |
|---|---|---|
| Font d'àudio | Fitxer AAC en PROGMEM | Micròfon INMP441 en temps real |
| Biblioteca | ESP8266Audio | Driver IDF natiu (`driver/i2s.h`) |
| Ports I2S | 1 (TX) | 2 (RX + TX simultanis) |
| Format entrada | AAC decodificat | PCM 32 bits brut |
| Latència | Bufferitzada | Mínima (pass-through) |

## Autors

Desenvolupat per l'equip **bf-upc** com a part de les pràctiques de l'assignatura de Sistemes Encastats a la UPC.

## Llicència

Aquest projecte té finalitat acadèmica. Per a qualsevol altre ús, consulta amb els autors.