# AuraAssistant — contrato BLE de provisionamento v1

Implementação STM32: `App/Src/app_provisioning.c` e `provision_protocol.c`.
Este documento descreve o código implementado, não apenas uma proposta.
Build e testes com transporte simulado foram executados; a integração com o
ST67 e o smartphone ainda precisa do roteiro de bancada abaixo.

## Texto para enviar ao Lovable

```text
Integre o app ao firmware AuraAssistant usando este contrato BLE v1.

Nome do dispositivo: AuraAssistant
Service UUID: 7e57a001-b5a3-4f3c-9a7d-2c6e8f901234
Command UUID: 7e57a002-b5a3-4f3c-9a7d-2c6e8f901234 (Write with response)
Event UUID:   7e57a003-b5a3-4f3c-9a7d-2c6e8f901234 (Notify)

Use o UUID do serviço para descoberta. O nome está no scan response.
Conecte, descubra o serviço e assine Event antes de escrever em Command.
O Aura inicia pareamento pelo próprio dispositivo. Trate o diálogo do sistema.
Espere status.security == "ready" e status.provisioning_open == true antes
de enviar scan, connect ou forget. get_status pode ser usado durante pairing.
Não invente PIN fixo e não envie senha enquanto a sessão estiver em pairing.

Transporte: JSON compacto UTF-8 seguido de um byte LF (0x0A).
Serialize com JSON.stringify(obj) + "\n" e então TextEncoder.
Divida OS BYTES em blocos de no máximo 20 bytes. Aguarde cada escrita COM
RESPOSTA antes de enviar a próxima. Não use writeWithoutResponse.
O MTU maior não altera o limite de 20 bytes deste protocolo v1.

Remonte os bytes das notificações até LF; somente depois decodifique UTF-8
e faça JSON.parse. Uma notificação não equivale a uma mensagem JSON.
Limite de entrada: 768 bytes por mensagem, sem o LF.
Limite o buffer de remontagem do app a 768 bytes e recupere por reconexão
se houver overflow, timeout ou JSON inválido nas respostas.

IDs: inteiros de 1 a 4294967295, estritamente crescentes dentro da sessão BLE.
ID 0 é reservado para notificações espontâneas e erros sem ID recuperável.
Após reconectar, reinicie o contador e limpe o buffer de remontagem.
Nunca reutilize um ID para repetir connect. Consulte get_status primeiro.
Mantenha somente UM comando pendente até resposta terminal.

Comandos exatos (não adicionar campos extras):
{"v":1,"id":1,"cmd":"get_status"}
{"v":1,"id":2,"cmd":"scan"}
{"v":1,"id":3,"cmd":"connect","ssid":"MinhaRede","password":"12345678"}
{"v":1,"id":4,"cmd":"forget"}

A varredura Wi-Fi é feita pelo AURA. Não use scan Wi-Fi do smartphone.
scan retorna scan_started, zero ou mais network, e scan_done ou error.
Cada network tem ssid, rssi, security, channel, bssid e supported.
Máximo de 10 redes nesta versão; não apresentar isso como lista exaustiva.
Agrupe visualmente por SSID/segurança se desejar e ordene por RSSI.
Não permita escolher resultados supported=false. Permita SSID manual.

connect retorna connecting e depois connected ou error.
connected significa que o AURA obteve um IP; não comprova acesso à internet.
Se o BLE cair durante connect, reconecte e consulte get_status: a operação
Wi-Fi pode ter concluído enquanto o telefone estava desconectado.

get_status retorna status com wifi, ssid, ip, security, provisioning_open,
max_chunk e max_message. Não confundir security (sessão BLE) com a segurança
das redes Wi-Fi retornada nas mensagens network.
Trate status espontâneo id=0 sem encerrar o comando pendente de outro ID.
Error id=0 encerra o pedido pendente como falha de protocolo.

Timeouts sugeridos no app: 70 s para pareamento, 60 s para scan,
60 s para connect, 30 s para forget, 15 s para get_status.
Não deixar mais de 10 s entre fragmentos da mesma mensagem.
Após timeout, não executar um segundo comando às cegas: reconectar e
consultar status. Para erro frame_timeout, reconectar limpa a remontagem.

Limites de SSID/senha em bytes UTF-8, não em caracteres JavaScript:
SSID: 1..32 bytes; senha: vazia para rede aberta ou 8..63 bytes.
Senha vazia deve ser enviada como password:""; o campo é obrigatório.
Não fazer trim nem pré-escape AT. JSON.stringify já faz o escape JSON.
O firmware faz separadamente o escape necessário ao módulo ST67.
Não aceitar caracteres de controle, NUL, WEP, Enterprise ou PSK hexadecimal
de 64 caracteres nesta versão. WPA2/WPA3 dependem também do firmware ST67/AP.

forget remove as credenciais Wi-Fi e desconecta o Aura; confirme essa ação
na interface. Não é reset de fábrica nem apaga os vínculos Bluetooth.
Em caso de forget_failed, a remoção pode ter sido parcial; informar falha.

A configuração fica aberta por 5 minutos após inicializar o serviço no boot.
Reiniciar o Aura abre outra janela. Após expirar, scan/connect/forget são
bloqueados e o advertising é interrompido quando não há cliente conectado.
Uma sessão já conectada ainda pode consultar status. Mostre instrução de
reiniciar o Aura se provisioning_open=false ou se não for encontrado.

Não armazenar senhas em logs, analytics, backend ou localStorage.
Limpar a senha da interface ao concluir/cancelar. Nunca exibir sucesso
baseado apenas na resolução da Promise de escrita BLE.
Preservar separação entre transporte BLE real e modo demonstração.
```

