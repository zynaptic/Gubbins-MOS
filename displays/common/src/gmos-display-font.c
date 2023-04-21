/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2023 Zynaptic Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/*
 * This file implements the portable API for accessing bitmap font data.
 * Standard ASCII monospaced and proportional bitmap fonts are currently
 * supported.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "gmos-display-font.h"

/*
 * Perform font character lookups for the standard ASCII encoding.
 */
static inline bool gmosDisplayFontLookupAscii (
    const gmosDisplayFontDef_t* fontDef, uint8_t codepoint,
    gmosDisplayFontChar_t* fontChar)
{
    uint32_t codeOffset;
    uint32_t dataOffset;

    // The first 10 code points are automatically mapped to the numeric
    // digits in order to support direct BCD decoding.
    if (codepoint < 10) {
        codepoint += 0x30;
    }

    // Check for the supported range of printable ASCII code points.
    if ((codepoint < 0x20) || (codepoint >= 0x7F)) {
        return false;
    }

    // Derive the data offset in the font data array. The character
    // data index is used for fonts with variable sized characters,
    // otherwise the offset is calculated from the fixed font character
    // size.
    codeOffset = codepoint - 0x20;
    if (fontDef->charIndex != NULL) {
        dataOffset = fontDef->charIndex [codeOffset];
    } else {
        dataOffset = codeOffset * fontDef->fontHeight *
            ((fontDef->fontWidth + 7) / 8);
    }

    // Populate the font character data structure.
    fontChar->charData = &(fontDef->charData [dataOffset]);
    fontChar->charEncoding = codepoint;
    fontChar->charHeight = fontDef->fontHeight;
    fontChar->charBaseline = fontDef->fontBaseline;

    // Select character width for monospaced or proportionally spaced
    // fonts.
    if (fontDef->charWidths == NULL) {
        fontChar->charWidth = fontDef->fontWidth;
    } else {
        fontChar->charWidth = fontDef->charWidths [codeOffset];
    }
    return true;
}

/*
 * Performs a font character lookup using a given font definition and
 * code point.
 */
bool gmosDisplayFontLookup (const gmosDisplayFontDef_t* fontDef,
    uint8_t codepoint, gmosDisplayFontChar_t* fontChar)
{
    bool lookupOk;

    // Perform lookups using the specified font encoding.
    switch (fontDef->fontEncoding) {
        case GMOS_DISPLAY_FONT_ENCODING_ASCII :
            lookupOk = gmosDisplayFontLookupAscii (
                fontDef, codepoint, fontChar);
            break;
        default :
            lookupOk = false;
            break;
    }
    return lookupOk;
}
