/* MIT License
 *
 * Copyright (c) 2019 - 2021 Andreas Merkle <web@blue-andi.de>
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
 * @brief  Web request, used for deferred processing.
 * @author Andreas Merkle <web@blue-andi.de>
 *
 * @addtogroup web
 *
 * @{
 */

#ifndef __WEB_REQ_HPP__
#define __WEB_REQ_HPP__

/******************************************************************************
 * Compile Switches
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/
#include <ESPAsyncWebServer.h>
#include "WsCmd.h"

/******************************************************************************
 * Macros
 *****************************************************************************/

/******************************************************************************
 * Types and Classes
 *****************************************************************************/

/**
 * Web request base class.
 * It contains only the common members. See the derrived classes for the
 * different type of requests.
 */
class WebReq
{
public:

    /**
     * Constructs a web request object.
     * 
     * @param[in] req   Web request
     */
    WebReq(AsyncWebServerRequest* req) :
        m_request(req)
    {
    }

    /**
     * Constructs a web request object by using another web request.
     * 
     * @param[in] webReq    Web request
     */
    WebReq(const WebReq& webReq) :
        m_request(webReq.m_request)
    {
    }

    /**
     * Constructs a web request object by assigning a web request.
     * 
     * @param[in] webReq    Web request
     */
    WebReq& operator=(const WebReq& webReq)
    {
        if (this != &webReq)
        {
            m_request = webReq.m_request;
        }

        return *this;
    }

    /**
     * Destroys the web request.
     */
    virtual ~WebReq()
    {
    }

    /**
     * Get the web request.
     * 
     * @return The web request.
     */
    AsyncWebServerRequest* getRequest()
    {
        return m_request;
    }

    /**
     * Handle the web request with the corresponding handler.
     */
    virtual void call() = 0;

protected:

    /**
     * Constructs a empty web request.
     */
    WebReq() :
        m_request(nullptr)
    {
    }

private:

    AsyncWebServerRequest*  m_request;  /** The web request, received from the webserver. */

};

/**
 * A web page request, which is handled deferred.
 */
class WebPageReq : public WebReq
{
public:

    /**
     * Constructs a web page request.
     * 
     * @param[in] req           The web request.
     * @param[in] pageHandler   The page request handler.
     */
    WebPageReq(AsyncWebServerRequest* req, ArRequestHandlerFunction pageHandler) :
        WebReq(req),
        m_pageHandler(pageHandler)
    {
    }

    /**
     * Constructs a web page request by using another web page request.
     * 
     * @param[in] webPageReq    The web page request.
     */
    WebPageReq(const WebPageReq& webPageReq) :
        WebReq(webPageReq),
        m_pageHandler(webPageReq.m_pageHandler)
    {
    }

    /**
     * Constructs a web page request by using assigning a web page request.
     * 
     * @param[in] webPageReq    The web page request.
     */
    WebPageReq& operator=(const WebPageReq& webPageReq)
    {
        if (this != &webPageReq)
        {
            m_pageHandler = webPageReq.m_pageHandler;
        }

        return *this;
    }

    /**
     * Destroys the web page request.
     */
    ~WebPageReq()
    {
    }

    /**
     * Handle the web request with the corresponding handler.
     */
    void call() final
    {
        if (nullptr != m_pageHandler)
        {
            m_pageHandler(getRequest());
        }
    }

protected:

    /**
     * Constructs a empty web page request.
     */
    WebPageReq() :
        WebReq(nullptr),
        m_pageHandler(nullptr)
    {
    }

private:

    ArRequestHandlerFunction    m_pageHandler;  /**< The page request handler. */

};

/**
 * A web upload request, which is handled deferred.
 */
class WebUploadReq : public WebReq
{
public:

    /**
     * Constructs a web upload request.
     * 
     * @param[in] req           The web request.
     * @param[in] uploadHandler The deferred request handler.
     * @param[in] filename      Filename of the uploaded file.
     * @param[in] index         Index number of the received packet.
     * @param[in] data          Packet data
     * @param[in] len           Packet length in byte
     * @param[in] final         Final bit is set for the last packet.
     */
    WebUploadReq(AsyncWebServerRequest* req, ArUploadHandlerFunction uploadHandler, const String& filename, size_t index, const uint8_t* data, size_t len, bool final) :
        WebReq(req),
        m_uploadHandler(uploadHandler),
        m_filename(filename),
        m_index(index),
        m_data(nullptr),
        m_len(0U),
        m_final(final)
    {
        copy(data, len);
    }

    /**
     * Constructs a web upload request by using another web upload request.
     * 
     * @param[in] webUploadReq  The web upload request.
     */
    WebUploadReq(const WebUploadReq& webUploadReq) :
        WebReq(webUploadReq),
        m_uploadHandler(webUploadReq.m_uploadHandler),
        m_filename(webUploadReq.m_filename),
        m_index(webUploadReq.m_index),
        m_data(nullptr),
        m_len(0U),
        m_final(webUploadReq.m_final)
    {
        copy(webUploadReq.m_data, webUploadReq.m_len);
    }

    /**
     * Constructs a web upload request by assigning a web upload request.
     * 
     * @param[in] webUploadReq  The web upload request.
     */
    WebUploadReq& operator=(const WebUploadReq& webUploadReq)
    {
        if (this != &webUploadReq)
        {
            if (nullptr != m_data)
            {
                delete[] m_data;
                m_data  = nullptr;
                m_len   = 0U;
            }

            m_uploadHandler = webUploadReq.m_uploadHandler;
            m_filename      = webUploadReq.m_filename;
            m_index         = webUploadReq.m_index;
            m_final         = webUploadReq.m_final;

            copy(webUploadReq.m_data, webUploadReq.m_len);
        }

        return *this;
    }