## Exemplos de respostas

Cada linha abaixo termina com LF no transporte. A ordem dos campos não deve
ser usada para interpretar mensagens. O app deve tolerar campos adicionais nas
respostas para permitir extensão futura; os comandos v1 são estritos.

```json
{"v":1,"id":0,"event":"status","wifi":"disconnected","ssid":"","ip":"0.0.0.0","security":"ready","provisioning_open":true,"max_chunk":20,"max_message":768}
{"v":1,"id":2,"event":"scan_started"}
{"v":1,"id":2,"event":"network","ssid":"MinhaRede","rssi":-42,"security":"wpa2","channel":6,"supported":true,"bssid":"aa:bb:cc:dd:ee:ff"}
{"v":1,"id":2,"event":"scan_done","count":1,"limit":10,"skipped":0}
{"v":1,"id":3,"event":"connecting"}
{"v":1,"id":3,"event":"connected","wifi":"connected","ssid":"MinhaRede","ip":"192.168.1.55","security":"ready","provisioning_open":true,"max_chunk":20,"max_message":768}
{"v":1,"id":4,"event":"forgotten"}
{"v":1,"id":5,"event":"error","code":"connection_failed"}
```

Valores de `wifi`: `disconnected`, `connecting`, `associated`, `connected`.
Valores de `status.security`: `pairing`, `ready`. Durante pairing, SSID/IP não
são divulgados; `0.0.0.0` não deve ser exibido como IP operacional.
Segurança em `network`: `open`, `wpa`, `wpa2`, `wpa_wpa2`, `wpa3`,
`wpa2_wpa3`, `wep`, `enterprise`, `unknown`.
`skipped` conta resultados com SSID que não pode ser representado em UTF-8.
SSID vazio em um resultado pode representar rede oculta; o nome deve ser informado.
O comando connect escolhe por SSID, não fixa BSSID/canal.

Erros de protocolo: `invalid_json`, `invalid_request`, `unsupported_version`,
`unknown_field`, `duplicate_field`, `unknown_command`, `invalid_ssid`,
`invalid_password`, `unexpected_credentials`, `invalid_frame`, `frame_timeout`,
`stale_id`. Erros que impedem parse confiável usam `id=0`.

