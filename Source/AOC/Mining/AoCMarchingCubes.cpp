// AoCMarchingCubes.cpp
// Architect of Creation - Marching Cubes Implementation
// Complete implementation of Paul Bourke's marching cubes algorithm.
// Tables from: http://paulbourke.net/geometry/polygonise/

#include "AoCMarchingCubes.h"

// ─── Edge Table ────────────────────────────────────────────────────────────────
// Maps 8-bit cube vertex configuration to 12-bit edge intersection mask.

const int32 FAoCMarchingCubes::EdgeTable[256] = {
	0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
	0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
	0x190, 0x099, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
	0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
	0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
	0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
	0x3a0, 0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
	0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
	0x460, 0x569, 0x663, 0x76a, 0x066, 0x16f, 0x265, 0x36c,
	0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
	0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5, 0x2fc,
	0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
	0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c,
	0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
	0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0x0cc,
	0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
	0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
	0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
	0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
	0x15c, 0x055, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
	0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
	0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
	0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
	0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460,
	0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
	0x4ac, 0x5a5, 0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0,
	0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
	0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x033, 0x339, 0x230,
	0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
	0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190,
	0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
	0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000
};

// ─── Triangle Table ────────────────────────────────────────────────────────────
// Maps cube configuration to triangle vertex sequences (edge indices).
// Up to 5 triangles per cube, -1 terminated.