    /**
     * Destroys the web upload request.
     */
    ~WebUploadReq()
    {
        if (nullptr != m_data)
        {
            delete[] m_data;
            m_data  = nullptr;
            m_len   = 0;
        }
    }

    /**
     * Handle the web request with the corresponding handler.
     */
    void call() final
    {
        if (nullptr != m_uploadHandler)
        {
            m_uploadHandler(getRequest(), m_filename, m_index, m_data, m_len, m_final);
        }
    }

protected:

    /**
     * Constructs a empty web upload request.
     */
    WebUploadReq() :
        WebReq(nullptr),
        m_uploadHandler(nullptr),
        m_filename(),
        m_index(0),
        m_data(nullptr),
        m_len(0),
        m_final(false)
    {
    }

private:

    ArUploadHandlerFunction m_uploadHandler;    /**< The upload handler, which is called deferred. */
    String                  m_filename;         /**< Filename */
    size_t                  m_index;            /**< Index of recevied packet */
    uint8_t*                m_data;             /**< Data packet */
    size_t                  m_len;              /**< Data packet length in byte */
    bool                    m_final;            /**< Final bit marks the last received data packet */

    /**
     * Copy data.
     * 
     * @param[in] src   Data source
     * @param[in] size  Data source size in byte
     */
    void copy(const uint8_t* src, size_t size)
    {
        if (nullptr != src)
        {
            m_data = new uint8_t[size];
            
            if (nullptr != m_data)
            {
                m_len = size;

                memcpy(m_data, src, m_len);
            }
        }
    }
};

/**
 * A websocket request, which is handled deferred.
 */
class WebWsReq : public WebReq
{
public:

    /**
     * Websocket command handler.
     * 
     * @param[in] server    The websocket server.
     * @param[in] client    The websocket client.
     * @param[in] data      The message itself, non-terminated.
     * @param[in] len       The message length in byte.
     */
    typedef std::function<void(AsyncWebSocket* server, AsyncWebSocketClient* client, const uint8_t* data, size_t len)> WsHandler;

    /**
     * Constructs a websocket request.
     * 
     * @param[in] server    The websocket server.
     * @param[in] client    The websocket client.
     * @param[in] data      The message itself, non-terminated.
     * @param[in] len       The message length in byte.
     */
    WebWsReq(AsyncWebSocket* server, AsyncWebSocketClient* client, const uint8_t* data, size_t len, WsHandler wsHandler) :
        WebReq(nullptr),
        m_server(server),
        m_client(client),
        m_data(nullptr),
        m_len(0U),
        m_wsHandler(wsHandler)
    {
        copy(data, len);
    }

    /**
     * Constructs a websocket request.
     * 
     * @param[in] webWsReq  The websocket request.
     */
    WebWsReq(const WebWsReq& webWsReq) :
        WebReq(webWsReq),
        m_server(webWsReq.m_server),
        m_client(webWsReq.m_client),
        m_data(nullptr),
        m_len(0U),
        m_wsHandler(webWsReq.m_wsHandler)
    {
        copy(webWsReq.m_data, webWsReq.m_len);
    }

    /**
     * Constructs a websocket request.
     * 
     * @param[in] webWsReq  The websocket request.
     */
    WebWsReq& operator=(const WebWsReq& webWsReq)
    {
        if (this != &webWsReq)
        {
            if (nullptr != m_data)
            {
                delete[] m_data;
                m_data  = nullptr;
                m_len   = 0U;
            }

            m_server    = webWsReq.m_server;
            m_client    = webWsReq.m_client;
            m_wsHandler = webWsReq.m_wsHandler;

            copy(webWsReq.m_data, webWsReq.m_len);
        }

        return *this;
    }

    /**
     * Destroys the web page request.
     */
    ~WebWsReq()
    {
        if (nullptr != m_data)
        {
            delete[] m_data;
            m_data = nullptr;
            m_len = 0;
        }
    }

    /**
     * Handle the web request with the corresponding handler.
     */
    void call() final
    {
        if (nullptr != m_wsHandler)
        {
            m_wsHandler(m_server, m_client, m_data, m_len);
        }
    }

protected:

    /**
     * Constructs a empty web page request.
     */
    WebWsReq() :
        WebReq(nullptr),
        m_server(nullptr),
        m_client(nullptr),
        m_data(nullptr),
        m_len(0U),
        m_wsHandler(nullptr)
    {
    }

private:

    AsyncWebSocket*         m_server;       /**< Websocket server */
    AsyncWebSocketClient*   m_client;       /**< Websocket client */
    uint8_t*                m_data;         /**< Websocket message, non-terminated. */
    size_t                  m_len;          /**< Websocket message length in byte. */
    WsHandler               m_wsHandler;    /**< Websocket message handler */

    /**
     * Copy data.
     * 
     * @param[in] src   Data source
     * @param[in] size  Data source size in byte
     */
    void copy(const uint8_t* src, size_t size)
    {
        if (nullptr != src)
        {
            m_data = new uint8_t[size];
            
            if (nullptr != m_data)
            {
                m_len = size;
                
                memcpy(m_data, src, m_len);
            }
        }
    }
};

/******************************************************************************
 * Functions
 *****************************************************************************/

#endif  /* __WEB_REQ_HPP__ */

/** @} */