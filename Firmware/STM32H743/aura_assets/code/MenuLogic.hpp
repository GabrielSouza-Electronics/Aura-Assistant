#ifndef MENULOGIC_HPP
#define MENULOGIC_HPP

#include <stdint.h>

/*
 * Maquina de estados da interacao. NAO DESENHA NADA.
 *
 * Nesta versao os icones, rotulos e aneis sao widgets do Designer, que voce
 * pode mover e editar la. Esta classe so calcula ONDE cada um deve estar e
 * QUAL deve ser seu estado; a View aplica nos widgets.
 *
 * Separar as duas coisas tem uma vantagem pratica: voce pode reposicionar
 * qualquer elemento no Designer sem tocar na logica, e pode testar a logica
 * sem desenhar nada.
 *
 * AS CINCO DECISOES DE INTERACAO, validadas em simulador Python antes de
 * virarem C++:
 *
 *  1. SNAP MAGNETICO. O carrossel e atraido para o multiplo de 72 graus mais
 *     proximo. Sem isso o usuario nunca sabe se ja selecionou, e a duvida
 *     incomoda mais que a lentidao.
 *  2. ZONA MORTA. Mao parada tremendo nao gira nada.
 *  3. HISTERESE. Depois de trocar de opcao e preciso passar de 60% do
 *     caminho para trocar de novo, senao pisca na fronteira.
 *  4. DWELL com anel de progresso. Nao ha clique com ToF.
 *  5. ARMED. Depois de cancelar, novas confirmacoes ficam bloqueadas ate o
 *     usuario girar o carrossel ou tirar a mao. Sem isso o dwell continuava
 *     correndo e reabria sozinho o menu que a pessoa acabara de recusar.
 */

#define ML_COUNT 5

class MenuLogic
{
public:
    MenuLogic();

    /* Uma vez por tick, com o estado do ToF. */
    void tick(bool handPresent, float handX, float handY);

    /* --- estado --------------------------------------------------------- */
    int   getScreen() const { return screen; }      /* -1 = carrossel        */
    int   getSelected() const { return sel; }
    float getVisibility() const { return vis; }     /* 0..1                  */
    float getAngle() const { return angle; }
    float getTiltNorm() const { return tilt / 26.0f; }
    int   getDwellStage() const;                    /* -1, ou 0..15          */

    /* 0 = nada, 1 = entrou num menu, 2 = saiu. Devolve uma vez e limpa. */
    int takeNavEvent() { int e = navEvent; navEvent = 0; return e; }

    /* Pose de um icone do carrossel, em coordenadas JA ROTACIONADAS.
       size recebe 36, 48 ou 64; alpha ja considera profundidade e vis.   */
    void getIconPose(int i, int16_t& cx, int16_t& cy,
                     int16_t& size, uint8_t& alpha) const;

    /* Qual mensagem de estado mostrar: 0 hold, 1 cancelled, 2 move */
    int getStateMsg() const;

private:
    float angle;
    int   sel;
    int   dwell;
    bool  armed;
    int   screen;
    int   msg;
    bool  cancelled;
    int   actHold;
    int   navEvent;
    float tilt;
    float vis;
    float grow[ML_COUNT];

    void open(int idx);
    void back();
};

#endif
