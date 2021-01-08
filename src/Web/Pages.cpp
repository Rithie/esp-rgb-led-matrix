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
 * @brief  Web pages
 * @author Andreas Merkle <web@blue-andi.de>
 */

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "Pages.h"
#include "HttpStatus.h"
#include "WebConfig.h"
#include "Settings.h"
#include "Version.h"
#include "UpdateMgr.h"
#include "LedMatrix.h"
#include "DisplayMgr.h"
#include "RestApi.h"
#include "PluginMgr.h"
#include "TaskDecoupler.hpp"
#include "WebReq.hpp"

#include <WiFi.h>
#include <Esp.h>
#include <SPIFFS.h>
#include <Update.h>
#include <Logging.h>
#include <Util.h>
#include <ArduinoJson.h>
#include <lwip/init.h>

/******************************************************************************
 * Compiler Switches
 *****************************************************************************/

/******************************************************************************
 * Macros
 *****************************************************************************/

/******************************************************************************
 * Types and classes
 *****************************************************************************/

/**
 * This type defines a template keyword to function.
 */
struct TmplKeyWordFunc
{
    const char* keyword;        /**< Keyword */
    String      (*func)(void);  /**< Function to call */
};

/******************************************************************************
 * Prototypes
 *****************************************************************************/

static void safeReqHandler(AsyncWebServerRequest* request, ArRequestHandlerFunction requestHandler);
static void safeUploadHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final, ArUploadHandlerFunction uploadHandler);
static bool isValidHostname(const String& hostname);
static String tmplPageProcessor(const String& var);

static void handleNotFound(AsyncWebServerRequest* request);
static bool storeSetting(KeyValue* parameter, const String& value, DynamicJsonDocument& jsonDoc);
static void settingsPage(AsyncWebServerRequest* request);
static void uploadPage(AsyncWebServerRequest* request);
static void uploadHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);

namespace tmpl
{
    static String getEspChipId();
    static String getEspType();
    static String getFlashChipMode();
    static String getHostname();
    static String getIPAddress();
    static String getRSSI();
    static String getSettingsData();
    static String getSSID();
};

/******************************************************************************
 * Local Variables
 *****************************************************************************/

/** Max. number of requests, which to store in the task decoupling queue. */
static const uint32_t           REQ_QUEUE_MAX_ITEMS     = 20U;

/** Firmware binary filename, used for update. */
static const char*              FIRMWARE_FILENAME       = "firmware.bin";

/** Filesystem binary filename, used for update. */
static const char*              FILESYSTEM_FILENAME     = "spiffs.bin";

/**
 * Task decoupler, to handle all REST requests in the main loop. This shall
 * prevent the AsyncTCP task from not being able to feed the watchdog and
 * to have any kind of flash access in the main loop (less artifacts on the
 * display).
 */
static TaskDecoupler<WebReq*>   gTaskDecoupler;

/** Flag used to signal any kind of file upload error. */
static bool                     gIsUploadError          = false;

/**
 * List of all used template keywords and the function how to retrieve the information.
 * The list is alphabetic sorted in ascending order.
 */
