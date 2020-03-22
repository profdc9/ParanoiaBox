/*
 * Copyright (c) 2020 Daniel Marks

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 */

#include <stdarg.h>
#include <Arduino.h>
#include "debugmsg.h"
#include "consoleio.h"
#include "random.h"
#include "cryptotool.h"

#define AD_RAND_PIN1 0   /* PA0 */
#define AD_RAND_PIN2 8   /* PB0 */

#define USE_MINIPRINTF

#ifdef USE_MINIPRINTF
#include "mini-printf.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ROTATE8BITSLEFT(x) (((uint8_t)(x) << 1) | ((uint8_t)(x) >> 7))

void randomness_get_raw_random_bits(uint8_t randomdata[], int bytes)
{
  while (bytes>0)
  {
    ADC1->regs->SQR3 = AD_RAND_PIN1;
  	ADC1->regs->CR2 |= ADC_CR2_SWSTART;
    ADC2->regs->SQR3 = AD_RAND_PIN2;
  	ADC2->regs->CR2 |= ADC_CR2_SWSTART;
  	while (!(ADC1->regs->SR & ADC_SR_EOC));
  	randomdata[--bytes] = ((unsigned char)(ADC1->regs->DR & ADC_DR_DATA));
  	while (!(ADC2->regs->SR & ADC_SR_EOC));
    if (bytes>0) randomdata[--bytes] = ((unsigned char)(ADC2->regs->DR & ADC_DR_DATA));
  }
}

void random_initialize(void)
{
	pinMode(PA0,INPUT_ANALOG);
	pinMode(PB0,INPUT_ANALOG);
	adc_set_sample_rate(ADC1, ADC_SMPR_1_5);
	adc_set_reg_seqlen(ADC1, 1);
	adc_set_sample_rate(ADC2, ADC_SMPR_1_5);
	adc_set_reg_seqlen(ADC2, 1);
}

int random_circuit_check(void)
{
}

void randomness_get_whitened_bits(uint8_t whitenedbytes[], size_t bytes)
{
	for (int b=0;b<bytes;b+=32)
	{
    uint8_t predigest[KEYMANAGER_HASHLEN*8];   
		randomness_get_raw_random_bits(predigest, sizeof(predigest));
    int digest_bytes = bytes - b;
    ctblake2s(&whitenedbytes[b],
       digest_bytes > KEYMANAGER_HASHLEN ? KEYMANAGER_HASHLEN : digest_bytes,
       (void *)predigest, sizeof(predigest), NULL,0);
	}
}

#define NUMBER_BLOCK 400

void randomness_test(void)
{
  unsigned int histogram[32];
  for (int n=0;n<(sizeof(histogram)/sizeof(int));n++) histogram[n] = 0;
  unsigned long startime = millis();

  unsigned int bits=0;
  for (;;)
  {
	int ch = console_inchar();
	if (ch == ' ') break;
	for (int n=0;n<NUMBER_BLOCK;n++)
	{
		uint8_t randbits[32];
		randomness_get_raw_random_bits(randbits, sizeof(randbits));
//	    randomness_get_whitened_bits(randbits, sizeof(randbits));
	    for (int j=0;j<sizeof(randbits);j++)
			histogram[randbits[j] & 0x1F]++;
	}
  bits += (NUMBER_BLOCK*32*8);
	console_clrscr();
	console_puts("Histogram:\r\n\n");
	for (int n=0;n<16;n++)
	{
    console_gotoxy(1,n+3);
		console_printint(n);
		console_puts(": ");
		console_printuint(histogram[n]);
    console_gotoxy(20,n+3);
    console_printint(n+16);
    console_puts(": ");
    console_printuint(histogram[n+16]);
	} 
  unsigned long elapsed = ((unsigned long)(millis()-startime))/1000;
  console_gotoxy(1,20);
  console_puts("Total bits: ");
  console_printuint(bits);
  console_puts(" bit/s ");
  console_printuint(bits/elapsed);
	console_puts("\r\nPress SPACE to end");
  }	  
}

void randomness_show(void)
{
	for (;;)
	{
	  uint8_t samp[32];
    //  randomness_get_whitened_bits(samp, sizeof(samp));
    randomness_get_raw_random_bits(samp, sizeof(samp));
	  console_clrscr();
	  for (int i=0;i<(sizeof(samp)/sizeof(samp[0]));i+=2)
	  {
		  console_printuint(samp[i]);
		  console_puts(" ");
		  console_printuint(samp[i+1]);
		  console_printcrlf();		  
	  }
	  delay(1000);
	  int ch = console_inchar();
	  if (ch == ' ') break;
	}
}

#ifdef __cplusplus
}
#endif
