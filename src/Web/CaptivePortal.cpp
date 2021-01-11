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
 * @brief  Captive portal web page
 * @author Andreas Merkle <web@blue-andi.de>
 */

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "CaptivePortal.h"
#include "HttpStatus.h"
#include "WebConfig.h"
#include "Settings.h"
#include "CaptivePortalHandler.h"
#include "TaskDecoupler.hpp"
#include "WebReq.hpp"
#include "FileSystem.h"

#include <Logging.h>

/******************************************************************************
 * Compiler Switches
 *****************************************************************************/

/******************************************************************************
 * Macros
 *****************************************************************************/

/******************************************************************************
 * Types and classes
 *****************************************************************************/

/******************************************************************************
 * Prototypes
 *****************************************************************************/

static void reqRestart();
static void safeReqHandler(AsyncWebServerRequest* request, ArRequestHandlerFunction requestHandler);
static void handleNotFound(AsyncWebServerRequest* request);

/******************************************************************************
 * Local Variables
 *****************************************************************************/

/** Max. number of requests, which to store in the task decoupling queue. */
static const uint32_t           REQ_QUEUE_MAX_ITEMS     = 5U;

/**
 * Task decoupler, to handle all page requests in the main loop. This shall
 * prevent the AsyncTCP task from not being able to feed the watchdog and
 * to have any kind of flash access in the main loop (less artifacts on the
 * display).
 */
static TaskDecoupler<WebReq*>   gTaskDecoupler;

/**
 * Captive portal requst handler.
 */
static CaptivePortalHandler     gCaptivePortalReqHandler(gTaskDecoupler, reqRestart);

/**
 * Flag to request a restart.
 */
static bool                     gIsRestartRequested     = false;

/******************************************************************************
 * Public Methods
 *****************************************************************************/

/******************************************************************************
 * Protected Methods
 *****************************************************************************/

/******************************************************************************
 * Private Methods
 *****************************************************************************/

/******************************************************************************
 * External Functions
 *****************************************************************************/

void CaptivePortal::init(AsyncWebServer& srv)
{
    gTaskDecoupler.init(REQ_QUEUE_MAX_ITEMS, sizeof(WebReq*));

    /* Add the captive portal request handler at last, because it will handle everything. */
    (void)srv.addHandler(&gCaptivePortalReqHandler).setFilter(ON_AP_FILTER);

    return;
}

void CaptivePortal::process()
{
    WebReq* msg = nullptr;

    if (true == gTaskDecoupler.getItem(msg))
    {
        if (nullptr != msg)
        {
            msg->call();
            delete msg;
        }
    }

    return;
}

void CaptivePortal::error(AsyncWebServerRequest* request)
{
    if (nullptr == request)
    {
        return;
    }

    /* Handles all static served files and of course the case if a request
     * can not be handled.
     */
    safeReqHandler(request, handleNotFound);

    return;
}

bool CaptivePortal::isRestartRequested()
{
    return gIsRestartRequested;
}

/******************************************************************************
 * Local Functions
 *****************************************************************************/

/**
 * Request restart.
 */
static void reqRestart()
{
    gIsRestartRequested = true;
}

/**
 * Queues a authenticated web request in. If there is no space available, the request will be
 * aborted.
 * 
 * @param[in] request           The web request.
 * @param[in] requestHandler    The deferred request handler.
 */
static void safeReqHandler(AsyncWebServerRequest* request, ArRequestHandlerFunction requestHandler)
{
    WebPageReq* item        = nullptr;
    WebReq*     itemBase    = nullptr;

    if ((nullptr == request) ||
        (nullptr == requestHandler))
    {
        return;
    }

    /* Force authentication! */
    if (false == request->authenticate(WebConfig::WEB_LOGIN_USER, WebConfig::WEB_LOGIN_PASSWORD))
    {
        /* Request DIGEST authentication */
        request->requestAuthentication();
        return;
    }

    item = new WebPageReq(request, requestHandler);
    itemBase = item;

    if (nullptr == item)
    {
        request->send(HttpStatus::STATUS_CODE_INSUFFICIENT_STORAGE);
    }
    else if (false == gTaskDecoupler.addItem(itemBase))
    {
        request->send(HttpStatus::STATUS_CODE_INSUFFICIENT_STORAGE);
    }
    else
    {
        /* Successful added to queue. */
        ;
    }
}

/**
 * Handle all static served files and the of course will respond a error
 * page if request can not be handled.
 *
 * @param[in] request   HTTP request
 */
static void handleNotFound(AsyncWebServerRequest* request)
{
    if (nullptr == request)
    {
        return;
    }

    /* Some browsers requests for the favorite icon on different places. */
    if (0 != request->url().endsWith("/favicon.png"))
    {
        request->send(FILESYSTEM, "/favicon.png");
    }
    /* Handle all other static files with cache control. */
    else if ((0 != request->url().startsWith("/images")) ||
             (0 != request->url().startsWith("/js")) ||
             (0 != request->url().startsWith("/style")))
    {
        AsyncWebServerResponse* response = request->beginResponse(FILESYSTEM, request->url());

        response->addHeader("Cache-Control", "max-age=3600");
        request->send(response);
    }
    /* Handle any other request. */
    else
    {
        LOG_INFO("Invalid web request: %s", request->url().c_str());
        request->send(HttpStatus::STATUS_CODE_NOT_FOUND);
    }

    return;
}