static TmplKeyWordFunc          gTmplKeyWordToFunc[]    =
{
    "ARDUINO_IDF_BRANCH",   []() -> String { return CONFIG_ARDUINO_IDF_BRANCH; },
    "ESP_CHIP_ID",          tmpl::getEspChipId,
    "ESP_CHIP_REV",         []() -> String { return String(ESP.getChipRevision()); },
    "ESP_CPU_FREQ",         []() -> String { return String(ESP.getCpuFreqMHz()); },
    "ESP_SDK_VERSION",      []() -> String { return ESP.getSdkVersion(); },
    "ESP_TYPE",             tmpl::getEspType,
    "FILESYSTEM_FILENAME",  []() -> String { return FILESYSTEM_FILENAME; },
    "FIRMWARE_FILENAME",    []() -> String { return FIRMWARE_FILENAME; },
    "FLASH_CHIP_MODE",      tmpl::getFlashChipMode,
    "FLASH_CHIP_SIZE",      []() -> String { return String(ESP.getFlashChipSize() / (1024U * 1024U)); },
    "FLASH_CHIP_SPEED",     []() -> String { return String(ESP.getFlashChipSpeed() / (1000U * 1000U)); },
    "FS_SIZE",              []() -> String { return String(SPIFFS.totalBytes()); },
    "FS_SIZE_USED",         []() -> String { return String(SPIFFS.usedBytes()); },
    "HEAP_SIZE",            []() -> String { return String(ESP.getHeapSize()); },
    "HEAP_SIZE_AVAILABLE",  []() -> String { return String(ESP.getFreeHeap()); },
    "HOSTNAME",             tmpl::getHostname,
    "IPV4",                 tmpl::getIPAddress,
    "LWIP_VERSION",         []() -> String { return LWIP_VERSION_STRING; },
    "MAC_ADDR",             []() -> String { return WiFi.macAddress(); },
    "RSSI",                 tmpl::getRSSI,
    "SETTINGS_DATA",        tmpl::getSettingsData,
    "SSID",                 tmpl::getSSID,
    "SW_BRANCH",            []() -> String { return Version::SOFTWARE_BRANCH; },
    "SW_REVISION",          []() -> String { return Version::SOFTWARE_REV; },
    "SW_VERSION",           []() -> String { return Version::SOFTWARE_VER; },
    "WS_ENDPOINT",          []() -> String { return WebConfig::WEBSOCKET_PATH; },
    "WS_PORT",              []() -> String { return String(WebConfig::WEBSOCKET_PORT); },
    "WS_PROTOCOL",          []() -> String { return WebConfig::WEBSOCKET_PROTOCOL; }
};

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

void Pages::init(AsyncWebServer& srv)
{
    gTaskDecoupler.init(REQ_QUEUE_MAX_ITEMS, sizeof(WebReq*));

    /* Here are only request handlers, which can not be served static and need
     * further algorithmic.
     * 
     * Every static served file will be handled via handleNotFound().
     */

    (void)srv.on("/settings.html",
        HTTP_GET | HTTP_POST,
        [](AsyncWebServerRequest* request) { safeReqHandler(request, settingsPage); });

    (void)srv.on("/upload.html",
        HTTP_POST,
        [](AsyncWebServerRequest* request) { safeReqHandler(request, uploadPage); },
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) { safeUploadHandler(request, filename, index, data, len, final, uploadHandler); });

    (void)srv.on("/", [](AsyncWebServerRequest* request) {
        if (nullptr != request)
        {
            request->redirect("/index.html");
        }
    });

    return;
}

void Pages::process()
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

void Pages::error(AsyncWebServerRequest* request)
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

/******************************************************************************
 * Local Functions
 *****************************************************************************/

/**
 * Queues a authenticated web request in. If there is no space available, the request will be
 * aborted.
 * 
 * @param[in] request           The web request.
 * @param[in] requestHandler    The deferred request handler.
 */
