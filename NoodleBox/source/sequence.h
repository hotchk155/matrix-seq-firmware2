//////////////////////////////////////////////////////////////////////////////
// sixty four pixels 2020                                       CC-NC-BY-SA //
//                                //  //          //                        //
//   //////   /////   /////   //////  //   /////  //////   /////  //   //   //
//   //   // //   // //   // //   //  //  //   // //   // //   //  // //    //
//   //   // //   // //   // //   //  //  /////// //   // //   //   ///     //
//   //   // //   // //   // //   //  //  //      //   // //   //  // //    //
//   //   //  /////   /////   //////   //  /////  //////   /////  //   //   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// SEQUENCER TOP LEVEL
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
#ifndef SEQUENCE_H_
#define SEQUENCE_H_

///////////////////////////////////////////////////////////////////////////////
// SEQUENCER CLASS
class CSequence {

public:
	enum {
		NUM_LAYERS = 4,	// number of layers in the sequence
		MIDI_TRANSPOSE_ZERO = 60,
		MIDI_TRANSPOSE_RANGE = 24,
		MIDI_TRANSPOSE_MIN = (MIDI_TRANSPOSE_ZERO - MIDI_TRANSPOSE_RANGE),
		MIDI_TRANSPOSE_MAX = (MIDI_TRANSPOSE_ZERO + MIDI_TRANSPOSE_RANGE)
	};

private:
	CScale m_scale;
	CSequenceLayer m_layer_content[NUM_LAYERS];
	CSequenceLayer *m_layers[NUM_LAYERS];

	byte m_is_running;		// whether the sequencer is running

	int m_rec_layer;
	CSequenceLayer::REC_SESSION m_rec;
public:

	///////////////////////////////////////////////////////////////////////////////
	CSequence() {
	}

