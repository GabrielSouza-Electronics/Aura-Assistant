> Atualizacao: consultar TOF_GESTURES_VALIDATION.md. Todos os menus voltam com ToF <30 mm por 2 s; Tasks conclui na retirada antes de 2 s. O fluxo anterior de IMU/mao alta descrito abaixo foi substituido.

# Tasks, Settings e sincronização web — implementação

## Files inspected
AuraAssistant.ioc, Core/Src/main.c, gpio.c, i2c.c e freertos.c;
STM32H743xx_FLASH.ld; CMakeLists.txt e CMakePresets.json;
Screen1View, Model, SettingsLogic, GlyphText, CalendarWidget/Logic;
app_hand_tracking, app_calendar, app_wifi e BSP IMU;
driver oficial ISM330DLC; geradores atuais de assets; fontes Poppins;
esquema Hardware/Main_Board/Schematic_PCB_Main_REV01.PDF, folha IMU;
testes existentes de navegação e serviço do calendário.

## Files modified
- App/Inc/app_tasks.h e App/Src/app_tasks.c: snapshot das tarefas, fila
  de conclusões em RAM, confirmação/rejeição e evento da IMU.
- Components/Tasks/tasks_data.c/.h e web_state.h: ordenação, paginação,
  validação, prazos relativos e contrato JSON.
- App/Src/app_calendar.c e Components/Calendar/calendar_data.c:
  cliente HTTPS compartilhado e parsing do snapshot web.
- App/Inc/app_web_config.h: configuração de host, token e CA reais.
- BSP/Src/bsp_imu.c, BSP/Inc/bsp_imu.h e App/Src/app.c: detecção e consumo
  do toque único da IMU no SensorTask, sem transações I2C em ISR.
- Core/Src/freertos.c: inicialização de Tasks no USER CODE antes das threads.
- STM32H743xx_FLASH.ld: armazenamento CPU em `.tasks` na SRAM4 D3.
- TouchGFX/gui/common: TasksLogic/TasksWidget e texto compacto do Settings.
- Screen1View/Model: entrada e saída de Tasks, clique, animação do título,
  status compartilhado e camadas do background.
- CMakeLists.txt e TouchGFX/simulator/gcc/Makefile: fontes/include explícitos.
- aura_assets/gen/gen_tasks_firmware.py e gen_settings_tasks_style.py:
  assets próprios na QSPI, fontes, baseline comum e medidas do risco.
- aura_assets/compiled/tasks/ e settings_style/: sprites ARGB8888.
- testes nativos, runner tests/test_tasks.py e contrato Lovable em
  aura_assets/LOVABLE_DEVICE_API.md.

Alterações locais anteriores em Screen1View foram preservadas. Ícones do
carrossel continuam os originais; as propostas canceladas não foram instaladas.

## LCD hardware configuration confirmed
Nenhum timing, pino, registro do painel ou inicialização LTDC foi alterado.
Framebuffer RGB565 permanece na AXI D1, com 450 KiB e um único buffer.
Sprites usam a convenção Portrait já existente: (x,y) -> (y,479-x).
Os assets PixelDataWidget seguem a mesma conversão de pixels usada pelos
sprites atuais do calendário/startup.

O main.c atual já habilita I-Cache e D-Cache e tem região MPU específica
para o framebuffer. Isso diverge da nota histórica de caches desabilitados
no AGENTS.md. Foi respeitado o código atual; cache/MPU não foram alterados.

Para a IMU, esquema, `.ioc` e i2c.c concordam: ISM330DLCTR, I2C4 em
PF14/PF15 e INT1 em PF13. O driver continua sondando os endereços 0x6A/0x6B
por causa da configuração SA0 da REV01. Prioridades NVIC permanecem existentes.

