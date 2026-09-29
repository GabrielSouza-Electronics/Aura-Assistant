#include <gui/common/CircuitField.hpp>
#include <touchgfx/hal/HAL.hpp>
#include <BitmapDatabase.hpp>
#include <math.h>
#include <stdlib.h>

using namespace touchgfx;

extern const int16_t cfRouteX[CF_ROUTES][3];
extern const int16_t cfRouteY[CF_ROUTES][3];
extern const uint16_t cfRouteLen[CF_ROUTES];

/* --- esfera -----------------------------------------------------------
   Centro e raio em coordenadas JA ROTACIONADAS. O centro da tela e' (240,239)
   depois do giro; a esfera fica no meio, com os icones do carrossel
   orbitando por fora.                                                     */
static const float SPH_CX  = 236.0f;    /* era 240 no sistema nao girado */
static const float SPH_CY  = 239.0f;
static const float SPH_R   = 86.0f;
static const float SPH_CAM = 6.0f;

/* Piso de profundidade. A metade de tras da esfera PRECISA aparecer: ela e'
   parte da forma. Com brilho perto de zero sobrava so a calota da frente,
   que le como tigela.                                                     */
static const float DEPTH_FLOOR = 0.45f;

static const float GATHER_K = 0.055f;
static const float YAW_K    = 0.080f;

/* --- sprites ---------------------------------------------------------- */
static const int16_t PULSE_PX[4]  = { 7, 11, 16, 22 };
static const uint16_t PULSE_ID[4] = {
    BITMAP_PULSE_7_0_ID, BITMAP_PULSE_11_0_ID,
    BITMAP_PULSE_16_0_ID, BITMAP_PULSE_22_0_ID
};
static const int16_t RING_PX[3]  = { 14, 20, 28 };
static const uint16_t RING_ID[3] = {
    BITMAP_RINGED_14_0_ID, BITMAP_RINGED_20_0_ID, BITMAP_RINGED_28_0_ID
};
#define SPRITE_FRAMES 8

static uint32_t rngState = 0x1234567u;
static float frnd()
{
    rngState = rngState * 1664525u + 1013904223u;
    return (float)((rngState >> 8) & 0xFFFF) / 65535.0f;
}


CircuitField::CircuitField()
{
    gatherNow = 0.0f;
    yaw = 0.0f;
    frameTick = 0;
    master = 255;

    for (int i = 0; i < CF_COUNT; i++)
    {
        Particle& q = p[i];
        q.route = (uint8_t)(frnd() * (CF_ROUTES - 1));
        q.s = frnd();
        /* rotas mais longas correm um pouco mais devagar, senao as curtas
           parecem paradas em comparacao                                   */
        float L = (float)cfRouteLen[q.route];
        q.v = (0.0030f + 0.0028f * frnd()) * (140.0f / (L + 40.0f));
        q.phase = (uint8_t)(frnd() * SPRITE_FRAMES);
        q.alpha = (uint8_t)(110 + frnd() * 145);

        /* 30% levam anel concentrico. Nem todas: a mistura de pontos
           simples e pontos com anel e' que da relevo ao conjunto.        */
        q.ringed = (frnd() < 0.30f);
        q.sprite = q.ringed ? (uint8_t)(frnd() * 3) : (uint8_t)(frnd() * 4);

        /* destino na esfera: espiral de Fibonacci, que espalha sem os polos
           ficarem mais densos que o equador                               */
        float k = i + 0.5f;
        float phi = acosf(1.0f - 2.0f * k / CF_COUNT);
        float th = 3.14159265f * (1.0f + 2.2360680f) * k;
        q.tx = cosf(th) * sinf(phi);
        q.ty = cosf(phi);
        q.tz = sinf(th) * sinf(phi);

        q.px = q.py = 0;
        q.depth = 0.0f;
        order[i] = (uint16_t)i;
    }
}

