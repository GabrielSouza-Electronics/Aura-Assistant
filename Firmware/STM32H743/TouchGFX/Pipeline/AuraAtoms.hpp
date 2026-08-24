// Gerado por prep_aura_atoms.py - nao editar a mao.

#ifndef AURA_ATOMS_HPP
#define AURA_ATOMS_HPP

#include <cstdint>

namespace aura
{
static const int16_t CANVAS_W = 480;
static const int16_t CANVAS_H = 480;

struct Placed   { int16_t x, y, w, h; };
struct Instance { int16_t x, y; uint8_t alpha; };

static const Placed POS_BG_SPACE = {   0,   0, 480, 480 };
static const Placed POS_NEBULA_0 = {   0,  21, 249, 228 };
static const Placed POS_NEBULA_1 = { 236, 221, 244, 248 };
static const Placed POS_NEBULA_2 = { 216,  11, 228, 168 };
static const Placed POS_NEBULA_3 = {   0, 306, 234, 174 };
static const Placed POS_ORBIT_0 = {  57, 169, 367, 160 };
static const Placed POS_ORBIT_1 = {  56, 171, 366, 160 };
static const Placed POS_ORBIT_2 = {  89, 121, 302, 258 };
static const Placed POS_ORBIT_3 = { 100, 151, 280, 198 };
static const Placed POS_ORBIT_4 = { 100, 151, 280, 198 };
static const Placed POS_ORBIT_5 = { 125, 139, 230, 222 };
static const Placed POS_CORE_GLOW = { 168, 178, 144, 144 };
static const Placed POS_CORE_DISCOS = { 141, 267, 198,  58 };
static const Placed POS_CORE_ANEIS = { 173, 181, 136, 136 };
static const Placed POS_CORE_BOJO = { 187, 197, 106, 106 };
static const Placed POS_CORE_ARCOS = { 174, 190, 132, 120 };
static const Placed POS_CORE_TEXTO = { 208, 241,  64,  17 };
static const Placed POS_CORE_HASTE = { 237, 154,   6,  39 };
static const Placed POS_PILL_TASKS_IDLE = {  85, 227,  54,  32 };
static const Placed POS_PILL_TASKS_SEL = {  82, 237,  60,  26 };
static const Placed POS_PILL_EMAIL_IDLE = { 341, 227,  54,  32 };
static const Placed POS_PILL_EMAIL_SEL = { 338, 237,  60,  26 };
static const Placed POS_PILL_REMINDERS_IDLE = { 109, 343,  78,  32 };
static const Placed POS_PILL_REMINDERS_SEL = { 105, 353,  86,  26 };
static const Placed POS_PILL_ASSISTANT_IDLE = { 293, 343,  78,  32 };
static const Placed POS_PILL_ASSISTANT_SEL = { 289, 353,  86,  26 };
static const Placed POS_PILL_CALENDAR_IDLE = { 204, 143,  72,  32 };
static const Placed POS_PILL_CALENDAR_SEL = { 200, 153,  80,  26 };
static const Placed POS_HDR_ANEL = { 229,  11,  22,  22 };
static const Placed POS_HDR_TITULO = { 207,  35,  66,  16 };
static const Placed POS_HDR_WIFI = { 128,  34,  30,  23 };
static const Placed POS_HDR_BATERIA = { 321,  35,  37,  19 };
static const Placed POS_FTR_ICONE_LR = {  82, 390,  28,  16 };
static const Placed POS_FTR_TEXTO_LR = { 113, 386,  59,  25 };
static const Placed POS_FTR_ICONE_UD = { 200, 384,  16,  28 };
static const Placed POS_FTR_TEXTO_UD = { 225, 387,  62,  25 };
static const Placed POS_FTR_ICONE_HD = { 305, 383,  30,  30 };
static const Placed POS_FTR_TEXTO_HD = { 337, 386,  31,  25 };

static const int N_STAR_0P45 = 22;
static const Instance INST_STAR_0P45[N_STAR_0P45] = {
    { 248, 414, 201 },
    { 406, 306, 230 },
    {  25, 242, 204 },
    {  35, 247, 194 },
    { 299,  45, 143 },
    {  76, 206, 212 },
    { 335, 274, 189 },
    { 352, 347, 171 },
    { 216, 170, 196 },
    {  96,  78, 158 },
    { 430, 291, 230 },
    { 299, 363, 235 },
    { 388, 419, 201 },
    {  71, 212, 201 },
    { 224, 398, 186 },
    { 233,  74, 204 },
    { 345, 170, 161 },
    { 436, 123, 153 },
    { 310,  84, 176 },
    { 373, 429, 222 },
    { 338, 186, 176 },
    { 206, 108, 235 },
};

static const int N_STAR_1P4 = 11;
static const Instance INST_STAR_1P4[N_STAR_1P4] = {
    { 316, 315, 153 },
    { 226,  76, 194 },
    { 116,  45, 186 },
    { 311, 180, 166 },
    { 342, 349, 199 },
    { 375, 221, 181 },
    { 206, 198, 214 },
    {  43, 123, 227 },
    {  63, 233, 143 },
    { 420, 298, 222 },
    { 164, 344, 184 },
};

static const int N_STAR_1P1 = 13;
static const Instance INST_STAR_1P1[N_STAR_1P1] = {
    {  63, 191, 199 },
    { 301, 221, 150 },
    { 221, 105, 232 },
    { 392, 393, 166 },
    { 134, 255, 227 },
    { 150, 377, 204 },
    { 232, 328, 150 },
    { 268, 244, 145 },
    { 122,  75, 150 },
    { 308,  62, 145 },
    { 172, 241, 207 },
    { 284, 277, 224 },
    { 207, 136, 219 },
};

static const int N_STAR_0P85 = 18;
static const Instance INST_STAR_0P85[N_STAR_0P85] = {
    { 290, 426, 191 },
    { 292, 123, 212 },
    { 319, 142, 232 },
    { 302, 303, 196 },
    { 368, 409, 168 },
    { 281, 465, 212 },
    { 302,  95, 145 },
    { 419, 303, 209 },
    { 190,  19, 189 },
    { 188, 296, 148 },
    {  76, 220, 189 },
    { 268,  29, 212 },
    { 387, 314, 145 },
    {  52, 331, 230 },
    { 116, 114, 176 },
    { 100,  61, 143 },
    { 198, 225, 176 },
    { 225, 270, 212 },
};

static const int N_STAR_0P7 = 20;
static const Instance INST_STAR_0P7[N_STAR_0P7] = {
    { 185, 396, 181 },
    { 281, 299, 158 },
    { 192, 181, 140 },
    {  20, 312, 227 },
    { 391, 403, 214 },
    { 210,  62, 173 },
    { 240, 409, 219 },
    { 138, 285, 191 },
    { 328, 442, 222 },
    { 143, 387, 161 },
    { 229, 338, 161 },
    { 416,  86, 173 },
    { 206,  73, 181 },
    { 295, 242, 230 },
    { 413, 230, 230 },
    { 336, 331, 186 },
    { 335, 300, 196 },
    { 404, 145, 227 },
    {   6, 182, 227 },
    {  76, 212, 168 },
};

static const int N_STAR_0P55 = 15;
static const Instance INST_STAR_0P55[N_STAR_0P55] = {
    {  70, 292, 199 },
    { 217, 452, 140 },
    { 356, 160, 143 },
    { 262, 103, 158 },
    { 286,  36, 196 },
    { 380, 174, 201 },
    {  16, 222, 158 },
    { 157, 326, 230 },
    { 177, 126, 173 },
    { 449, 179, 168 },
    { 257, 443, 184 },
    { 281, 167, 178 },
    {  97, 184, 166 },
    { 196, 183, 168 },
    { 361, 180, 166 },
};

static const int N_GLOWDOT_0 = 1;
static const Instance INST_GLOWDOT_0[N_GLOWDOT_0] = {
    {  74, 112, 255 },
};

static const int N_GLOWDOT_1 = 1;
static const Instance INST_GLOWDOT_1[N_GLOWDOT_1] = {
    { 394, 144, 255 },
};

static const int N_GLOWDOT_2 = 1;
static const Instance INST_GLOWDOT_2[N_GLOWDOT_2] = {
    { 144, 386, 255 },
};

static const int N_GLOWDOT_3 = 2;
static const Instance INST_GLOWDOT_3[N_GLOWDOT_3] = {
    { 340, 400, 255 },
    {  47, 277, 255 },
};

static const int N_GLOWDOT_4 = 1;
static const Instance INST_GLOWDOT_4[N_GLOWDOT_4] = {
    { 433, 257, 255 },
};

static const int N_ORBIT_MARKER = 4;
static const Instance INST_ORBIT_MARKER[N_ORBIT_MARKER] = {
    { 109, 336, 255 },
    { 359, 336, 255 },
    {  56, 270, 255 },
    { 412, 270, 255 },
};

static const int N_SIDE_ARROW = 2;
static const Instance INST_SIDE_ARROW[N_SIDE_ARROW] = {
    {  36, 223, 255 },
    { 428, 223, 255 },
};

static const int N_FTR_DIVISOR = 2;
static const Instance INST_FTR_DIVISOR[N_FTR_DIVISOR] = {
    { 179, 383, 255 },
    { 291, 383, 255 },
};

static const int N_DOT_ATIVO = 1;
static const Instance INST_DOT_ATIVO[N_DOT_ATIVO] = {
    { 205, 431, 255 },
};

static const int N_DOT_INATIVO = 4;
static const Instance INST_DOT_INATIVO[N_DOT_INATIVO] = {
    { 221, 433, 255 },
    { 235, 433, 255 },
    { 249, 433, 255 },
    { 263, 433, 255 },
};

static const int NUM_NODES = 5;
enum NodeId {
    NODE_TASKS = 0,
    NODE_EMAIL = 1,
    NODE_REMINDERS = 2,
    NODE_ASSISTANT = 3,
    NODE_CALENDAR = 4,
};

// centro de cada no no canvas
static const Instance NODE_CENTER[NUM_NODES] = {
    { 112, 196, 255 },  // Tasks
    { 368, 196, 255 },  // Email
    { 148, 312, 255 },  // Reminders
    { 332, 312, 255 },  // Assistant
    { 240, 112, 255 },  // Calendar
};

// canto sup-esq do disco de vidro (idle)
static const Instance GLASS_IDLE[NUM_NODES] = {
    {  71, 155, 255 },  // Tasks
    { 327, 155, 255 },  // Email
    { 107, 271, 255 },  // Reminders
    { 291, 271, 255 },  // Assistant
    { 199,  71, 255 },  // Calendar
};

// canto sup-esq do disco de vidro (sel)
static const Instance GLASS_SEL[NUM_NODES] = {
    {  53, 136, 255 },  // Tasks
    { 309, 136, 255 },  // Email
    {  89, 252, 255 },  // Reminders
    { 273, 252, 255 },  // Assistant
    { 181,  52, 255 },  // Calendar
};

// canto sup-esq do icone (idle)
static const Instance ICON_IDLE[NUM_NODES] = {
    {  97, 181, 255 },  // Tasks
    { 354, 184, 255 },  // Email
    { 136, 299, 255 },  // Reminders
    { 320, 298, 255 },  // Assistant
    { 226,  95, 255 },  // Calendar
};

// canto sup-esq do icone (sel)
static const Instance ICON_SEL[NUM_NODES] = {
    {  94, 178, 255 },  // Tasks
    { 350, 182, 255 },  // Email
    { 134, 296, 255 },  // Reminders
    { 318, 295, 255 },  // Assistant
    { 222,  92, 255 },  // Calendar
};

}  // namespace aura

#endif  // AURA_ATOMS_HPP