## Datasheet / initialization sources used
[ST AN5125 Rev 2, seções 5.5.1 e 5.5.4](https://www.st.com/resource/en/application_note/an5125-ism330dlc-3d-accelerometer-and-3d-gyroscope-with-digital-output-for-industrial-applications-stmicroelectronics.pdf).
Perfil inicial documentado: acelerômetro 416 Hz, ±2 g, threshold 9
(562,5 mg), shock 2, quiet 1, X/Y/Z habilitados. Toque único, com latch
e roteamento INT1 conforme seção 5.5.1, lido/limpo no SensorTask.
Esses valores são ponto inicial de validação, não calibração do gabinete.
Driver oficial já presente em Components/ISM330DLC, sem recriar registradores.

## Any unresolved assumptions
- API ainda não existe. O contrato foi entregue para o Lovable. Host,
  token exclusivo e root CA reais permanecem sem valores inventados.
- Build final usa AURA_WEB_ENABLED=OFF: fontes públicas de relógio/calendário
  existentes permanecem ativas; Tasks no alvo aguarda dados reais.
  Build com a opção ON também foi validado.
- Sincronização usa polling a cada 2 segundos após cada ciclo de rede;
  não há WebSocket. Timeout/backoff podem aumentar essa latência.
- Limite explícito de 32 tarefas, títulos ASCII de até 63 bytes,
  datas 2020–2099 e timezone Asia/Dubai. O backend deve projetar/transliterar
  dados dentro desse contrato, conforme instruções entregues.
- Pendências de conclusão sobrevivem à desconexão durante a sessão,
  mas não ao reboot. Estado definitivo pertence ao servidor.
- Reminders e Chat não receberam implementações novas: não há comportamento
  especificado para esses menus neste pedido.
- Wi-Fi/bateria usam os widgets existentes; não foi implementada uma nova
  curva de conversão ADC para percentual da bateria.
- Sem execução visual no display e sem requisições a um servidor real.

## Architecture implemented
TasksWidget lê snapshots limitados, nunca bloqueia na rede. Cliente HTTPS
roda no worker já existente do calendário. App gerencia dados e confirmações;
componentes implementam lógica pura; BSP configura/lê a IMU com o driver ST.
Não há novas alocações dinâmicas. Snapshots ficam na SRAM4 CPU, não em buffers DMA.

Tasks mostra quatro itens por página, ordenados por deadline completo.
Cima/baixo mudam foco; lados paginam; glow acompanha a direção disponível.
Mão alta por 60 ticks (~1 segundo) retorna ao carrossel. Toque único da IMU
conclui o item focado mesmo sem mão no ToF. Clique de proximidade existente
e clique de mouse no simulador também exercitam a conclusão.
O risco atravessa o centro dos limites reais das letras do título.

Títulos: Poppins 14 px; prazos/etiquetas: 11 px. O título TASKS reutiliza
o sprite do carrossel e desliza para o cabeçalho. Status Wi-Fi/bateria e
background de circuito permanecem visíveis. Settings usa o mesmo design
aprovado. Background continua habilitado no loading.

## Build command
`cmake --preset Debug -DAURA_WEB_ENABLED=ON` e `cmake --build --preset Debug`
para verificar o cliente web; depois `cmake --preset Debug -DAURA_WEB_ENABLED=OFF`
e `cmake --build --preset Debug` para gerar os artefatos finais sem endpoint.
Toolchain STM32 GNU Arm 14.3.1, CMake 4.3.1 e Ninja existentes.

Simulador: target `build_executable` do Makefile gerado, com os
ADDITIONAL_SOURCES/INCLUDE_PATHS definidos no Makefile de customização.
Não é necessário regenerar o Designer para os novos sprites customizados.

## Build result
- Build Debug nas opções ON e OFF: sucesso, exit code 0.
- Simulador TouchGFX: compilação/link completos, exit code 0.
- `python -B tests/test_tasks.py`: sucesso. 531.360 combinações de prazos,
  ordenação, revisão antiga, pendência preservada, ack/rejeição, páginas,
  limites, viradas de ano/bissexto e JSON truncado.
- Navegação Tasks/calendário e serviço legado passaram nos testes nativos.
- Worker web testado com sockets simulados: Bearer, PUT idempotente,
  polling, relógio/feriados, envios/recebimentos parciais e liberação TLS/socket.
- Geradores validam medidas de texto e bordas transparentes. O maior texto
  relativo medido tem cerca de 99 px, cabendo nos 166 px disponíveis.
- `git diff --check`: sem erros de whitespace.
- Aviso existente do objcopy: segmento vazio em 0x90000000.
- O runner antigo tests/test_calendar.py com ctypes não executa neste ambiente:
  Python é 64 bits e o MinGW disponível gera DLLs de 32 bits (WinError 193).
  O runner novo executa binários nativos e passou; os testes nativos do serviço
  e navegação existentes também passaram, sem ignorar warnings.

## Memory impact if meaningful
Build final OFF:
- DTCM: 126.000 bytes, 96,13% de 128 KiB.
- AXI D1: 450 KiB, 87,89% de 512 KiB.
- SRAM D2: 96.960 bytes, 32,88%.
- SRAM4 D3: 31.352 bytes, 47,84%.
- Flash interna: 734.568 bytes, 35,03% de 2 MiB.
- QSPI: 5.166.892 bytes, 30,80% de 16 MiB.

Em relação ao início deste pedido (Settings já instalado), Tasks acrescenta
275.616 bytes na QSPI, 544 bytes na DTCM e 5.280 bytes na SRAM4.
O stack estático do worker de rede segue em SRAM4. Inspeção .su:
web_run 192 bytes e http_json 1.176 bytes; SensorTask 88 bytes locais,
dentro do stack configurado de 4.096 bytes. Isso não substitui medição de
high-water mark no alvo com toda a cadeia HAL/middleware.

## Hardware tests to perform when PCB arrives
Programar a imagem interna e a imagem QSPI atualizadas. Validar rotação,
baseline, corte de nomes longos, risco central, transição do título,
glows/foco, paginação e gesto de saída. Testar IMU/WHO_AM_I e endereços,
toques reais no gabinete, falsos disparos e ausência de IMU sem travar ToF.
Calibrar sensibilidade e medir latência/stack. Testar rede real, CA/SNI,
token revogado, desconexão/reconexão, idempotência e edições concorrentes.

## Recommended next step
Enviar aura_assets/LOVABLE_DEVICE_API.md ao Lovable. Após publicar o backend,
configurar host/token/CA privados, ativar AURA_WEB_ENABLED e validar ponta a ponta.
