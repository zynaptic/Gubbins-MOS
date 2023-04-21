#!/usr/bin/env python3

#
# The Gubbins Microcontroller Operating System
#
# Copyright 2023 Zynaptic Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#

#
# This tool is used to generate 'C' source code for bitmap font
# definitions that can be used with the GubbinsMOS common display
# libraries. Only fonts represented using BDF (Adobe Glyph Bitmap
# Distribution Format) with ASCII encoding are currently supported.
#

import re
import argparse
from pathlib import Path
from string import Template

#
# Extracts the command line arguments.
#
def parseCommandLine():
    parser = argparse.ArgumentParser(
        description="This script is used to generate 'C' source code "
        + "for GubbinsMOS bitmap font definitions.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--font_dir",
        required=True,
        help="the source directory holding the BDF font files",
    )
    parser.add_argument(
        "--code_file",
        required=True,
        help="the name of the generated 'C' source code output file",
    )
    parser.add_argument(
        "--header_file",
        required=True,
        help="the name of the generated 'C' header output file",
    )
    args = parser.parse_args()
    return args


#
# Defines the text block to be included at the start of the C header
# file.
#
headerFilePrefix = """/*
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
 * GubbinsMOS auto generated code - do not edit.
 */

#ifndef GMOS_DISPLAY_FONT_DEFS_H
#define GMOS_DISPLAY_FONT_DEFS_H

#include "gmos-display-font.h"
"""

#
# Defines the template for the font information comment.
#
fontInfoCommentTemplate = """
/*
 * Font name     : $fontName
 * Font height   : $fontHeight
 * Font width    : $fontWidth
 * Font spacing  : $fontSpacing
 * Character set : $charSetName
 */
"""

#
# Defines the template for the generated header information.
#
headerFileTemplate = Template(
    fontInfoCommentTemplate
    + """
extern const gmosDisplayFontDef_t gmosDisplayFontDef_$safeFontName;
"""
)

#
# Defines the text block to be included at the end of the C header
# file.
#
headerFilePostfix = """
#endif // GMOS_DISPLAY_FONT_DEFS_H
"""

#
# Defines the text block to be included at the start of the C source
# code.
#
codeFilePrefix = """/*
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
 * GubbinsMOS auto generated code - do not edit.
 */

#include <stdint.h>
#include <stddef.h>

#include "gmos-display-font.h"
"""

#
# Defines the template for the generated monospace font C source code.
#
monospacedFileTemplate = Template(
    fontInfoCommentTemplate
    + """
static const uint8_t gmosDisplayFontCharData_$safeFontName[] = {
$charDataBlock
};

const gmosDisplayFontDef_t gmosDisplayFontDef_$safeFontName = {
    "$fontName",
    gmosDisplayFontCharData_$safeFontName,
    NULL,
    NULL,
    $fontEncoding,
    $fontWidth,
    $fontHeight,
    $fontBaseline
};
"""
)

#
# Defines the template for the generated proportional font C source code
# using fixed sized character data (used for small fonts).
#
proportionalFileTemplate = Template(
    fontInfoCommentTemplate
    + """
static const uint8_t gmosDisplayFontCharData_$safeFontName[] = {
$charDataBlock
};

static const uint8_t gmosDisplayFontCharWidths_$safeFontName[] = {
$charWidthsBlock
};

const gmosDisplayFontDef_t gmosDisplayFontDef_$safeFontName = {
    "$fontName",
    gmosDisplayFontCharData_$safeFontName,
    gmosDisplayFontCharWidths_$safeFontName,
    NULL,
    $fontEncoding,
    $fontWidth,
    $fontHeight,
    $fontBaseline
};
"""
)

#
# Defines the template for the generated proportional font C source code
# using indexed character data (used for large fonts).
#
proportionalIndexedFileTemplate = Template(
    fontInfoCommentTemplate
    + """
static const uint8_t gmosDisplayFontCharData_$safeFontName[] = {
$charDataBlock
};

static const uint8_t gmosDisplayFontCharWidths_$safeFontName[] = {
$charWidthsBlock
};

static const uint16_t gmosDisplayFontCharIndex_$safeFontName[] = {
$charIndexBlock
};

const gmosDisplayFontDef_t gmosDisplayFontDef_$safeFontName = {
    "$fontName",
    gmosDisplayFontCharData_$safeFontName,
    gmosDisplayFontCharWidths_$safeFontName,
    gmosDisplayFontCharIndex_$safeFontName,
    $fontEncoding,
    $fontWidth,
    $fontHeight,
    $fontBaseline
};
"""
)

