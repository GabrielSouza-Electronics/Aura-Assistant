# Diagnóstico da inicialização BLE

## Atualização: bisseção do payload de advertising

Teste A (físico, confirmado): sem `SetAdvData` e sem `SetScanRespData`, mantendo
GATT, `SetSecurityParam` e `SetAdvParam(160,320,0,7)`, o resultado foi
`provision_init_status = 0` e `state = APP_WIFI_STATE_BLE_ADVERTISING`.
Conclusão: o `ERROR` em `AT+BLEADVSTART` vem do conteúdo do advertising e/ou
do scan response enviados antes. Ainda não se sabe qual campo causa o erro.

A chave `AURA_PROV_ADV_MODE` em `App/Src/app_provisioning.c` seleciona:
0 = dados padrão do módulo (teste A), 1 = só o UUID de 128 bits
`11073412908f6e2c7d9a3c4fa3b501a0577e`, sem Flags e sem scan response (teste B,
imagem atual), 2 = payload original (Flags + UUID, nome no scan response).

Teste B (físico, confirmado): advertising iniciado, UUID do serviço visível no
scanner, conexão e troca de MTU (evento 133) funcionando.

Reinício após desconexão: o ST67 (SDK 2.0.89) volta a anunciar sozinho e rejeita
um `AT+BLEADVSTART` redundante com ERROR. Evidência: todas as tentativas falharam,
inclusive a última, e ainda assim o celular reconectou. Correção: `AdvStop`
(resultado ignorado) antes de `AdvStart` em `APP_ProvisionPoll`. Confirmado na
placa: conectar, desconectar (evento 122) e reconectar com `error_count = 0` e
`ble_adv_status = 0`.

Pendente: recolocar apenas o scan response com o nome, para separar Flags de
scan response como causa do erro original; validar o fluxo do app.

## Captura restrita da resposta de AdvStart

O teste com parâmetros explícitos continuou falhando em AdvStart. Assim, esses
parâmetros não resolveram a falha. Foi acrescentado `w61_adv_diagnostics` para
observar linhas já delimitadas pelo parser durante `AT+BLEADVSTART`:

- `attempts`: número de tentativas;
- `terminal`: 0 sem terminal exato observado; 1 OK; 2 ERROR;
- `raw_return`: resultado de modem_cmd_send antes da tradução W61/W6X;
- `error_code_valid` e `error_code`: ERR CODE hexadecimal, se fornecido;
- `active`: captura em andamento, normalmente 0 após a operação.

Não são armazenadas strings, comandos de credenciais ou payloads GATT. A captura
não modifica o reconhecimento de respostas, semáforos, sequência ou timeouts.
O terminal é uma observação das linhas recebidas no intervalo da chamada, não
uma prova de associação em cenários com comandos concorrentes. O fluxo de boot
da aplicação executa esses comandos sequencialmente.

Arquivos alterados: `Driver/W61_at/w61_at_common.c`, `modem_cmd_handler.c`, novo
`w61_adv_diag.h`, `tests/test_w61_gatt.c` e este documento. Foram revisados os
handlers OK/ERROR, SetExecute, registro GATT e advertising do driver local.
Nenhuma alteração de LCD/hardware. O runner passou, incluindo interpretação de
terminais/códigos, descarte de códigos inválidos e ignorar dados GATT.
Build Debug passou: DTCM 124632 bytes (+24), flash 678616 bytes; mesmo aviso objcopy.

Próximo teste: gravar a imagem interna atual, reiniciar, Continue e pausar somente
após o erro. Capturar a estrutura inteira `w61_adv_diagnostics`. Isso não é uma
correção confirmada: o objetivo é distinguir rejeição do módulo de erro interno
do transporte/parser antes de alterar novamente o comportamento.

## Atualização: falha ao iniciar advertising

Novo teste físico: BLE Init, provisionamento GATT e leitura do endereço retornam
0; `ble_adv_status = 2`, primeira função `W6X_Ble_AdvStart`. O problema anterior
de criação de serviço não aparece nesse teste. Versões informadas pelo módulo:
SDK 2.0.89 e AT 1.0.0. Não há evidência suficiente para atribuir a falha à versão.