void CircuitField::project()
{
    const float g = gatherNow * gatherNow * (3.0f - 2.0f * gatherNow);
    const float cy_ = cosf(yaw), sy_ = sinf(yaw);

    for (int i = 0; i < CF_COUNT; i++)
    {
        Particle& q = p[i];

        /* --- posicao ao longo da rota --- */
        q.s += q.v;
        if (q.s >= 1.0f)
        {
            q.s -= 1.0f;
        }

        const int16_t* rx = cfRouteX[q.route];
        const int16_t* ry = cfRouteY[q.route];
        float d0 = sqrtf((float)((rx[1] - rx[0]) * (rx[1] - rx[0]) +
                                 (ry[1] - ry[0]) * (ry[1] - ry[0])));
        float d1 = sqrtf((float)((rx[2] - rx[1]) * (rx[2] - rx[1]) +
                                 (ry[2] - ry[1]) * (ry[2] - ry[1])));
        float tot = d0 + d1;
        float dd = q.s * tot;
        float bx, by;
        if (dd <= d0 || tot < 1.0f)
        {
            float u = (d0 > 0.5f) ? dd / d0 : 0.0f;
            bx = rx[0] + (rx[1] - rx[0]) * u;
            by = ry[0] + (ry[1] - ry[0]) * u;
        }
        else
        {
            float u = (d1 > 0.5f) ? (dd - d0) / d1 : 0.0f;
            bx = rx[1] + (rx[2] - rx[1]) * u;
            by = ry[1] + (ry[2] - ry[1]) * u;
        }

        /* --- destino na esfera, ja no sistema girado ---
           A projecao normal daria (CX + x1*k, CY - ty*k). Girar 90 a
           esquerda troca os eixos: o que era horizontal vira vertical.   */
        float x1 = q.tx * cy_ + q.tz * sy_;
        float z1 = -q.tx * sy_ + q.tz * cy_;
        float k = (SPH_CAM * SPH_R) / (SPH_CAM - z1);
        float ox = SPH_CX - q.ty * k;
        float oy = SPH_CY + x1 * k;

        q.px = (int16_t)(bx + (ox - bx) * g);
        q.py = (int16_t)(by + (oy - by) * g);
        q.depth = z1 * g;
    }
}

void CircuitField::sortByDepth()
{
    /* insercao: a ordem muda pouco entre frames, entao ela roda quase
       sempre em O(n). Qsort seria mais lento no caso tipico.             */
    for (int i = 1; i < CF_COUNT; i++)
    {
        uint16_t key = order[i];
        float kd = p[key].depth;
        int j = i - 1;
        while (j >= 0 && p[order[j]].depth > kd)
        {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }
}

void CircuitField::tick(float gather, float yawDeg)
{
    if (gather < 0.0f) gather = 0.0f;
    if (gather > 1.0f) gather = 1.0f;
    gatherNow += (gather - gatherNow) * GATHER_K;

    float yt = yawDeg * 0.0174533f;
    yaw += (yt - yaw) * YAW_K;

    frameTick++;
    project();
    if (gatherNow > 0.02f)
    {
        sortByDepth();
    }
    invalidate();
}

void CircuitField::blit(uint16_t id, int16_t cx, int16_t cy,
                        int16_t w, int16_t h, uint8_t a, const Rect& inv) const
{
    if (a <= 3)
    {
        return;
    }
    Rect r((int16_t)(cx - w / 2), (int16_t)(cy - h / 2), w, h);
    Rect dirty = r & inv;
    if (dirty.isEmpty())
    {
        return;
    }
    dirty.x = (int16_t)(dirty.x - r.x);
    dirty.y = (int16_t)(dirty.y - r.y);
    translateRectToAbsolute(r);
    HAL::lcd().drawPartialBitmap(Bitmap(id), r.x, r.y, dirty, a, true);
}

void CircuitField::draw(const Rect& inv) const
{
    const float g = gatherNow;
    const uint8_t frame = (uint8_t)((frameTick / 3) % SPRITE_FRAMES);

    for (int n = 0; n < CF_COUNT; n++)
    {
        const Particle& q = p[order[n]];

        float dep = 1.0f;
        if (g > 0.02f)
        {
            float z01 = q.depth * 0.5f + 0.5f;
            dep = 1.0f + (DEPTH_FLOOR + (1.0f - DEPTH_FLOOR) * z01 - 1.0f) * g;
        }
        int a = (int)(q.alpha * dep * (master / 255.0f));
        if (a > 255) a = 255;

        uint8_t f = (uint8_t)((frame + q.phase) % SPRITE_FRAMES);
        if (q.ringed)
        {
            int16_t sz = RING_PX[q.sprite];
            blit((uint16_t)(RING_ID[q.sprite] + f), q.px, q.py, sz, sz,
                 (uint8_t)a, inv);
        }
        else
        {
            int16_t sz = PULSE_PX[q.sprite];
            blit((uint16_t)(PULSE_ID[q.sprite] + f), q.px, q.py, sz, sz,
                 (uint8_t)a, inv);
        }
    }
}
