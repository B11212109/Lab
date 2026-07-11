#ifndef SCRAMBLE_TABLE_H
#define SCRAMBLE_TABLE_H

#include <stdint.h>

#define SCRAMBLE_TABLE_COUNT              30U
#define SCRAMBLE_TABLE_MOVE_MAX           21U
#define SCRAMBLE_TABLE_LINE_LEN           ((SCRAMBLE_TABLE_MOVE_MAX * 3U) + 1U)

static const char g_scramble_table[SCRAMBLE_TABLE_COUNT][SCRAMBLE_TABLE_LINE_LEN] = {
    "D R2 U2 L2 B2 D F2 D L2 B2 D L2 F' U B R F2 U L D2 R2 ",
    "B2 F2 D L2 D B2 U' R2 B2 L2 U2 L B' L2 R2 U' R2 U2 R2 B' R ",
    "D2 B' R2 D2 B' U2 B' D2 B2 R2 B U' F' L2 F2 R D' B U' R B' ",
    "B2 D2 F2 D' B2 U B2 U R2 D' L2 U' R D F2 U2 R' F' R F2 R' ",
    "F2 U2 L' B2 U2 F2 L2 F2 U2 R D' B' U' L' F' D F2 D B D' R' ",
    "R' F2 L2 D2 U2 B2 L U2 L2 R' U' F' L2 B D2 R D2 R2 U F R2 ",
    "D L2 F2 L2 D F2 U' R2 U2 B2 F2 R' F' L' B' U B2 D' B D2 U' ",
    "D2 R2 B2 D2 B' F' R2 D2 B L2 D' L' B D F D B2 R' B U2 R' ",
    "D2 L2 F2 R2 U' R2 D2 F2 D' U2 R2 B L2 F L F R D2 U' B L ",
    "R2 D2 F' L2 R2 B' F' R2 B L' B2 D' B R' F' D B' F2 L2 D2 F' ",
    "D U2 F2 L2 B2 L2 D2 F2 U2 L F U L B2 D2 L' B' L2 D' R D ",
    "R' U2 L D2 R2 D2 F2 R' B2 R D2 U' L' D' B2 L R' B D U' F' ",
    "R2 F' R2 U2 F2 D2 R2 B D2 L2 D2 R2 D' B2 R F' D2 B2 D2 F' D2 ",
    "R2 U2 F' U2 L2 B' F' U2 F R2 F2 L' D L B' R' D' F L2 D2 U2 ",
    "D2 B D2 L2 D2 R2 U2 F' D2 F' L F U' B F L B2 L' U R2 U ",
    "R2 B2 F2 R D2 F2 D2 L U2 L B' F U L2 F D2 L' D' R' F2 U' ",
    "F2 D U2 B2 R2 U R2 D' U2 L2 U2 L D B' U2 R' D2 R2 D2 B D ",
    "L2 B2 L F2 U2 B2 F2 R2 D2 L B2 F' U' L2 D2 B' R2 U2 F D L' ",
    "D F2 U' F2 R2 U F2 R2 D2 B2 F2 L' B' R' U F' U2 L F R' F ",
    "L D2 L' U2 F2 D2 B2 R B2 L' B2 L2 D B L U2 F' U R2 F' D ",
    "U2 L2 R2 F D2 B' F2 D2 R2 B2 U' B2 F' L' D F' L2 R' U' F' R2 ",
    "R2 U' R2 U F2 D' R2 D' R2 D2 F R B2 R2 B2 U B L2 F D2 U2 ",
    "L' F2 U2 L2 F2 L' B2 D2 R' U2 F R B2 D R U' L U B2 L2 U2 ",
    "R2 U2 B F2 D2 L2 F2 L2 F2 U2 L' F2 D F' L B' R F L' F L2 ",
    "D2 U R2 U' F2 L2 F2 U' F2 R' F2 D B2 R' B2 D B R' U B2 U2 ",
    "D B2 U2 L2 B2 D B2 L2 B2 L2 D' L2 F' R' B U' F' R D B2 D2 ",
    "L2 R D2 U2 B2 U2 R' D2 L' R' D' B F' D L U B D2 F R2 D2 ",
    "B2 D2 U2 L' F2 L R2 F2 R2 D2 U R2 B L' U2 F2 R D' U' R2 B' ",
    "U2 F2 D' B2 F2 D B2 U R2 U' B' L2 R F2 R D2 U2 F2 U' L U2 ",
    "L R2 B2 U2 L U2 F2 U2 B2 F2 R2 D' F D B2 R' U B2 F2 U R' "
};

#endif /* SCRAMBLE_TABLE_H */
