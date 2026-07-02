/**
 ******************************************************************************
 * @file    STM32H5OSPI.c
 * @author  MCD Application Team
 * @brief   This file defines the operations of the external loader for
 *          MX66LM1G45G OSPI memory of STM32H573I-DK.
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "STM32H5OSPI.h"

/* Exported variables --------------------------------------------------------*/

/**
 * @brief status of the mapped mode MEM_DISABLE or MEM_ENABLE
 */
MEM_MAPSTAT MemoryMappedMode = MEM_MAPDISABLE;

BSP_OSPI_NOR_Init_t Flash;

/* Private functions ---------------------------------------------------------*/
/**
 * @brief This function configures the source of the time base:
 *        The time source is configured to have 1ms time base with a dedicated
 *        Tick interrupt priority.
 * @note The function is an override of the HAL function to have tick
 *       management functional without interrupt
 * @param TickPriority  Tick interrupt priority.
 * @retval HAL status
 */

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  HAL_StatusTypeDef retr = HAL_ERROR;
  /* Check uwTickFreq for MisraC 2012 (even if uwTickFreq is a enum type that doesn't take the value zero)*/
  if ((uint32_t)uwTickFreq != 0U)
  {
    uint32_t ticks = SystemCoreClock / (1000U / (uint32_t)uwTickFreq);
    SysTick->LOAD = (uint32_t)(ticks - 1UL);     /* Set reload register */
    SysTick->VAL = 0UL;                          /* Load the SysTick Counter Value */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | /* Set processor clock */
                    SysTick_CTRL_ENABLE_Msk;     /* Enable SysTick Timer */
    retr = HAL_OK;
  }
  return retr;
}

/**
 * @brief Provide a tick value in millisecond.
 * @note The function is an override of the HAL function to increment the
 *       tick on a count flag event.
 * @retval tick value
 */
uint32_t HAL_GetTick(void)
{
  if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == SysTick_CTRL_COUNTFLAG_Msk)
  {
    uwTick++;
  }
  return uwTick;
}

/* Exported functions ---------------------------------------------------------*/
/**
 * @brief  System initialization.
 * @param  None
 * @retval  1 : Operation succeeded
 * @retval  0 : Operation failed
 */
int Init_OSPI()
{
  /* Declare and initialize the ret variable */
  uint32_t ret = LOADER_STATUS_SUCCESS;

  /* Disable interrupts */
  __disable_irq();

  /* Get the ZI (Zero Initialized) data location */
  extern uint32_t Image$$PrgData$$ZI$$Base;
  extern uint32_t Image$$PrgData$$ZI$$Limit;

  /* Calculate the start address and size of the ZI data */
  uint8_t *start_address = (uint8_t *)&Image$$PrgData$$ZI$$Base;
  uint32_t size = (uint32_t)&Image$$PrgData$$ZI$$Limit - (uint32_t)start_address;

  /* Initialize the ZI data to zero */
  memset(start_address, 0, size * sizeof(uint8_t));
	
  if ((FLASH->OPTSR_CUR & FLASH_OPTSR_PRODUCT_STATE) == OB_PROD_STATE_OPEN)
  {
    /* Initialize system */
    SystemInit();
    HAL_Init();
  }
 
    /* Deinitialize RCC to allow PLL reconfiguration when configuring system clock */
    HAL_RCC_DeInit();

    /* Configure the system clock */
    SystemClock_Config();


  /* Configure Flash interface mode and transfer rate */
  Flash.InterfaceMode = BSP_OSPI_NOR_SPI_MODE;
  Flash.TransferRate = BSP_OSPI_NOR_STR_TRANSFER;

  if (BSP_OSPI_NOR_Init(0, &Flash) != BSP_ERROR_NONE)
  {
    ret = LOADER_STATUS_FAIL;
  }
  else
  {
    /* Enable the OSPI in memory-mapped mode */
    if (BSP_OSPI_NOR_EnableMemoryMappedMode(0) != BSP_ERROR_NONE)
    {
      ret = LOADER_STATUS_FAIL;
    }
    else
    {
      /* Set the return status to success */
      ret = LOADER_STATUS_SUCCESS;
    }
  }
  /* Set the MemoryMappedMode variable to indicate that memory-mapped mode is enabled */
  MemoryMappedMode = MEM_MAPENABLE;

  /* Return the Loader status */
  return ret;
}

/**
 * @brief   Erases the entire memory.
 * @param   Parallelism The parallelism mode.
 * @retval  Loader status.
 * @retval  1       Operation succeeded.
 * @retval  0       Operation failed.
 */
int MassErase(void)
{
  /* Declare and initialize the ret variable */
  uint32_t ret = LOADER_STATUS_SUCCESS;

  /* Disable Interrupts */
  __disable_irq();

  /* Exit form memory-mapped mode if enabled */
  if (MemoryMappedMode == MEM_MAPENABLE)
  {
    if (BSP_OSPI_NOR_DisableMemoryMappedMode(0) != BSP_ERROR_NONE)
    {
      /* Set the return status to failure */
      ret = LOADER_STATUS_FAIL;
    }

    /* Set the MemoryMappedMode variable to indicate that memory-mapped mode is disabled */
    MemoryMappedMode = MEM_MAPDISABLE;
  }

  /* Erase the entire OSPI memory */
  if (BSP_OSPI_NOR_Erase_Chip(0) != BSP_ERROR_NONE)
  {
    ret = LOADER_STATUS_FAIL;
  }

  /* Read current status of the OSPI memory */
  while (BSP_OSPI_NOR_GetStatus(0) != BSP_ERROR_NONE)
    ;

  /* Return the Loader status */
  return ret;
}