#
# Generates the C header file external label for a font definition data
# structure, together with useful information about the font.
#
def formatFontHeader(fontDef):
    fontName = fontDef["fontName"]
    safeFontName = re.sub("\W+", "_", fontName)
    if fontDef["fontMonospaced"]:
        fontSpacing = "Monospaced"
    else:
        fontSpacing = "Proportional"
    return headerFileTemplate.substitute(
        fontName=fontName,
        safeFontName=safeFontName,
        fontSpacing=fontSpacing,
        fontHeight=fontDef["fontHeight"],
        fontWidth=fontDef["fontWidth"],
        charSetName=fontDef["charSetName"],
    )


#
# Generates the C source code for a font definition, given a source font
# definition, a font encoding type name and a list of codepoints to be
# included.
#
def formatFontData(fontDef, fontEncoding, codePointList):
    fontName = fontDef["fontName"]
    fontWidth = fontDef["fontWidth"]
    fontHeight = fontDef["fontHeight"]
    fontBaseline = fontDef["fontBaseline"]
    fontMonospaced = fontDef["fontMonospaced"]
    charSet = fontDef["charSet"]
    charWidths = fontDef.get("charWidths")
    charDataBlock = "    "
    charWidthsBlock = "    "
    charIndexBlock = "    "
    charIndex = 0
    omitCharIndex = True
    if fontMonospaced:
        fontSpacing = "Monospaced"
    else:
        fontSpacing = "Proportional"

    # Copy the numeric digit character data over.
    for codePoint in codePointList:

        # Populate the character index array.
        charIndexBlock += "%d" % charIndex
        if codePoint != codePointList[-1]:
            charIndexBlock += ", "

        # Populate the character width array.
        if codePoint in charWidths:
            charWidth = charWidths[codePoint]
        else:
            charWidth = fontWidth
        charWidthsBlock += "%d" % charWidth
        if codePoint != codePointList[-1]:
            charWidthsBlock += ", "

        # Check for large font configuration.
        if ((charWidth - 1) // 8) != ((fontWidth - 1) // 8):
            omitCharIndex = False

        # Substitute a space for undefined characters.
        if codePoint in charSet:
            charData = charSet[codePoint]
        else:
            charData = charSet[0x20]

        # Copy the character data, trimming excess octets.
        for i in range(fontHeight):
            for j in range(((charWidth - 1) // 8) + 1):
                charDataBlock += "0x%02X" % (
                    charData[i * (((fontWidth - 1) // 8) + 1) + j]
                )
                charIndex += 1
                if (i < fontHeight - 1) or (j < ((charWidth - 1) // 8)):
                    charDataBlock += ", "
                elif codePoint != codePointList[-1]:
                    charDataBlock += ",\n    "

    # To make the font name a safe C variable name, replace all
    # non-alphanumeric characters with an underscore.
    safeFontName = re.sub("\W+", "_", fontName)

    # Populate the monospaced font definition source file template.
    if fontMonospaced:
        sourceFileData = monospacedFileTemplate.substitute(
            fontName=fontName,
            safeFontName=safeFontName,
            charSetName=fontDef["charSetName"],
            fontEncoding=fontEncoding,
            charDataBlock=charDataBlock,
            fontSpacing=fontSpacing,
            fontWidth=fontWidth,
            fontHeight=fontHeight,
            fontBaseline=fontBaseline,
        )

    # Populate the proportional font definition source file template.
    elif omitCharIndex:
        sourceFileData = proportionalFileTemplate.substitute(
            fontName=fontName,
            safeFontName=safeFontName,
            charSetName=fontDef["charSetName"],
            fontEncoding=fontEncoding,
            charDataBlock=charDataBlock,
            charWidthsBlock=charWidthsBlock,
            fontSpacing=fontSpacing,
            fontWidth=fontWidth,
            fontHeight=fontHeight,
            fontBaseline=fontBaseline,
        )

    # Populate the proportional font definition source file template
    # using a character index for large fonts.
    else:
        sourceFileData = proportionalIndexedFileTemplate.substitute(
            fontName=fontName,
            safeFontName=safeFontName,
            charSetName=fontDef["charSetName"],
            fontEncoding=fontEncoding,
            charDataBlock=charDataBlock,
            charWidthsBlock=charWidthsBlock,
            charIndexBlock=charIndexBlock,
            fontSpacing=fontSpacing,
            fontWidth=fontWidth,
            fontHeight=fontHeight,
            fontBaseline=fontBaseline,
        )
    return sourceFileData


#
# Generates the C source code for a font definition in standard ASCII
# format. This includes the ASCII codepoints from 0x20 (space) to 0x7E.
#
def formatStandardAsciiCode(fontDef):
    fontDef["charSetName"] = "ASCII"
    fontEncoding = "GMOS_DISPLAY_FONT_ENCODING_ASCII"
    codePointList = range(0x20, 0x7F)
    return formatFontData(fontDef, fontEncoding, codePointList)


#
# Parsea a font character definition in the BDF text format.
#
def parseFontCharBDF(fontDef, charLines):
    fontWidth = fontDef["fontWidth"]
    fontHeight = fontDef["fontHeight"]
    fontBaseline = fontDef["fontBaseline"]

    # Specify no padding data by default.
    codePoint = -1
    padUpper = 0
    padLower = 0
    padLeft = 0
    padRight = 0
    charWidth = fontWidth

    # Extract the codepoint and bounding box padding settings.
    lineIter = iter(charLines)
    while True:
        line = next(lineIter)
        if line.startswith("BITMAP"):
            break
        elif line.startswith("ENCODING"):
            codePoint = int(line.split()[1])
        elif line.startswith("BBX"):
            boundsText = line.split()[1:]
            bounds = [int(bt) for bt in boundsText]
            if bounds[2] < 0:
                return
            padLeft = bounds[2]
            padRight = fontWidth - padLeft - bounds[0]
            padLower = fontBaseline + bounds[3]
            padUpper = fontHeight - padLower - bounds[1]
        elif line.startswith("DWIDTH"):
            stepsText = line.split()[1:]
            steps = [int(bt) for bt in stepsText]
            charWidth = steps[0]
            if charWidth != fontWidth:
                fontDef["fontMonospaced"] = False

    # Convert each pixel line into the little endian format required for
    # GubbinsMOS display bitmaps.
    charBytes = []
    for i in range(fontHeight):
        if (i < padUpper) or (i >= fontHeight - padLower):
            charBytes.append(0)
            if fontWidth > 8:
                charBytes.append(0)
            if fontWidth > 16:
                charBytes.append(0)
            if fontWidth > 24:
                charBytes.append(0)
        else:
            line = next(lineIter)
            sourceData = int(line, 16)
            if bounds[0] <= 8:
                sourceData <<= 24
            elif bounds[0] <= 16:
                sourceData <<= 16
            elif bounds[0] <= 24:
                sourceData <<= 8
            sourceData >>= padLeft
            charData = 0
            for j in range(32):
                charData <<= 1
                if (sourceData & 1) != 0:
                    charData |= 1
                sourceData >>= 1
            charBytes.append(charData & 0xFF)
            if fontWidth > 8:
                charBytes.append((charData >> 8) & 0xFF)
            if fontWidth > 16:
                charBytes.append((charData >> 16) & 0xFF)
            if fontWidth > 24:
                charBytes.append((charData >> 24) & 0xFF)
        fontDef["charSet"][codePoint] = charBytes
        fontDef["charWidths"][codePoint] = charWidth


#
# Parses a font definition file in the BDF text format.
#
def parseFontBDF(fontName, fileName):
    with open(fileName, mode="r") as f:
        fileLines = [line.rstrip() for line in f]
    fontDef = {
        "fontName": fontName,
        "charSet": {},
        "charWidths": {},
    }
    print("Parsing font file : " + str(fileName))

    # Get the bounding box from the header. This assumes that the 'X'
    # offset for the font bounding box will always be set to zero.
    lineIter = iter(fileLines)
    while True:
        line = next(lineIter)
        if line.startswith("FONTBOUNDINGBOX"):
            boundsText = line.split()[1:]
            bounds = [int(bt) for bt in boundsText]
            fontWidth = bounds[0]
            fontHeight = bounds[1]
            fontBaseline = -bounds[3]
            fontDef["fontWidth"] = fontWidth
            fontDef["fontHeight"] = fontHeight
            fontDef["fontBaseline"] = fontBaseline
            fontDef["fontMonospaced"] = True
            break

    # Process each character in turn.
    while True:
        line = next(lineIter)
        if line.startswith("ENDFONT"):
            break
        if line.startswith("STARTCHAR"):
            charLines = [
                line,
            ]
            while not line.startswith("ENDCHAR"):
                line = next(lineIter)
                charLines.append(line)
            parseFontCharBDF(fontDef, charLines)

    return fontDef


#
# Provide main entry point.
#
def main(params):
    fontDir = params.font_dir
    codeFileName = params.code_file
    headerFileName = params.header_file

    # Set up the header and source file code blocks.
    codeFile = open(codeFileName, mode="w")
    codeFile.write(codeFilePrefix)
    headerFile = open(headerFileName, mode="w")
    headerFile.write(headerFilePrefix)

    # Process fonts which contain the standard ASCII character set.
    asciiDir = Path(fontDir).joinpath("ascii")
    fontFileNames = asciiDir.glob("**/*.bdf")
    for fontFileName in sorted(fontFileNames):
        fontName = fontFileName.stem
        fontDef = parseFontBDF(fontName, asciiDir.joinpath(fontFileName))
        codeFile.write(formatStandardAsciiCode(fontDef))
        headerFile.write(formatFontHeader(fontDef))

    # Cleanup output files.
    headerFile.write(headerFilePostfix)
    headerFile.close()
    codeFile.close()


#
# Run the script with the provided command line options.
#
try:
    params = parseCommandLine()
    main(params)
except KeyboardInterrupt as e:
    exit()
except Exception as e:
    print(e)
    exit()
