# ∞ Infinity

**Firmware en español (España) para los lectores de tinta electrónica Xteink X4 y X3.**

Infinity nace sobre los hombros de dos proyectos de código abierto: el firmware [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader), que aporta el motor de lectura EPUB y la base del sistema, y [CrossPet](https://github.com/trilwu/crosspet), la adaptación de la que Infinity toma el relevo. A partir de aquí, todo lo que leas es Infinity.

---

## 🧠 Filosofía

La mayoría de los firmwares de e-reader se conforman con *dejarte* leer. Infinity quiere que **no puedas parar**.

Leer es un hábito, y los hábitos se construyen con refuerzo. Por eso Infinity trata la lectura como lo que es —el mejor entretenimiento del mundo— y le añade una capa de **gamificación** pensada para engancharte: metas diarias, rachas que te da rabia romper, recompensas que aparecen cuando menos lo esperas y logros que desbloquear. La idea es sencilla: convertir "debería leer un rato" en "necesito mantener mi racha".

Y cuando cierras el libro, el dispositivo no se queda mudo: una colección de **minijuegos** cuidadosamente elegidos te espera para esos ratos muertos, todos jugables con los pocos botones del aparato.

---

## 📖 Experiencia de lectura

Todo lo que esperas de un buen lector, pulido y en castellano:

- 📚 **EPUB 2/3** con imágenes, estilos CSS y separación silábica multilingüe
- ⚡ **XTC** (formato pre-renderizado, soporta libros de >2 GB) y **TXT / Markdown**
- 🔤 **3 fuentes integradas** (Bookerly, Lexend, Bokerlam) + **fuentes propias desde la tarjeta SD**
- 🔠 **4 tamaños de letra** con renderizado en escala de grises suavizado (anti-aliasing)
- 🚀 **Caché de fuentes** para pasar página al instante + pre-indexado silencioso del siguiente capítulo
- 🔖 **Marcadores** con pulsación larga, estadísticas de lectura y rachas
- 🌙 **Modo oscuro**, 5 temas de interfaz y barra de estado totalmente configurable
- 🖼️ **9 modos de pantalla en reposo** — reloj, estadísticas, portada, imágenes propias y más
- 🎯 **Modo enfoque** — esconde todos los extras: solo tú y tus libros
- 🔄 **Sincronización KOReader** para continuar tu progreso entre dispositivos
- 🔀 **4 orientaciones de pantalla** con botones reasignables
- 🎛️ **9 acciones del botón de encendido** por pulsación simple/doble/triple

---

## 🏆 Gamificación

El corazón adictivo de Infinity. Todo opcional y desactivable, pero difícil de abandonar una vez empiezas:

- 🎯 **Meta diaria de lectura** — tú pones los minutos; Infinity te empuja a cumplirlos
- 🔥 **Racha de días leídos** — con **fichas de congelación** que perdonan un día flojo sin romper la racha
- 🎁 **Recompensas sorpresa** — refuerzo de proporción variable: a veces completar la meta te regala una ficha, a veces no. Justo lo que engancha
- 📜 **Misiones diarias** — retos rotativos que cambian cada día
- 🏅 **Logros** — insignias que se desbloquean por hitos de lectura
- 📊 **Récords e historial semanal** — para ver cuánto has avanzado

---

## 🎮 Juegos

Para los ratos muertos. No son un cajón de sastre: cada juego está **elegido porque se controla bien con la cruceta y los pocos botones del dispositivo**, sin necesidad de puntero ni pantalla táctil. Cada uno guarda su propio récord.

- 🔢 **2048** — desliza y fusiona fichas con el mismo número hasta llegar a 2048. Un clásico de lógica adictivo, perfecto para controles direccionales.
- 🧩 **Puzzle deslizante** — el rompecabezas de fichas numeradas: deslízalas por el hueco vacío hasta ordenarlas.
- 💡 **Apagón** (Lights Out) — pulsa una casilla y cambias su estado y el de sus vecinas. Objetivo: apagar todas las luces en el menor número de movimientos.
- ♟️ **Senku** (solitario de fichas) — salta unas fichas sobre otras para eliminarlas. La partida perfecta deja una sola ficha en el tablero.
- 🌀 **Laberinto** — encuentra la salida recorriendo laberintos generados por niveles, contando tus pasos.
- 🚗 **Desatasco** (estilo Rush Hour) — desliza los bloques que estorban para liberar el coche atascado, en los menos movimientos posibles.

---

## ⏰ Reloj fiable por NTP

Una de las batallas técnicas de las que más orgulloso está Infinity, y un buen ejemplo de lo cuidado que está el firmware por dentro.

**El problema:** el Xteink X4 **no tiene reloj de tiempo real con batería (RTC)**. Al despertar del sueño profundo, la hora se restaura desde un oscilador interno impreciso que va acumulando deriva. Resultado: el dispositivo podía marcar las 23:55 cuando en realidad eran las 09:00.

**La solución:** Infinity resincroniza la hora de forma oportunista. Al despertar, si detecta que el reloj es "aproximado", se conecta en segundo plano a una de tus redes WiFi guardadas, pone la hora en hora vía **NTP** y vuelve a apagar la radio. Mientras tanto, un discreto **marcador `~`** en la barra de estado te avisa de que la hora aún es aproximada, hasta que la sincronización termina. Se incluye además un **ajuste de zona horaria** (por defecto Madrid, con cambio horario verano/invierno).

Por el camino hubo que domar un cuelgue serio: levantar el WiFi desde una tarea de baja prioridad con poca memoria libre **congelaba el aparato entero**. La solución fue arrancar la radio siempre desde la tarea principal, mediante una máquina de estados que avanza un pasito por ciclo, para que la interfaz nunca se bloquee.

---

## 💾 Instalación

### Flasheador web (recomendado)

1. Conecta tu X3/X4 por USB-C y enciéndelo
2. Entra en https://xteink.dve.al/ y flashea desde el navegador

### Manual

```sh
git clone --recursive https://github.com/alexfdez1010/Infinity.git
cd Infinity
pio run --target upload
```

---

## 🗂️ Tarjeta SD

```
/fonts/          # Fuentes propias (formato .bin de Xteink: Nombre_tamaño_AxB.bin)
/sleep/          # Imágenes de pantalla en reposo (PNG/BMP)
```

### 🔤 Fuentes personalizadas

1. Entra en [xteink.lakafior.com](https://xteink.lakafior.com/) — el generador web de fuentes Xteink
2. Sube un `.ttf` / `.otf` y ajusta grosor y suavizado en la vista previa
3. Pulsa **Convert to .BIN**
4. Renómbrala como `Nombre_tamaño_AxB.bin` — p. ej. `Lexend_38_33x39.bin` (el firmware lee el tamaño y la caja del glifo del nombre del archivo)
5. Cópiala en `/fonts/` de la SD y reinicia. Aparecerá en **Ajustes → Fuente**

Modelo de doble fuente: elige una **principal** (p. ej. latina) y una **suplementaria** (p. ej. CJK) para que los textos con escritura mixta se rendericen sin huecos de glifos.

---

## 🛠️ Desarrollo

Infinity está construido con **PlatformIO** y el framework Arduino, en **C++17**, para el **ESP32-C3** (RISC-V a 160 MHz, ~380 KB de RAM sin PSRAM, 16 MB de flash, pantalla e-ink SSD1677 de 800×480). Cada byte de RAM cuenta: los buffers de TLS están recortados a 4 KB para que WiFi y cifrado quepan en el poco heap libre.

La arquitectura sigue un modelo de **actividades** al estilo Android: cada pantalla hereda de una clase `Activity` y se apila en un `ActivityManager` que gestiona el ciclo de vida y un único hilo de renderizado. Los ajustes y el estado de gamificación se guardan como JSON en la SD; las estadísticas de lectura, en binario.

```bash
pio run -e default      # Compilación estándar (con log serie)
pio run -e slim         # Versión de publicación (sin serie, más pequeña)
pio run -e ble          # Compilación con Bluetooth (beta)
pio run -t upload       # Flashear por USB
pio run -t monitor      # Monitor serie
```

Los scripts de pre-compilación (`build_html.py`, `gen_i18n.py`, `patch_jpegdec.py`) se ejecutan automáticamente. Consulta [`CLAUDE.md`](./CLAUDE.md) para más detalle sobre la estructura del código.

**Contribuir:** haz *fork* → rama → cambios → *pull request*. Toda cadena de texto visible se traduce en `lib/I18n/translations/` (inglés y castellano).

---

<sub>Infinity **no está afiliado a Xteink**. Publicado bajo licencia MIT. Basado en [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) y [CrossPet](https://github.com/trilwu/crosspet).</sub>
