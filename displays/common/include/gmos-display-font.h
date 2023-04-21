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
 * This header defines the portable API for accessing bitmap font data.
 * Standard ASCII monospaced and proportional bitmap fonts are currently
 * supported.
 */

#ifndef GMOS_DISPLAY_FONT_H
#define GMOS_DISPLAY_FONT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Defines the set of font character encodings that may be used by
 * individual font definitions. The selected font encoding determines
 * the characters that are available for display using each font.
 */
typedef enum {

    // This font encoding specifies the set of numeric digits using
    // direct BCD decoding. In addition, the codepoints from 0x30 to
    // 0x39 are mapped to the appropriate digits to be consistent with
    // standard ASCII, ISO 8849 and UTF-8 character encodings.
    GMOS_DISPLAY_FONT_ENCODING_DIGITS,

    // This font encoding specifies the printable ASCII character set
    // from codepoint 0x20 (space) to 0x7E. In addition, the control
    // character codepoints from 0 to 9 are mapped to the ASCII
    // codepoints for characters '0' to '9' to support direct BCD
    // decoding.
    GMOS_DISPLAY_FONT_ENCODING_ASCII

} gmosDisplayFontEncoding_t;

/**
 * Defines the data structure that is used to encapsulate a single font
 * definition. This includes the basic font information and pointers to
 * various data arrays that contain character specific data.
 */
typedef struct gmosDisplayFontDef_t {

    // The font name is a pointer to a character array that includes the
    // canonical font name.
    const char* fontName;

    // The character data pointer refers to an array of octets that
    // contain the character bitmap data for all supported characters.
    // The character data format is consistent with the common display
    // bitmap image format.
    const uint8_t* charData;

    // The character widths data pointer refers to an array of octets
    // that specify the widths of each character in a proportionally
    // spaced font. A null reference is used for monospaced fonts.
    const uint8_t* charWidths;

    // The character index data pointer refers to an array of offsets
    // into the character data array at which the data for each
    // character is located. This is only required for large fonts that
    // may use a different number of octets to represent different
    // characters. A null reference is used for fonts where all
    // characters are represented by the same number of octets.
    const uint16_t* charIndex;

    // The font encoding specifies the set of supported font code
    // points. The valid options are specified by the enumeration
    // defined by gmosDisplayFontEncoding_t.
    uint8_t fontEncoding;

    // The font width specifies the maximum width of a character in the
    // font definition, expressed as an integer number of pixels. For
    // monospaced fonts, all characters will have this width.
    uint8_t fontWidth;

    // The font height specifies the common character height for all
    // characters in the font definition, expressed as an integer number
    // of pixels.
    uint8_t fontHeight;

    // The font baseline value specifies the number of pixels of the
    // specified font height that fall below the font baseline.
    uint8_t fontBaseline;

} gmosDisplayFontDef_t;

/**
 * Defines the data structure that is used to encapsulate the data for a
 * single font character. This is typically populated during a font
 * character lookup so that the character may be plotted using the
 * appropriate display driver.
 */
typedef struct gmosDisplayFontChar_t {

    // The character data pointer refers to an array of octets that
    // contain the character bitmap data for the requested code point.
    // The character data format is consistent with the common display
    // bitmap image format.
    const uint8_t* charData;

    // The character encoding specifies the code point that was used to
    // access the font character data.
    uint8_t charEncoding;

    // The character width specifies the width of the character bitmap
    // as an integer number of pixels.
    uint8_t charWidth;

    // The character height specifies the height of the character bitmap
    // as an integer number of pixels.
    uint8_t charHeight;

    // The character baseline value specifies the number of pixels of
    // the specified character height that fall below the font baseline.
    uint8_t charBaseline;

} gmosDisplayFontChar_t;

/**
 * Performs a font character lookup using a given font definition and
 * code point. The font character information is used to populate a font
 * character data structure allocated by the caller.
 * @param fontDef This is a pointer to the font definition that is to be
 *     used for the font character lookup.
 * @param codepoint This is the code point that is to be used for
 *     selecting the character data. The mapping of code point to
 *     character glyph is dictated by the font's encoding option.
 * @param fontChar This is a pointer to the font character data
 *     structure which is to be populated with the font character data
 *     that corresponds to the requested code point.
 * @return Returns a boolean status value which will be set to 'true'
 *     if the specified code point is supported by the font definition
 *     and 'false' otherwise.
 */
bool gmosDisplayFontLookup (const gmosDisplayFontDef_t* fontDef,
    uint8_t codepoint, gmosDisplayFontChar_t* fontChar);

#endif // GMOS_DISPLAY_FONT_H