const int32 FAoCMarchingCubes::TriTable[256][16] = {
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 0
	{ 0,  8,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 1
	{ 0,  1,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 2
	{ 1,  8,  3,  9,  8,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 3
	{ 1,  2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 4
	{ 0,  8,  3,  1,  2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 5
	{ 9,  2, 10,  0,  2,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 6
	{ 2,  8,  3,  2, 10,  8, 10,  9,  8, -1, -1, -1, -1, -1, -1, -1}, // 7
	{ 3, 11,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 8
	{ 0, 11,  2,  8, 11,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 9
	{ 1,  9,  0,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 10
	{ 1, 11,  2,  1,  9, 11,  9,  8, 11, -1, -1, -1, -1, -1, -1, -1}, // 11
	{ 3, 10,  1, 11, 10,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 12
	{ 0, 10,  1,  0,  8, 10,  8, 11, 10, -1, -1, -1, -1, -1, -1, -1}, // 13
	{ 3,  9,  0,  3, 11,  9, 11, 10,  9, -1, -1, -1, -1, -1, -1, -1}, // 14
	{ 9,  8, 10, 10,  8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 15
	{ 4,  7,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 16
	{ 4,  3,  0,  7,  3,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 17
	{ 0,  1,  9,  8,  4,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 18
	{ 4,  1,  9,  4,  7,  1,  7,  3,  1, -1, -1, -1, -1, -1, -1, -1}, // 19
	{ 1,  2, 10,  8,  4,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 20
	{ 3,  4,  7,  3,  0,  4,  1,  2, 10, -1, -1, -1, -1, -1, -1, -1}, // 21
	{ 9,  2, 10,  9,  0,  2,  8,  4,  7, -1, -1, -1, -1, -1, -1, -1}, // 22
	{ 2, 10,  9,  2,  9,  7,  2,  7,  3,  7,  9,  4, -1, -1, -1, -1}, // 23
	{ 8,  4,  7,  3, 11,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 24
	{11,  4,  7, 11,  2,  4,  2,  0,  4, -1, -1, -1, -1, -1, -1, -1}, // 25
	{ 9,  0,  1,  8,  4,  7,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1}, // 26
	{ 4,  7, 11,  9,  4, 11,  9, 11,  2,  9,  2,  1, -1, -1, -1, -1}, // 27
	{ 3, 10,  1,  3, 11, 10,  7,  8,  4, -1, -1, -1, -1, -1, -1, -1}, // 28
	{ 1, 11, 10,  1,  4, 11,  1,  0,  4,  7, 11,  4, -1, -1, -1, -1}, // 29
	{ 4,  7,  8,  9,  0, 11,  9, 11, 10, 11,  0,  3, -1, -1, -1, -1}, // 30
	{ 4,  7, 11,  4, 11,  9,  9, 11, 10, -1, -1, -1, -1, -1, -1, -1}, // 31
	{ 9,  5,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 32
	{ 9,  5,  4,  0,  8,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 33
	{ 0,  5,  4,  1,  5,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 34
	{ 8,  5,  4,  8,  3,  5,  3,  1,  5, -1, -1, -1, -1, -1, -1, -1}, // 35
	{ 1,  2, 10,  9,  5,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 36
	{ 3,  0,  8,  1,  2, 10,  4,  9,  5, -1, -1, -1, -1, -1, -1, -1}, // 37
	{ 5,  2, 10,  5,  4,  2,  4,  0,  2, -1, -1, -1, -1, -1, -1, -1}, // 38
	{ 2, 10,  5,  3,  2,  5,  3,  5,  4,  3,  4,  8, -1, -1, -1, -1}, // 39
	{ 9,  5,  4,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 40
	{ 0, 11,  2,  0,  8, 11,  4,  9,  5, -1, -1, -1, -1, -1, -1, -1}, // 41
	{ 0,  5,  4,  0,  1,  5,  2,  3, 11, -1, -1, -1, -1, -1, -1, -1}, // 42
	{ 2,  1,  5,  2,  5,  8,  2,  8, 11,  4,  8,  5, -1, -1, -1, -1}, // 43
	{10,  3, 11, 10,  1,  3,  9,  5,  4, -1, -1, -1, -1, -1, -1, -1}, // 44
	{ 4,  9,  5,  0,  8,  1,  8, 10,  1,  8, 11, 10, -1, -1, -1, -1}, // 45
	{ 5,  4,  0,  5,  0, 11,  5, 11, 10, 11,  0,  3, -1, -1, -1, -1}, // 46
	{ 5,  4,  8,  5,  8, 10, 10,  8, 11, -1, -1, -1, -1, -1, -1, -1}, // 47
	{ 9,  7,  8,  5,  7,  9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 48
	{ 9,  3,  0,  9,  5,  3,  5,  7,  3, -1, -1, -1, -1, -1, -1, -1}, // 49
	{ 0,  7,  8,  0,  1,  7,  1,  5,  7, -1, -1, -1, -1, -1, -1, -1}, // 50
	{ 1,  5,  3,  3,  5,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 51
	{ 9,  7,  8,  9,  5,  7, 10,  1,  2, -1, -1, -1, -1, -1, -1, -1}, // 52
	{10,  1,  2,  9,  5,  0,  5,  3,  0,  5,  7,  3, -1, -1, -1, -1}, // 53
	{ 8,  0,  2,  8,  2,  5,  8,  5,  7, 10,  5,  2, -1, -1, -1, -1}, // 54
	{ 2, 10,  5,  2,  5,  3,  3,  5,  7, -1, -1, -1, -1, -1, -1, -1}, // 55
	{ 7,  9,  5,  7,  8,  9,  3, 11,  2, -1, -1, -1, -1, -1, -1, -1}, // 56
	{ 9,  5,  7,  9,  7,  2,  9,  2,  0,  2,  7, 11, -1, -1, -1, -1}, // 57
	{ 2,  3, 11,  0,  1,  8,  1,  7,  8,  1,  5,  7, -1, -1, -1, -1}, // 58
	{11,  2,  1, 11,  1,  7,  7,  1,  5, -1, -1, -1, -1, -1, -1, -1}, // 59
	{ 9,  5,  8,  8,  5,  7, 10,  1,  3, 10,  3, 11, -1, -1, -1, -1}, // 60
	{ 5,  7,  0,  5,  0,  9,  7, 11,  0,  1,  0, 10, 11, 10,  0, -1}, // 61
	{11, 10,  0, 11,  0,  3, 10,  5,  0,  8,  0,  7,  5,  7,  0, -1}, // 62
	{11, 10,  5,  7, 11,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 63
	{10,  6,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 64
	{ 0,  8,  3,  5, 10,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 65
	{ 9,  0,  1,  5, 10,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 66
	{ 1,  8,  3,  1,  9,  8,  5, 10,  6, -1, -1, -1, -1, -1, -1, -1}, // 67
	{ 1,  6,  5,  2,  6,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 68
	{ 1,  6,  5,  1,  2,  6,  3,  0,  8, -1, -1, -1, -1, -1, -1, -1}, // 69
	{ 9,  6,  5,  9,  0,  6,  0,  2,  6, -1, -1, -1, -1, -1, -1, -1}, // 70
	{ 5,  9,  8,  5,  8,  2,  5,  2,  6,  3,  2,  8, -1, -1, -1, -1}, // 71
	{ 2,  3, 11, 10,  6,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 72
	{11,  0,  8, 11,  2,  0, 10,  6,  5, -1, -1, -1, -1, -1, -1, -1}, // 73
	{ 0,  1,  9,  2,  3, 11,  5, 10,  6, -1, -1, -1, -1, -1, -1, -1}, // 74
	{ 5, 10,  6,  1,  9,  2,  9, 11,  2,  9,  8, 11, -1, -1, -1, -1}, // 75
	{ 6,  3, 11,  6,  5,  3,  5,  1,  3, -1, -1, -1, -1, -1, -1, -1}, // 76
	{ 0,  8, 11,  0, 11,  5,  0,  5,  1,  5, 11,  6, -1, -1, -1, -1}, // 77
	{ 3, 11,  6,  0,  3,  6,  0,  6,  5,  0,  5,  9, -1, -1, -1, -1}, // 78
	{ 6,  5,  9,  6,  9, 11, 11,  9,  8, -1, -1, -1, -1, -1, -1, -1}, // 79
	{ 5, 10,  6,  4,  7,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 80
	{ 4,  3,  0,  4,  7,  3,  6,  5, 10, -1, -1, -1, -1, -1, -1, -1}, // 81
	{ 1,  9,  0,  5, 10,  6,  8,  4,  7, -1, -1, -1, -1, -1, -1, -1}, // 82
	{10,  6,  5,  1,  9,  7,  1,  7,  3,  7,  9,  4, -1, -1, -1, -1}, // 83
	{ 6,  1,  2,  6,  5,  1,  4,  7,  8, -1, -1, -1, -1, -1, -1, -1}, // 84
	{ 1,  2,  5,  5,  2,  6,  3,  0,  4,  3,  4,  7, -1, -1, -1, -1}, // 85
	{ 8,  4,  7,  9,  0,  5,  0,  6,  5,  0,  2,  6, -1, -1, -1, -1}, // 86
	{ 7,  3,  9,  7,  9,  4,  3,  2,  9,  5,  9,  6,  2,  6,  9, -1}, // 87
	{ 3, 11,  2,  7,  8,  4, 10,  6,  5, -1, -1, -1, -1, -1, -1, -1}, // 88
	{ 5, 10,  6,  4,  7,  2,  4,  2,  0,  2,  7, 11, -1, -1, -1, -1}, // 89
	{ 0,  1,  9,  4,  7,  8,  2,  3, 11,  5, 10,  6, -1, -1, -1, -1}, // 90
	{ 9,  2,  1,  9, 11,  2,  9,  4, 11,  7, 11,  4,  5, 10,  6, -1}, // 91
	{ 8,  4,  7,  3, 11,  5,  3,  5,  1,  5, 11,  6, -1, -1, -1, -1}, // 92
	{ 5,  1, 11,  5, 11,  6,  1,  0, 11,  7, 11,  4,  0,  4, 11, -1}, // 93
	{ 0,  5,  9,  0,  6,  5,  0,  3,  6, 11,  6,  3,  8,  4,  7, -1}, // 94
	{ 6,  5,  9,  6,  9, 11,  4,  7,  9,  7, 11,  9, -1, -1, -1, -1}, // 95
	{10,  4,  9,  6,  4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 96
	{ 4, 10,  6,  4,  9, 10,  0,  8,  3, -1, -1, -1, -1, -1, -1, -1}, // 97
	{10,  0,  1, 10,  6,  0,  6,  4,  0, -1, -1, -1, -1, -1, -1, -1}, // 98
	{ 8,  3,  1,  8,  1,  6,  8,  6,  4,  6,  1, 10, -1, -1, -1, -1}, // 99
	{ 1,  4,  9,  1,  2,  4,  2,  6,  4, -1, -1, -1, -1, -1, -1, -1}, // 100
	{ 3,  0,  8,  1,  2,  9,  2,  4,  9,  2,  6,  4, -1, -1, -1, -1}, // 101
	{ 0,  2,  4,  4,  2,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 102
	{ 8,  3,  2,  8,  2,  4,  4,  2,  6, -1, -1, -1, -1, -1, -1, -1}, // 103
	{10,  4,  9, 10,  6,  4, 11,  2,  3, -1, -1, -1, -1, -1, -1, -1}, // 104
	{ 0,  8,  2,  2,  8, 11,  4,  9, 10,  4, 10,  6, -1, -1, -1, -1}, // 105
	{ 3, 11,  2,  0,  1,  6,  0,  6,  4,  6,  1, 10, -1, -1, -1, -1}, // 106
	{ 6,  4,  1,  6,  1, 10,  4,  8,  1,  2,  1, 11,  8, 11,  1, -1}, // 107
	{ 9,  6,  4,  9,  3,  6,  9,  1,  3, 11,  6,  3, -1, -1, -1, -1}, // 108
	{ 8, 11,  1,  8,  1,  0, 11,  6,  1,  9,  1,  4,  6,  4,  1, -1}, // 109
	{ 3, 11,  6,  3,  6,  0,  0,  6,  4, -1, -1, -1, -1, -1, -1, -1}, // 110
	{ 6,  4,  8, 11,  6,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 111
	{ 7, 10,  6,  7,  8, 10,  8,  9, 10, -1, -1, -1, -1, -1, -1, -1}, // 112
	{ 0,  7,  3,  0, 10,  7,  0,  9, 10,  6,  7, 10, -1, -1, -1, -1}, // 113
	{10,  6,  7,  1, 10,  7,  1,  7,  8,  1,  8,  0, -1, -1, -1, -1}, // 114
	{10,  6,  7, 10,  7,  1,  1,  7,  3, -1, -1, -1, -1, -1, -1, -1}, // 115
	{ 1,  2,  6,  1,  6,  8,  1,  8,  9,  8,  6,  7, -1, -1, -1, -1}, // 116
	{ 2,  6,  9,  2,  9,  1,  6,  7,  9,  0,  9,  3,  7,  3,  9, -1}, // 117
	{ 7,  8,  0,  7,  0,  6,  6,  0,  2, -1, -1, -1, -1, -1, -1, -1}, // 118
	{ 7,  3,  2,  6,  7,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 119
	{ 2,  3, 11, 10,  6,  8, 10,  8,  9,  8,  6,  7, -1, -1, -1, -1}, // 120
	{ 2,  0,  7,  2,  7, 11,  0,  9,  7,  6,  7, 10,  9, 10,  7, -1}, // 121
	{ 1,  8,  0,  1,  7,  8,  1, 10,  7,  6,  7, 10,  2,  3, 11, -1}, // 122
	{11,  2,  1, 11,  1,  7, 10,  6,  1,  6,  7,  1, -1, -1, -1, -1}, // 123
	{ 8,  9,  6,  8,  6,  7,  9,  1,  6, 11,  6,  3,  1,  3,  6, -1}, // 124
	{ 0,  9,  1, 11,  6,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 125
	{ 7,  8,  0,  7,  0,  6,  3, 11,  0, 11,  6,  0, -1, -1, -1, -1}, // 126
	{ 7, 11,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 127
	{ 7,  6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 128
	{ 3,  0,  8, 11,  7,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 129
	{ 0,  1,  9, 11,  7,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 130
	{ 8,  1,  9,  8,  3,  1, 11,  7,  6, -1, -1, -1, -1, -1, -1, -1}, // 131
	{10,  1,  2,  6, 11,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 132
	{ 1,  2, 10,  3,  0,  8,  6, 11,  7, -1, -1, -1, -1, -1, -1, -1}, // 133
	{ 2,  9,  0,  2, 10,  9,  6, 11,  7, -1, -1, -1, -1, -1, -1, -1}, // 134
	{ 6, 11,  7,  2, 10,  3, 10,  8,  3, 10,  9,  8, -1, -1, -1, -1}, // 135
	{ 7,  2,  3,  6,  2,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 136
	{ 7,  0,  8,  7,  6,  0,  6,  2,  0, -1, -1, -1, -1, -1, -1, -1}, // 137
	{ 2,  7,  6,  2,  3,  7,  0,  1,  9, -1, -1, -1, -1, -1, -1, -1}, // 138
	{ 1,  6,  2,  1,  8,  6,  1,  9,  8,  8,  7,  6, -1, -1, -1, -1}, // 139
	{10,  7,  6, 10,  1,  7,  1,  3,  7, -1, -1, -1, -1, -1, -1, -1}, // 140
	{10,  7,  6,  1,  7, 10,  1,  8,  7,  1,  0,  8, -1, -1, -1, -1}, // 141
	{ 0,  3,  7,  0,  7, 10,  0, 10,  9,  6, 10,  7, -1, -1, -1, -1}, // 142
	{ 7,  6, 10,  7, 10,  8,  8, 10,  9, -1, -1, -1, -1, -1, -1, -1}, // 143
	{ 6,  8,  4, 11,  8,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 144
	{ 3,  6, 11,  3,  0,  6,  0,  4,  6, -1, -1, -1, -1, -1, -1, -1}, // 145
	{ 8,  6, 11,  8,  4,  6,  9,  0,  1, -1, -1, -1, -1, -1, -1, -1}, // 146
	{ 9,  4,  6,  9,  6,  3,  9,  3,  1, 11,  3,  6, -1, -1, -1, -1}, // 147
	{ 6,  8,  4,  6, 11,  8,  2, 10,  1, -1, -1, -1, -1, -1, -1, -1}, // 148
	{ 1,  2, 10,  3,  0, 11,  0,  6, 11,  0,  4,  6, -1, -1, -1, -1}, // 149
	{ 4, 11,  8,  4,  6, 11,  0,  2,  9,  2, 10,  9, -1, -1, -1, -1}, // 150
	{10,  9,  3, 10,  3,  2,  9,  4,  3, 11,  3,  6,  4,  6,  3, -1}, // 151
	{ 8,  2,  3,  8,  4,  2,  4,  6,  2, -1, -1, -1, -1, -1, -1, -1}, // 152
	{ 0,  4,  2,  4,  6,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 153
	{ 1,  9,  0,  2,  3,  4,  2,  4,  6,  4,  3,  8, -1, -1, -1, -1}, // 154
	{ 1,  9,  4,  1,  4,  2,  2,  4,  6, -1, -1, -1, -1, -1, -1, -1}, // 155
	{ 8,  1,  3,  8,  6,  1,  8,  4,  6,  6, 10,  1, -1, -1, -1, -1}, // 156
	{10,  1,  0, 10,  0,  6,  6,  0,  4, -1, -1, -1, -1, -1, -1, -1}, // 157
	{ 4,  6,  3,  4,  3,  8,  6, 10,  3,  0,  3,  9, 10,  9,  3, -1}, // 158
	{10,  9,  4,  6, 10,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 159
	{ 4,  9,  5,  7,  6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 160
	{ 0,  8,  3,  4,  9,  5, 11,  7,  6, -1, -1, -1, -1, -1, -1, -1}, // 161
	{ 5,  0,  1,  5,  4,  0,  7,  6, 11, -1, -1, -1, -1, -1, -1, -1}, // 162
	{11,  7,  6,  8,  3,  4,  3,  5,  4,  3,  1,  5, -1, -1, -1, -1}, // 163
	{ 9,  5,  4, 10,  1,  2,  7,  6, 11, -1, -1, -1, -1, -1, -1, -1}, // 164
	{ 6, 11,  7,  1,  2, 10,  0,  8,  3,  4,  9,  5, -1, -1, -1, -1}, // 165
	{ 7,  6, 11,  5,  4, 10,  4,  2, 10,  4,  0,  2, -1, -1, -1, -1}, // 166
	{ 3,  4,  8,  3,  5,  4,  3,  2,  5, 10,  5,  2, 11,  7,  6, -1}, // 167
	{ 7,  2,  3,  7,  6,  2,  5,  4,  9, -1, -1, -1, -1, -1, -1, -1}, // 168
	{ 9,  5,  4,  0,  8,  6,  0,  6,  2,  6,  8,  7, -1, -1, -1, -1}, // 169
	{ 3,  6,  2,  3,  7,  6,  1,  5,  0,  5,  4,  0, -1, -1, -1, -1}, // 170
	{ 6,  2,  8,  6,  8,  7,  2,  1,  8,  4,  8,  5,  1,  5,  8, -1}, // 171
	{ 9,  5,  4, 10,  1,  6,  1,  7,  6,  1,  3,  7, -1, -1, -1, -1}, // 172
	{ 1,  6, 10,  1,  7,  6,  1,  0,  7,  8,  7,  0,  9,  5,  4, -1}, // 173
	{ 4,  0, 10,  4, 10,  5,  0,  3, 10,  6, 10,  7,  3,  7, 10, -1}, // 174
	{ 7,  6, 10,  7, 10,  8,  5,  4, 10,  4,  8, 10, -1, -1, -1, -1}, // 175
	{ 6,  9,  5,  6, 11,  9, 11,  8,  9, -1, -1, -1, -1, -1, -1, -1}, // 176
	{ 3,  6, 11,  0,  6,  3,  0,  5,  6,  0,  9,  5, -1, -1, -1, -1}, // 177
	{ 0, 11,  8,  0,  5, 11,  0,  1,  5,  5,  6, 11, -1, -1, -1, -1}, // 178
	{ 6, 11,  3,  6,  3,  5,  5,  3,  1, -1, -1, -1, -1, -1, -1, -1}, // 179
	{ 1,  2, 10,  9,  5, 11,  9, 11,  8, 11,  5,  6, -1, -1, -1, -1}, // 180
	{ 0, 11,  3,  0,  6, 11,  0,  9,  6,  5,  6,  9,  1,  2, 10, -1}, // 181
	{11,  8,  5, 11,  5,  6,  8,  0,  5, 10,  5,  2,  0,  2,  5, -1}, // 182
	{ 6, 11,  3,  6,  3,  5,  2, 10,  3, 10,  5,  3, -1, -1, -1, -1}, // 183
	{ 5,  8,  9,  5,  2,  8,  5,  6,  2,  3,  8,  2, -1, -1, -1, -1}, // 184
	{ 9,  5,  6,  9,  6,  0,  0,  6,  2, -1, -1, -1, -1, -1, -1, -1}, // 185
	{ 1,  5,  8,  1,  8,  0,  5,  6,  8,  3,  8,  2,  6,  2,  8, -1}, // 186
	{ 1,  5,  6,  2,  1,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 187
	{ 1,  3,  6,  1,  6, 10,  3,  8,  6,  5,  6,  9,  8,  9,  6, -1}, // 188
	{10,  1,  0, 10,  0,  6,  9,  5,  0,  5,  6,  0, -1, -1, -1, -1}, // 189
	{ 0,  3,  8,  5,  6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 190
	{10,  5,  6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 191
	{11,  5, 10,  7,  5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 192
	{11,  5, 10, 11,  7,  5,  8,  3,  0, -1, -1, -1, -1, -1, -1, -1}, // 193
	{ 5, 11,  7,  5, 10, 11,  1,  9,  0, -1, -1, -1, -1, -1, -1, -1}, // 194
	{10,  7,  5, 10, 11,  7,  9,  8,  1,  8,  3,  1, -1, -1, -1, -1}, // 195
	{11,  1,  2, 11,  7,  1,  7,  5,  1, -1, -1, -1, -1, -1, -1, -1}, // 196
	{ 0,  8,  3,  1,  2,  7,  1,  7,  5,  7,  2, 11, -1, -1, -1, -1}, // 197
	{ 9,  7,  5,  9,  2,  7,  9,  0,  2,  2, 11,  7, -1, -1, -1, -1}, // 198
	{ 7,  5,  2,  7,  2, 11,  5,  9,  2,  3,  2,  8,  9,  8,  2, -1}, // 199
	{ 2,  5, 10,  2,  3,  5,  3,  7,  5, -1, -1, -1, -1, -1, -1, -1}, // 200
	{ 8,  2,  0,  8,  5,  2,  8,  7,  5, 10,  2,  5, -1, -1, -1, -1}, // 201
	{ 9,  0,  1,  5, 10,  3,  5,  3,  7,  3, 10,  2, -1, -1, -1, -1}, // 202
	{ 9,  8,  2,  9,  2,  1,  8,  7,  2, 10,  2,  5,  7,  5,  2, -1}, // 203
	{ 1,  3,  5,  3,  7,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 204
	{ 0,  8,  7,  0,  7,  1,  1,  7,  5, -1, -1, -1, -1, -1, -1, -1}, // 205
	{ 9,  0,  3,  9,  3,  5,  5,  3,  7, -1, -1, -1, -1, -1, -1, -1}, // 206
	{ 9,  8,  7,  5,  9,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 207
	{ 5,  8,  4,  5, 10,  8, 10, 11,  8, -1, -1, -1, -1, -1, -1, -1}, // 208
	{ 5,  0,  4,  5, 11,  0,  5, 10, 11, 11,  3,  0, -1, -1, -1, -1}, // 209
	{ 0,  1,  9,  8,  4, 10,  8, 10, 11, 10,  4,  5, -1, -1, -1, -1}, // 210
	{10, 11,  4, 10,  4,  5, 11,  3,  4,  9,  4,  1,  3,  1,  4, -1}, // 211
	{ 2,  5,  1,  2,  8,  5,  2, 11,  8,  4,  5,  8, -1, -1, -1, -1}, // 212
	{ 0,  4, 11,  0, 11,  3,  4,  5, 11,  2, 11,  1,  5,  1, 11, -1}, // 213
	{ 0,  2,  5,  0,  5,  9,  2, 11,  5,  4,  5,  8, 11,  8,  5, -1}, // 214
	{ 9,  4,  5,  2, 11,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 215
	{ 2,  5, 10,  3,  5,  2,  3,  4,  5,  3,  8,  4, -1, -1, -1, -1}, // 216
	{ 5, 10,  2,  5,  2,  4,  4,  2,  0, -1, -1, -1, -1, -1, -1, -1}, // 217
	{ 3, 10,  2,  3,  5, 10,  3,  8,  5,  4,  5,  8,  0,  1,  9, -1}, // 218
	{ 5, 10,  2,  5,  2,  4,  1,  9,  2,  9,  4,  2, -1, -1, -1, -1}, // 219
	{ 8,  4,  5,  8,  5,  3,  3,  5,  1, -1, -1, -1, -1, -1, -1, -1}, // 220
	{ 0,  4,  5,  1,  0,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 221
	{ 8,  4,  5,  8,  5,  3,  9,  0,  5,  0,  3,  5, -1, -1, -1, -1}, // 222
	{ 9,  4,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 223
	{ 4, 11,  7,  4,  9, 11,  9, 10, 11, -1, -1, -1, -1, -1, -1, -1}, // 224
	{ 0,  8,  3,  4,  9,  7,  9, 11,  7,  9, 10, 11, -1, -1, -1, -1}, // 225
	{ 1, 10, 11,  1, 11,  4,  1,  4,  0,  7,  4, 11, -1, -1, -1, -1}, // 226
	{ 3,  1,  4,  3,  4,  8,  1, 10,  4,  7,  4, 11, 10, 11,  4, -1}, // 227
	{ 4, 11,  7,  9, 11,  4,  9,  2, 11,  9,  1,  2, -1, -1, -1, -1}, // 228
	{ 9,  7,  4,  9, 11,  7,  9,  1, 11,  2, 11,  1,  0,  8,  3, -1}, // 229
	{11,  7,  4, 11,  4,  2,  2,  4,  0, -1, -1, -1, -1, -1, -1, -1}, // 230
	{11,  7,  4, 11,  4,  2,  8,  3,  4,  3,  2,  4, -1, -1, -1, -1}, // 231
	{ 2,  9, 10,  2,  7,  9,  2,  3,  7,  7,  4,  9, -1, -1, -1, -1}, // 232
	{ 9, 10,  7,  9,  7,  4, 10,  2,  7,  8,  7,  0,  2,  0,  7, -1}, // 233
	{ 3,  7, 10,  3, 10,  2,  7,  4, 10,  1, 10,  0,  4,  0, 10, -1}, // 234
	{ 1, 10,  2,  8,  7,  4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 235
	{ 4,  9,  1,  4,  1,  7,  7,  1,  3, -1, -1, -1, -1, -1, -1, -1}, // 236
	{ 4,  9,  1,  4,  1,  7,  0,  8,  1,  8,  7,  1, -1, -1, -1, -1}, // 237
	{ 4,  0,  3,  7,  4,  3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 238
	{ 4,  8,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 239
	{ 9, 10,  8, 10, 11,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 240
	{ 3,  0,  9,  3,  9, 11, 11,  9, 10, -1, -1, -1, -1, -1, -1, -1}, // 241
	{ 0,  1, 10,  0, 10,  8,  8, 10, 11, -1, -1, -1, -1, -1, -1, -1}, // 242
	{ 3,  1, 10, 11,  3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 243
	{ 1,  2, 11,  1, 11,  9,  9, 11,  8, -1, -1, -1, -1, -1, -1, -1}, // 244
	{ 3,  0,  9,  3,  9, 11,  1,  2,  9,  2, 11,  9, -1, -1, -1, -1}, // 245
	{ 0,  2, 11,  8,  0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 246
	{ 3,  2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 247
	{ 2,  3,  8,  2,  8, 10, 10,  8,  9, -1, -1, -1, -1, -1, -1, -1}, // 248
	{ 9, 10,  2,  0,  9,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 249
	{ 2,  3,  8,  2,  8, 10,  0,  1,  8,  1, 10,  8, -1, -1, -1, -1}, // 250
	{ 1, 10,  2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 251
	{ 1,  3,  8,  9,  1,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 252
	{ 0,  9,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 253
	{ 0,  3,  8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // 254
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1} // 255
};

// ─── Material Color Mapping ────────────────────────────────────────────────────

FColor FAoCMarchingCubes::GetMaterialColor(EVoxelMaterial Material)
{
	switch (Material)
	{
	// Terrain
	case EVoxelMaterial::Air:          return FColor(0, 0, 0, 0);
	case EVoxelMaterial::Soil:         return FColor(139, 90, 43);
	case EVoxelMaterial::FertileSoil:  return FColor(62, 39, 18);
	case EVoxelMaterial::ForestSoil:   return FColor(54, 44, 28);
	case EVoxelMaterial::Clay:         return FColor(194, 133, 72);
	case EVoxelMaterial::Sand:         return FColor(210, 190, 140);
	case EVoxelMaterial::Rock:         return FColor(128, 128, 128);

	// Tier 1 — Common
	case EVoxelMaterial::OreCopper:    return FColor(34, 139, 34);     // Malachite green
	case EVoxelMaterial::OreTin:       return FColor(64, 48, 32);      // Cassiterite brown-black

	// Tier 2 — Common
	case EVoxelMaterial::OreIron:      return FColor(139, 26, 26);     // Hematite dark red
	case EVoxelMaterial::OreZinc:      return FColor(178, 160, 60);    // Sphalerite yellow-brown
	case EVoxelMaterial::OreLead:      return FColor(160, 160, 176);   // Galena silver-grey

	// Tier 3 — Uncommon
	case EVoxelMaterial::OreNickel:    return FColor(180, 160, 50);    // Pentlandite bronze-yellow
	case EVoxelMaterial::OreSilver:    return FColor(192, 192, 200);   // Bright silver
	case EVoxelMaterial::OreGold:      return FColor(255, 215, 0);     // Native gold yellow

	// Tier 4 — Rare
	case EVoxelMaterial::OreChromium:  return FColor(48, 32, 24);      // Chromite dark brown
	case EVoxelMaterial::OreCobalt:    return FColor(180, 180, 200);   // Cobaltite silver-white-pink
	case EVoxelMaterial::OreManganese: return FColor(100, 100, 108);   // Pyrolusite steel-grey
	case EVoxelMaterial::OreMolybdenum:return FColor(140, 140, 150);   // Molybdenite lead-grey

	// Tier 5 — Very Rare
	case EVoxelMaterial::OreTitanium:  return FColor(40, 20, 10);      // Ilmenite black-brown
	case EVoxelMaterial::OreTungsten:  return FColor(60, 40, 24);      // Wolframite dark brown
	case EVoxelMaterial::OreVanadium:  return FColor(220, 60, 20);     // Vanadinite bright red-orange
	case EVoxelMaterial::OrePlatinum:  return FColor(210, 210, 220);   // Native platinum silver-white
	case EVoxelMaterial::OrePalladium: return FColor(170, 170, 180);   // Braggite steel-grey

	// Tier 6 — Extremely Rare
	case EVoxelMaterial::OreRhodium:   return FColor(200, 200, 210);   // Silver-white
	case EVoxelMaterial::OreIridium:   return FColor(190, 200, 220);   // Silver with rainbow
	case EVoxelMaterial::OreNiobium:   return FColor(50, 36, 24);      // Columbite black-brown
	case EVoxelMaterial::OreTantalum:  return FColor(60, 30, 24);      // Tantalite black-red-brown
	case EVoxelMaterial::OreOsmium:    return FColor(180, 190, 220);   // Osmiridium bluish-white

	default: return FColor(255, 0, 255); // Magenta = unmapped material
	}
}

// ─── Density Sampling ──────────────────────────────────────────────────────────

float FAoCMarchingCubes::SampleDensity(const TArray<FVoxelData>& VoxelGrid, int32 X, int32 Y, int32 Z, int32 ChunkSize)
{
	X = FMath::Clamp(X, 0, ChunkSize - 1);
	Y = FMath::Clamp(Y, 0, ChunkSize - 1);
	Z = FMath::Clamp(Z, 0, ChunkSize - 1);
	const int32 Idx = X + Y * ChunkSize + Z * ChunkSize * ChunkSize;
	if (Idx >= 0 && Idx < VoxelGrid.Num())
	{
		return VoxelGrid[Idx].Density;
	}
	return 0.f;
}

EVoxelMaterial FAoCMarchingCubes::SampleMaterial(const TArray<FVoxelData>& VoxelGrid, int32 X, int32 Y, int32 Z, int32 ChunkSize)
{
	X = FMath::Clamp(X, 0, ChunkSize - 1);
	Y = FMath::Clamp(Y, 0, ChunkSize - 1);
	Z = FMath::Clamp(Z, 0, ChunkSize - 1);
	const int32 Idx = X + Y * ChunkSize + Z * ChunkSize * ChunkSize;
	if (Idx >= 0 && Idx < VoxelGrid.Num())
	{
		return VoxelGrid[Idx].Material;
	}
	return EVoxelMaterial::Air;
}

// ─── Vertex Interpolation ──────────────────────────────────────────────────────

FVector FAoCMarchingCubes::VertexInterp(const FVector& P1, const FVector& P2, float V1, float V2, float IsoLevel)
{
	if (FMath::Abs(IsoLevel - V1) < KINDA_SMALL_NUMBER)
	{
		return P1;
	}
	if (FMath::Abs(IsoLevel - V2) < KINDA_SMALL_NUMBER)
	{
		return P2;
	}
	if (FMath::Abs(V1 - V2) < KINDA_SMALL_NUMBER)
	{
		return P1;
	}

	const float Mu = (IsoLevel - V1) / (V2 - V1);
	return P1 + Mu * (P2 - P1);
}

// ─── Normal Computation (Central Differences) ──────────────────────────────────

FVector FAoCMarchingCubes::ComputeNormal(const TArray<FVoxelData>& VoxelGrid, int32 X, int32 Y, int32 Z, int32 ChunkSize)
{
	// Gradient of the density field — normals point from solid toward air
	const float Dx = SampleDensity(VoxelGrid, X + 1, Y, Z, ChunkSize) - SampleDensity(VoxelGrid, X - 1, Y, Z, ChunkSize);
	const float Dy = SampleDensity(VoxelGrid, X, Y + 1, Z, ChunkSize) - SampleDensity(VoxelGrid, X, Y - 1, Z, ChunkSize);
	const float Dz = SampleDensity(VoxelGrid, X, Y, Z + 1, ChunkSize) - SampleDensity(VoxelGrid, X, Y, Z - 1, ChunkSize);

	FVector Normal(-Dx, -Dy, -Dz);
	if (!Normal.IsNearlyZero())
	{
		Normal.Normalize();
	}
	else
	{
		Normal = FVector::UpVector;
	}
	return Normal;
}

// ─── Main Mesh Generation ──────────────────────────────────────────────────────

void FAoCMarchingCubes::GenerateMesh(
	const TArray<FVoxelData>& VoxelGrid,
	int32 ChunkSize,
	float VoxelSize,
	float IsoLevel,
	TArray<FVector>& OutVertices,
	TArray<int32>& OutTriangles,
	TArray<FVector>& OutNormals,
	TArray<FVector2D>& OutUVs,
	TArray<FColor>& OutColors)
{
	GenerateMeshLOD(VoxelGrid, ChunkSize, VoxelSize, IsoLevel, 1, OutVertices, OutTriangles, OutNormals, OutUVs, OutColors);
}

void FAoCMarchingCubes::GenerateMeshLOD(
	const TArray<FVoxelData>& VoxelGrid,
	int32 ChunkSize,
	float VoxelSize,
	float IsoLevel,
	int32 LODStep,
	TArray<FVector>& OutVertices,
	TArray<int32>& OutTriangles,
	TArray<FVector>& OutNormals,
	TArray<FVector2D>& OutUVs,
	TArray<FColor>& OutColors)
{
	OutVertices.Reset();
	OutTriangles.Reset();
	OutNormals.Reset();
	OutUVs.Reset();
	OutColors.Reset();

	if (VoxelGrid.Num() < ChunkSize * ChunkSize * ChunkSize)
	{
		return;
	}

	LODStep = FMath::Max(LODStep, 1);
	const float StepSize = VoxelSize * LODStep;

	// Estimate capacity to reduce allocations
	const int32 EstimatedVerts = (ChunkSize / LODStep) * (ChunkSize / LODStep) * 6;
	OutVertices.Reserve(EstimatedVerts);
	OutTriangles.Reserve(EstimatedVerts);
	OutNormals.Reserve(EstimatedVerts);
	OutUVs.Reserve(EstimatedVerts);
	OutColors.Reserve(EstimatedVerts);

	// Cube corner offsets (relative indices)
	// Vertex layout follows Paul Bourke convention:
	//   4 ---- 5
	//  /|     /|
	// 7 ---- 6 |     Y
	// | 0 ---| 1     |  Z
	// |/     |/      | /
	// 3 ---- 2       +--- X
	const int32 CornerOffsets[8][3] = {
		{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
		{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
	};

	// Edge vertex pairs (which two corners each of the 12 edges connects)
	const int32 EdgePairs[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0},
		{4,5}, {5,6}, {6,7}, {7,4},
		{0,4}, {1,5}, {2,6}, {3,7}
	};

	// Iterate over cubes in the grid
	for (int32 Z = 0; Z < ChunkSize - LODStep; Z += LODStep)
	{
		for (int32 Y = 0; Y < ChunkSize - LODStep; Y += LODStep)
		{
			for (int32 X = 0; X < ChunkSize - LODStep; X += LODStep)
			{
				// Sample density at 8 cube corners
				float CubeValues[8];
				EVoxelMaterial CubeMaterials[8];
				FVector CubePositions[8];

				for (int32 C = 0; C < 8; ++C)
				{
					const int32 Cx = X + CornerOffsets[C][0] * LODStep;
					const int32 Cy = Y + CornerOffsets[C][1] * LODStep;
					const int32 Cz = Z + CornerOffsets[C][2] * LODStep;

					CubeValues[C] = SampleDensity(VoxelGrid, Cx, Cy, Cz, ChunkSize);
					CubeMaterials[C] = SampleMaterial(VoxelGrid, Cx, Cy, Cz, ChunkSize);
					CubePositions[C] = FVector(Cx * VoxelSize, Cy * VoxelSize, Cz * VoxelSize);
				}

				// Build cube index from corner densities vs isolevel
				int32 CubeIndex = 0;
				for (int32 C = 0; C < 8; ++C)
				{
					if (CubeValues[C] < IsoLevel)
					{
						CubeIndex |= (1 << C);
					}
				}

				// Skip entirely inside or outside cubes
				if (EdgeTable[CubeIndex] == 0)
				{
					continue;
				}

				// Find interpolated vertices on intersected edges
				FVector EdgeVertices[12];
				FVector EdgeNormals[12];
				EVoxelMaterial EdgeMaterials[12];

				for (int32 E = 0; E < 12; ++E)
				{
					if (EdgeTable[CubeIndex] & (1 << E))
					{
						const int32 V0 = EdgePairs[E][0];
						const int32 V1 = EdgePairs[E][1];

						EdgeVertices[E] = VertexInterp(
							CubePositions[V0], CubePositions[V1],
							CubeValues[V0], CubeValues[V1],
							IsoLevel);

						// Interpolate normal
						const int32 X0 = X + CornerOffsets[V0][0] * LODStep;
						const int32 Y0 = Y + CornerOffsets[V0][1] * LODStep;
						const int32 Z0 = Z + CornerOffsets[V0][2] * LODStep;
						const int32 X1 = X + CornerOffsets[V1][0] * LODStep;
						const int32 Y1 = Y + CornerOffsets[V1][1] * LODStep;
						const int32 Z1 = Z + CornerOffsets[V1][2] * LODStep;

						const FVector N0 = ComputeNormal(VoxelGrid, X0, Y0, Z0, ChunkSize);
						const FVector N1 = ComputeNormal(VoxelGrid, X1, Y1, Z1, ChunkSize);

						const float Mu = (FMath::Abs(CubeValues[V1] - CubeValues[V0]) > KINDA_SMALL_NUMBER)
							? (IsoLevel - CubeValues[V0]) / (CubeValues[V1] - CubeValues[V0])
							: 0.5f;

						EdgeNormals[E] = FMath::Lerp(N0, N1, Mu);
						EdgeNormals[E].Normalize();

						// Pick the dominant (solid) material for this edge
						EdgeMaterials[E] = (CubeValues[V0] >= IsoLevel) ? CubeMaterials[V0] : CubeMaterials[V1];
					}
				}

				// Build triangles from the TriTable
				for (int32 T = 0; TriTable[CubeIndex][T] != -1; T += 3)
				{
					const int32 BaseIndex = OutVertices.Num();

					for (int32 V = 0; V < 3; ++V)
					{
						const int32 EdgeIdx = TriTable[CubeIndex][T + V];

						OutVertices.Add(EdgeVertices[EdgeIdx]);
						OutNormals.Add(EdgeNormals[EdgeIdx]);
						OutColors.Add(GetMaterialColor(EdgeMaterials[EdgeIdx]));

						// Simple planar UV from world position
						const FVector& Pos = EdgeVertices[EdgeIdx];
						OutUVs.Add(FVector2D(Pos.X / (ChunkSize * VoxelSize), Pos.Y / (ChunkSize * VoxelSize)));
					}

					// Add triangle indices (wound CCW for UE5)
					OutTriangles.Add(BaseIndex);
					OutTriangles.Add(BaseIndex + 1);
					OutTriangles.Add(BaseIndex + 2);
				}
			}
		}
	}
}
