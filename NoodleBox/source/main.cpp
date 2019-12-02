/*
 * Copyright (c) 2017, NXP Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of NXP Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/**
 */

//
// API INCLUDES
//
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MKE04Z1284.h"
#include "fsl_clock.h"
#include "fsl_spi.h"
#include "fsl_pit.h"
#include "fsl_common.h"
#include "fsl_kbi.h"
#include "fsl_gpio.h"
#include "fsl_i2c.h"
#include "fsl_uart.h"


//
// APPLICATION INCLUDES
//
#include "defs.h"
#include "digital_out.h"
#include "chars.h"
#include "ui_driver.h"
#include "leds.h"
#include "midi.h"
#include "clock.h"
#include "popup.h"
#include "i2c_bus.h"
#include "storage.h"
#include "scale.h"
#include "outs.h"
#include "sequence_step.h"
#include "sequence_page.h"
#include "sequence_layer.h"
#include "sequence.h"
#include "sequence_editor.h"
#include "params.h"
#include "menu.h"


const uint32_t title_screen[] = {
		/*
	(uint32_t)0x1C63390C,
	(uint32_t)0x2294A512,
	(uint32_t)0x4294A490,
	(uint32_t)0x2294951C,
	(uint32_t)0x22949510,
	(uint32_t)0x4452A516,
	(uint32_t)0x4461B9C8,
	(uint32_t)0x0,
	(uint32_t)0x618800,
	(uint32_t)0x596504,
	(uint32_t)0x3049220A,
	(uint32_t)0x4871220A,
	(uint32_t)0x40492502,
	(uint32_t)0x21452942,
	(uint32_t)0x1E7CC93C,
	(uint32_t)0x0
	*/
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF,
		(uint32_t)0xFFFFFFFF
};



 CDigitalOut<kGPIO_PORTE, 2> PowerControl;
 CDigitalIn<kGPIO_PORTE, 1> OffSwitch;



typedef enum:byte {
	 VIEW_SEQUENCER,
	 VIEW_MENU
 } VIEW_TYPE;

VIEW_TYPE g_view = VIEW_SEQUENCER;



void dispatch_event(int event, uint32_t param) {
	switch(g_view) {
	case VIEW_SEQUENCER:
		g_sequence_editor.event(event, param);
		break;
	case VIEW_MENU:
		g_menu.event(event, param);
		break;
	}
}

void fire_event(int event, uint32_t param) {

	switch(event) {
	///////////////////////////////////
	case EV_KEY_PRESS:
		switch(param) {
			case KEY_RUN:
				fire_event(g_sequence.is_running()? EV_SEQ_STOP : EV_SEQ_CONTINUE, 0);
				break;
			case KEY_CV|KEY_RUN:
				fire_event(EV_SEQ_RESTART, 0);
				break;
			default:
				dispatch_event(event, param);
				break;
		}
		break;
	///////////////////////////////////
	case EV_KEY_CLICK:
		switch(param) {
		case KEY_LAYER:
			if(g_view != VIEW_MENU) {
				g_view = VIEW_MENU;
				g_menu.activate();
			}
			else {
				g_view = VIEW_SEQUENCER;
				g_menu.activate();
			}
			break;
		default:
			dispatch_event(event, param);
			break;
		}
		break;
	///////////////////////////////////
	case EV_SEQ_STOP:
		g_popup.text("STOP");
		g_clock.event(event, param);
		g_sequence.event(event, param);
		g_outs.close_all_gates();
		break;
	///////////////////////////////////
	case EV_SEQ_RESTART:
		g_popup.text("RST");
		g_clock.event(event, param);
		g_sequence.event(event, param);
		break;
	///////////////////////////////////
	case EV_SEQ_CONTINUE:
		g_popup.text("RUN");
		g_clock.event(event, param);
		g_sequence.event(event, param);
		break;
	///////////////////////////////////
	case EV_CLOCK_RESET:
		g_clock.event(event, param);
		g_sequence.event(event, param);
		break;
	///////////////////////////////////
	case EV_CHANGE_LAYER:
		g_sequence_editor.event(event, param);
		break;
	///////////////////////////////////
	default:
		dispatch_event(event, param);
		break;
	}
}


void force_full_repaint() {
	g_popup.force_repaint();
	g_menu.force_repaint();
}

void test() {
	int dac = 0;
	int gate = 0;
	int gate_tmr = 0;
	while(1) {
    	if(g_clock.is_ms_tick()) {
    		g_outs.test_dac(2,  dac);
    		g_outs.test_dac(3,  dac);
    		if(++dac > 4095) {
    			dac = 0;
    		}
    		if(++gate_tmr > 50) {
    			gate_tmr = 0;
    			gate = !gate;
    			g_outs.gate(3, gate? COuts::GATE_OPEN: COuts::GATE_CLOSED);

    		}

    	}

		g_outs.run_i2c();
	}
}

int main(void) {
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();

    g_clock.init();
    g_ui.init();
    for(int i=15; i>=0; --i) {
        g_ui.lock_for_update();
        for(int j=0; j<16; ++j) {
        	g_ui.raster(j) = title_screen[j];
        	if(j>=i) {
        		g_ui.hilite(j) = title_screen[j];
        	}
        	else {
        		g_ui.hilite(j) = 0;
        	}
        }
        g_ui.unlock_for_update();
        g_clock.wait_ms(50);
    }
    PowerControl.set(1);
    g_i2c_bus.init();
    g_midi.init();
    g_sequence.init();
    if(!g_ui.is_key_down(KEY_CV)) {
        g_sequence.load_patch(0);
    }




    while(1) {

    	if(g_clock.is_ms_tick()) {

        	g_clock.run();
       		g_sequence.run();
        	g_outs.run();
        	g_midi.run();

       		g_sequence_editor.run();
        	g_ui.run();
    		g_popup.run();

			g_ui.lock_for_update();
			switch(g_view) {
			case VIEW_SEQUENCER:
				g_sequence_editor.repaint();
				break;
			case VIEW_MENU:
				g_menu.repaint();
				break;
			}
			g_popup.repaint();
			g_ui.unlock_for_update();

    		if(!OffSwitch.get()) {
    			break;
    		}

    		g_midi_led.run();
    		g_gate_led.run();
    		g_tempo_led.run();
    		g_clock_out.run();

    	}

    	// run the i2c bus.
    	g_outs.run_i2c();
    }
    g_sequence.save_patch(0);
	PowerControl.set(0);
    return 0 ;
}