Erros operacionais: `pairing_required`, `provisioning_closed`, `scan_busy`,
`scan_failed`, `scan_timeout`, `status_failed`, `ip_unavailable`,
`disconnect_failed`, `connection_failed`, `forget_failed`.
Não traduzir todo `connection_failed` como senha incorreta: o middleware também
retorna falha genérica para AP ausente, falha de associação e timeout DHCP.

## Arquitetura e comportamento do firmware

- `APP_WiFiTask` mantém a inicialização existente e executa o provisionamento.
  Nenhuma nova tarefa/heap da aplicação foi criada. A tarefa existente tem 8192
  bytes de stack, conforme `Core/Src/freertos.c`.
- Callbacks BLE copiam fragmentos para uma fila fixa de 48 posições e sinalizam
  estado. Não enviam comandos AT. Buffers recebidos não são retidos por ponteiro.
- A tarefa interpreta os comandos e chama as APIs ST. Scan/conexão podem bloquear
  essa tarefa enquanto o middleware trabalha; o aplicativo não deve fazer polling
  paralelo ou enviar múltiplos comandos pendentes.
- As respostas são fragmentadas em 20 bytes e cada envio verifica o vínculo da
  sessão. Desconexão limpa a fila e invalida respostas antigas mesmo se o ST67
  reutilizar o mesmo connection handle. Falha de envio parcial encerra a sessão.
- Escrita maior que 20 bytes ou overflow da fila encerra a sessão. Escritas sem
  inscrição em Event são descartadas. O buffer do driver continua com 256 bytes,
  mas isso não aumenta o limite do protocolo.
- Scan usa snapshot de até 10 APs. Um scan que terminou em timeout mantém a posse
  do callback até a conclusão tardia; pedidos seguintes retornam `scan_busy`.
  Se o módulo nunca concluir, reinicie o Aura. Isso evita atribuir resultados de
  uma operação antiga a uma nova.
- O módulo salva a última conexão bem-sucedida; o override em USER CODE de
  `w6x_config.h` habilita autoconnect. A persistência/reconexão deve ser confirmada
  com desligamento real. Não há nova cópia da senha na flash do STM32.
- Retries de conexão são configurados em 5 s, até 3 tentativas no módulo.
  Em erro da tentativa solicitada, a aplicação pede desconexão para cancelar
  retries tardios. Falha de transporte AT também precisa de diagnóstico de bancada.
- O framebuffer permanece em AXI SRAM. A fila, remontagem e snapshot usam a seção
  CPU-only `.provisioning` na região RAM_D3 existente, com limpeza explícita no
  início porque a seção é NOLOAD. Não são buffers DMA.

## Segurança e limitações desta versão

O servidor inicia `W6X_Ble_SecurityStart(handle, 2)` com IO NoInputNoOutput,
pareamento Just Works. A aplicação exige o evento PAIRING_COMPLETED antes de
aceitar comandos operacionais; timeout/falha encerra o vínculo, sem fallback
para credenciais não pareadas. O cliente deve esperar `security:ready` **antes
de transmitir qualquer senha**.

Just Works não autentica o proprietário e não protege contra MITM ativo. A janela
de cinco minutos limita a exposição, mas qualquer pessoa próxima durante essa
janela pode tentar parear/reconfigurar. Para produto final, falta aprovação física
no Aura ou identidade/prova de posse por dispositivo. Esta implementação não
declara essa autenticação implementada. Não há PIN universal embutido.

