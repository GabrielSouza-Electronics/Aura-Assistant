// Teste de host da navegacao do Settings (SettingsLogic + regras HandInput).
// Nao entra no build do firmware. Para rodar (MinGW do TouchGFX):
//   set PATH=C:\TouchGFX\4.26.1\env\MinGW\bin;%PATH%
//   g++ -std=gnu++17 -Wall -Wextra -ITouchGFX/gui/include Tests/host/test_settings_nav.cpp ^
//       TouchGFX/gui/src/common/SettingsLogic.cpp -o test_nav.exe
//   test_nav.exe
//
// Simula o pipeline do ToF: amostra nova a cada 12 ticks da GUI (5 Hz @ 60 Hz),
// filtro do App ((anterior + bruto) / 2, inteiro -10..10, zona morta +-1),
// valor mantido entre amostras.
#include <gui/common/SettingsLogic.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

struct Tof
{
    bool init = false;
    int fx = 0, fy = 0, tick = 0;
    static int clampAxis(int v) { v = v > 10 ? 10 : (v < -10 ? -10 : v); return (v >= -1 && v <= 1) ? 0 : v; }
    void step(bool present, float x, float y, float& ox, float& oy, bool& op)
    {
        if (!present) { init = false; op = false; tick = 0; ox = oy = 0; return; }
        if (tick % 12 == 0)
        {
            int rx = clampAxis((int)lroundf(x * 10)), ry = clampAxis((int)lroundf(y * 10));
            if (!init) { fx = rx; fy = ry; init = true; }
            else { fx = clampAxis((fx + rx) / 2); fy = clampAxis((fy + ry) / 2); }
        }
        tick++;
        op = true; ox = fx / 10.0f; oy = fy / 10.0f;
    }
};

struct Result { int moves = 0, bumps = 0, enters = 0, leaves = 0, exits = 0, plus = 0, minus = 0; };

/* roda o Settings aberto; path(t) devolve a mao BRUTA. check(t, s) opcional. */
static Result run(SettingsLogic& s, int ticks,
                  void (*path)(int, bool&, float&, float&),
                  void (*check)(int, SettingsLogic&) = nullptr,
                  unsigned seed = 1, float noise = 0.0f)
{
    srand(seed);
    Tof tof; Result r;
    for (int t = 0; t < ticks; t++)
    {
        bool p = true; float x = 0, y = 0; path(t, p, x, y);
        if (noise > 0) { x += noise * (rand() / (float)RAND_MAX * 2 - 1); y += noise * (rand() / (float)RAND_MAX * 2 - 1); }
        float fx, fy; bool fp; tof.step(p, x, y, fx, fy, fp);
        s.tick(r.exits == 0, fp, fx, fy, false);
        int8_t it, d;
        if (s.takeRequest(it, d)) (d > 0 ? r.plus : r.minus)++;
        switch (s.takeFeedback())
        {
        case SettingsLogic::Feedback::Move:  r.moves++; break;
        case SettingsLogic::Feedback::Bump:  r.bumps++; break;
        case SettingsLogic::Feedback::Enter: r.enters++; break;
        case SettingsLogic::Feedback::Leave: r.leaves++; break;
        default: break;
        }
        if (s.takeExit()) r.exits++;
        if (check) check(t, s);
    }
    return r;
}

/* abre o menu e espera a cascata terminar, mao no centro */
static void openMenu(SettingsLogic& s)
{
    run(s, 60, [](int, bool& p, float& x, float& y) { p = true; x = y = 0; });
}

static void focusBluetooth(SettingsLogic& s)
{
    while (s.getFocus() == 0) s.tick(true, true, 0, -1, false);
    for (int i=0; i<90; ++i) s.tick(true, true, 0, 0, false);
    s.takeFeedback();
}