	void init() {
		for(int i=0; i<NUM_LAYERS; ++i) {
			m_layers[i] = &m_layer_content[i];
			m_layers[i]->init();
			m_layers[i]->set_id(i);
		}
		m_is_running = 0;
	}
	///////////////////////////////////////////////////////////////////////////////
	void clear() {
		for(int i=0; i<NUM_LAYERS; ++i) {
			m_layers[i]->clear();
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	void init_state() {
		for(int i=0; i<NUM_LAYERS; ++i) {
			m_layers[i] = &m_layer_content[i];
			m_layers[i]->set_id(i);
			m_layers[i]->init_state();
		}
		m_rec_layer = -1;

		m_rec.arm = V_SEQ_REC_ARM_OFF;
		m_rec.gate_state = CSequenceLayer::REC_GATE_STATE::REC_GATE_OFF;
		m_rec.mode = V_SEQ_REC_MODE_NONE;
		m_rec.is_clear = 0;
		m_rec.note = 0;
	}

	///////////////////////////////////////////////////////////////////////////////
	void set(PARAM_ID param, int value) {
		switch(param) {
		case P_SEQ_REC_ARM: m_rec.arm = (V_SEQ_REC_ARM)value; break;
		case P_SEQ_REC_MODE: m_rec.mode = (V_SEQ_REC_MODE)value; break;
		case P_SEQ_SCALE_TYPE: m_scale.set((V_SQL_SCALE_TYPE)value, m_scale.get_root()); break;
		case P_SEQ_SCALE_ROOT: m_scale.set(m_scale.get_type(), (V_SQL_SCALE_ROOT)value); break;
		default: break;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	int get(PARAM_ID param) {
		switch(param) {
		case P_SEQ_REC_ARM: return m_rec.arm;
		case P_SEQ_REC_MODE: return m_rec.mode;
		case P_SEQ_SCALE_TYPE: return m_scale.get_type();
		case P_SEQ_SCALE_ROOT: return m_scale.get_root();
		default:return 0;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	int is_valid_param(PARAM_ID param) {
		return 1;
	}

	///////////////////////////////////////////////////////////////////////////////
	CSequenceLayer& get_layer(int index) {
		return *m_layers[index];
	}

	///////////////////////////////////////////////////////////////////////////////
	void move_layer(byte from, byte to) {
		int i;
		CSequenceLayer *layer = m_layers[from];
		for(i=from; i<NUM_LAYERS-1; ++i) {
			m_layers[i] = m_layers[i+1];
		}
		for(i=NUM_LAYERS-1; i>to; --i) {
			m_layers[i] = m_layers[i-1];
		}
		m_layers[to] = layer;
		for(i=0; i<NUM_LAYERS; ++i) {
			m_layers[i]->set_id(i);
		}
	}


	///////////////////////////////////////////////////////////////////////////////
	void event(int event, uint32_t param) {
		switch(event) {
		case EV_SEQ_RESTART:
		case EV_SEQ_CONTINUE:
			m_is_running = 1;
			break;
		case EV_SEQ_STOP:
			m_is_running = 0;
			break;
		case EV_SAVE_OK:
			if(param>SLOT_CONFIG) {
				save_patch_complete(param);
			}
			break;
		case EV_LOAD_OK:
			if(param>SLOT_CONFIG) {
				load_patch_complete(param);
			}
			break;
		}
		for(int i=0; i<NUM_LAYERS; ++i) {
			m_layers[i]->event(event,param);
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	byte is_running() {
		return m_is_running;
	}

	///////////////////////////////////////////////////////////////////////////////
	void midi_note_on(int layer, byte note, byte vel, byte is_clear) {
		m_rec_layer = layer;
		m_rec.note = note;
		m_rec.is_clear = is_clear;
		m_rec.gate_state = vel? CSequenceLayer::REC_GATE_STATE::REC_GATE_TRIG: CSequenceLayer::REC_GATE_STATE::REC_GATE_ON;

		if(m_rec_layer >= 0 && m_rec.mode == V_SEQ_REC_MODE_TRANSPOSE) {
			if(note>=MIDI_TRANSPOSE_MIN && note<=MIDI_TRANSPOSE_MAX) {
				m_layers[layer]->set(P_SQL_CV_TRANSPOSE, (int)note-MIDI_TRANSPOSE_ZERO);
				fire_event(EV_REPAINT_MENU,0);
			}
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	void midi_note_off() {
		if(m_rec_layer >= 0 && m_rec.mode == V_SEQ_REC_MODE_TRANSPOSE && m_rec.arm != V_SEQ_REC_ARM_ON) {
			m_layers[m_rec_layer]->set(P_SQL_CV_TRANSPOSE, 0);
			fire_event(EV_REPAINT_MENU,0);
		}

		m_rec_layer = -1;
		m_rec.note = 0;
		m_rec.gate_state = CSequenceLayer::REC_GATE_STATE::REC_GATE_OFF;


	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	// called once per ms
	void run() {

		// ensure the sequencer is running
		byte played_step = 0;
		if(m_is_running) {

			// get a random dice roll for any random triggers
			// this is a number between 1 and 16
			int dice_roll = 1+rand()%16;

			// get the current clock tick count
			clock::TICKS_TYPE ticks = g_clock.get_ticks();

			int is_accented_step = 0;

			// call the play method for each layer to run
			// the scheduling of that layer's playback
			for(int i=0; i<NUM_LAYERS; ++i) {
				CSequenceLayer *layer = m_layers[i];
				is_accented_step |= layer->is_accented_step();
				if(m_rec_layer == i) {
					if(layer->play(ticks, dice_roll, &m_rec)) {
						played_step = 1;
					}
				}
				else {
					if(layer->play(ticks, dice_roll, NULL)) {
						played_step = 1;
					}
				}
			}

			g_clock.set_accent(is_accented_step);
		}

		// did any new step start playing?
		if(played_step) {

			// update each layer
			CV_TYPE prev_output = 0;
			for(int i=0; i<NUM_LAYERS; ++i) {
				CSequenceLayer *layer = m_layers[i];

				// update the voltage output of layer
				prev_output = layer->process_cv(prev_output);

				// if the layer has stepped, may need to trigger the gate
				if(layer->is_played_step()) {
					layer->process_gate();
					switch(layer->get_midi_out_mode()) {
					case V_SQL_MIDI_OUT_NOTE:
						layer->process_midi_note();
						break;
					case V_SQL_MIDI_OUT_CC:
						layer->process_midi_cc();
						break;
					}
				}
			}
		}

		// finally do once per ms housekeeping for layers
		for(int i=0; i<NUM_LAYERS; ++i) {
			m_layers[i]->run();
		}

	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	static int get_cfg_size() {
		return CScale::get_cfg_size() + NUM_LAYERS * CSequenceLayer::get_cfg_size();
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	void get_cfg(byte **dest) {
		m_scale.get_cfg(dest);
		for(int i=0; i<NUM_LAYERS; ++i) {
			m_layers[i]->get_cfg(dest);
		}
	}


	/////////////////////////////////////////////////////////////////////////////////////////////
	void set_cfg(byte **src) {
		m_scale.set_cfg(src);
		for(int i=0; i<NUM_LAYERS; ++i) {
			m_layers[i]->set_cfg(src);
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	byte save_patch(int slot) {
		byte *ptr = g_i2c_eeprom.buf();
		int len = get_cfg_size();
		*ptr++ = PATCH_DATA_COOKIE1;
		*ptr++ = PATCH_DATA_COOKIE2;
		get_cfg(&ptr);
		*ptr++ = g_i2c_eeprom.buf_checksum(len + 2);
		return g_i2c_eeprom.write(slot, len + 3);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	void save_patch_complete(int slot) {
		g_popup.text("SAVED");
	}
	/////////////////////////////////////////////////////////////////////////////////////////////
	byte load_patch(int slot) {
		return g_i2c_eeprom.read(slot, get_cfg_size() + 3);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	void load_patch_complete(int slot) {
		byte *buf = g_i2c_eeprom.buf();
		int len = get_cfg_size();
		byte checksum = g_i2c_eeprom.buf_checksum(len + 2);
		if(buf[0] == PATCH_DATA_COOKIE1 && buf[1] == PATCH_DATA_COOKIE2 && buf[len+2] == checksum) {
			buf+=2;
			set_cfg(&buf);
			init_state();
			switch(slot) {
			case SLOT_CONFIG:
			//case SLOT_AUTOSAVE:
				break;
			case SLOT_TEMPLATE:
				clear();
				g_popup.text("INIT");
				break;
			default:
				g_popup.text("LOADED");
				break;
			}
		}
		else {
			switch(slot) {
			case SLOT_CONFIG:
			//case SLOT_AUTOSAVE:
				break;
			default:
				g_popup.text("EMPTY");
				break;
			}
		}
		fire_event(EV_CLOCK_RESET,0);
	}

};

CSequence g_sequence;

#endif /* SEQUENCE_H_ */