O middleware local só expõe permissões GATT read/write, sem bit de exigência de
criptografia na característica. Essa limitação e o início explícito do pareamento
são também descritos pela [ST sobre pairing/bonding no ST67](https://community.st.com/interface-and-connectivity-ics-52/st67w61-ble-pairing-and-bonding-for-ios-163049).
Logo, a proteção depende do pareamento prévio e do cliente respeitar `ready`;
não se deve afirmar que uma escrita malcomportada antes dele é bloqueada no rádio.

É admitido um cliente de cada vez; se outro cliente conectar antes do advertising
parar, a sessão é encerrada para não misturar notificações. O estado de segurança
é reiniciado a cada conexão. Confirmar na bancada que o firmware ST67 instalado
emite PAIRING_COMPLETED também ao reconectar a um telefone já vinculado. Se não
emitir, o firmware falha fechado e desconecta após 60 s; investigar a versão do
NCP e os vínculos, sem remover a verificação de segurança para contornar o problema.

Buffers da aplicação com credenciais são apagados após uso. Logs brutos AT/modem
estão desabilitados e há bloqueio de compilação caso sejam habilitados. Isso não
constitui auditoria do armazenamento interno/persistente do NCP ou do sistema do
telefone. Redes Enterprise, WEP e SSIDs binários não UTF-8 não são suportados.

## Alterações locais no middleware ST

Preservar/reaplicar ao atualizar X-CUBE-ST67W61:

1. `Driver/W61_at/w61_at_credential.h`: escape AT limitado e limpeza de temporários.
2. `w61_at_wifi.c`: connect/delete aceitam SSID/senha crus, fazem escape de
   `,`, `"`, `\` e recusam controles. Comando de conexão tem 256 bytes para
   suportar expansão máxima; não mudou o tamanho global de buffers do middleware.
3. `Core/w6x_wifi.c`: eventos CONNECT/GOT_IP são preparados antes de enviar AT;
   bits de tentativas anteriores são limpos. Corrigido nome do argumento no assert.
   Disconnect também cancela conexão pendente/esquece configuração quando offline.
4. `Api/w6x_api.h`: documentação de connect atualizada para impedir duplo escape.

Não houve alteração de HAL, clocks, GPIO, NVIC, MPU, TouchGFX ou UI.

## Validação executada

Comandos (toolchain existente no PATH):

```powershell
cmake --preset Debug
cmake --build --preset Debug
python tests/test_provisioning.py --compiler C:/ST/STEdgeAI/4.0/Utilities/windows/mingw64/bin/gcc.exe
```

O teste compila o parser e o serviço reais com um transporte/RTOS simulado.
Também compila as funções reais connect/disconnect do middleware com respostas
AT simuladas; não substitui essas funções por cópias da implementação.

Cobertura: JSON incompleto/malformado, Unicode e surrogates, UTF-8 inválido,
limites de SSID/senha, escape e tamanho máximo de AT, controles/injeção,
fragmentação, timeout, overflow, pareamento ausente, IDs repetidos, troca de
sessão, falha de envio, falha Wi-Fi, scan tardio e eventos rápidos do roteador.

Build Debug: sucesso. Testes host: sucesso. Uso observado:

| Região | Uso |
|---|---:|
| DTCMRAM | 124544 bytes (95,02%) |
| AXI SRAM / framebuffer | 460800 bytes |
| RAM_D2 | 96960 bytes |
| RAM_D3 / provisionamento | 2620 bytes |
| Flash interna | 677752 bytes |
| QSPI / assets existentes | 1767840 bytes |

Existe um warning do objcopy na extração das imagens sobre segmento vazio em
`0x90000000`; o link e a geração das imagens terminam com código 0. A seção QSPI
é removida deliberadamente da imagem interna pelo fluxo já existente.
Inspeção dos `.su` indica 880 bytes de frame local para Status e 760 para Scan;
medir o high-water mark da tarefa na placa para confirmar o pior caso real.

## Fontes e arquivos inspecionados

- `AuraAssistant.ioc`, `Core/Src/main.c`, `gpio.c`, `spi.c`, `freertos.c`.
- `App/Src/app_wifi.c`, `App/Inc/app.h`, `BSP/Src/bsp_wifi.c`, `BSP/Inc/bsp_wifi.h`.
- Configuração W6X/driver/logging em `ST67W6X_Network_Driver/Target/`.
- APIs/tipos e implementações BLE/Wi-Fi/AT do middleware local ST.
- `CMakeLists.txt`, `CMakePresets.json`, toolchain, linker, mapa e stack usage.
- Esquemático atual `Hardware/Main_Board/Schematic_PCB_Main_REV01.PDF`, folha ST
  Wi-Fi: módulo identificado como `ST67W611M1U6B`. Integração preserva SPI2,
  GPIOs e sequência de alimentação já existentes; não importa timings de outra placa.
- [Referência de comandos AT da ST](https://wiki.st.com/stm32mcu/wiki/Connectivity%3AST67W611M_AT_Command)
  e discussão oficial de segurança citada acima; o código local é a referência
  das assinaturas e limites usados.

As notas antigas diziam caches desabilitados e PCB indisponível. O `main.c`
atual habilita os caches, o BSP registra validação REV01 e o usuário já observa
BLE na placa. Esta tarefa preservou esse estado atual. LCD: nenhuma mudança de
configuração ou nova validação física; nenhuma tabela de inicialização utilizada.

## Arquivos modificados/criados

- `App/Inc/app_provisioning.h`, `App/Src/app_provisioning.c`: serviço, sessão e operações.
- `App/Inc/provision_protocol.h`, `App/Src/provision_protocol.c`: framing/parser sem heap.
- `App/Src/app_wifi.c`: integração na tarefa e callbacks existentes.
- `CMakeLists.txt`, `STM32H743xx_FLASH.ld`: fontes explícitas e scratch SRAM4.
- `ST67W6X_Network_Driver/Target/w6x_config.h`: autoconnect em USER CODE.
- Middleware ST: `Api/w6x_api.h`, `Core/w6x_wifi.c`,
  `Driver/W61_at/w61_at_wifi.c`, `Driver/W61_at/w61_at_credential.h`.
- `tests/test_provisioning.py`, `test_provisioning.c`, `test_provision_protocol.c`,
  `test_w6x_connection.c`, `test_w61_credentials.c` e `tests/provisioning_fakes/`.
- `docs/BLE_PROVISIONING.md`: contrato, relatório e roteiro de teste.

Nenhum commit/push executado.

## Roteiro de bancada e próximo passo

Gravar `build/Debug/AuraAssistant_Internal.elf` ou `_Internal.bin` pelo fluxo
existente do projeto. O binário interno corresponde à flash do MCU em
`0x08000000`, conforme o linker. Os assets QSPI não foram alterados nesta tarefa.
Não foi feita gravação remota/automática na placa.

1. Reiniciar e descobrir AuraAssistant pelo UUID. Conferir as duas características.
2. Parear em Android/iOS, assinar Event e confirmar `security:ready`. Repetir com
   telefone já vinculado e após desligar/religar. Conferir versões do módulo nas
   variáveis de diagnóstico existentes.
3. Enviar get_status e scan, sempre com LF e blocos de até 20 bytes. Confirmar
   SSID/RSSI, lista vazia e atualização. Testar SSID manual/oculto.
4. Enviar credenciais de teste WPA2; confirmar connected, IP no roteador e estado
   do módulo. Testar senha errada, AP ausente e DHCP indisponível.
5. Testar SSID/senha com vírgula, aspas, barra invertida, espaços e UTF-8, incluindo
   os limites. Confirmar o conteúdo interpretado pelo NCP sem ativar logs de senha.
6. Desconectar o telefone durante scan/conexão, reconectar e consultar estado.
   Confirmar que nenhuma resposta da sessão anterior é entregue à nova.
7. Desligar/religar sem telefone e confirmar reconexão Wi-Fi. Executar forget,
   inclusive offline, reiniciar e confirmar ausência de conexão automática.
8. Após cinco minutos, confirmar bloqueio de reconfiguração e fim do advertising.
   Reiniciar abre nova janela. Testar timeout de pareamento e segundo cliente BLE.
9. Observar `app_provision_diagnostics`, `app_wifi_diagnostics`, heap livre e
   high-water mark da WiFiTask durante testes reais. Não confundir sucesso dos
   testes simulados com validação RF, de pareamento ou persistência no NCP.

Próximo passo: fornecer este contrato ao Lovable, gravar a imagem interna e
executar primeiro get_status → scan → connect em um smartphone real.
