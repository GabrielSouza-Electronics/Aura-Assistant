# Sons de navegação

## Arquivos inspecionados

WAVs fornecidos, `App/Src/app.c`, `BSP/Src/bsp_audio_out.c`, headers,
`Core/Src/i2s.c`, `AuraAssistant.ioc`, esquema REV01, CMake,
Screen1View/Presenter, Model, MenuLogic e Makefile do simulador.

## Arquivos modificados

- `aura_assets/audio/ui/`: cópias dos três WAVs e conversor reproduzível.
- `Components/Audio/ui_audio.c/.h`: amostras PCM constantes.
- `App/Inc/app_ui_audio.h`, `App/Src/app_ui_audio.c`: solicitação não bloqueante e consumidor na task de áudio.
- `App/Src/app.c`: serviço dos efeitos após o áudio de boas-vindas.
- `BSP/Inc/bsp_audio_out.h`, `BSP/Src/bsp_audio_out.c`: reprodução assíncrona com reinício.
- `TouchGFX/gui/include/gui/common/MenuSound.hpp`: seleção do evento sonoro.
- Model, Screen1Presenter e Screen1View: encaminhamento dos eventos.
- `CMakeLists.txt`: fontes da aplicação e assets.
- `tests/test_ui_audio.py`, `tests/test_tof_pointer.py`: testes de assets, BSP e eventos.

## Configuração de hardware e fontes

LCD, framebuffer, orientação, MPU e clocks não foram alterados.
I2S1 permanece em 48 kHz, 16 bits, conforme `.ioc` e inicialização atual.
O esquema identifica MAX98357AETE+T. Reutilizado o BSP de áudio existente;
nenhuma nova configuração elétrica ou tabela de datasheet foi introduzida.
O efeito usa DMA normal com comprimento exato. O welcome mantém seu fluxo
circular existente. DMA é parada antes de preencher o buffer de um novo efeito.
Parar I2S também para BCLK, relevante para a limitação de SD_MODE da REV01.

## Arquitetura implementada

Screen1View detecta a transição → Presenter → Model → mailbox App →
AudioOutputTask → BSP → DMA/I2S. A interface nunca espera a duração do som.
A mailbox mantém a solicitação mais recente, evitando uma fila de ticks atrasados.
Uma solicitação acorda a task imediatamente e substitui o efeito em andamento.

- Mudança do item selecionado no carrossel: `10_nav_tick.wav`, 35 ms.
- Transição do carrossel para um menu: `20_menu_enter.wav`, 260 ms.
- Transição de um menu para o carrossel: `21_menu_exit.wav`, 260 ms.
- Entrada/saída têm prioridade sobre troca de item no mesmo tick.
- Retirar a mão sem sair do menu não toca saída.
- Alterações de seleção enquanto o menu está aberto não tocam tick.

Os WAVs são mono PCM16 a 48 kHz; a conversão para C preserva as amostras,
sem síntese, reamostragem ou fade adicional. Na reprodução o volume existente
é aplicado e cada amostra mono é duplicada nos dois canais.
O buffer DMA existente em D2 é reutilizado.

No simulador Windows, PlaySoundW assíncrono reproduz os WAVs copiados no
repositório, substituindo o anterior. Os caminhos partem de
`TouchGFX/build/bin/simulator.exe`, independentemente do diretório de execução.
O executável requer a pasta `aura_assets/audio/ui` na estrutura do projeto.

## Build e testes

Executados com os respectivos toolchains no PATH:

```text
python aura_assets/audio/ui/convert.py
python tests/test_ui_audio.py --compiler D:/TouchGFX/4.26.1/env/MinGW/bin/gcc.exe
python tests/test_tof_pointer.py --compiler D:/TouchGFX/4.26.1/env/MinGW/bin/g++.exe
cmake --build --preset Debug
```

Na pasta TouchGFX, compilação do simulador usando os assets existentes:

```text
mingw32-make -f generated/simulator/gcc/Makefile -j4 build_executable
```

Todos concluíram com sucesso. O teste usa o BSP real com HAL substituída:
confere restart, substituição tick/enter, volume positivo/negativo, estéreo,
fim da reprodução, argumentos inválidos, exclusividade do echo e erro de início.
Também compara todas as amostras C com os WAVs, e verifica o roteamento do Model,
seleção com wrap, prioridade de saída e ausência de ticks espúrios.

Aviso remanescente: objcopy informa segmento carregável vazio em `0x90000000`.

## Memória

Amostras novas: 53.280 bytes em Flash interna. Build final: Flash 650.148 bytes,
DTCM 124.384 bytes (94,90%), D2 96.960 bytes, framebuffer AXI 450 KiB.
Comparado ao início da tarefa: +54.432 bytes de Flash, +8 bytes de DTCM,
sem aumento de buffer DMA ou heap.

## Premissas e testes físicos pendentes

O welcome termina antes de iniciar o serviço de efeitos. Durante o welcome,
somente o evento mais recente fica pendente.

Os testes de host não validam temporização real de interrupções, latência audível
ou comportamento do amplificador. Próximo passo: na placa, girar rapidamente,
confirmar um item e voltar; ouvir se o tick reinicia sem atraso e avaliar volume
e possíveis estalos nos cortes abruptos. Não foi declarado áudio validado fisicamente.

## Correction: first effect after welcome

The existing HAL_DMA_Abort returns HAL_ERROR / HAL_DMA_ERROR_NO_XFER when
its state is not BUSY. PlayEffect used to reject that return unconditionally,
so an idle DMA after welcome prevented every effect from starting.
The BSP now accepts only NO_XFER combined with HAL_DMA_STATE_READY after
DMAStop has disabled I2S. Real stop errors still reject playback.
No HAL library changes were made. Host tests now simulate idle-stop failure,
completion followed by another effect, and a genuine stop error.
Hardware playback after this correction still needs confirmation.

## Home-screen presence sounds

Screen1View now remembers the previous hand-presence state. On the home/carousel
screen, absent -> present plays Enter and present -> absent plays Exit, once
per transition. Holding the hand in place is silent. Presence changes while
inside an open menu remain silent. Menu open/close transitions take priority,
then presence transitions, then navigation ticks, preventing duplicate sounds.
The existing 150 mm gate applies; mouse press/release simulates the same behavior.

Modified: MenuSound.hpp, Screen1View.cpp/.hpp and transition tests. Debug build
and host transition tests passed; Flash 650276 bytes, RAM usage unchanged.
LCD/audio hardware configuration and source WAVs were not modified.
Next hardware check: approach/withdraw on home, hold steady, then repeat inside
an open menu and verify no extra enter/exit sounds there. Physical validation pending.
