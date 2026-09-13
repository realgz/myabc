// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/config/guids.cpp --- GUID 定义（唯一 translation unit）
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.2

#include <initguid.h>   // 让随后的 DEFINE_GUID 产生定义而非声明

#include <guiddef.h>

// DECISION: src/config/guids.hpp —— 设计常量，发布后不可变。
// {8BA238DD-B6B4-42B4-95C1-8062D25B3652}
DEFINE_GUID(CLSID_MyabcTextService,
            0x8ba238dd, 0xb6b4, 0x42b4, 0x95, 0xc1, 0x80, 0x62, 0xd2, 0x5b, 0x36, 0x52);

// {C4FBBA94-26BC-4C7A-B9F1-4F91A723C60A}
DEFINE_GUID(GUID_MyabcProfile,
            0xc4fbba94, 0x26bc, 0x4c7a, 0xb9, 0xf1, 0x4f, 0x91, 0xa7, 0x23, 0xc6, 0x0a);

// {68B73434-2D30-4DB5-B479-C4B7A5E96B12}
DEFINE_GUID(GUID_MyabcSchemeLangBar,
            0x68b73434, 0x2d30, 0x4db5, 0xb4, 0x79, 0xc4, 0xb7, 0xa5, 0xe9, 0x6b, 0x12);
