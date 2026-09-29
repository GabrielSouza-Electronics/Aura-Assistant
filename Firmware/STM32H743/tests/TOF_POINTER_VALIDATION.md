# Controle da interface pelo ToF

> Current state: Y restored to its native -10..+10 range, zero neutral.
> The temporary -10..0 calibration described below is historical and removed.
> X/Y retain the original sensor orientation and temporal filtering.
> Direct Model/Presenter input and X-only linear rotation remain enabled.
> Native Y tests (-10, -5, 0, +10) and Debug build passed.
> Files changed in this revision: app_hand_tracking.c/.h and host test/report.
> Final internal Flash: 595716 bytes; DTCM unchanged at 124376 bytes.
> Hardware confirmation remains pending; objcopy QSPI warning persists.


## Arquivos inspecionados

- `App/Inc/app_hand_tracking.h`, `App/Src/app_hand_tracking.c`, `App/Src/app.c`.
- `BSP/Inc/bsp_tof.h`, `BSP/Src/bsp_tof.c`.
- `TouchGFX/gui/src/screen1_screen/Screen1View.cpp` e seu header.
- `TouchGFX/gui/src/common/MenuLogic.cpp`, `TouchGFX/gui/src/model/Model.cpp`.
- `TouchGFX/target/STM32TouchController.cpp` e seu header; configuração gerada do controlador.
- `AuraAssistant.ioc`, `Core/Src/i2c.c`, `Core/Src/gpio.c`, `Core/Src/freertos.c`.
- `CMakeLists.txt`, `CMakePresets.json`, toolchain e cache Debug.
- `Hardware/Main_Board/Schematic_PCB_Main_REV01.PDF` (texto extraído).

## Arquivos modificados

- `App/Inc/app_hand_tracking.h`: API C/C++ para consultar a entrada da mão.
- `App/Src/app_hand_tracking.c`: publicação consistente entre tasks, timeout e limiar sem atraso do filtro de profundidade.
- `TouchGFX/target/STM32TouchController.cpp`: conversão da mão em entrada TouchGFX.
- `TouchGFX/gui/src/common/MenuLogic.cpp`: seleção por permanência bloqueada durante o gesto de voltar; atalho de entrada por movimento removido.
- `tests/test_tof_pointer.py`: teste de integração no host.
- Este relatório.

Havia diversas alterações locais antes desta tarefa, inclusive no rastreamento e na UI; elas foram preservadas.

## Configuração LCD confirmada

A UI atual trabalha com coordenadas 480 × 480 e transforma o mouse por
`handX = (240 - screenY) / 240`, `handY = (screenX - 240) / 240`.
O adaptador usa a transformação inversa e limita as coordenadas a 0–479.
Não houve mudança de inicialização, orientação do painel, LTDC ou framebuffer.

## Fontes de datasheet e inicialização

Foi reutilizado o BSP existente, com ULD ST VL53L5CX, resolução 4 × 4 e 5 Hz.
O esquema identifica VL53L5CXV0GC/1; a configuração atual usa I2C2 em PF0/PF1,
LPn em PF2 e INT em PF3. Nenhuma nova sequência de inicialização ou valor
de registrador foi introduzido; não foi necessário adotar um novo datasheet.

## Premissas pendentes

O sentido físico dos eixos depende da montagem. Foram preservados os ajustes
existentes do rastreador, inclusive a inversão de Y. A distância usada no limiar
é a média ponderada das zonas selecionadas pelo rastreador (`raw_z_mm`), antes
do filtro temporal; não é a menor distância isolada de uma zona.
As leituras a 5 Hz limitam a resposta a movimentos rápidos.

O teste físico informado pelo usuário encontrou Y entre −10 e 0, com posição
neutra em −5. A calibração aplica `Y = 2 * (raw_y + 5)`, limitada a −10..+10,
antes da zona morta e do filtro temporal. `raw_y` mantém o centroide orientado
sem calibração; `y` e `tof_y` são calibrados e filtrados. Valores acima da faixa
observada saturam em +10. O rastreamento de X mantém a configuração anterior.
Essa calibração ainda precisa ser conferida pelo usuário na placa.

O controle pelo ToF usa apenas X filtrado (-10..+10): sinal define sentido,
modulo define velocidade, com zona morta perto de zero. position_direction
fica apenas para diagnostico. Y mantem sua calibracao. Conferir na placa
se raw_x e x atingem os dois sinais; a causa fisica ainda nao foi confirmada.

