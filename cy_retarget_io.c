/***************************************************************************//**
* \file cy_retarget_io.c
*
* \brief
* Provides APIs for retargeting stdio to UART hardware contained on the XMC
* kits.
*
********************************************************************************
* \copyright
* Copyright 2021-2022 Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation
*
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "cy_retarget_io.h"
#include "cy_utils.h"
#include "xmc_uart.h"
#include <stdbool.h>
#include <stdlib.h>
#include "xmc_device.h"

#if (defined(CY_RTOS_AWARE) || defined(COMPONENT_RTOS_AWARE)) && defined(__GNUC__) && \
    !defined(__ARMCC_VERSION) && !defined(__clang__)

// The XMC™ UART driver is not necessarily thread-safe. To avoid concurrent
// access, the ARM and IAR libraries use mutexes to control access to stdio
// streams. For Newlib, the mutex must be implemented in _write(). For all
// libraries, the program must start the RTOS kernel before calling any stdio
// functions.

#include "cyabs_rtos.h"

static cy_mutex_t cy_retarget_io_mutex;
static bool       cy_retarget_io_mutex_initialized = false;
//--------------------------------------------------------------------------------------------------
// cy_retarget_io_mutex_init
//--------------------------------------------------------------------------------------------------
static cy_rslt_t cy_retarget_io_mutex_init(void)
{
    cy_rslt_t rslt;
    if (cy_retarget_io_mutex_initialized)
    {
        rslt = CY_RSLT_SUCCESS;
    }
    else if (CY_RSLT_SUCCESS == (rslt = cy_rtos_init_mutex(&cy_retarget_io_mutex)))
    {
        cy_retarget_io_mutex_initialized = true;
    }
    return rslt;
}


//--------------------------------------------------------------------------------------------------
// cy_retarget_io_mutex_acquire
//--------------------------------------------------------------------------------------------------
static void cy_retarget_io_mutex_acquire(void)
{
    CY_ASSERT(cy_retarget_io_mutex_initialized);
    cy_rslt_t rslt = cy_rtos_get_mutex(&cy_retarget_io_mutex, CY_RTOS_NEVER_TIMEOUT);
    if (rslt != CY_RSLT_SUCCESS)
    {
        abort();
    }
}


//--------------------------------------------------------------------------------------------------
// cy_retarget_io_mutex_release
//--------------------------------------------------------------------------------------------------
static void cy_retarget_io_mutex_release(void)
{
    CY_ASSERT(cy_retarget_io_mutex_initialized);
    cy_rslt_t rslt = cy_rtos_set_mutex(&cy_retarget_io_mutex);
    if (rslt != CY_RSLT_SUCCESS)
    {
        abort();
    }
}


//--------------------------------------------------------------------------------------------------
// cy_retarget_io_mutex_deinit
//--------------------------------------------------------------------------------------------------
static void cy_retarget_io_mutex_deinit(void)
{
    CY_ASSERT(cy_retarget_io_mutex_initialized);
    cy_rslt_t rslt = cy_rtos_deinit_mutex(&cy_retarget_io_mutex);
    if (rslt != CY_RSLT_SUCCESS)
    {
        abort();
    }
}


#else // if (defined(CY_RTOS_AWARE) || defined(COMPONENT_RTOS_AWARE)) && defined(__GNUC__) &&
// !defined(__ARMCC_VERSION) && !defined(__clang__)
#ifdef __ICCARM__
// Ignore unused functions
#pragma diag_suppress=Pe177
#endif

//--------------------------------------------------------------------------------------------------
// cy_retarget_io_mutex_init
//--------------------------------------------------------------------------------------------------
static inline cy_rslt_t cy_retarget_io_mutex_init(void)
{
    return CY_RSLT_SUCCESS;
}


#if defined(__ARMCC_VERSION) // ARM-MDK
__attribute__((unused))
#endif
//--------------------------------------------------------------------------------------------------
// cy_retarget_io_mutex_acquire
//--------------------------------------------------------------------------------------------------
static inline void cy_retarget_io_mutex_acquire(void)
{
}


#if defined(__ARMCC_VERSION) // ARM-MDK
__attribute__((unused))
#endif
//--------------------------------------------------------------------------------------------------
// cy_retarget_io_mutex_release
//--------------------------------------------------------------------------------------------------
static inline void cy_retarget_io_mutex_release(void)
{
}


#if defined(__ARMCC_VERSION) // ARM-MDK
__attribute__((unused))
#endif
//--------------------------------------------------------------------------------------------------
// cy_retarget_io_mutex_deinit
//--------------------------------------------------------------------------------------------------
static inline void cy_retarget_io_mutex_deinit(void)
{
}


#endif // if (defined(CY_RTOS_AWARE) || defined(COMPONENT_RTOS_AWARE)) && defined(__GNUC__) &&
// !defined(__ARMCC_VERSION) && !defined(__clang__)

#if defined(__cplusplus)
extern "C" {
#endif

// UART channel handle
cy_retarget_io_uart_t cy_retarget_io_uart_obj;

// Tracks the previous character sent to output stream
#ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
static char cy_retarget_io_stdout_prev_char = 0;
#endif // CY_RETARGET_IO_CONVERT_LF_TO_CRLF

//--------------------------------------------------------------------------------------------------
// cy_retarget_io_getchar
//--------------------------------------------------------------------------------------------------
static inline cy_rslt_t cy_retarget_io_getchar(char* c)
{
    while ((XMC_UART_CH_GetStatusFlag(cy_retarget_io_uart_obj.channel) &
            (XMC_UART_CH_STATUS_FLAG_RECEIVE_INDICATION |
             XMC_UART_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION)) == 0U)
    {
        // Block indefinitely waiting for data in the receive buffer
    }

    // Read single character from the receive buffer
    *c = XMC_UART_CH_GetReceivedData(cy_retarget_io_uart_obj.channel);

    // Clear the receive buffer indication flag
    XMC_UART_CH_ClearStatusFlag(cy_retarget_io_uart_obj.channel,
                                XMC_UART_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION |
                                XMC_UART_CH_STATUS_FLAG_RECEIVE_INDICATION);
    return CY_RSLT_SUCCESS;
}


//--------------------------------------------------------------------------------------------------
// cy_retarget_io_putchar
//--------------------------------------------------------------------------------------------------
static inline cy_rslt_t cy_retarget_io_putchar(char c)
{
    // Send single character to the transmit buffer
    XMC_UART_CH_Transmit(cy_retarget_io_uart_obj.channel, c);
    return CY_RSLT_SUCCESS;
}


#if defined(__ARMCC_VERSION) // ARM-MDK
//--------------------------------------------------------------------------------------------------
// fputc
//--------------------------------------------------------------------------------------------------
__attribute__((weak)) int fputc(int ch, FILE* f)
{
    (void)f;
    cy_rslt_t rslt = CY_RSLT_SUCCESS;
    #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
    if (((char)ch == '\n') && (cy_retarget_io_stdout_prev_char != '\r'))
    {
        rslt = cy_retarget_io_putchar('\r');
    }
    #endif // CY_RETARGET_IO_CONVERT_LF_TO_CRLF

    if (CY_RSLT_SUCCESS == rslt)
    {
        rslt = cy_retarget_io_putchar(ch);
    }

    #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
    if (CY_RSLT_SUCCESS == rslt)
    {
        cy_retarget_io_stdout_prev_char = (char)ch;
    }
    #endif // CY_RETARGET_IO_CONVERT_LF_TO_CRLF

    return (CY_RSLT_SUCCESS == rslt) ? ch : EOF;
}


#elif defined (__ICCARM__) // IAR
    #include <yfuns.h>

//--------------------------------------------------------------------------------------------------
// __write
//--------------------------------------------------------------------------------------------------
__weak size_t __write(int handle, const unsigned char* buffer, size_t size)
{
    size_t nChars = 0;
    // This template only writes to "standard out", for all other file handles it returns failure.
    if (handle != _LLIO_STDOUT)
    {
        return (_LLIO_ERROR);
    }
    if (buffer != NULL)
    {
        cy_rslt_t rslt = CY_RSLT_SUCCESS;
        for (; nChars < size; ++nChars)
        {
            #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
            if ((*buffer == '\n') && (cy_retarget_io_stdout_prev_char != '\r'))
            {
                rslt = cy_retarget_io_putchar('\r');
            }
            #endif // CY_RETARGET_IO_CONVERT_LF_TO_CRLF

            if (rslt == CY_RSLT_SUCCESS)
            {
                rslt = cy_retarget_io_putchar(*buffer);
            }

            if (rslt != CY_RSLT_SUCCESS)
            {
                break;
            }

            #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
            cy_retarget_io_stdout_prev_char = *buffer;
            #endif // CY_RETARGET_IO_CONVERT_LF_TO_CRLF
            ++buffer;
        }
    }
    return (nChars);
}


#else // (__GNUC__)  GCC
#if !defined(CY_RETARGET_IO_NO_FLOAT)
// Add an explicit reference to the floating point printf library to allow the usage of floating
// point conversion specifier.
__asm(".global _printf_float");
#endif
//--------------------------------------------------------------------------------------------------
// _write
//--------------------------------------------------------------------------------------------------
__attribute__((weak)) int _write(int fd, const char* ptr, int len)
{
    int nChars = 0;
    (void)fd;
    if (ptr != NULL)
    {
        cy_rslt_t rslt = CY_RSLT_SUCCESS;
        cy_retarget_io_mutex_acquire();
        for (; nChars < len; ++nChars)
        {
            #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
            if ((*ptr == '\n') && (cy_retarget_io_stdout_prev_char != '\r'))
            {
                rslt = cy_retarget_io_putchar('\r');
            }
            #endif // CY_RETARGET_IO_CONVERT_LF_TO_CRLF

            if (CY_RSLT_SUCCESS == rslt)
            {
                rslt = cy_retarget_io_putchar((uint32_t)*ptr);
            }

            if (CY_RSLT_SUCCESS != rslt)
            {
                break;
            }

            #ifdef CY_RETARGET_IO_CONVERT_LF_TO_CRLF
            cy_retarget_io_stdout_prev_char = *ptr;
            #endif // CY_RETARGET_IO_CONVERT_LF_TO_CRLF
            ++ptr;
        }
        cy_retarget_io_mutex_release();
    }
    return (nChars);
}


#endif // if defined(__ARMCC_VERSION)


#if defined(__ARMCC_VERSION) // ARM-MDK
//--------------------------------------------------------------------------------------------------
// fgetc
//--------------------------------------------------------------------------------------------------
__attribute__((weak)) int fgetc(FILE* f)
{
    (void)f;
    char c;
    cy_rslt_t rslt = cy_retarget_io_getchar(&c);
    return (CY_RSLT_SUCCESS == rslt) ? c : EOF;
}


#elif defined (__ICCARM__) // IAR
//--------------------------------------------------------------------------------------------------
// __read
//--------------------------------------------------------------------------------------------------
__weak size_t __read(int handle, unsigned char* buffer, size_t size)
{
    // This template only reads from "standard in", for all other file handles it returns failure.
    if ((handle != _LLIO_STDIN) || (buffer == NULL))
    {
        return (_LLIO_ERROR);
    }
    else
    {
        cy_rslt_t rslt = cy_retarget_io_getchar((char*)buffer);
        return (CY_RSLT_SUCCESS == rslt) ? 1 : 0;
    }
}


#else // (__GNUC__)  GCC
#if !defined(CY_RETARGET_IO_NO_FLOAT)
// Add an explicit reference to the floating point scanf library to allow the usage of floating
// point conversion specifier.
__asm(".global _scanf_float");
#endif
//--------------------------------------------------------------------------------------------------
// _read
//--------------------------------------------------------------------------------------------------
__attribute__((weak)) int _read(int fd, char* ptr, int len)
{
    (void)fd;

    cy_rslt_t rslt;
    int nChars = 0;
    if (ptr != NULL)
    {
        for (; nChars < len; ++ptr)
        {
            rslt = cy_retarget_io_getchar(ptr);
            if (rslt == CY_RSLT_SUCCESS)
            {
                ++nChars;
                if ((*ptr == '\n') || (*ptr == '\r'))
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }
    return (nChars);
}


#endif // if defined(__ARMCC_VERSION)

#if defined(__ARMCC_VERSION) // ARM-MDK
// Include _sys_* prototypes provided by ARM Compiler runtime library
    #include <rt_sys.h>

// Prevent linkage of library functions that use semihosting calls
__asm(".global __use_no_semihosting\n\t");

// Enable the linker to select an optimized library that does not include code to handle input
// arguments to main()
__asm(".global __ARM_use_no_argv\n\t");

//--------------------------------------------------------------------------------------------------
// _sys_open
//
// Open a file: dummy implementation.
// Everything goes to the same output, no need to translate the file names
// (__stdin_name/__stdout_name/__stderr_name) to descriptor numbers
//--------------------------------------------------------------------------------------------------
FILEHANDLE __attribute__((weak)) _sys_open(const char* name, int openmode)
{
    (void)name;
    (void)openmode;
    return 1;
}


//--------------------------------------------------------------------------------------------------
// _sys_close
//
// Close a file: dummy implementation.
//--------------------------------------------------------------------------------------------------
int __attribute__((weak)) _sys_close(FILEHANDLE fh)
{
    (void)fh;
    return 0;
}


//--------------------------------------------------------------------------------------------------
// _sys_write
//
// Write to a file: dummy implementation.
// The low-level function fputc retargets output to use UART TX
//--------------------------------------------------------------------------------------------------
int __attribute__((weak)) _sys_write(FILEHANDLE fh, const unsigned char* buf, unsigned len,
                                     int mode)
{
    (void)fh;
    (void)buf;
    (void)len;
    (void)mode;
    return 0;
}


//--------------------------------------------------------------------------------------------------
// _sys_read
//
// Read from a file: dummy implementation.
// The low-level function fputc retargets input to use UART RX
//--------------------------------------------------------------------------------------------------
int __attribute__((weak)) _sys_read(FILEHANDLE fh, unsigned char* buf, unsigned len, int mode)
{
    (void)fh;
    (void)buf;
    (void)len;
    (void)mode;
    return -1;
}


//--------------------------------------------------------------------------------------------------
// _ttywrch
//
// Write a character to the output channel: dummy implementation.
//--------------------------------------------------------------------------------------------------
void __attribute__((weak)) _ttywrch(int ch)
{
    (void)ch;
}


//--------------------------------------------------------------------------------------------------
// _sys_istty
//
// Check if the file is connected to a terminal: dummy implementation
//--------------------------------------------------------------------------------------------------
int __attribute__((weak)) _sys_istty(FILEHANDLE fh)
{
    (void)fh;
    return 0;
}


//--------------------------------------------------------------------------------------------------
// _sys_seek
//
// Move the file position to a given offset: dummy implementation
//--------------------------------------------------------------------------------------------------
int __attribute__((weak)) _sys_seek(FILEHANDLE fh, long pos)
{
    (void)fh;
    (void)pos;
    return -1;
}


//--------------------------------------------------------------------------------------------------
// _sys_flen
// Return the current length of a file: dummy implementation
//--------------------------------------------------------------------------------------------------
long __attribute__((weak)) _sys_flen(FILEHANDLE fh)
{
    (void)fh;
    return 0;
}


//--------------------------------------------------------------------------------------------------
// _sys_exit
//
// Terminate the program: dummy implementation
//--------------------------------------------------------------------------------------------------
void __attribute__((weak)) _sys_exit(int returncode)
{
    (void)returncode;
    for (;;)
    {
        // Halt here forever
    }
}


//--------------------------------------------------------------------------------------------------
// _sys_command_string
//
// Return a pointer to the command line: dummy implementation
//--------------------------------------------------------------------------------------------------
char __attribute__((weak)) *_sys_command_string(char* cmd, int len)
{
    (void)cmd;
    (void)len;
    return NULL;
}


#endif // ARM-MDK

//--------------------------------------------------------------------------------------------------
// cy_retarget_io_init
//--------------------------------------------------------------------------------------------------
cy_rslt_t cy_retarget_io_init(XMC_USIC_CH_t* base)
{
    cy_retarget_io_uart_obj.channel = base;
    return cy_retarget_io_mutex_init();
}


//--------------------------------------------------------------------------------------------------
// cy_retarget_io_is_tx_active
//--------------------------------------------------------------------------------------------------
bool cy_retarget_io_is_tx_active()
{
    return (XMC_UART_CH_GetStatusFlag(cy_retarget_io_uart_obj.channel) &
            XMC_UART_CH_STATUS_FLAG_TRANSFER_STATUS_BUSY) > 0U;
}


//--------------------------------------------------------------------------------------------------
// cy_retarget_io_deinit
//--------------------------------------------------------------------------------------------------
void cy_retarget_io_deinit()
{
    // Since the largest hardware buffer would be 256 bytes
    // it takes about 500 ms to transmit the 256 bytes at 9600 baud.
    // Thus 1000 ms gives roughly 50% padding to this time.
    int timeout_remaining_ms = 1000;
    volatile uint32_t cycle_time_ms = (SystemCoreClock / 1000);
    while (timeout_remaining_ms > 0)
    {
        if (!cy_retarget_io_is_tx_active())
        {
            break;
        }
        while (--cycle_time_ms > 0)
        {
        }
        timeout_remaining_ms--;
    }
    CY_ASSERT(timeout_remaining_ms != 0);
    cy_retarget_io_mutex_deinit();
}


#if defined(__cplusplus)
}
#endif
