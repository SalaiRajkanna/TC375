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
#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "Bsp.h"
#include <stdio.h>
#include <string.h>
#include "IfxCan_Can.h"
#include "IfxCan.h"
#include "IfxCpu_Irq.h"
#include "IfxPort.h"

IFX_ALIGN(4) IfxCpu_syncEvent g_cpuSyncEvent = 0;

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define CAN_MESSAGE_ID              (uint32)0x60           /* Message ID that will be used in arbitration phase    */
#define PORT0_PIN5                  5                       /* LED1 used in TX ISR is connected to this pin         */
#define PORT0_PIN6                  6                       /* LED2 used in RX ISR is connected to this pin         */
#define PORT20_PIN6                 6                       /* LED2 used in RX ISR is connected to this pin         */
#define ISR_PRIORITY_CAN_TX         2                       /* Define the CAN TX interrupt priority                 */
#define ISR_PRIORITY_CAN_RX         1                       /* Define the CAN RX interrupt priority                 */
#define TX_DATA_LOW_WORD            (uint32)0x0DEAD007      /* Define CAN data lower word to be transmitted         */
#define TX_DATA_HIGH_WORD           (uint32)0xBA5EBA11      /* Define CAN data higher word to be transmitted        */
#define MAXIMUM_CAN_DATA_PAYLOAD    2                       /* Define maximum classical CAN payload in 4-byte words */



#define TX_PIN                      IfxCan_TXD00_P20_8_OUT
#define RX_PIN                      IfxCan_RXD00B_P20_7_IN

#define CAN_STB                           1

/*********************************************************************************************************************/
/*--------------------------------------------------Data Structures--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    IfxCan_Can_Config canConfig;                            /* CAN module configuration structure                   */
    IfxCan_Can canModule;                                   /* CAN module handle                                    */
    IfxCan_Can_Node canSrcNode;                             /* CAN source node handle data structure                */
    IfxCan_Can_Node canDstNode;                             /* CAN destination node handle data structure           */
    IfxCan_Can_NodeConfig canNodeConfig;                    /* CAN node configuration structure                     */
    IfxCan_Filter canFilter;                                /* CAN filter configuration structure                   */
    IfxCan_Message txMsg;                                   /* Transmitted CAN message structure                    */
    IfxCan_Message rxMsg;                                   /* Received CAN message structure                       */
    uint32 txData[MAXIMUM_CAN_DATA_PAYLOAD];                /* Transmitted CAN data array                           */
    uint32 rxData[MAXIMUM_CAN_DATA_PAYLOAD];                /* Received CAN data array                              */
} McmcanType;


/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/

McmcanType                  g_mcmcan;                       /* Global MCMCAN configuration and control structure    */
IfxPort_Pin_Config          g_led1;                         /* Global LED1 configuration and control structure      */
IfxPort_Pin_Config          g_led2;                         /* Global LED2 configuration and control structure      */

// CAN STB port is not used in this test <<--- Try with and without once and see how it is working..
#if CAN_STB
    IfxPort_Pin_Config          g_stb;
#endif



/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
/* Macro to define Interrupt Service Routine.
 * This macro:
 * - defines linker section as .intvec_tc<vector number>_<interrupt priority>.
 * - defines compiler specific attribute for the interrupt functions.
 * - defines the Interrupt service routine as ISR function.
 *
 * IFX_INTERRUPT(isr, vectabNum, priority)
 *  - isr: Name of the ISR function.
 *  - vectabNum: Vector table number.
 *  - priority: Interrupt priority. Refer Usage of Interrupt Macro for more details.
 */
IFX_INTERRUPT(canIsrTxHandler, 0, ISR_PRIORITY_CAN_TX);
IFX_INTERRUPT(canIsrRxHandler, 0, ISR_PRIORITY_CAN_RX);

/* Interrupt Service Routine (ISR) called once the TX interrupt has been generated.
 * Turns on the LED1 to indicate successful CAN message transmission.
 */
void canIsrTxHandler(void)
{
    /* Clear the "Transmission Completed" interrupt flag */
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canSrcNode.node, IfxCan_Interrupt_transmissionCompleted);

    /* Just to indicate that the CAN message has been transmitted by turning on LED1 */
    IfxPort_togglePin(g_led1.port, g_led1.pinIndex);

}

/* Interrupt Service Routine (ISR) called once the RX interrupt has been generated.
 * Compares the content of the received CAN message with the content of the transmitted CAN message
 * and in case of success, turns on the LED2 to indicate successful CAN message reception.
 */
void canIsrRxHandler(void)
{
    /* Clear the "Message stored to Dedicated RX Buffer" interrupt flag */
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canSrcNode.node, IfxCan_Interrupt_messageStoredToDedicatedRxBuffer);

    /* Read the received CAN message */
    IfxCan_Can_readMessage(&g_mcmcan.canSrcNode, &g_mcmcan.rxMsg, g_mcmcan.rxData);

    /* Check if the received data matches with the transmitted one */
    if ( g_mcmcan.rxMsg.messageId == 0x60 )
    {
        /* Turn on the LED2 to indicate correctness of the received message */
        IfxPort_togglePin(g_led2.port, g_led2.pinIndex);
    }

}

