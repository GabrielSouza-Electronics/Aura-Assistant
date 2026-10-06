# Prompt para enviar ao Lovable — AuraAssistant

Implemente a aplicação web e uma API HTTPS para sincronizar um dispositivo
AuraAssistant STM32H743 via Wi-Fi. O firmware já contém o cliente para o
contrato abaixo. Não altere nomes de campos, rotas, tipos ou respostas sem
coordenar com o firmware.

## Aplicação web

- CRUD de tarefas: título, deadline com data e horário, categoria
  `Personal`, `Work`, `Priority` ou `Project`, e estado `completed`.
- Exibir tarefas por deadline crescente. Em empate, ordenar por ID.
- Mostrar imediatamente mudanças feitas no site e conclusões vindas do
  dispositivo, usando os recursos de atualização do backend escolhido.
- Tela de calendário com feriados configuráveis. Entregar o ano pedido pelo
  dispositivo, inclusive anos diferentes do ano corrente.
- Relógio do servidor em `Asia/Dubai` (UTC+4), mesma zona das deadlines.
  O contrato inicial do firmware aceita essa zona, anos 2020 a 2099.
- Tela de gerenciamento de dispositivos: gerar, revogar e renovar tokens
  exclusivos por dispositivo, com acesso apenas às informações do proprietário.

## Autenticação e transporte

O dispositivo envia `Authorization: Bearer <device_token>`. Validar o token
no backend e associá-lo ao usuário/dispositivo. Não usar tokens administrativos
ou chaves de service role no firmware ou no navegador. Armazenar o hash dos
tokens no backend; exibir o segredo apenas na geração.

HTTPS porta 443, TLS 1.2, SNI, HTTP/1.1. Responder JSON UTF-8, sem gzip/brotli,
com `Content-Length` correto ou `Transfer-Encoding: chunked`. Não usar redirects.
GET e PUT bem-sucedidos retornam **HTTP 200**, inclusive quando não há tarefas
ou uma operação de conclusão já foi aplicada. O firmware não aceita 204/304.
Respostas de erro de autenticação podem usar 401/403; o dispositivo preserva
os últimos dados e tenta novamente com backoff.

Limite da resposta HTTP completa: 16.384 bytes. Limite de tarefas do dispositivo:
32. A projeção para o dispositivo deve ser explícita no site: permitir escolher
até 40 tarefas sincronizadas, e não descartar tarefas silenciosamente. Títulos
para o dispositivo: ASCII imprimível, de 1 a 63 bytes, sem quebras de linha.
O site pode guardar o título Unicode completo e um `deviceTitle` transliterado;
o campo `title` da API abaixo deve conter `deviceTitle`. A UI abrevia visualmente
nomes longos com `..`; não é necessário enviar abreviações prontas.

## GET /api/device/v1/state?calendar_year=2026

Retornar um snapshot consistente da conta do dispositivo:

```json
{
  "schemaVersion": 1,
  "revision": 1,
  "timeZone": "Asia/Dubai",
  "clock": {
    "year": 2026, "month": 10, "day": 6,
    "hour": 14, "minute": 30, "second": 0
  },
  "calendar": {
    "year": 2026,
    "holidays": ["2026-12-02", "2026-12-03"]
  },
  "tasks": [
    {
      "id": 1,
      "title": "Prepare project report",
      "category": "Project",
      "deadline": {
        "year": 2026, "month": 10, "day": 6,
        "hour": 16, "minute": 0, "second": 0
      },
      "completed": false
    }
  ]
}
```

- `revision`: inteiro de 1 a 4.294.967.295, monotonicamente crescente por conta
  quando tarefas/calendário mudam. Não incrementar só porque o relógio avançou.
  Não reutilizar/reduzir uma revisão. A mesma revisão pode ser entregue em
  múltiplos polls, com `clock` atualizado e outro `calendar_year`.
- `id`: inteiro estável, único, de 1 a 4.294.967.295; nunca reutilizar para
  outra tarefa. Não enviar UUIDs ou números como strings.
- `completed`: booleano JSON, não 0/1/string.
- `deadline`: objeto com seis inteiros; não enviar ISO strings nem timestamps
  Unix neste campo. Data/horário locais coerentes com `Asia/Dubai`.
- `calendar.year` deve corresponder ao parâmetro solicitado; todos os feriados
  devem pertencer a esse ano. Se não houver feriados, enviar `[]`.
- Não omitir `tasks`, `clock`, `calendar`, `holidays` ou campos obrigatórios.
  Lista vazia de tarefas é válida: `"tasks": []`.
- O firmware reordena as tarefas pelo deadline completo e exibe quatro por página.
- Ontem: `Yesterday • HH:MM`; hoje: `HH:MM`; amanhã: `Tomorrow • HH:MM`;
  demais datas: `DD/MM • HH:MM`. O firmware calcula esses textos; não enviá-los.

## PUT /api/device/v1/tasks/{id}/completion

Autenticação igual ao GET. Body:

```json
{ "completed": true }
```

Operação idempotente: repetir a requisição não alterna o estado e não cria
eventos duplicados. Se aplicada ou já aplicada, retornar HTTP 200:

```json
{ "schemaVersion": 1, "id": 1, "accepted": true }
```

Se o usuário autenticado tem acesso à conta, mas a tarefa foi removida ou não
pode mais ser concluída, retornar HTTP 200 com `accepted: false` e o mesmo ID.
O firmware desfaz a conclusão otimista. Falhas de transporte e erros HTTP deixam
a conclusão pendente para nova tentativa; não usar `accepted: false` para erro
transitório. Verificar propriedade da tarefa no backend antes de qualquer escrita.

