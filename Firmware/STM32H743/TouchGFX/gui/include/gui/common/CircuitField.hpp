#ifndef CIRCUITFIELD_HPP
#define CIRCUITFIELD_HPP

#include <touchgfx/widgets/Widget.hpp>
#include <touchgfx/Bitmap.hpp>

/*
 * Particulas correndo pelos traços da placa, e a esfera que elas formam
 * quando a mao se aproxima.
 *
 * E o unico elemento da cena que nao e' widget nativo: sao 150 objetos
 * recalculados por frame e nao existe widget do Designer para isso. Todo o
 * resto - fundo, logo, icones, textos, status, borda - esta no Designer e
 * pode ser movido e editado la.
 *
 * ROTACAO
 * O display esta em portrait com o painel nativamente landscape, entao a
 * cena inteira e' desenhada girada. Em vez de girar cada sprite em tempo de
 * execucao, giro o SISTEMA DE COORDENADAS no fim da projecao: duas
 * operacoes por particula, e toda a matematica de orbita fica intacta.
 */

#define CF_COUNT   150          /* particulas                               */
#define CF_ROUTES  45           /* rotas gravadas em CircuitRoutes.cpp      */

class CircuitField : public touchgfx::Widget
{
public:
    CircuitField();

    virtual void draw(const touchgfx::Rect& invalidatedArea) const;
    virtual touchgfx::Rect getSolidRect() const { return touchgfx::Rect(); }

    /* Uma vez por tick. gather 0..1 vem do OrbitalMenu: 0 = particulas nos
       traços, 1 = esfera formada. yawDeg acopla a rotacao da esfera ao giro
       do carrossel.                                                        */
    void tick(float gather, float yawDeg);

    void setMasterAlpha(uint8_t a) { master = a; }

private:
    struct Particle
    {
        uint8_t  route;      /* qual rota percorre                          */
        uint8_t  sprite;     /* indice do sprite (tamanho)                  */
        uint8_t  phase;      /* deslocamento na animacao de cintilacao      */
        uint8_t  alpha;      /* brilho proprio                              */
        bool     ringed;     /* leva anel concentrico?                      */
        float    s;          /* posicao 0..1 ao longo da rota               */
        float    v;          /* velocidade                                  */
        float    tx, ty, tz; /* destino na esfera (unitario)                */
        int16_t  px, py;     /* posicao projetada                           */
        float    depth;      /* z apos rotacao, para ordenar                */
    };

    Particle p[CF_COUNT];
    uint16_t order[CF_COUNT];

    float gatherNow;
    float yaw;
    uint32_t frameTick;
    uint8_t master;

    void project();
    void sortByDepth();
    void blit(uint16_t id, int16_t cx, int16_t cy, int16_t w, int16_t h,
              uint8_t a, const touchgfx::Rect& inv) const;
};

#endif
