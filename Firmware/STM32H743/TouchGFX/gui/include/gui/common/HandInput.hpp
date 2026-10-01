#ifndef HANDINPUT_HPP
#define HANDINPUT_HPP

/*
 * Regras de entrada da mao COMPARTILHADAS pelo carrossel (MenuLogic) e pelo
 * menu Settings (SettingsLogic). Ajustar aqui muda os dois juntos.
 *
 * Entrada: handX / handY normalizados -1..+1 em relacao ao CENTRO do sensor,
 * positivo = direita / cima (APP_HandTracking_ReadPointer). E um joystick
 * absoluto: a posicao da mao define direcao e velocidade.
 *
 *   |eixo| <= CENTER_ENTER  -> mao "no centro": para e alinha no item mais
 *                              proximo (CENTER_GAIN por tick)
 *   |eixo| >= CENTER_EXIT   -> volta a mover; entre os dois vale o estado
 *                              anterior (histerese)
 *   movimento               -> SPIN_GAIN por tick, proporcional a posicao
 *   troca de item           -> so depois de passar HYST do caminho
 *   acao (entrar/cancelar)  -> mao alem de ACT_THRESHOLD por ACT_HOLD ticks
 *   voltar de um submenu    -> mao para BAIXO por BACK_HOLD ticks (500 ms)
 */
namespace HandInput
{
static const float PI_F          = 3.14159265f;
static const float SPIN_GAIN     = -0.055f;  /* rad/tick no carrossel; negativo = sentido invertido */
static const float HYST          = 0.60f;
static const float CENTER_ENTER  = 0.20f;
static const float CENTER_EXIT   = 0.35f;
static const float CENTER_GAIN   = 0.14f;
static const float CENTER_EPS    = 0.003f;   /* rad no carrossel */
static const float ACT_THRESHOLD = 0.55f;
static const int   ACT_HOLD      = 12;       /* 200 ms a 60 Hz */
static const int   BACK_HOLD     = 30;       /* 500 ms a 60 Hz: "pull down to go back" */
}

#endif
