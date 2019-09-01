/* MIT License
 *
 * Copyright (c) 2019 Andreas Merkle <web@blue-andi.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*******************************************************************************
    DESCRIPTION
*******************************************************************************/
/**
@brief  Text Widget
@author Andreas Merkle <web@blue-andi.de>

@section desc Description
This module provides the a text widget, showing a colored string.

*******************************************************************************/
/** @defgroup textwidget Text Widget
 * This module provides the a text widget, showing a colored string.
 *
 * @{
 */

#ifndef __TEXTWIDGET_H__
#define __TEXTWIDGET_H__

/******************************************************************************
 * Compile Switches
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/
#include <stdint.h>
#include <WString.h>
#include <Widget.hpp>

/******************************************************************************
 * Macros
 *****************************************************************************/

/******************************************************************************
 * Types and Classes
 *****************************************************************************/

/**
 * A text widget, showing a colored string.
 */
class TextWidget : public Widget
{
public:

    /**
     * Constructs a text widget with a empty string in default color.
     */
    TextWidget() :
        Widget(WIDGET_TYPE),
        m_str(),
        m_textColor(DEFAULT_TEXT_COLOR),
        m_font(DEFAULT_FONT),
        m_checkScrollingNeed(false),
        m_isScrollingEnabled(false),
        m_scrollIndex(0u),
        m_lastTimestamp(0u)
    {
    }

    /**
     * Constructs a text widget with the given string and its color.
     * If there is no color given, it will be default color.
     * 
     * @param[in] str   String
     * @param[in] color Color of the string
     */
    TextWidget(const String& str, uint16_t color = DEFAULT_TEXT_COLOR) :
        Widget(WIDGET_TYPE),
        m_str(str),
        m_textColor(color),
        m_font(DEFAULT_FONT),
        m_checkScrollingNeed(false),
        m_isScrollingEnabled(false),
        m_scrollIndex(0u),
        m_lastTimestamp(0u)
    {
    }

    /**
     * Constructs a text widget by copying another one.
     * 
     * @param[in] widget Widget, which to copy
     */
    TextWidget(const TextWidget& widget) :
        Widget(WIDGET_TYPE),
        m_str(widget.m_str),
        m_textColor(widget.m_textColor),
        m_font(widget.m_font),
        m_checkScrollingNeed(widget.m_checkScrollingNeed),
        m_isScrollingEnabled(widget.m_isScrollingEnabled),
        m_scrollIndex(widget.m_scrollIndex),
        m_lastTimestamp(widget.m_lastTimestamp)
    {
    }

    /**
     * Assign the content of a text widget.
     * 
     * @param[in] widget Widget, which to assign
     */
    TextWidget& operator=(const TextWidget& widget)
    {
        m_str                   = widget.m_str;
        m_textColor             = widget.m_textColor;
        m_font                  = widget.m_font;
        m_checkScrollingNeed    = widget.m_checkScrollingNeed;
        m_isScrollingEnabled    = widget.m_isScrollingEnabled;
        m_scrollIndex           = widget.m_scrollIndex;
        m_lastTimestamp         = widget.m_lastTimestamp;

        return *this;
    }

    /**
     * Update/Draw the text widget.
     * 
     * @param[in] gfx Graphics interface
     */
    void update(Adafruit_GFX& gfx);

    /**
     * Set the text string.
     * 
     * @param[in] str String
     */
    void setStr(const String& str)
    {
        m_str                   = str;
        m_checkScrollingNeed    = true;
        return;
    }

    /**
     * Get the text string.
     * 
     * @return String
     */
    String getStr(void) const
    {
        return m_str;
    }

    /**
     * Set the text color of the string.
     * 
     * @param[in] color Text color
     */
    void setTextColor(uint16_t color)
    {
        m_textColor = color;
        return;
    }

    /**
     * Get the text color of the string.
     * 
     * @return Text color
     */
    uint16_t getTextColor(void) const
    {
        return m_textColor;
    }

    /**
     * Set font.
     * 
     * @param[in] font  New font to set
     */
    void setFont(const GFXfont* font)
    {
        m_font                  = font;
        m_checkScrollingNeed    = true;
        return;
    }

    /**
     * Get font.
     * 
     * @return If a font is set, it will be returned otherwise NULL.
     */
    const GFXfont* getFont(void) const
    {
        return m_font;
    }

    /** Default text color */
    static const uint16_t   DEFAULT_TEXT_COLOR;
    
    /** Widget type string */
    static const char*      WIDGET_TYPE;

    /** Default font */
    static const GFXfont*   DEFAULT_FONT;

    /** Default pause between character scrolling in ms */
    static const uint32_t   DEFAULT_SCOLL_PAUSE;

private:

    String          m_str;                  /**< String */
    uint16_t        m_textColor;            /**< Text color of the string */
    const GFXfont*  m_font;                 /**< Current font */
    bool            m_checkScrollingNeed;   /**< Check for scrolling need or not */
    bool            m_isScrollingEnabled;   /**< Is scrolling enabled or disabled */
    uint8_t         m_scrollIndex;          /**< Scroll index of the string */
    uint32_t        m_lastTimestamp;        /**< Timestamp in ms of last processing. */

};

/******************************************************************************
 * Functions
 *****************************************************************************/

#endif  /* __TEXTWIDGET_H__ */

/** @} */