/* Function to initialize MCMCAN module and nodes related for this application use case */
void initMcmcan(void)
{
    /* ==========================================================================================
     * CAN module configuration and initialization:
     * ==========================================================================================
     *  - load default CAN module configuration into configuration structure
     *  - initialize CAN module with the default configuration
     * ==========================================================================================
     */
    // Disabling all Interrupts
    boolean iState = IfxCpu_disableInterrupts();
    // Initializing CAN Module with canConfig  with CAN0 object(HW)
    // This initializes all the Register associated with CAN
    IfxCan_Can_initModuleConfig(&g_mcmcan.canConfig, &MODULE_CAN0);
    //Here we are configuring the registers for CAN configuration
    IfxCan_Can_initModule(&g_mcmcan.canModule, &g_mcmcan.canConfig);
    // Here we configure Node Number type configurations
    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_0;

    g_mcmcan.canNodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;
    g_mcmcan.canNodeConfig.txConfig.dedicatedTxBuffersNumber = 2;

    g_mcmcan.canNodeConfig.messageRAM.baseAddress = (uint32)&MODULE_CAN0;

    g_mcmcan.canNodeConfig.interruptConfig.transmissionCompletedEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.traco.priority = ISR_PRIORITY_CAN_TX;
    g_mcmcan.canNodeConfig.interruptConfig.traco.interruptLine = IfxCan_InterruptLine_0;
    g_mcmcan.canNodeConfig.interruptConfig.traco.typeOfService = IfxSrc_Tos_cpu0;


    g_mcmcan.canNodeConfig.interruptConfig.messageStoredToDedicatedRxBufferEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.reint.priority = ISR_PRIORITY_CAN_RX;
    g_mcmcan.canNodeConfig.interruptConfig.reint.interruptLine = IfxCan_InterruptLine_1;
    g_mcmcan.canNodeConfig.interruptConfig.reint.typeOfService = IfxSrc_Tos_cpu0;

    IfxCan_Can_Pins pins;
    pins.rxPin = &RX_PIN;
    pins.rxPinMode = IfxPort_InputMode_pullUp;
    pins.txPin = &TX_PIN;
    pins.txPinMode = IfxPort_OutputMode_pushPull;
    pins.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed2;
    g_mcmcan.canNodeConfig.pins = &pins;

    IfxCan_Can_initNode(&g_mcmcan.canSrcNode, &g_mcmcan.canNodeConfig);

    g_mcmcan.canFilter.number = 0;
    g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxBuffer;
    g_mcmcan.canFilter.id1 = 0x60;
    g_mcmcan.canFilter.rxBufferOffset = IfxCan_RxBufferId_0;

    IfxCan_Can_setStandardFilter(&g_mcmcan.canSrcNode, &g_mcmcan.canFilter);


    IfxCpu_restoreInterrupts(iState);
}

/* Function to initialize both TX and RX messages with the default data values.
 * After initialization of the messages, the TX message is transmitted.
 */
void transmitCanMessage(void)
{

    /* Initialization of the TX message with the default configuration */
    IfxCan_Can_initMessage(&g_mcmcan.txMsg);

    /* Define the content of the data to be transmitted */
    g_mcmcan.txData[0] = TX_DATA_LOW_WORD;
    g_mcmcan.txData[1] = TX_DATA_HIGH_WORD;

    /* Set the message ID that is used during the receive acceptance phase */
    g_mcmcan.txMsg.messageId = 0x55;

    IfxCan_Can_sendMessage(&g_mcmcan.canSrcNode, &g_mcmcan.txMsg, &g_mcmcan.txData[0]);
}

/* Function to initialize the LEDs */
void initLeds(void)
{
    /* ======================================================================
     * Configuration of the pins connected to the LEDs:
     * ======================================================================
     *  - define the GPIO port
     *  - define the GPIO pin that is connected to the LED
     *  - define the general GPIO pin usage (no alternate function used)
     *  - define the pad driver strength
     * ======================================================================
     */
    g_led1.port      = &MODULE_P00;
    g_led1.pinIndex  = PORT0_PIN5;
    g_led1.mode      = IfxPort_OutputIdx_general;
    g_led1.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    g_led2.port      = &MODULE_P00;
    g_led2.pinIndex  = PORT0_PIN6;
    g_led2.mode      = IfxPort_OutputIdx_general;
    g_led2.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

#if CAN_STB
    g_stb.port      = &MODULE_P20;
    g_stb.pinIndex  = PORT20_PIN6;
    g_stb.mode      = IfxPort_OutputIdx_general;
    g_stb.padDriver = IfxPort_PadDriver_cmosAutomotiveSpeed1;

    IfxPort_setPinLow(g_stb.port, g_stb.pinIndex);
    IfxPort_setPinModeOutput(g_stb.port, g_stb.pinIndex, IfxPort_OutputMode_pushPull, g_stb.mode);
    IfxPort_setPinPadDriver(g_stb.port, g_stb.pinIndex, g_stb.padDriver);
#endif

    /* Initialize the pins connected to LEDs to level "HIGH", which keep the LEDs turned off as default state */
    IfxPort_setPinHigh(g_led1.port, g_led1.pinIndex);
    IfxPort_setPinHigh(g_led2.port, g_led2.pinIndex);


    /* Set the pin input/output mode for both pins connected to the LEDs */
    IfxPort_setPinModeOutput(g_led1.port, g_led1.pinIndex, IfxPort_OutputMode_pushPull, g_led1.mode);
    IfxPort_setPinModeOutput(g_led2.port, g_led2.pinIndex, IfxPort_OutputMode_pushPull, g_led2.mode);


    /* Set the pad driver mode for both pins connected to the LEDs */
    IfxPort_setPinPadDriver(g_led1.port, g_led1.pinIndex, g_led1.padDriver);
    IfxPort_setPinPadDriver(g_led2.port, g_led2.pinIndex, g_led2.padDriver);

}

/*********************************************************************************************************************/
/*-----------------------------------------------------Main Function-------------------------------------------------*/
/*********************************************************************************************************************/
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


    initMcmcan();
    initLeds();

    while(1)
    {
        transmitCanMessage();
        waitTime(IfxStm_getTicksFromMilliseconds(BSP_DEFAULT_TIMER, 1000));    /* Wait 1000 milliseconds            */

    }
}