## Arquitetura implementada

SensorTask → BSP ToF → rastreador → snapshot protegido por seção crítica →
STM32TouchController → eventos normais TouchGFX → Screen1View → MenuLogic.

- Distância menor que 150 mm: pressionado, incluindo movimento enquanto segura.
- Distância maior ou igual a 150 mm: solto na próxima leitura.
- Quadro sem alvo válido: solto imediatamente, sem reutilizar a retenção dos diagnósticos.
- Sem quadros novos durante 600 ms: solto; uma leitura válida permite retomar.
- Mouse no simulador: mantém o caminho SDL existente.

Nenhuma leitura I2C ocorre na task gráfica. O snapshot requer contexto de task;
o processamento/reset permanece sob responsabilidade de um único produtor.

## Comando de build

Com os executáveis dos bundles STM32Cube no PATH:

```text
cmake --preset Debug
cmake --build --preset Debug
```

## Resultado do build e testes

Build Debug concluído com código 0 e geração de `AuraAssistant.elf`.
O objcopy emitiu aviso de segmento carregável vazio em `0x90000000` durante
a geração das imagens separadas; não houve erro de compilação/linkedição.

Teste executado:

```text
python tests/test_tof_pointer.py --compiler D:/TouchGFX/4.26.1/env/MinGW/bin/g++.exe
```

Passaram: pressionar, manter, mover, soltar em 150 mm, alvo inválido, timeout,
recuperação, reset, transformação dos eixos, extremos da tela e wrap do tick.
Também passaram: calibração de Y em −10, −5 e 0; seleção por permanência no
centro; retorno ao carrossel no extremo negativo; ausência de seleção durante
o gesto de voltar; e ausência do antigo atalho de entrada por movimento.
Foram testados X em -10, -5, -3, 0, +3, +5 e +10, velocidades crescentes,
sentidos opostos e inversao durante o mesmo clique.
O teste compila os fontes reais C e C++; apenas relógio e seções críticas do
RTOS são substituídos. Não valida concorrência real nem hardware.

## Impacto de memória

Snapshot estático pequeno, sem alocação dinâmica ou novos buffers DMA.
Uso final: DTCM 124.376 bytes (94,89%), framebuffer em RAM AXI 450 KiB,
RAM D2 96.960 bytes, Flash interna 596.044 bytes, QSPI 1.767.740 bytes.
A margem disponível em DTCM é de 6.696 bytes.

## Testes físicos e próximo passo

Na placa, aproximar e manter a mão abaixo de 150 mm, mover nos quatro sentidos,
afastar para 150 mm ou mais e retirar a mão do campo de visão. Confirmar também
a liberação quando as atualizações param. Conferir o sentido dos eixos e a
estabilidade perto do limiar antes de ajustar sensibilidade ou taxa de leitura.
O comportamento físico ainda não foi validado.

## Direct input revision

The hardware now uses tracker snapshot -> Model::tick -> ModelListener ->
Screen1Presenter -> Screen1View::setHand -> MenuLogic. Coordinates are divided
by 10 only; there is no ToF-to-mouse conversion. STM32TouchController returns
false to prevent duplicate input. The simulator still receives SDL mouse events.
The root CMakeLists adds App/Inc and BSP/Inc to the TouchGFX target.

MenuLogic no longer remaps X through a dead zone or snaps the carousel to an
item. Rotation per tick is -0.055 * X/10; X=0 or absent hand stops rotation.
Y directly controls tilt and retains the back gesture. Dwell requires X=0.
Tracker calibration/filtering, the 150 mm gate and the stale-data timeout remain.
menu_speed and position_direction are diagnostics, not inputs to MenuLogic.

Tests exercise the actual Model and MenuLogic, every integer X/Y in -10..10,
Y independence, immediate reversal, no snap, input timeout and navigation.
They do not execute the full GUI event loop or validate physical behavior.
Debug build passed; final internal Flash 595756 bytes, DTCM 124376 bytes.
The existing objcopy empty QSPI segment warning remains.

On hardware, check X=0 stops immediately, +/-5 produces half the +/-10 speed,
and moving Y alone cannot rotate the carousel. The screenshot with raw_z_mm=339
and hand_active=false is outside the enabled interaction range.
