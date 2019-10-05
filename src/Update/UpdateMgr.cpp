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
 * @brief  Update manager
 * @author Andreas Merkle <web@blue-andi.de>
 */

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "UpdateMgr.h"

#include <SPIFFS.h>
#include <DisplayMgr.h>
#include <Logging.h>

#include "LedMatrix.h"

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

/******************************************************************************
 * Local Variables
 *****************************************************************************/

/* Set over-the-air update password */
const char*     UpdateMgr::OTA_PASSWORD             = "maytheforcebewithyou";

/* Set standard wait time for showing a system message in ms */
const uint32_t  UpdateMgr::SYS_MSG_WAIT_TIME_STD    = 2000u;

/* Instance of the update manager. */
UpdateMgr       UpdateMgr::m_instance;

/******************************************************************************
 * Public Methods
 *****************************************************************************/

void UpdateMgr::init(void)
{
    /* Prepare over the air update. */
    ArduinoOTA.begin();
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart(onStart);
    ArduinoOTA.onEnd(onEnd);
    ArduinoOTA.onProgress(onProgress);
    ArduinoOTA.onError(onError);

    LOG_INFO(String("OTA hostname: ") + ArduinoOTA.getHostname());
    LOG_INFO(String("Sketch size: ") + ESP.getSketchSize() + " bytes");
    LOG_INFO(String("Free size: ") + ESP.getFreeSketchSpace() + " bytes");

    m_isInitialized = true;

    return;
}

void UpdateMgr::process(void)
{
    if (true == m_isInitialized)
    {
        ArduinoOTA.handle();
    }

    return;
}

/******************************************************************************
 * Protected Methods
 *****************************************************************************/

/******************************************************************************
 * Private Methods
 *****************************************************************************/

UpdateMgr::UpdateMgr() :
    m_isInitialized(false),
    m_updateIsRunning(false),
    m_progressBar()
{
}

UpdateMgr::~UpdateMgr()
{
}

void UpdateMgr::onStart(void)
{
    String  infoStr = "Update ";
    Canvas* canvas  = NULL;

    m_instance.m_updateIsRunning = true;

    if (U_FLASH == ArduinoOTA.getCommand())
    {
        infoStr += "sketch.";
    }
    else
    {
        infoStr += "filesystem.";

        /* Close filesystem before continue. 
         * Note, this needs a restart after update is finished.
         */
        SPIFFS.end();
    }

    LOG_INFO(infoStr);
    DisplayMgr::getInstance().showSysMsg(infoStr);

    /* Give the user a chance to read it. */
    DisplayMgr::getInstance().delay(SYS_MSG_WAIT_TIME_STD);

    /* Prepare to show the progress in the next steps. */
    LedMatrix::getInstance().clear();

    /* Reset progress */
    m_instance.m_progressBar.setProgress(0u);

    /* Add progress bar to slot canvas */
    canvas = DisplayMgr::getInstance().getSlot(0u);
    if (NULL == canvas)
    {
        LOG_WARNING("Progress bar could't be added to the slot canvas.");
    }
    else
    {
        canvas->addWidget(m_instance.m_progressBar);
    }

    return;
}

void UpdateMgr::onEnd(void)
{
    String  infoStr = "Update successful finished.";
    Canvas* canvas  = DisplayMgr::getInstance().getSlot(0u);

    m_instance.m_updateIsRunning = false;

    /* Remove progress bar */
    if (NULL == canvas)
    {
        LOG_WARNING("Couldn't remove progress bar from slot canvas.");
    }
    else
    {
        canvas->removeWidget(m_instance.m_progressBar);
    }

    LOG_INFO(infoStr);
    DisplayMgr::getInstance().showSysMsg(infoStr);

    /* Give the user a chance to read it. */
    DisplayMgr::getInstance().delay(SYS_MSG_WAIT_TIME_STD);

    m_instance.restart();

    return;
}

void UpdateMgr::onProgress(unsigned int progress, unsigned int total)
{
    const uint32_t  PROGRESS_PERCENT    = (progress * 100u) / total;

    LOG_INFO(String("Progress: ") + PROGRESS_PERCENT + "%");

    m_instance.m_progressBar.setProgress(static_cast<uint8_t>(PROGRESS_PERCENT));

    return;
}

void UpdateMgr::onError(ota_error_t error)
{
    String infoStr;

    m_instance.m_updateIsRunning = false;
    
    switch(error)
    {
    case OTA_AUTH_ERROR:
        infoStr = "OTA - Authentication error.";
        break;

    case OTA_BEGIN_ERROR:
        infoStr = "OTA - Begin error.";
        break;

    case OTA_CONNECT_ERROR:
        infoStr = "OTA - Connect error.";
        break;

    case OTA_RECEIVE_ERROR:
        infoStr = "OTA - Receive error.";
        break;

    case OTA_END_ERROR:
        infoStr = "OTA - End error.";
        break;

    default:
        infoStr = "OTA - Unknown error.";
        break;
    }

    LOG_INFO(infoStr);
    DisplayMgr::getInstance().showSysMsg(infoStr);

    /* Give the user a chance to read it. */
    DisplayMgr::getInstance().delay(SYS_MSG_WAIT_TIME_STD);

    m_instance.restart();

    return;
}

void UpdateMgr::restart(void)
{
    wifi_mode_t wifiMode = WiFi.getMode();

    /* Disconnect first all connections */
    if (WIFI_MODE_STA == wifiMode)
    {
        (void)WiFi.disconnect();
    }
    else if ((WIFI_MODE_AP == wifiMode) ||
             (WIFI_MODE_APSTA == wifiMode))
    {
        (void)WiFi.softAPdisconnect();
    }

    ESP.restart();

    return;
}

/******************************************************************************
 * External Functions
 *****************************************************************************/

/******************************************************************************
 * Local Functions
 *****************************************************************************/