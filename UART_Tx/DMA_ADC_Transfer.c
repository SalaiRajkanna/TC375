/**********************************************************************************************************************
 * \file DMA_ADC_Transfer.c
 * \copyright Copyright (C) Infineon Technologies AG 2019
 *
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are solely in the form of
 * machine-executable object code generated by a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *********************************************************************************************************************/

/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "DMA_ADC_Transfer.h"
#include "IfxAsclin_Asc.h"

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* ASCLIN handle */
IfxAsclin_Asc g_asc;

/* The transfer buffers allocate memory for the data itself and for FIFO runtime variables.
 * 8 more bytes have to be added to ensure a proper circular buffer handling independent from
 * the address to which the buffers have been located.
 */
uint8 g_ascTxBuffer[ASC_TX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];
uint8 g_ascRxBuffer[ASC_RX_BUFFER_SIZE + sizeof(Ifx_Fifo) + 8];

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/* Function to read the EVADC used channel */
/* ISR triggered by the DMA to notify the user that the transfer has been executed */

IFX_INTERRUPT(ASCLIN_ISR_Tx, 0, ISR_PRIORITY_ASCLIN_TX);
void ASCLIN_ISR_Tx(void)
{
    IfxAsclin_Asc_isrTransmit(&g_asc);
}

IFX_INTERRUPT(ASCLIN_ISR_Rx, 0, ISR_PRIORITY_ASCLIN_RX);
void ASCLIN_ISR_Rx(void)
{
    IfxAsclin_Asc_isrReceive(&g_asc);
}

IFX_INTERRUPT(ASCLIN_ISR_Er, 0, ISR_PRIORITY_ASCLIN_ER);
void ASCLIN_ISR_Er(void)
{
    IfxAsclin_Asc_isrError(&g_asc);
}

/* Function to initialize the ASCLIN module */
void init_UART(void)
{
    IfxAsclin_Asc_Config ascConf;

    IfxAsclin_Asc_initModuleConfig(&ascConf, &MODULE_ASCLIN0);

    /* Set the desired baud rate */
    ascConf.baudrate.prescaler = ASC_PRESCALER;
    ascConf.baudrate.baudrate = ASC_BAUDRATE;                                   /* Set the baud rate in bit/s       */
    ascConf.baudrate.oversampling = IfxAsclin_OversamplingFactor_16;            /* Set the oversampling factor      */

    /* Configure the sampling mode */
    ascConf.bitTiming.medianFilter = IfxAsclin_SamplesPerBit_three;             /* Set the number of samples per bit*/
    ascConf.bitTiming.samplePointPosition = IfxAsclin_SamplePointPosition_8;    /* Set the first sample position    */

    /* ISR priorities and interrupt target */
    ascConf.interrupt.txPriority = ISR_PRIORITY_ASCLIN_TX;
    ascConf.interrupt.rxPriority = ISR_PRIORITY_ASCLIN_RX;
    ascConf.interrupt.erPriority = ISR_PRIORITY_ASCLIN_ER;
    ascConf.interrupt.typeOfService = IfxSrc_Tos_cpu0;

    /* FIFO configuration */
    ascConf.txBuffer = g_ascTxBuffer;
    ascConf.txBufferSize = ASC_TX_BUFFER_SIZE;
    ascConf.rxBuffer = g_ascRxBuffer;
    ascConf.rxBufferSize = ASC_RX_BUFFER_SIZE;

    /* Pin configuration */
    const IfxAsclin_Asc_Pins pins = {
            NULL, IfxPort_InputMode_pullUp,                                     /* CTS port pin not used            */
            &IfxAsclin0_RXA_P14_1_IN, IfxPort_InputMode_pullUp,                 /* RX port pin                      */
            NULL, IfxPort_OutputMode_pushPull,                                  /* RTS port pin not used            */
            &IfxAsclin0_TX_P14_0_OUT, IfxPort_OutputMode_pushPull,              /* TX port pin                      */
            IfxPort_PadDriver_cmosAutomotiveSpeed1
    };
    ascConf.pins = &pins;

    /* Initialize module */
    IfxAsclin_Asc_initModule(&g_asc, &ascConf);
}

/* Function to transmit data over ASC */
void send_data(char *data, Ifx_SizeT length)
{
   /* Transmit data */
   IfxAsclin_Asc_write(&g_asc, data, &length, TIME_INFINITE);
}

/* Function to receive data over ASC */
void receive_data(char *data, Ifx_SizeT length)
{
   /* Receive data */
   IfxAsclin_Asc_read(&g_asc, data, &length, TIME_INFINITE);
}