/*
* ========================================================================== *
*                                                                            *
*    This file is part of the Openterface Mini KVM App QT version            *
*                                                                            *
*    Copyright (C) 2024   <info@openterface.com>                             *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation version 3.                                 *
*                                                                            *
*    This program is distributed in the hope that it will be useful, but     *
*    WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
*    General Public License for more details.                                *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see <http://www.gnu.org/licenses/>.    *
*                                                                            *
*    This program incorporates portions of the AutoHotkey source code,       *
*    which is licensed under the GNU General Public License version 2 or     *
*    later. The original AutoHotkey source code can be found at:             *
*    https://github.com/Lexikos/AutoHotkey_L                                 *
*                                                                            *
*                                                                            *
*    The AutoHotkey source code is copyrighted by the AutoHotkey             *
*    development team and contributors.                                      *
*                                                                            *
* ========================================================================== *
*/

#ifndef KEYBOARDMOUSE_H
#define KEYBOARDMOUSE_H
#include <cstdint>
#include <array>
#include <QByteArray>

// keyboard data packet
struct keyPacket
{
    uint8_t control = 0x00;
    uint8_t costant = 0x00;
    std::array<uint8_t, 6> general;

    keyPacket(const std::array<uint8_t, 6>& gen, uint8_t ctrl = 0x00)
        : control(ctrl), general(gen) {}
    
    QByteArray toQByteArray() const {
        QByteArray byteArray;
        byteArray.append(control);          
        byteArray.append(constant);         
        for (const auto& byte : general) {  
            byteArray.append(byte);
        }
        return byteArray;
    }
};


#endif // KEYBOARDMOUSE_H