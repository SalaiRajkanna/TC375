/**********************************************************************************************************************
 * \file Cpu0_Main.c
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
 /*\title DMA transfer of ADC conversion results
 * \abstract The DMA is used to transfer ADC measurements results to CPU0 DSPR.
 * \description At the end of an analog-to-digital conversion of the Enhanced Versatile Analog-to-Digital Converter
 *              (EVADC) module, an interrupt is triggered, which starts the data transfer of the converted ADC results
 *              via DMA to the CPU0 Data Scratch-Pad SRAM (DSPR0). The ADC conversion is started manually via a
 *              command of a serial monitor.
 *
 * \name DMA_ADC_Transfer_1_KIT_TC375_LK
 * \version V1.0.0
 * \board AURIX TC375 lite Kit, KIT_A2G_TC375_LITE, TC37xTP_A-Step
 * \keywords ADC, AURIX, DMA, DMA_ADC_Transfer_1, EVADC, data transfer
 * \documents https://www.infineon.com/aurix-expert-training/Infineon-AURIX_DMA_ADC_Transfer_1_KIT_TC375_LK-TR-v01_00_00-EN.pdf
 * \documents https://www.infineon.com/aurix-expert-training/TC37A_iLLD_UM_1_0_1_12_1.chm
 * \lastUpdated 2021-03-22
 *********************************************************************************************************************/
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "DMA_ADC_Transfer.h"

IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent = 0;

void core0_main(void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
    
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);
    
    //char rxData[RX_LENGTH];     /* Variable to store the user input                     */

    init_UART();                /* Initialization for the UART communication            */

    while(1)
    {
        // rxData[0] = RESET_CHARACTER;        /* Reset received data                      */

        // while(rxData[0] != PASS_CHARACTER)  /* Wait for receive the correct character   */
        // {
        //     receive_data(rxData, RX_LENGTH);
        // }

        /* Convert the configured EVADC channel */
        send_data(COMPLETED_TRANSFER_TEXT, ACK_TEXT_LENGTH); /* Acknowledgment via UART - Triggered by DMA transaction done */
    }
}

