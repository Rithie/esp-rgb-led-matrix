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
 * @brief  Task decoupler
 * @author Andreas Merkle <web@blue-andi.de>
 *
 * @addtogroup web
 *
 * @{
 */

#ifndef __TASK_DCOUPLER_HPP__
#define __TASK_DCOUPLER_HPP__

/******************************************************************************
 * Compile Switches
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/
#include <Arduino.h>

/******************************************************************************
 * Macros
 *****************************************************************************/

/******************************************************************************
 * Types and Classes
 *****************************************************************************/

/**
 * The task decoupler uses a operating system queue to decouple task
 * data communication.
 * 
 * @tparam T    Item type
 */
template < typename T >
class TaskDecoupler
{
public:

    /**
     * Creates a task decoupler.
     */
    TaskDecoupler() :
        m_hQueue(nullptr)
    {

    }

    /**
     * Destroys a task decoupler.
     */
    ~TaskDecoupler()
    {
        destroyQueue();
    }

    /**
     * Initialize the task decoupler. If it already is initialized, the queue
     * and its data is lost and a new queue will be created.
     * 
     * @param[in] maxItems  Max. number of queue items.
     * @param[in] itemSize  Item size in byte.
     * 
     * @return If successful created, it will return true otherwise false.
     */
    bool init(uint32_t maxItems, size_t itemSize)
    {
        destroyQueue();
        return createQueue(maxItems, itemSize);
    }

    /**
     * Add one single item to the queue.
     * If the queue is full, it will wait a specific time. If after this
     * the queue is still full, it will return.
     * 
     * @param[in] item  Item which to add
     * 
     * @return If successful added, it will return true otherwise false.
     */
    bool addItem(T& item)
    {
        bool isSuccessful = false;

        if (nullptr != m_hQueue)
        {
            if (pdTRUE == xQueueSend(m_hQueue, &item, WAIT_TIME))
            {
                isSuccessful = true;
            }
        }

        return isSuccessful;
    }

    /**
     * Get a single item from the queue.
     * If the queue is empty, it will wait a specific time. If after this
     * the queue is still empty, it will return.
     * 
     * @param[in] item  Item which to overwrite
     * 
     * @return If successful received, it will return true otherwise false.
     */
    bool getItem(T& item)
    {
        bool isSuccessful = false;

        if (nullptr != m_hQueue)
        {
            if (pdTRUE == xQueueReceive(m_hQueue, &item, WAIT_TIME))
            {
                isSuccessful = true;
            }
        }

        return isSuccessful;
    }

    /**
     * The maximum amount of time in ticks the task should block waiting for
     * space to become available on the queue.
     */
    static const TickType_t WAIT_TIME   = 100U;

private:

    QueueHandle_t   m_hQueue;   /**< Queue handle */

    TaskDecoupler(const TaskDecoupler& decoupler);
    TaskDecoupler& operator=(const TaskDecoupler& decoupler);

    /**
     * Create a operating system queue. If a queue already exists, it won't
     * be replaced. To ensure this, call @destroyQueue previously.
     * 
     * @param[in] maxItems  Max. number of queue items.
     * @param[in] itemSize  Item size in byte.
     * 
     * @return If successful created, it will return true otherwise false.
     */
    bool createQueue(uint32_t maxItems, size_t itemSize)
    {
        if (nullptr == m_hQueue)
        {
            m_hQueue = xQueueCreate(maxItems, itemSize);
        }

        return (nullptr != m_hQueue);
    }

    /**
     * Destroys the queue if applicable.
     */
    void destroyQueue()
    {
        if (nullptr != m_hQueue)
        {
            vQueueDelete(m_hQueue);
            m_hQueue = nullptr;
        }
    }

};

/******************************************************************************
 * Functions
 *****************************************************************************/

#endif  /* __TASK_DCOUPLER_HPP__ */

/** @} */