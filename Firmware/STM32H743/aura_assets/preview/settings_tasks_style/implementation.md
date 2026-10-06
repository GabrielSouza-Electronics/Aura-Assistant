# Settings — visual de Tasks

## Files inspected
Screen1View.cpp/.hpp, SettingsLogic.cpp, SettingsLayout.hpp,
GlyphText.cpp/.hpp, CalendarWidget.cpp, PixelDataWidget.cpp,
gen_settings.py, preview_tasks.py, CMakeLists.txt e CMakePresets.json.

## Files modified
- Screen1View.cpp/.hpp: cartões e indicador ciano com PixelDataWidget;
  valores abaixo dos nomes; transição e interações existentes preservadas.
- GlyphText.cpp/.hpp: sprites Poppins Medium 11 px, com caixa ASCII preservada.
- CMakeLists.txt: inclusão explícita dos assets customizados.
- aura_assets/gen/gen_settings_tasks_style.py: gerador e verificações.
- aura_assets/compiled/settings_style/SettingsStyleAssets.cpp/.hpp:
  assets ARGB8888 na seção ExtFlashSection.
- aura_assets/preview/settings_tasks_style/: prévia e sprites de origem.

## LCD hardware configuration confirmed
Nenhuma configuração de hardware foi alterada. Integração usa a convenção
Portrait dos sprites existentes e a transformação (x,y) -> (y,479-x).
Não foi feita nova auditoria elétrica ou de timings nesta alteração visual.

## Datasheet / initialization sources used
Não aplicável: alteração apenas da interface. Usado o caminho de sprites
PixelDataWidget já presente no calendário e nos textos de inicialização.

## Any unresolved assumptions
Renderização, rotação e contraste ainda precisam de inspeção no display.
O atlas compacto cobre ASCII imprimível, não caracteres Unicode acentuados.
O menu Tasks e sua sincronização web continuam fora desta implementação.

## Architecture implemented
UI -> SettingsLogic/Presenter existentes. Nenhuma operação de rede ou
controle de hardware foi adicionada à View. Dados dinâmicos continuam
vindo pelo Model. Assets ficam fora dos diretórios gerados do TouchGFX.
Não há novas alocações dinâmicas ou buffers de imagem na SRAM.

## Build command
`cmake --build --preset Debug`, usando CMake 4.3.1 e GNU Arm 14.3.1
dos bundles STM32, com os executáveis do toolchain no PATH do processo.

## Build result
Sucesso, exit code 0. Verificados limites de largura dos nomes, etiquetas,
valores representativos e bordas transparentes dos 103 sprites.
Aviso de objcopy: empty loadable segment em 0x90000000, também presente
no build anterior. Nenhum teste físico executado.

## Memory impact if meaningful
- QSPI: 4.891.276 bytes (29,15% de 16 MiB); incremento de 714.256 bytes.
- DTCM: 125.456 bytes (95,72%); incremento de 24 bytes.
- Flash interna: 723.528 bytes; incremento de 664 bytes.
- Framebuffer AXI: 450 KiB, sem alteração.

## Hardware tests to perform when PCB arrives
Programar também a imagem atualizada da QSPI. Conferir fonte e rotação,
movimento do indicador ciano, transição do título, ajustes de Bluetooth,
brilho e volume, valores longos, indicadores de Wi-Fi/bateria e saída.

## Recommended next step
Validar visualmente Settings no display, incluindo a seleção em movimento.