Quando a conclusão altera o estado, incrementar `revision` em transação com
a atualização. O GET seguinte deve refletir o valor de `completed` solicitado (`true` ou `false`). A reabertura de
uma tarefa pelo site também deve incrementar `revision`.

## Atualização e comportamento offline

O firmware faz polling a cada 2 segundos após concluir o ciclo de rede. Isso é
atualização próxima de tempo real, não uma conexão WebSocket. Latência TLS/rede
e timeout aumentam o intervalo. Falhas usam backoff até 60 segundos; a GUI nunca
espera pela rede. Quando offline, preserva os últimos dados e a hora continua
avançando localmente. Conclusões pendentes são mantidas em RAM durante a sessão;
não há fila persistente que sobreviva a um reboot.

## Entregar ao engenheiro após publicar

1. Host HTTPS sem `https://` nem caminho, atendendo as rotas acima.
2. Token exclusivo do dispositivo, enviado de forma privada.
3. Cadeia TLS e certificado da autoridade raiz PEM para validar o host.
4. Exemplos reais de GET e PUT, inclusive conta vazia, 40 tarefas e conclusão repetida.
5. Confirmação de que o servidor não comprime nem redireciona as respostas.

## Ativação no firmware

Configurar `App/Inc/app_web_config.h` com `APP_WEB_HOST`,
`APP_WEB_DEVICE_TOKEN` e `APP_WEB_ROOT_CA_PEM` reais. Para um PEM multilinha,
usar uma string C com linhas concatenadas e `\n`. Não colocar tokens reais
em commits. Pode-se fornecer os valores por configuração privada de build.

Ativar o cliente e compilar:

```text
cmake --preset Debug -DAURA_WEB_ENABLED=ON
cmake --build --preset Debug
```

Sem configuração da API, `AURA_WEB_ENABLED=OFF` mantém as fontes públicas de
calendário/horário existentes e Tasks fica aguardando sincronização. Com `ON`,
o worker usa exclusivamente esta API para tarefas, relógio e feriados.
Programar a flash interna e também `build/Debug/AuraAssistant_ExternalFlash.bin`.

## Gestos do dispositivo

Todos os menus voltam ao carrossel ao manter o dedo a menos de 30 mm do
ToF por 2 segundos. Em Tasks, aproximar a menos de 30 mm e retirar antes
de 2 segundos alterna a tarefa selecionada: concluir toca o som de sucesso;
reabrir remove o risco e toca um som diferente.
A conclusão ocorre na retirada; segurar por 2 segundos somente volta,
sem concluir. O IMU não participa dessas ações. Enquanto o dedo estiver
próximo, o foco e a página não mudam. Navegação lateral e vertical continua
pelo ToF com o dedo afastado. Em Settings, esquerda encerra o ajuste de
um valor; a saída do menu usa o mesmo gesto de 2 segundos.

## Critérios de aceite do backend

Testar isolamento entre dois usuários/dispositivos, token revogado, projeção
de 0/1/4/5/40 tarefas, deadlines em 29/02 e na virada do ano, todas as categorias,
conclusão repetida, remoção durante conclusão, edição simultânea, revisões
monotônicas, feriados de outro ano e limites de payload. Não requerer segredo
administrativo no dispositivo.


## Reminders (lembretes)

O firmware agora possui um menu Reminders independente de Tasks, com o mesmo
layout, tags, deadline, quatro itens por pagina e ordenacao cronologica.
Inclua no GET `/api/device/v1/state` um campo `reminders` ao lado de `tasks`.
Cada lembrete usa os mesmos campos: `id`, `title`, `category`, `completed` e
`deadline`. IDs precisam ser unicos dentro da propria lista; uma task e um
reminder podem compartilhar o mesmo ID sem compartilhar estado.

```json
"reminders": [
  {
    "id": 1,
    "title": "Call family",
    "category": "Personal",
    "completed": false,
    "deadline": { "year": 2026, "month": 10, "day": 7,
                  "hour": 9, "minute": 0, "second": 0 }
  }
]
```

Use `PUT /api/device/v1/reminders/{id}/completion` com `{ "completed": true }`
ou `{ "completed": false }`. Autenticacao, resposta `schemaVersion/id/accepted`,
idempotencia, propriedade e incremento de `revision` seguem as regras de Tasks.
O GET seguinte deve refletir a alteracao. Limite: 40 lembretes e 40 tasks,
com titulos ASCII de ate 63 caracteres. A resposta HTTP deve caber em 32 KiB;
use JSON compacto e respeite esse limite incluindo ambas as listas.

Para apagar todos os lembretes envie `reminders: []`. Temporariamente, a ausencia
do campo e aceita para compatibilidade com APIs antigas e preserva a lista
existente no device. Antes da primeira sincronizacao o menu tem oito exemplos;
eles desaparecem ao receber a primeira lista real. Os exemplos reiniciam no
boot e suas conclusoes nunca sao enviadas ao servidor.

Clique curto pelo ToF alterna concluido/reaberto, com sons distintos; manter o
dedo proximo por 2 segundos volta ao carrossel. Este menu nao agenda notificacoes
ou alarmes automaticos: esta implementacao cobre a lista e seus gestos, como Tasks.


## Progresso no device
Tasks e Reminders aceitam ate 40 itens por lista. O cabe?alho mostra conclu?dos/total
e um arco com 40 passos de 2,5%, arredondado ao passo mais pr?ximo. Com 40 itens,
cada conclus?o acrescenta exatamente 2,5%; com menos itens, a propor??o depende
do total e o contador continua exato. O progresso inclui todas as paginas e
considera altera??es locais pendentes; reabrir um item reduz o arco.
