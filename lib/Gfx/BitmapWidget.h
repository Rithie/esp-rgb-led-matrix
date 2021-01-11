/* MIT License
 *
 * Copyright (c) 2019 - 2021 Andreas Merkle Merkle <web@blue-andi.de>
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
 * @brief  Bitmap Widget
 * @author Andreas Merkle <web@blue-andi.de>
 *
 * @addtogroup gfx
 *
 * @{
 */

#ifndef __BITMAPWIDGET_H__
#define __BITMAPWIDGET_H__

/******************************************************************************
 * Compile Switches
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/
#include <stdint.h>
#include <Widget.hpp>

#ifndef NATIVE
#include <FS.h>
#endif  /* NATIVE */

/******************************************************************************
 * Macros
 *****************************************************************************/

/******************************************************************************
 * Types and Classes
 *****************************************************************************/

/**
 * Bitmap widget, showing a simple bitmap.
 */
class BitmapWidget : public Widget
{
public:

    /**
     * Constructs a bitmap widget, which is empty.
     */
    BitmapWidget() :
        Widget(WIDGET_TYPE),
        m_buffer(nullptr),
        m_bufferSize(0U),
        m_width(0U),
        m_height(0U)
    {
    }

    /**
     * Constructs a bitmap widget by copying another one.
     *
     * @param[in] widget Bitmap widge, which to copy
     */
    BitmapWidget(const BitmapWidget& widget) :
        Widget(WIDGET_TYPE),
        m_buffer(nullptr),
        m_bufferSize(widget.m_bufferSize),
        m_width(widget.m_width),
        m_height(widget.m_height)
    {
        if (nullptr != widget.m_buffer)
        {
            m_buffer = new Color[m_bufferSize];

            if (nullptr == m_buffer)
            {
                m_bufferSize = 0U;
            }
            else
            {
                size_t index = 0U;

                for(index = 0U; index < m_bufferSize; ++index)
                {
                    m_buffer[index] = widget.m_buffer[index];
                }
            }
        }
    }

    /**
     * Destroys the bitmap widget.
     */
    ~BitmapWidget()
    {
        if (nullptr != m_buffer)
        {
            delete[] m_buffer;
            m_buffer = nullptr;
            m_bufferSize = 0U;
        }
    }

    /**
     * Assigns a existing bitmap widget.
     *
     * @param[in] widget Bitmap widge, which to assign
     */
    BitmapWidget& operator=(const BitmapWidget& widget);

    /**
     * Update/Draw the bitmap widget on the canvas.
     *
     * @param[in] gfx Graphics interface
     */
    void update(IGfx& gfx) override
    {
        if (nullptr != m_buffer)
        {
            gfx.drawRGBBitmap(m_posX, m_posY, m_buffer, m_width, m_height);
        }

        return;
    }

    /**
     * Set a new bitmap.
     *
     * @param[in] bitmap    Ext. bitmap buffer
     * @param[in] width     Bitmap width in pixel
     * @param[in] height    Bitmap height in pixel
     */
    void set(const Color* bitmap, uint16_t width, uint16_t height);

    /**
     * Get the bitmap.
     *
     * @param[out] width    Bitmap width in pixel
     * @param[out] height   Bitmap height in pixel
     *
     * @return Bitmap buffer
     */
    const Color* get(uint16_t& width, uint16_t& height) const
    {
        width   = m_width;
        height  = m_height;

        return m_buffer;
    }

    #ifndef NATIVE

    /**
     * Load bitmap image from filesystem.
     *
     * @param[in] fs        Filesystem
     * @param[in] filename  Filename with full path
     *
     * @return If successful loaded it will return true otherwise false.
     */
    bool load(FS& fs, const String& filename);

    #endif  /* NATIVE */

    /** Widget type string */
    static const char* WIDGET_TYPE;

private:

    Color*      m_buffer;       /**< Raw bitmap buffer */
    size_t      m_bufferSize;   /**< Raw bitmap buffer size in number of elements */
    uint16_t    m_width;        /**< Bitmap width in pixel */
    uint16_t    m_height;       /**< Bitmap height in pixel */

};

/******************************************************************************
 * Functions
 *****************************************************************************/

#endif  /* __BITMAPWIDGET_H__ */

/** @} */