int main()
{
    printf("1) mao parada no centro 5 s -> nada acontece\n");
    {
        SettingsLogic s; openMenu(s);
        Result r = run(s, 300, [](int, bool& p, float& x, float& y) { p = true; x = 0.1f; y = -0.1f; }, nullptr, 3, 0.12f);
        CHECK(r.moves == 0 && r.enters == 0 && r.exits == 0 && s.getFocus() == 0, "moves %d enters %d exits %d foco %d", r.moves, r.enters, r.exits, s.getFocus());
    }

    printf("2) mao para BAIXO ~1 s: desce linhas; volta ao centro: alinha e para\n");
    {
        SettingsLogic s; openMenu(s);
        Result r = run(s, 200, [](int t, bool& p, float& x, float& y) { p = true; x = 0; y = (t < 50) ? -0.8f : 0.0f; });
        printf("   foco final %d, glow %.3f\n", s.getFocus(), s.getGlowRow());
        CHECK(s.getFocus() >= 1 && s.getFocus() <= 3, "foco %d", s.getFocus());
        CHECK(fabsf(s.getGlowRow() - s.getFocus()) < 0.01f, "nao alinhou: %.3f", s.getGlowRow());
        CHECK(r.moves == s.getFocus(), "moves %d", r.moves);
    }

    printf("3) segura para BAIXO no fim da lista -> para na ultima e bate UMA vez\n");
    {
        SettingsLogic s; openMenu(s);
        Result r = run(s, 400, [](int t, bool& p, float& x, float& y) { p = true; x = 0; y = (t < 350) ? -1.0f : 0.0f; });
        CHECK(s.getFocus() == SL_ROWS - 1 && r.bumps == 1, "foco %d bumps %d", s.getFocus(), r.bumps);
    }

    printf("4) mao para CIMA volta para a linha de cima\n");
    {
        SettingsLogic s; openMenu(s);
        run(s, 200, [](int t, bool& p, float& x, float& y) { p = true; x = 0; y = (t < 60) ? -0.8f : 0.0f; });
        const int f0 = s.getFocus();
        run(s, 200, [](int t, bool& p, float& x, float& y) { p = true; x = 0; y = (t < 40) ? 0.8f : 0.0f; });
        CHECK(s.getFocus() < f0, "antes %d depois %d", f0, s.getFocus());
    }

    printf("5) segura a DIREITA 2 s -> entra UMA vez, nao muda valor\n");
    {
        SettingsLogic s; openMenu(s); focusBluetooth(s);
        Result r = run(s, 180, [](int t, bool& p, float& x, float& y) { p = true; y = 0; x = (t < 120) ? 0.8f : 0.0f; });
        CHECK(r.enters == 1 && s.isEditing() && r.plus + r.minus == 0, "enters %d edit %d", r.enters, s.isEditing());
    }

    printf("6) no ajuste: mao para CIMA ~1 s -> alguns +1; centro -> para\n");
    {
        SettingsLogic s; openMenu(s); focusBluetooth(s);
        run(s, 150, [](int t, bool& p, float& x, float& y) { p = true; y = 0; x = (t < 60) ? 0.8f : 0.0f; });
        Result r = run(s, 200, [](int t, bool& p, float& x, float& y) { p = true; x = 0; y = (t < 60) ? 0.8f : 0.0f; });
        printf("   +1 x%d, -1 x%d\n", r.plus, r.minus);
        CHECK(r.plus >= 1 && r.plus <= 4 && r.minus == 0, "plus %d minus %d", r.plus, r.minus);
        Result r2 = run(s, 200, [](int, bool& p, float& x, float& y) { p = true; x = 0; y = 0; });
        CHECK(r2.plus + r2.minus == 0, "mudou parado");
    }

    printf("7) no ajuste: BAIXO da -1\n");
    {
        SettingsLogic s; openMenu(s); focusBluetooth(s);
        run(s, 150, [](int t, bool& p, float& x, float& y) { p = true; y = 0; x = (t < 60) ? 0.8f : 0.0f; });
        Result r = run(s, 200, [](int t, bool& p, float& x, float& y) { p = true; x = 0; y = (t < 60) ? -0.8f : 0.0f; });
        CHECK(r.minus >= 1 && r.plus == 0, "plus %d minus %d", r.plus, r.minus);
    }

    printf("8) ESQUERDA segurada 2 s no ajuste -> sai so do ajuste (nao do menu)\n");
    {
        SettingsLogic s; openMenu(s); focusBluetooth(s);
        run(s, 150, [](int t, bool& p, float& x, float& y) { p = true; y = 0; x = (t < 60) ? 0.8f : 0.0f; });
        Result r = run(s, 200, [](int t, bool& p, float& x, float& y) { p = true; y = 0; x = (t < 120) ? -0.8f : 0.0f; });
        CHECK(r.leaves == 1 && r.exits == 0 && !s.isEditing(), "leaves %d exits %d", r.leaves, r.exits);
        printf("   ...volta ao centro e ESQUERDA de novo -> permanece no menu\n");
        Result r2 = run(s, 120, [](int t, bool& p, float& x, float& y) { p = true; y = 0; x = (t < 60) ? -0.8f : 0.0f; });
        CHECK(r2.exits == 0, "exits %d", r2.exits);
    }

    printf("9) mao na direita com um pouco de Y nao rola a lista\n");
    {
        SettingsLogic s; openMenu(s);
        run(s, 150, [](int, bool& p, float& x, float& y) { p = true; x = 0.8f; y = -0.4f; });
        CHECK(s.getFocus() == 0, "rolou: foco %d", s.getFocus());
    }

    printf("10) mao sai no meio do ajuste -> nada muda sozinho\n");
    {
        SettingsLogic s; openMenu(s); focusBluetooth(s);
        run(s, 150, [](int t, bool& p, float& x, float& y) { p = true; y = 0; x = (t < 60) ? 0.8f : 0.0f; });
        Result r = run(s, 300, [](int t, bool& p, float& x, float& y) { p = (t < 20) || (t > 200); x = 0; y = (t < 20) ? 0.3f : 0.0f; });
        CHECK(r.plus + r.minus == 0 && s.isEditing(), "plus %d minus %d edit %d", r.plus, r.minus, s.isEditing());
    }

    printf("11) fluxo completo: desce ate Sound, entra, +2, sai do ajuste, permanece no menu\n");
    {
        SettingsLogic s; openMenu(s);
        Result r = run(s, 1400, [](int t, bool& p, float& x, float& y) {
            p = true; x = 0.05f; y = -0.05f;
            if (t < 200) y = -1.0f;                          /* desce ate o fim */
            else if (t >= 260 && t < 300) x = 0.9f;          /* entra           */
            else if (t >= 360 && t < 420) y = 0.9f;          /* +               */
            else if (t >= 600 && t < 660) x = -0.9f;         /* sai do ajuste   */
            else if (t >= 760 && t < 820) x = -0.9f;         /* sai do menu     */
        });
        printf("   foco %d  +%d  -%d  enters %d leaves %d exits %d\n", s.getFocus(), r.plus, r.minus, r.enters, r.leaves, r.exits);
        CHECK(s.getFocus() == SL_ROWS - 1 && r.enters == 1 && r.plus >= 1 && r.minus == 0 &&
              r.leaves == 1 && r.exits == 0, "fluxo incorreto");
    }

    {
        SettingsLogic s; openMenu(s);
        for (int i=0; i<60; ++i) s.tick(true, true, 1, 0, true);
        int8_t item, delta;
        CHECK(!s.isEditing() && !s.takeRequest(item, delta), "Wi-Fi must be read-only");
    }
    {
        SettingsLogic s; openMenu(s); focusBluetooth(s);
        for (int i=0; i<12; ++i) s.tick(true, true, 0.8f, 0, false);
        CHECK(s.isEditing(), "entry");
        // No sampled neutral frame; diagonal left and click at the same time.
        for (int i=0; i<12; ++i) s.tick(true, true, -0.8f, -0.8f, true);
        CHECK(!s.isEditing() && !s.takeExit(), "left must beat click and leave one level");
        for (int i=0; i<120; ++i) s.tick(true, true, -0.8f, 0, true);
        CHECK(!s.isEditing() && !s.takeExit(), "held left cannot exit twice");
        s.tick(true, true, 0, 0, false);
        for (int i=0; i<12; ++i) s.tick(true, true, -0.8f, 0, false);
        CHECK(!s.takeExit(), "left cannot exit Settings; global ToF hold owns exit");
    }

    printf(fails ? "\n%d FALHA(S)\n" : "\nOK - todos os cenarios passaram\n", fails);
    return fails ? 1 : 0;
}