/**
 * @brief   Programs memory.
 * @param   Address The page address.
 * @param   Size    The size of data to write.
 * @param   buffer  Pointer to the data buffer.
 * @retval  Loader status.
 * @retval  1       Operation succeeded.
 * @retval  0       Operation failed.
 */
int Write(uint32_t Address, uint32_t Size, uint8_t *buffer)
{
  /* Declare and initialize the ret variable */
  uint32_t ret = LOADER_STATUS_SUCCESS;

  /* Disable Interrupts */
  __disable_irq();

  Address = Address & 0x0FFFFFFF;

  /* Exit form memory-mapped mode if enabled */
  if (MemoryMappedMode == MEM_MAPENABLE)
  {
    if (BSP_OSPI_NOR_DisableMemoryMappedMode(0) != BSP_ERROR_NONE)
    {
      /* Set the return status to failure */
      ret = LOADER_STATUS_FAIL;
    }

    /* Set the MemoryMappedMode variable to indicate that memory-mapped mode is disabled */
    MemoryMappedMode = MEM_MAPDISABLE;
  }

  /* Write an amount of data to the OSPI memory */
  if (BSP_OSPI_NOR_Write(0, buffer, Address, Size) != BSP_ERROR_NONE)
  {
    ret = LOADER_STATUS_FAIL;
  }

  /* Return the Loader status */
  return ret;
}

/**
 * @brief   Erases sectors.
 * @param   EraseStartAddress The erase start address.
 * @param   EraseEndAddress   The erase end address.
 * @retval  Loader status.
 * @retval  1       Operation succeeded.
 * @retval  0       Operation failed.
 */
int SectorErase(uint32_t EraseStartAddress, uint32_t EraseEndAddress)
{
  /* Declare and initialize variable */
  uint32_t ret = LOADER_STATUS_SUCCESS;
  uint32_t current_end_addr;
  uint32_t current_start_addr;

  /* define the Sector Size */
  uint32_t sector_size = 0x10000;

  /* Disable Interrupts */
  __disable_irq();

  /* Exit form memory-mapped mode if enabled */
  if (MemoryMappedMode == MEM_MAPENABLE)
  {
    if (BSP_OSPI_NOR_DisableMemoryMappedMode(0) != BSP_ERROR_NONE)
    {
      /* Set the return status to failure */
      ret = LOADER_STATUS_FAIL;
    }

    /* Set the MemoryMappedMode variable to indicate that memory-mapped mode is disabled */
    MemoryMappedMode = MEM_MAPDISABLE;
  }

  /* Mask the addresses to 28 bits */
  current_end_addr = EraseEndAddress & 0x0FFFFFFF;
  current_start_addr = EraseStartAddress & 0x0FFFFFFF;

  /* Align the start address to the nearest 64K boundary */
  current_start_addr = current_start_addr - (current_start_addr % sector_size);

  while ((current_end_addr > current_start_addr) && (ret != LOADER_STATUS_FAIL))
  {
    /* Erase the specified block of the OSPI memory */
    if (BSP_OSPI_NOR_Erase_Block(0, current_start_addr, MX25LM51245G_ERASE_64K) != BSP_ERROR_NONE)
    {
      ret = LOADER_STATUS_FAIL;
    }
    /* Read current status of the OSPI memory */
    while (BSP_OSPI_NOR_GetStatus(0) != BSP_ERROR_NONE)
      ;

    current_start_addr += sector_size;
  }
  /* Exit form memory-mapped mode if enabled */
  if (MemoryMappedMode == MEM_MAPDISABLE)
  {
    if (BSP_OSPI_NOR_EnableMemoryMappedMode(0) != BSP_ERROR_NONE)
    {
      /* Set the return status to failure */
      ret = LOADER_STATUS_FAIL;
    }

    /* Set the MemoryMappedMode variable to indicate that memory-mapped mode is disabled */
    MemoryMappedMode = MEM_MAPENABLE;
  }
  /* Return the Loader status */
  return ret;
}

/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follows :
 *            System Clock source            = PLL (HSI)
 *            SYSCLK(Hz)                     = 240000000  (CPU Clock)
 *            HCLK(Hz)                       = 240000000  (Bus matrix and AHBs Clock)
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 1 (APB1 Clock  240MHz)
 *            APB2 Prescaler                 = 1 (APB2 Clock  240MHz)
 *            APB3 Prescaler                 = 1 (APB3 Clock  240MHz)
 *            HSI Frequency(Hz)              = 64000000
 *            PLL_M                          = 8
 *            PLL_N                          = 60
 *            PLL_P                          = 2
 *            PLL_Q                          = 2
 *            PLL_R                          = 2
 *            VDD(V)                         = 3.3
 *            Flash Latency(WS)              = 5
 * @param  None
 * @retval None
 */

static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  /* The voltage scaling allows optimizing the power consumption when the device is
  clocked below the maximum system frequency, to update the voltage scaling value
  regarding system frequency refer to product datasheet.
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /* Use HSE in bypass mode and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DIGITAL;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* Initialization Error */
    while(1);
  }
  /* Select PLL as system clock source and configure the HCLK, PCLK1, PCLK2 and PCLK3
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2| RCC_CLOCKTYPE_PCLK3);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    while(1);
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}