static void safeReqHandler(AsyncWebServerRequest* request, ArRequestHandlerFunction requestHandler)
{
    WebPageReq* item = nullptr;

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
    
    if (nullptr == item)
    {
        request->send(HttpStatus::STATUS_CODE_INSUFFICIENT_STORAGE);
    }
    else if (false == gTaskDecoupler.addItem(item))
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
 * Queues a authenticated web upload request in. If there is no space available, the request will be
 * aborted.
 * 
 * @param[in] request           The web request.
 * @param[in] filename          Filename of the uploaded file.
 * @param[in] index             Index number of the received packet.
 * @param[in] data              Packet data
 * @param[in] len               Packet length in byte
 * @param[in] final             Final bit is set for the last packet.
 * @param[in] uploadHandler     The deferred request handler.
 */
static void safeUploadHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final, ArUploadHandlerFunction uploadHandler)
{
    WebUploadReq* item = nullptr;

    if ((nullptr == request) ||
        (nullptr == uploadHandler))
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

    item = new WebUploadReq(request, uploadHandler, filename, index, data, len, final);
    
    if (nullptr == item)
    {
        request->send(HttpStatus::STATUS_CODE_INSUFFICIENT_STORAGE);
    }
    else if (false == gTaskDecoupler.addItem(item))
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
 * Check the given hostname and returns whether it is valid or not.
 * Validation is according to RFC952.
 *
 * @param[in] hostname  Hostname which to validate
 *
 * @return Is valid (true) or not (false).
 */
static bool isValidHostname(const String& hostname)
{
    bool            isValid             = true;
    const size_t    MIN_HOSTNAME_LENGTH = Settings::getInstance().getHostname().getMinLength();
    const size_t    MAX_HOSTNAME_LENGTH = Settings::getInstance().getHostname().getMaxLength();

    if ((MIN_HOSTNAME_LENGTH > hostname.length()) ||
        (MAX_HOSTNAME_LENGTH < hostname.length()))
    {
        isValid = false;
    }
    else
    {
        uint8_t index = 0;

        while((true == isValid) && (index < hostname.length()))
        {
            if (('0' <= hostname[index]) &&
                ('9' >= hostname[index]))
            {
                /* No digit at the begin */
                if (0 == index)
                {
                    isValid = false;
                }
            }
            else if (('A' <= hostname[index]) &&
                     ('Z' >= hostname[index]))
            {
                /* Ok */
                ;
            }
            else if (('a' <= hostname[index]) &&
                     ('z' >= hostname[index]))
            {
                /* Ok */
                ;
            }
            else if ('-' == hostname[index])
            {
                /* No - at the begin */
                if (0 == index)
                {
                    isValid = false;
                }
            }
            else
            {
                isValid = false;
            }

            ++index;
        }
    }

    return isValid;
}

/**
 * Processor for page template, containing the common part, which is available
 * in every page. It is responsible for the data binding.
 *
 * @param[in] var   Name of variable in the template
 */
static String tmplPageProcessor(const String& var)
{
    String  result  = var;
    uint8_t index   = 0U;
    bool    isFound = false;

    while ((index < UTIL_ARRAY_NUM(gTmplKeyWordToFunc)) && (false == isFound))
    {
        if (var == gTmplKeyWordToFunc[index].keyword)
        {
            result  = gTmplKeyWordToFunc[index].func();
            isFound = true;
        }

        ++index;
    }

    return result;
}

/**
 * Handle all static served files and the of course will respond a error
 * page if request can not be handled.
 *
 * @param[in] request   HTTP request
 */
static void handleNotFound(AsyncWebServerRequest* request)
{
    FS& fs = SPIFFS;

    if (nullptr == request)
    {
        return;
    }

    /* Serve static html files. */
    if ((0 != request->url().endsWith(".html")) ||
        (0 != request->url().endsWith("*.htm")))
    {
        /* If a requested html file down't exist, show the error page. */
        if (false == fs.exists(request->url()))
        {
            LOG_INFO("Invalid web request: %s", request->url().c_str());
            request->send(fs, "/error.html", "text/html", false, tmplPageProcessor);
        }
        else
        {
            request->send(fs, request->url(), "text/html", false, tmplPageProcessor);
        }
    }
    /* Some browsers requests for the favorite icon on different places. */
    else if (0 != request->url().endsWith("/favicon.png"))
    {
        request->send(fs, "/favicon.png");
    }
    /* Handle all other static files with cache control. */
    else if ((0 != request->url().startsWith("/images")) ||
             (0 != request->url().startsWith("/js")) ||
             (0 != request->url().startsWith("/style")))
    {
        AsyncWebServerResponse* response = request->beginResponse(fs, request->url());

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

/**
 * Store setting in persistent memory, considering the setting type.
 *
 * @param[in]   parameter   Key value pair
 * @param[in]   value       Value to write
 * @param[out]  jsonDoc     Response in JSON format, only applicable in error case.
 *
 * @return If successful stored, it will return true otherwise false.
 */
static bool storeSetting(KeyValue* parameter, const String& value, DynamicJsonDocument& jsonDoc)
{
    bool status = true;

    if (nullptr == parameter)
    {
        status = false;
        jsonDoc["status"]   = 1;
        jsonDoc["error"]    = "Internal error.";
    }
    else
    {
        switch(parameter->getValueType())
        {
        case KeyValue::TYPE_STRING:
            {
                KeyValueString* kvStr = static_cast<KeyValueString*>(parameter);

                /* If it is the hostname, verify it explicit. */
                if (0 == strcmp(Settings::getInstance().getHostname().getKey(), kvStr->getKey()))
                {
                    if (false == isValidHostname(value))
                    {
                        status = false;
                        jsonDoc["status"]   = 1;
                        jsonDoc["error"]    = "Invalid hostname.";
                    }
                }

                if (true == status)
                {
                    /* Check for min. and max. length */
                    if (kvStr->getMinLength() > value.length())
                    {
                        String  errorStr = "String length lower than ";
                        errorStr += kvStr->getMinLength();
                        errorStr += ".";

                        status = false;
                        jsonDoc["status"]   = 1;
                        jsonDoc["error"]    = errorStr;
                    }
                    else if (kvStr->getMaxLength() < value.length())
                    {
                        String  errorStr = "String length greater than ";
                        errorStr += kvStr->getMaxLength();
                        errorStr += ".";

                        status = false;
                        jsonDoc["status"]   = 1;
                        jsonDoc["error"]    = errorStr;
                    }
                    else
                    {
                        kvStr->setValue(value);
                    }
                }
            }
            break;

        case KeyValue::TYPE_BOOL:
            {
                KeyValueBool* kvBool = static_cast<KeyValueBool*>(parameter);

                if (0 == strcmp(value.c_str(), "false"))
                {
                    kvBool->setValue(false);
                }
                else if (0 == strcmp(value.c_str(), "true"))
                {
                    kvBool->setValue(true);
                }
                else
                {
                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = "Invalid value.";
                }
            }
            break;

        case KeyValue::TYPE_UINT8:
            {
                KeyValueUInt8*  kvUInt8     = static_cast<KeyValueUInt8*>(parameter);
                uint8_t         uint8Value  = 0;
                bool            convStatus  = Util::strToUInt8(value, uint8Value);

                /* Conversion failed? */
                if (false == convStatus)
                {
                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = "Invalid value.";
                }
                /* Check for min. and max. length */
                else if (kvUInt8->getMin() > uint8Value)
                {
                    String  errorStr = "Value lower than ";
                    errorStr += kvUInt8->getMin();
                    errorStr += ".";

                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = errorStr;
                }
                else if (kvUInt8->getMax() < uint8Value)
                {
                    String  errorStr = "Value greater than ";
                    errorStr += kvUInt8->getMax();
                    errorStr += ".";

                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = errorStr;
                }
                else
                {
                    kvUInt8->setValue(uint8Value);
                }
            }
            break;

        case KeyValue::TYPE_INT32:
            {
                KeyValueInt32*  kvInt32     = static_cast<KeyValueInt32*>(parameter);
                int32_t         int32Value  = 0;
                bool            convStatus  = Util::strToInt32(value, int32Value);

                /* Conversion failed? */
                if (false == convStatus)
                {
                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = "Invalid value.";
                }
                /* Check for min. and max. length */
                else if (kvInt32->getMin() > int32Value)
                {
                    String  errorStr = "Value lower than ";
                    errorStr += kvInt32->getMin();
                    errorStr += ".";

                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = errorStr;
                }
                else if (kvInt32->getMax() < int32Value)
                {
                    String  errorStr = "Value greater than ";
                    errorStr += kvInt32->getMax();
                    errorStr += ".";

                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = errorStr;
                }
                else
                {
                    kvInt32->setValue(int32Value);
                }
            }
            break;

        case KeyValue::TYPE_JSON:
            {
                KeyValueJson* kvJson = static_cast<KeyValueJson*>(parameter);

                /* Check for min. and max. length */
                if (kvJson->getMinLength() > value.length())
                {
                    String  errorStr = "JSON length lower than ";
                    errorStr += kvJson->getMinLength();
                    errorStr += ".";

                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = errorStr;
                }
                else if (kvJson->getMaxLength() < value.length())
                {
                    String  errorStr = "JSON length greater than ";
                    errorStr += kvJson->getMaxLength();
                    errorStr += ".";

                    status = false;
                    jsonDoc["status"]   = 1;
                    jsonDoc["error"]    = errorStr;
                }
                else
                {
                    kvJson->setValue(value);
                }
            }
            break;

        case KeyValue::TYPE_UNKNOWN:
            /* fallthrough */
        default:
            status = false;
            jsonDoc["status"]   = 1;
            jsonDoc["error"]    = "Unknown parameter.";
            break;
        }
    }

    return status;
}

/**
 * Settings page to show and store settings.
 *
 * @param[in] request   HTTP request
 */
static void settingsPage(AsyncWebServerRequest* request)
{
    if (nullptr == request)
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

    /* Store settings? */
    if ((HTTP_POST == request->method()) &&
        (0 < request->args()))
    {
        KeyValue**          list            = Settings::getInstance().getList();
        const size_t        JSON_DOC_SIZE   = 512U;
        DynamicJsonDocument jsonDoc(JSON_DOC_SIZE);
        String              rsp;

        if (false == Settings::getInstance().open(false))
        {
            JsonObject errorObj = jsonDoc.createNestedObject("error");

            LOG_WARNING("Couldn't open settings.");

            jsonDoc["status"]   = 1;
            errorObj["msg"]     = "Internal error.";
        }
        else
        {
            bool    isError = false;
            uint8_t index   = 0U;

            while((index < Settings::KEY_VALUE_PAIR_NUM) && (false == isError))
            {
                KeyValue* parameter = list[index];

                if (true == request->hasArg(parameter->getKey()))
                {
                    const String& value = request->arg(parameter->getKey());

                    if (false == storeSetting(parameter, value, jsonDoc))
                    {
                        isError = true;
                    }
                }

                ++index;
            }

            Settings::getInstance().close();

            if (true == isError)
            {
                JsonObject errorObj = jsonDoc.createNestedObject("error");

                LOG_WARNING("Internal error.");

                jsonDoc["status"]   = 1;
                errorObj["msg"]     = "Internal error.";
            }
            else
            {
                JsonObject dataObj = jsonDoc.createNestedObject("data");

                UTIL_NOT_USED(dataObj);

                jsonDoc["status"] = 0;
            }
        }

        if (true == jsonDoc.overflowed())
        {
            LOG_ERROR("JSON document has less memory available.");
        }
        else
        {
            LOG_INFO("JSON document size: %u", jsonDoc.memoryUsage());
        }

        (void)serializeJson(jsonDoc, rsp);
        request->send(HttpStatus::STATUS_CODE_OK, "application/json", rsp);
    }
    else if (HTTP_GET == request->method())
    {
        request->send(SPIFFS, "/settings.html", "text/html", false, tmplPageProcessor);
    }
    else
    {
        request->send(HttpStatus::STATUS_CODE_BAD_REQUEST, "plain/text", "Error");
    }

    return;
}

/**
 * Page for upload result.
 *
 * @param[in] request   HTTP request
 */
static void uploadPage(AsyncWebServerRequest* request)
{
    if (nullptr == request)
    {
        return;
    }

    if (true == gIsUploadError)
    {
        request->send(HttpStatus::STATUS_CODE_BAD_REQUEST, "text/plain", "Error");
    }
    else
    {
        /* Trigger restart after the client has disconnected. */
        request->onDisconnect(
            []()
            {
                UpdateMgr::getInstance().reqRestart();
            }
        );

        request->send(HttpStatus::STATUS_CODE_OK, "text/plain", "Ok");
    }

    return;
}

/**
 * File upload handler.
 *
 * @param[in] request   HTTP request.
 * @param[in] filename  Name of the uploaded file.
 * @param[in] index     Current file offset.
 * @param[in] data      Next data part of file, starting at offset.
 * @param[in] len       Data part size in byte.
 * @param[in] final     Is final packet or not.
 */
static void uploadHandler(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
    /* Begin of upload? */
    if (0 == index)
    {
        AsyncWebHeader* header      = request->getHeader("X-File-Size");
        uint32_t        fileSize    = UPDATE_SIZE_UNKNOWN;

        /* Upload firmware or filesystem? */
        int cmd = (filename == FILESYSTEM_FILENAME) ? U_SPIFFS : U_FLASH;

        /* File size available? */
        if (nullptr != header)
        {
            /* If conversion fails, it will contain UPDATE_SIZE_UNKNOWN. */
            (void)Util::strToUInt32(header->value(), fileSize);
        }

        if (UPDATE_SIZE_UNKNOWN == fileSize)
        {
            LOG_INFO("Upload of %s (unknown size) starts.", filename.c_str());
        }
        else
        {
            LOG_INFO("Upload of %s (%u byte) starts.", filename.c_str(), fileSize);
        }

        gIsUploadError = false;

        /* Update filesystem? */
        if (U_SPIFFS == cmd)
        {
            /* Close filesystem before continue. */
            SPIFFS.end();
        }

        /* Start update */
        if (false == Update.begin(fileSize, cmd))
        {
            LOG_ERROR("Upload failed: %s", Update.errorString());
            gIsUploadError = true;

            /* Mount filesystem again, it may be unmounted in case of filesystem update.*/
            if (false == SPIFFS.begin())
            {
                LOG_FATAL("Couldn't mount filesystem.");
            }

            /* Inform client about abort.*/
            request->send(HttpStatus::STATUS_CODE_PAYLOAD_TOO_LARGE, "text/plain", "Upload aborted.");
        }
        /* Update is now running. */
        else
        {
            /* Use UpdateMgr to show the user the update status.
             * Note, the display manager will be completey stopped during this,
             * to avoid artifacts on the display, because of long writes to flash.
             */
            UpdateMgr::getInstance().beginProgress();
        }
    }

    if (true == Update.isRunning())
    {
        if (false == gIsUploadError)
        {
            if(len != Update.write(data, len))
            {
                LOG_ERROR("Upload failed: %s", Update.errorString());
                gIsUploadError = true;
            }
            else
            {
                uint32_t progress = (Update.progress() * 100) / Update.size();

                UpdateMgr::getInstance().updateProgress(progress);
            }

            /* Upload finished? */
            if (true == final)
            {
                /* Finish update now. */
                if (false == Update.end(true))
                {
                    LOG_ERROR("Upload failed: %s", Update.errorString());
                    gIsUploadError = true;
                }
                /* Update was successful! */
                else
                {
                    LOG_INFO("Upload of %s finished.", filename.c_str());

                    /* Filesystem is not mounted here, because we will restart in the next seconds. */

                    /* Ensure that the user see 100% update status on the display. */
                    UpdateMgr::getInstance().updateProgress(100U);
                    UpdateMgr::getInstance().endProgress();

                    /* Restart is requested in upload page handler, see uploadPage(). */
                }
            }
        }
        else
        {
            /* Mount filesystem again, it may be unmounted in case of filesystem update. */
            if (false == SPIFFS.begin())
            {
                LOG_FATAL("Couldn't mount filesystem.");
            }

            /* Abort update */
            Update.abort();
            UpdateMgr::getInstance().endProgress();

            /* Inform client about abort.*/
            request->send(HttpStatus::STATUS_CODE_PAYLOAD_TOO_LARGE, "text/plain", "Upload aborted.");
        }
    }

    return;
}

/**
 * Functions which are called for the corresponding template keyword.
 */
namespace tmpl
{
    /**
     * Get ESP chip id.
     * 
     * @return ESP chip id
     */
    static String getEspChipId()
    {
        String      result;
        uint64_t    chipId      = ESP.getEfuseMac();
        uint32_t    highPart    = (chipId >> 32U) & 0x0000ffffU;
        uint32_t    lowPart     = (chipId >>  0U) & 0xffffffffU;
        char        chipIdStr[13];

        snprintf(chipIdStr, UTIL_ARRAY_NUM(chipIdStr), "%04X%08X", highPart, lowPart);

        result = chipIdStr;

        return result;
    }

    /**
     * Get ESP type.
     * 
     * @return ESP type
     */
    static String getEspType()
    {
        String result;

#if defined(ESP32)
        result = "ESP32";
#elif defined(ESP32S2)
        result = "ESP32S2";
#else
        result ="UNKNOWN";
#endif
        return result;
    }

    /**
     * Get flash chip mode.
     * 
     * @return Flash chip mode.
     */
    static String getFlashChipMode()
    {
        String result;

        switch(ESP.getFlashChipMode())
        {
        case FM_QIO:
            result = "QUIO";
            break;

        case FM_QOUT:
            result = "QOUT";
            break;

        case FM_DIO:
            result = "DIO";
            break;

        case FM_DOUT:
            result = "DOUT";
            break;

        case FM_FAST_READ:
            result = "FAST_READ";
            break;

        case FM_SLOW_READ:
            result = "SLOW_READ";
            break;

        case FM_UNKNOWN:
            /* fallthrough */

        default:
            result = "UNKNOWN";
            break;
        }
        
        return result;
    }

    /**
     * Get hostname, depended on current WiFi mode.
     * 
     * @return Hostname
     */
    static String getHostname()
    {
        String      result;
        const char* hostname = nullptr;

        if (WIFI_MODE_AP == WiFi.getMode())
        {
            hostname = WiFi.softAPgetHostname();
        }
        else
        {
            hostname = WiFi.getHostname();
        }

        if (nullptr != hostname)
        {
            result = hostname;
        }

        return result;
    }

    /**
     * Get IP address, depended on WiFi mode.
     * 
     * @return IPv4
     */
    static String getIPAddress()
    {
        String result;

        if (WIFI_MODE_AP == WiFi.getMode())
        {
            result = WiFi.softAPIP().toString();
        }
        else
        {
            result = WiFi.localIP().toString();
        }

        return result;
    }

    /**
     * Get wifi RSSI.
     * 
     * @return WiFi station SSID
     */
    static String getRSSI()
    {
        String result;

        /* Only in station mode it makes sense to retrieve the RSSI.
         * Otherwise keep it -100 dbm.
         */
        if (WIFI_MODE_STA == WiFi.getMode())
        {
            result = WiFi.RSSI();
        }
        else
        {
            result = "-100";
        }

        return result;
    }

    /**
     * Get all settings.
     * 
     * @return Settings data
     */
    static String getSettingsData()
    {
        String result;

        if (true == Settings::getInstance().open(true))
        {
            KeyValue**          list            = Settings::getInstance().getList();
            uint8_t             index           = 0U;
            const size_t        JSON_DOC_SIZE   = 4096U;
            DynamicJsonDocument jsonDoc(JSON_DOC_SIZE);

            result.clear();
            for(index = 0U; index < Settings::KEY_VALUE_PAIR_NUM; ++index)
            {
                KeyValue*   parameter   = list[index];
                JsonObject  jsonSetting = jsonDoc.createNestedObject();
                JsonObject  jsonInput   = jsonSetting.createNestedObject("input");

                jsonSetting["title"]    = parameter->getName();
                jsonInput["name"]       = parameter->getKey();

                switch(parameter->getValueType())
                {
                case KeyValue::TYPE_STRING:
                    {
                        KeyValueString* kvStr = static_cast<KeyValueString*>(parameter);
                        jsonInput["type"]       = "text";
                        jsonInput["value"]      = kvStr->getValue();
                        jsonInput["size"]       = kvStr->getMaxLength();
                        jsonInput["minlength"]  = kvStr->getMinLength();
                        jsonInput["maxlength"]  = kvStr->getMaxLength();
                    }
                    break;

                case KeyValue::TYPE_BOOL:
                    {
                        KeyValueBool* kvBool = static_cast<KeyValueBool*>(parameter);
                        jsonInput["type"]       = "checkbox";
                        jsonInput["value"]      = kvBool->getKey();

                        if (true == kvBool->getValue())
                        {
                            jsonInput["checked"] = "checked";
                        }
                    }
                    break;

                case KeyValue::TYPE_UINT8:
                    {
                        KeyValueUInt8* kvUInt8 = static_cast<KeyValueUInt8*>(parameter);
                        jsonInput["type"]   = "number";
                        jsonInput["value"]  = kvUInt8->getValue();
                        jsonInput["min"]    = kvUInt8->getMin();
                        jsonInput["max"]    = kvUInt8->getMax();
                    }
                    break;

                case KeyValue::TYPE_INT32:
                {
                    KeyValueInt32* kvInt32 = static_cast<KeyValueInt32*>(parameter);
                    jsonInput["type"]   = "number";
                    jsonInput["value"]  = kvInt32->getValue();
                    jsonInput["min"]    = kvInt32->getMin();
                    jsonInput["max"]    = kvInt32->getMax();
                }
                break;

                case KeyValue::TYPE_JSON:
                    {
                        KeyValueJson* kvJson = static_cast<KeyValueJson*>(parameter);
                        jsonInput["type"]       = "text";
                        jsonInput["value"]      = kvJson->getValue();
                        jsonInput["size"]       = kvJson->getMaxLength();
                        jsonInput["minlength"]  = kvJson->getMinLength();
                        jsonInput["maxlength"]  = kvJson->getMaxLength();
                    }
                    break;

                default:
                    break;
                }
            }

            Settings::getInstance().close();

            if (true == jsonDoc.overflowed())
            {
                LOG_ERROR("JSON document has less memory available.");
            }
            else
            {
                LOG_INFO("JSON document size: %u", jsonDoc.memoryUsage());
            }

            (void)serializeJson(jsonDoc, result);
        }

        return result;
    }

    /**
     * Get wifi station SSID.
     * 
     * @return WiFi station SSID
     */
    static String getSSID()
    {
        String result;

        if (true == Settings::getInstance().open(true))
        {
            result = Settings::getInstance().getWifiSSID().getValue();
            Settings::getInstance().close();
        }

        return result;
    }
};