O anúncio contém 21 bytes e a resposta ao scan 15 bytes, com estruturas AD de
comprimento consistente. O fluxo não configurava os parâmetros de advertising.
Agora `APP_ProvisionInit` chama `W6X_Ble_SetAdvParam(160,320,0,7)` antes de
configurar os dados: 100–200 ms, conectável/scannable, canais 37/38/39. Valores
do exemplo da [documentação ST](https://wiki.st.com/stm32mcu/index.php?oldid=77649&title=Connectivity%3AST67W611M_AT_Command),
confirmados pelos tipos/API locais. O conteúdo antigo ainda aparece no índice
de busca, mas o acesso direto a essa revisão retorna erro. Não foram usados
comandos de módulos ESP como substitutos. `app_provision_diagnostics.adv_param_status`
registra o resultado (UINT32_MAX enquanto não executado).

Arquivos inspecionados: driver W61 BLE, API/tipos W6X e fluxo da aplicação.
Modificados: `App/Inc/app_provisioning.h`, `App/Src/app_provisioning.c`,
`tests/provisioning_fakes/w6x_api.h`, `tests/test_provisioning.c`,
`tests/test_w61_gatt.c`, `tests/test_provisioning.py` e este relatório.
Sem alterações de LCD, pinos, clocks, parser ou transporte SPI.

O teste verifica o comando AT completo e exige configuração de parâmetros antes
dos dados. Runner de testes passou; `cmake --build --preset Debug` passou.
DTCM: 124608 bytes; flash: 678160 bytes. Permanece o aviso de objcopy documentado.
Esta alteração remove dependência dos parâmetros padrão; a correção do erro
AdvStart ainda precisa ser confirmada fisicamente. Regravar a imagem interna e
executar desde reset sem pausas. Esperado: adv_param_status/provision_init_status/
ble_adv_status em 0 e estado BLE_ADVERTISING. Se persistir, registrar esses campos
e a primeira função de erro, sem presumir falha no reconhecimento de OK.

## Revisão das alterações contra a versão anterior

Comparação com HEAD (`6d8eecb`) e histórico do parser:

- Antes: Wi-Fi Init → BLE Init → consulta de endereço → advertising.
- Agora: Wi-Fi Init → BLE Init → serviço/características GATT → segurança e
  dados de advertising → consulta de endereço → advertising.
- A etapa GATT foi introduzida pelo provisionamento. A falha registrada em
  CreateService impede chegar ao advertising; essa chamada não existia no
  fluxo anterior que aparecia no scanner.
- `modem_cmd_handler.c`, `w61_at_common.c`, `w61_at_ble.c` e transporte SPI
  não têm alterações contra HEAD. Os handlers de OK/ERROR não foram modificados.
- A adaptação de recepção de `\r\nready\r\n` está no commit `7552324` e foi
  removida em `6e9d0ff`, antes da versão base desta tarefa. Não foi removida
  pelas alterações de provisionamento.
- Autoconnect mudou de 0 para 1 para reconexão após reboot. É uma diferença
  real de inicialização, mas não há evidência de que cause o erro GATT observado.
- As correções de escape e eventos Wi-Fi afetam Connect/Disconnect/DeleteCredentials,
  que ainda não foram chamados quando a criação do serviço falha.

Os testes anteriores simulavam sucesso na API CreateService. A revisão adicionou
`tests/test_w61_gatt.c`, integrado em `tests/test_provisioning.py`, que compila as
funções W61 reais e verifica comandos completos: UUIDs, índices, propriedades,
permissões e CRLF. O transporte continua simulado: o teste não comprova que o
firmware instalado no módulo aceita os comandos.

Todos os testes passaram e `cmake --build --preset Debug` retornou sucesso
(`ninja: no work to do`). Nesta revisão foram alterados apenas os dois arquivos
de testes e este relatório; o firmware continua com o ajuste de UUID compacto
descrito abaixo. Sem impacto adicional em memória ou configuração de LCD/hardware.

## Atualização: erro ao criar serviço GATT

O teste seguinte na placa apresentou `ble_init_status = 0`,
`provision_init_status = 2` e primeira função de erro `W6X_Ble_CreateService`.
Portanto, nesse teste, a inicialização BLE passou e o advertising não foi
executado devido à falha de criação do serviço.

Inspecionados: inicialização de provisionamento, wrappers W6X, implementação
W61 de criação de serviço/característica e testes simulados. O driver W61 envia
o UUID recebido diretamente no comando AT. Apesar do comentário local mencionar
hífens, os exemplos oficiais usam 32 dígitos hexadecimais sem separadores:
[ST67 AT GATT commands](https://applist67.github.io/Web_AT_Documentation_ST67/v2.0.106/index.html#at-blegattssrvcre).

Alterados `App/Inc/app_provisioning.h` para usar essa representação compacta nos
três UUIDs e `tests/test_provisioning.c` para validar comprimento e caracteres
aceitos na interface AT. O UUID lógico e o contrato do smartphone permanecem
iguais; o app continua usando `7e57a001-b5a3-4f3c-9a7d-2c6e8f901234` para o serviço.
Este documento também foi atualizado. Nenhuma configuração de LCD, hardware,
clock ou temporização foi modificada; não foi necessária nova tabela de datasheet.

Build Debug e todos os testes do runner abaixo passaram novamente. DTCM permanece
124608 bytes; flash interna agora 677952 bytes. Mesmo aviso de objcopy descrito
abaixo. A hipótese de rejeição do UUID com hífens ainda exige confirmação física.
Gravar a nova imagem interna, reiniciar e executar sem pausas; conferir
`provision_init_status = 0`, `ble_adv_status = 0` e
`state = APP_WIFI_STATE_BLE_ADVERTISING`. Se falhar, coletar novamente a primeira
função de erro e as versões SDK/AT em `app_wifi_diagnostics`.

O registro original `ble_init_status = 3` representa `W6X_STATUS_TIMEOUT`.
Ele não identifica qual comando falhou dentro de `W6X_Ble_Init`.
O usuário confirmou execução contínua desde main, pausando somente após o erro.
A causa do timeout ainda depende de observação na placa.

## Teste sem smartphone

1. Grave `build/Debug/AuraAssistant_Internal.elf` pelo fluxo de debug existente.
2. Reinicie, dê Continue em main e aguarde aproximadamente 10 segundos.
3. Pause e adicione ao Watch:

```text
w6x_ble_init_diagnostics
app_wifi_diagnostics
app_wifi_diagnostics.first_driver_error_function
app_wifi_diagnostics.last_driver_error_function
```

Registre `step`, `status`, `cleanup_status`, `step_elapsed_ticks` e os nomes das
funções de erro. Os ticks usam a base do FreeRTOS. Não coloque breakpoints dentro
das transações AT durante esse teste.

| Etapa | Operação |
| --- | --- |
| GET_POWER_MODE | Consulta de economia de energia |
| GET_CLOCK_SOURCE | Consulta do clock quando power save está ativo |
| DISABLE_POWER_SAVE | Desativação condicional prevista pelo middleware |
| REGISTER_CALLBACK | Registro do callback BLE no driver |
| SEND_BLEINIT | Inicialização BLE no módulo |
| VERIFY_MODE | Consulta e confirmação do modo BLE |
| SET_NAME | Configuração do nome AuraAssistant |
| READY | Inicialização BLE concluída |

`status = 3` indica timeout na etapa registrada. `cleanup_status` é separado:
uma falha ao desinicializar não apaga a causa original. `4294967295` (`UINT32_MAX`)
indica operação não executada ou valor ainda desconhecido, conforme o campo.
`ble_adv_status = 0` agora só aparece depois de advertising iniciado com sucesso.
`READY` nesta estrutura confirma apenas W6X_Ble_Init; confira também
`provision_init_status`, `ble_adv_status` e o estado da aplicação.

## Alterações e validação

Arquivos inspecionados: `App/Src/app_wifi.c`, `App/Inc/app.h`, driver BLE/Wi-Fi ST,
configuração ST67, inicialização gerada, IOC e esquema atual da placa.
Alterados nesta investigação: `App/Inc/app.h`, `App/Src/app_wifi.c`,
`Middlewares/ST/ST67W6X_Network_Driver/Core/w6x_ble.c`, novo
`Core/w6x_ble_init_diag.h`, `tests/test_provisioning.py`, novo
`tests/test_w6x_ble_init.c` e este documento.

O middleware agora propaga falha na consulta do modo e preserva o código de erro
da configuração do nome. A aplicação retém a primeira função que reportou erro.
Diagnósticos fixos, sem tarefa ou heap adicional. As fontes de comportamento são
as implementações locais do middleware ST. Configuração de LCD, clock, SPI,
boot e tempos limite permanecem conforme o projeto; nenhuma nova sequência
de datasheet foi introduzida nesta investigação.

Validação executada:

```text
cmake --build --preset Debug
python tests/test_provisioning.py --compiler C:/ST/STEdgeAI/4.0/Utilities/windows/mingw64/bin/gcc.exe
```

Build e testes passaram. O teste de inicialização compila as funções reais do
middleware com transporte simulado: sucesso, timeouts em cinco etapas, falha
de cleanup, modo incorreto e callback ausente. Os testes anteriores de
provisionamento também passaram.

DTCM: 124608 bytes (95,07%; acréscimo de 64 bytes). Flash interna: 677968 bytes.
Permanece o aviso de objcopy sobre segmento vazio em 0x90000000 na geração da
imagem interna. Não houve gravação ou validação física nesta investigação.
Próximo passo: executar o roteiro acima e coletar a etapa exata que falha.
