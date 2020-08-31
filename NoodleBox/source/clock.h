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
// BPM CLOCK MODULE
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
#pragma GCC diagnostic ignored "-Wswitch"

#ifndef CLOCK_H_
#define CLOCK_H_


// define the GPIO pins used for clock (will initialise the port)
#ifdef NB_PROTOTYPE
CDigitalIn g_clock_in(kGPIO_PORTA, 0);
CDigitalOut g_clock_out(kGPIO_PORTC, 5);
#else
CDigitalIn g_clock_in(kGPIO_PORTA, 0);
CDigitalOut g_clock_out(kGPIO_PORTD, 3);
CDigitalIn g_aux_in(kGPIO_PORTC, 7);
CDigitalOut g_aux_out(kGPIO_PORTC, 5);
#define BIT_AUXIN	MK_GPIOA_BIT(PORTC_BASE, 7)
#endif

// This namespace wraps up various clock based utilities
namespace clock {

enum {
	KBI0_BIT_CLOCKIN = (1<<0),
};
// Noodlebox uses the following type for handling "musical time"...
// TICKS_TYPE is a 32 bit unsigned value where there are 256 * 24ppqn = 6144 LSB
// increments per quarter note.
// ~699050 beats before rolling over (about 38h @ 300bpm, 97h @ 120bpm)
typedef uint32_t TICKS_TYPE;
enum {
	TICKS_INFINITY = (TICKS_TYPE)(-1)
};

// Define different musical beat intervals based on number of 24ppqn ticks
enum
{
  PP24_1    	= 96,
  PP24_2D   	= 72,
  PP24_2    	= 48,
  PP24_4D   	= 36,
  PP24_2T   	= 32,
  PP24_4    	= 24,
  PP24_8D   	= 18,
  PP24_4T   	= 16,
  PP24_8    	= 12,
  PP24_16D  	= 9,
  PP24_8T   	= 8,
  PP24_16   	= 6,
  PP24_16T  	= 4,
  PP24_32   	= 3,
  PP24_24PPQN	= 1
};

///////////////////////////////////////////////////////////////////////////////
// Conversion from TICKS_TYPE to PP24
inline int ticks_to_pp24(TICKS_TYPE ticks) {
	return ticks>>8;
}

///////////////////////////////////////////////////////////////////////////////
// Conversion from PP24 to TICKS_TYPE
inline TICKS_TYPE pp24_to_ticks(int pp24) {
	return ((TICKS_TYPE)pp24)<<8;
}

///////////////////////////////////////////////////////////////////////////////
// Conversion from V_SQL_STEP_RATE to PP24
inline int pp24_per_measure(V_SQL_STEP_RATE step_rate) {
	const byte pp24[V_SQL_STEP_RATE_MAX] = {
		PP24_1,
		PP24_2D,
		PP24_2,
		PP24_4D,
		PP24_2T,
		PP24_4,
		PP24_8D,
		PP24_4T,
		PP24_8,
		PP24_16D,
		PP24_8T,
		PP24_16,
		PP24_16T,
		PP24_32
	};
	return pp24[step_rate];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface to be implemented by clock sources
class IClockSource {
public:
	virtual void event(int event, uint32_t param) = 0;
	virtual TICKS_TYPE min_ticks() = 0;
	virtual TICKS_TYPE max_ticks() = 0;
	virtual double ticks_per_ms() = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clock source to handle an incoming pulse clock
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CPulseClockSource : public IClockSource {
	typedef struct {
		V_CLOCK_IN_RATE m_clock_in_rate;
	} CONFIG;
	CONFIG m_cfg;

	TICKS_TYPE m_ticks; 	// tick counter incremented by m_period when external clock pulse is received
	TICKS_TYPE m_period;	// the period of the clock in whole ticks
	double m_ticks_per_ms;  // calculated tick rate from ext clock

	uint32_t m_last_ms;		// used to time between incoming pulses
	uint32_t m_timeout;		// used to decide when the external clock has stopped
	byte m_state;
	enum {
		CLOCK_UNKNOWN,
		CLOCK_STOPPED,
		CLOCK_RUNNING
	};

public:
	////////////////////////////////////////
	CPulseClockSource() {
		set_rate(V_CLOCK_IN_RATE_16);
	}
	////////////////////////////////////////
	void event(int event, uint32_t param) {
		switch(event) {
		case EV_CLOCK_RESET:
			m_state = CLOCK_UNKNOWN;
			m_ticks_per_ms = 0;
			m_last_ms = 0;
			// fallthru
		case EV_SEQ_RESTART:
			m_ticks = 0;
			break;
		case EV_REAPPLY_CONFIG:
			set_rate(m_cfg.m_clock_in_rate);
			break;
		}
	}
	////////////////////////////////////////
	TICKS_TYPE min_ticks() {
		return m_ticks;
	}
	////////////////////////////////////////
	TICKS_TYPE max_ticks() {
		return m_ticks + m_period;
	}
	////////////////////////////////////////
	double ticks_per_ms() {
		return m_ticks_per_ms;
	}
	////////////////////////////////////////
	void set_rate(V_CLOCK_IN_RATE clock_in_rate) {
		const byte rate[V_CLOCK_IN_RATE_MAX] = {PP24_8, PP24_16, PP24_32, PP24_24PPQN};
		m_cfg.m_clock_in_rate = clock_in_rate;
		m_period = pp24_to_ticks(rate[clock_in_rate]);
	}
	////////////////////////////////////////
	V_CLOCK_IN_RATE get_rate() {
		return m_cfg.m_clock_in_rate;
	}
	////////////////////////////////////////
	void on_pulse(uint32_t ms) {
		if(CLOCK_RUNNING == m_state) {
			// check we have not rolled over
			if(ms > m_last_ms) {
				uint32_t elapsed_ms = (ms - m_last_ms);
				m_ticks_per_ms = (double)m_period / elapsed_ms;
				m_timeout = ms + 4 * elapsed_ms;
			}
		}
		else {
			m_timeout = 0;
			m_state = CLOCK_RUNNING;
			fire_event(EV_SEQ_CONTINUE,0);
		}
		m_last_ms = ms;
		m_ticks += m_period;


	}
	////////////////////////////////////////
	void run(uint32_t ms) {
		if(CLOCK_RUNNING == m_state && !!m_timeout && ms > m_timeout) {
			m_timeout = 0;
			m_state = CLOCK_STOPPED;
			fire_event(EV_SEQ_STOP,0);
		}
	}
	static int get_cfg_size() {
		return sizeof(m_cfg);
	}
	void get_cfg(byte **dest) {
		memcpy(*dest, &m_cfg, sizeof m_cfg);
		(*dest)+=get_cfg_size();
	}
	void set_cfg(byte **src) {
		memcpy(&m_cfg, *src, sizeof m_cfg);
		(*src)+=get_cfg_size();
	}
};
CPulseClockSource g_pulse_clock_in;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clock source to handle an incoming midi clock
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMidiClockSource 	: public IClockSource {
	TICKS_TYPE m_ticks; 	// tick counter incremented by m_period when external clock pulse is received
	double m_ticks_per_ms;  // calculated tick rate from ext clock
	uint32_t m_last_ms;		// used to time between incoming pulses
	int m_transport:1;		// whether we should act on MIDI transport messages
	enum : byte { PENDING_NONE, PENDING_RESTART, PENDING_CONTINUE } m_pending_event;
	const TICKS_TYPE MIDI_CLOCK_RATE_TICKS = (1<<8);
public:
	///////////////////////////////////////////////////////////////////////////////
	CMidiClockSource() {
		m_transport = 1;
		m_pending_event = PENDING_NONE;
		m_ticks_per_ms = 0;
		m_last_ms = 0;
		m_ticks = 0;
	}
	///////////////////////////////////////////////////////////////////////////////
	void set_transport(byte transport) {
		m_transport = transport;
	}
	///////////////////////////////////////////////////////////////////////////////
	void event(int event, uint32_t param) {
		switch(event) {
		case EV_CLOCK_RESET:
			m_ticks_per_ms = 0;
			m_last_ms = 0;
			// fall thru
		case EV_SEQ_RESTART:
			m_ticks = 0;
			break;
		}
	}
	///////////////////////////////////////////////////////////////////////////////
	TICKS_TYPE min_ticks() {
		return m_ticks;
	};
	///////////////////////////////////////////////////////////////////////////////
	TICKS_TYPE max_ticks() {
		return m_ticks + MIDI_CLOCK_RATE_TICKS;
	};
	///////////////////////////////////////////////////////////////////////////////
	double ticks_per_ms() {
		return m_ticks_per_ms;
	};
	///////////////////////////////////////////////////////////////////////////////
	void on_midi_realtime(byte ch, uint32_t ms) {
		switch(ch) {
		case midi::MIDI_TICK:
			if(PENDING_RESTART != m_pending_event) { // the first tick after a MIDI restart is ignored
				m_ticks += MIDI_CLOCK_RATE_TICKS;
				if(PENDING_CONTINUE != m_pending_event) {
					if(ms > m_last_ms) {
						uint32_t elapsed_ms = (ms - m_last_ms);
						m_ticks_per_ms = (double)MIDI_CLOCK_RATE_TICKS / elapsed_ms;
					}
				}
				m_last_ms = ms;
			}
			m_pending_event = PENDING_NONE;
			break;
		case midi::MIDI_START:
			if(m_transport) {
				fire_event(EV_SEQ_RESTART,0);
				m_pending_event = PENDING_RESTART;
			}
			break;
		case midi::MIDI_CONTINUE:
			if(m_transport) {
				fire_event(EV_SEQ_CONTINUE,0);
				m_pending_event = PENDING_CONTINUE;
			}
			break;
		case midi::MIDI_STOP:
			if(m_transport) {
				fire_event(EV_SEQ_STOP,0);
				m_pending_event = PENDING_NONE;
			}
			break;
		}
	}
};
CMidiClockSource g_midi_clock_in;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Clock source to provide fixed clock info based on a specified BPM
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CFixedClockSource	: public IClockSource {
	friend class CClock;
	typedef struct {
		int m_bpm;
	} CONFIG;
	CONFIG m_cfg;

	double m_ticks_per_ms;
public:
	////////////////////////////////////////
	CFixedClockSource() {
		set_bpm(120);
	}
	////////////////////////////////////////
	void event(int event, uint32_t param) {
		switch(event) {
		case EV_REAPPLY_CONFIG:
			set_bpm(m_cfg.m_bpm);
			break;
		}
	}
	////////////////////////////////////////
	TICKS_TYPE min_ticks() {
		return 0;
	}
	////////////////////////////////////////
	TICKS_TYPE max_ticks() {
		return TICKS_INFINITY;
	}
	////////////////////////////////////////
	double ticks_per_ms() {
		return m_ticks_per_ms;
	}
	////////////////////////////////////////
	void set_bpm(int bpm) {
		m_ticks_per_ms = ((double)bpm * PP24_4 * 256) / (60.0 * 1000.0);
		m_cfg.m_bpm = bpm;
	}
	////////////////////////////////////////
	int get_bpm() {
		return m_cfg.m_bpm;
	}
	static int get_cfg_size() {
		return sizeof(m_cfg);
	}
	void get_cfg(byte **dest) {
		memcpy(*dest, &m_cfg, sizeof m_cfg);
		(*dest)+=get_cfg_size();
	}
	void set_cfg(byte **src) {
		memcpy(&m_cfg, *src, sizeof m_cfg);
		(*src)+=get_cfg_size();
	}
};
CFixedClockSource g_fixed_clock;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This class is used for managing the pulse clock output
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CPulseClockOut {
	friend class CClock;
	typedef struct {
		V_CLOCK_OUT_MODE m_clock_out_mode;
		V_CLOCK_OUT_RATE m_clock_out_rate;
	} CONFIG;
	CONFIG m_cfg;

	const int HIGH_MS = 15;				// duration of high part of pulse
	const int HIGH_MS_24PPQN = 5;		// duration of high part of pulse in 24ppqn mode
	const int LOW_MS = 2;				// MIN low time between pulses

	enum { ST_IDLE, ST_HIGH, ST_LOW } m_state; // pulse state
	int m_timeout;				// time to remain in state
	int m_pulses;				// number of pulses remaining to send
	int m_period;
	int m_running:1;
	CDigitalOut& m_out;

public:
	///////////////////////////////////////////////////////////////////////////////
	CPulseClockOut(CDigitalOut& out) : m_out(out) {
		m_timeout = 0;
		m_pulses = 0;
		m_period = 0;
		m_running = 0;
		set_rate(V_CLOCK_OUT_RATE_16);
	}
	///////////////////////////////////////////////////////////////////////////////
	void set_mode(V_CLOCK_OUT_MODE clock_out_mode) {
		if(V_CLOCK_OUT_MODE_RUNNING == clock_out_mode) {
			m_out.set(m_running);
			m_state = ST_IDLE;
			m_timeout = 0;
			m_pulses = 0;
		}
		else if(V_CLOCK_OUT_MODE_RUNNING == m_cfg.m_clock_out_mode) {
			m_out.set(0);
		}
		m_cfg.m_clock_out_mode = clock_out_mode;
	}
	///////////////////////////////////////////////////////////////////////////////
	V_CLOCK_OUT_MODE get_mode() {
		return m_cfg.m_clock_out_mode;
	}
	///////////////////////////////////////////////////////////////////////////////
	void set_rate(V_CLOCK_OUT_RATE clock_out_rate) {
		const byte rate[V_CLOCK_OUT_RATE_MAX] = { PP24_8, PP24_16, PP24_32, PP24_24PPQN };
		m_cfg.m_clock_out_rate = clock_out_rate;
		m_period = rate[clock_out_rate];
	}
	///////////////////////////////////////////////////////////////////////////////
	V_CLOCK_OUT_RATE get_rate() {
		return m_cfg.m_clock_out_rate;
	}
	///////////////////////////////////////////////////////////////////////////////
	inline void pulse() {
		// can we do it immediately?
		if(ST_IDLE == m_state) {
			m_out.set(1);
			m_state = ST_HIGH;
			m_timeout =
				(m_cfg.m_clock_out_rate == V_CLOCK_OUT_RATE_24PP)?
				HIGH_MS_24PPQN : HIGH_MS;
		}
		else {
			// queue it up
			++m_pulses;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	void event(int event, uint32_t param) {
		switch(event) {
		///////////////////////////////////////
		case EV_CLOCK_RESET:
			switch(m_cfg.m_clock_out_mode) {
			case V_CLOCK_OUT_MODE_RUNNING:
				break;
			default:
				m_out.set(0);
				m_state = ST_IDLE;
				m_timeout = 0;
				m_pulses = 0;
				break;
			}
			break;
			///////////////////////////////////////
		case EV_SEQ_RESTART:
			switch(m_cfg.m_clock_out_mode) {
			case V_CLOCK_OUT_MODE_START:
			case V_CLOCK_OUT_MODE_START_STOP:
			case V_CLOCK_OUT_MODE_RESET:
				pulse();
				break;
			case V_CLOCK_OUT_MODE_RUNNING:
				m_out.set(1);
				break;
			case V_CLOCK_OUT_MODE_ACCENT:
				m_out.set(0);
				break;
			case V_CLOCK_OUT_MODE_GATED_CLOCK:
			case V_CLOCK_OUT_MODE_CLOCK:
				if(m_running || V_CLOCK_OUT_MODE_CLOCK == m_cfg.m_clock_out_mode) {
					// if the clock output is currently running, we'll force a low period
					// and slightly delay the first clock pulse after a reset. This ensures
					// that that first pulse has a clean rising edge and also ensures that
					// reset output is high at the time (if applicable)
					m_out.set(0);
					m_state = ST_LOW;
					m_timeout = LOW_MS;
					pulse();
				}
				break;
			}
			m_running = 1;
			break;
			///////////////////////////////////////
		case EV_SEQ_STOP:
			switch(m_cfg.m_clock_out_mode) {
			case V_CLOCK_OUT_MODE_STOP:
			case V_CLOCK_OUT_MODE_START_STOP:
				pulse();
				break;
			case V_CLOCK_OUT_MODE_RUNNING:
			case V_CLOCK_OUT_MODE_ACCENT:
				m_out.set(0);
				break;
			}
			m_running = 0;
			break;
			///////////////////////////////////////
		case EV_SEQ_CONTINUE:
			switch(m_cfg.m_clock_out_mode) {
			case V_CLOCK_OUT_MODE_START:
			case V_CLOCK_OUT_MODE_START_STOP:
				pulse();
				break;
			case V_CLOCK_OUT_MODE_RUNNING:
				m_out.set(1);
				break;
			}
			m_running = 1;
			break;
			///////////////////////////////////////
		case EV_REAPPLY_CONFIG:
			set_rate(m_cfg.m_clock_out_rate);
			set_mode(m_cfg.m_clock_out_mode);
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	void set_accent(int value) {
		if(m_cfg.m_clock_out_mode == V_CLOCK_OUT_MODE_ACCENT) {
			m_out.set(value);
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	inline void on_pp24(int pp24) {
		// check if we need to send a clock out pulse
		ASSERT(m_period);
		if(m_cfg.m_clock_out_mode == V_CLOCK_OUT_MODE_CLOCK ||
			(m_cfg.m_clock_out_mode == V_CLOCK_OUT_MODE_GATED_CLOCK && m_running))	{
			if(!(pp24%m_period)) {
				pulse();
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////////
	inline void run() {
		// manage the clock output
		switch(m_state) {
		case ST_LOW:	// forced low state after a pulse
			if(!--m_timeout) { // see if end of low phase
				if(m_pulses) { // do we need another pulse?
					m_out.set(1);
					--m_pulses;
					m_state = ST_HIGH;
					m_timeout = HIGH_MS;
				}
				else { // nope
					m_state = ST_IDLE;
				}
			}
			break;
		case ST_HIGH:
			if(!--m_timeout) {
				m_out.set(0);
				m_state = ST_LOW;
				m_timeout = LOW_MS;
			}
			break;
		}
	}
	static int get_cfg_size() {
		return sizeof(m_cfg);
	}
	void get_cfg(byte **dest) {
		memcpy(*dest, &m_cfg, sizeof m_cfg);
		(*dest)+=get_cfg_size();
	}
	void set_cfg(byte **src) {
		memcpy(&m_cfg, *src, sizeof m_cfg);
		(*src)+=get_cfg_size();
	}
};
CPulseClockOut g_pulse_clock_out(g_clock_out);
#ifndef NB_PROTOTYPE
	CPulseClockOut g_pulse_aux_out(g_aux_out);
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This class is used for managing the MIDI clock output
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CMidiClockOut {
	friend class CClock;
	typedef struct {
		V_MIDI_CLOCK_OUT m_mode;
	} CONFIG;
	CONFIG m_cfg;

	byte m_is_running;
public:
	///////////////////////////////////////////////////////////////////////////////
	CMidiClockOut() {
		set_mode(V_MIDI_CLOCK_OUT_NONE);
		m_is_running = 0;
	}
	///////////////////////////////////////////////////////////////////////////////
	void set_mode(V_MIDI_CLOCK_OUT mode) {
		m_cfg.m_mode = mode;
	}
	///////////////////////////////////////////////////////////////////////////////
	V_MIDI_CLOCK_OUT get_mode() {
		return m_cfg.m_mode;
	}
	///////////////////////////////////////////////////////////////////////////////
	void event(int event, uint32_t param) {
		switch(event) {
		case EV_SEQ_RESTART:
			switch(m_cfg.m_mode) {
			case V_MIDI_CLOCK_OUT_ON_TRAN:
			case V_MIDI_CLOCK_OUT_GATE_TRAN:
				g_midi.send_byte(midi::MIDI_START);
				break;
			}
			m_is_running = 1;
			break;
		case EV_SEQ_STOP:
			switch(m_cfg.m_mode) {
			case V_MIDI_CLOCK_OUT_ON_TRAN:
			case V_MIDI_CLOCK_OUT_GATE_TRAN:
				g_midi.send_byte(midi::MIDI_STOP);
				break;
			}
			m_is_running = 0;
			break;
		case EV_SEQ_CONTINUE:
			switch(m_cfg.m_mode) {
			case V_MIDI_CLOCK_OUT_ON_TRAN:
			case V_MIDI_CLOCK_OUT_GATE_TRAN:
				g_midi.send_byte(midi::MIDI_CONTINUE);
				break;
			}
			m_is_running = 1;
			break;
		case EV_CLOCK_RESET:
			break;
		case EV_REAPPLY_CONFIG:
			set_mode(m_cfg.m_mode);
			break;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	inline void on_pp24() {
		switch(m_cfg.m_mode) {
		case V_MIDI_CLOCK_OUT_GATE:
		case V_MIDI_CLOCK_OUT_GATE_TRAN:
			if(!m_is_running) {
				break;
			}
			// else fall thru
		case V_MIDI_CLOCK_OUT_ON_TRAN:
		case V_MIDI_CLOCK_OUT_ON:
			g_midi.send_byte(midi::MIDI_TICK);
			break;
		}
	}
	static int get_cfg_size() {
		return sizeof(m_cfg);
	}
	void get_cfg(byte **dest) {
		memcpy(*dest, &m_cfg, sizeof m_cfg);
		(*dest)+=get_cfg_size();
	}
	void set_cfg(byte **src) {
		memcpy(&m_cfg, *src, sizeof m_cfg);
		(*src)+=get_cfg_size();
	}
};
CMidiClockOut g_midi_clock_out;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This class is used for blinking the beat LED
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CBeatLedOut {
public:
	inline void on_pp24(int pp24) {
		if(!(pp24%PP24_4)) {
			g_tempo_led.blink(g_tempo_led.MEDIUM_BLINK);
		}
	}

};
CBeatLedOut g_beat_led_out;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This class implements the clock for the sequencer functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CClock {
	typedef struct {
		V_CLOCK_SRC m_source_mode;
		V_AUX_IN_MODE m_aux_in_mode;
	} CONFIG;
	CONFIG m_cfg;

	IClockSource *m_source;
	volatile byte m_ms_tick;					// flag set each time 1ms is up
	volatile uint32_t m_ms;					// ms counter
	volatile TICKS_TYPE m_ticks;
	volatile double m_ticks_remainder;
	byte m_aux_in_state;


	///////////////////////////////////////////////////////////////////////////////
	void set_source_mode(V_CLOCK_SRC source_mode) {
		switch(source_mode) {
		case V_CLOCK_SRC_EXTERNAL:
			m_source = &g_pulse_clock_in;
			break;
		case V_CLOCK_SRC_MIDI_CLOCK_ONLY:
			m_source = &g_midi_clock_in;
			g_midi_clock_in.set_transport(0);
			break;
		case V_CLOCK_SRC_MIDI_TRANSPORT:
			m_source = &g_midi_clock_in;
			g_midi_clock_in.set_transport(1);
			break;
		case V_CLOCK_SRC_INTERNAL:
		default:
			m_source = &g_fixed_clock;
			break;
		}
		m_cfg.m_source_mode = source_mode;
	}

public:
	///////////////////////////////////////////////////////////////////////////////
	CClock() : m_source(&g_fixed_clock) {
		init_config();
		init_state();
	}

	///////////////////////////////////////////////////////////////////////////////
	void init() {
		// configure a timer to cause an interrupt once per millisecond
		CLOCK_EnableClock(kCLOCK_Pit0);
		pit_config_t timerConfig = {
		 .enableRunInDebug = true,
		};
		PIT_Init(PIT, &timerConfig);
		EnableIRQ(PIT_CH0_IRQn);
		PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);
		PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, (uint32_t) MSEC_TO_COUNT(1, CLOCK_GetBusClkFreq()));
		PIT_StartTimer(PIT, kPIT_Chnl_0);

		// configure the KBI peripheral to cause an interrupt when sync pulse in is triggered
		kbi_config_t kbiConfig;
		kbiConfig.mode = kKBI_EdgesDetect;
		kbiConfig.pinsEnabled = KBI0_BIT_CLOCKIN;
		kbiConfig.pinsEdge = 0; // Falling Edge (after Schmitt Trigger inverter on input)
		KBI_Init(KBI0, &kbiConfig);
	}

	///////////////////////////////////////////////////////////////////////////////
	void init_state() {
		m_ms = 0;
		m_ms_tick = 0;
		m_ticks = 0;
		m_ticks_remainder = 0;
		m_aux_in_state = 0;
	}

	///////////////////////////////////////////////////////////////////////////////
	void init_config() {
	}

	///////////////////////////////////////////////////////////////////////////////
	void set(PARAM_ID param, int value) {
		switch(param) {
			case P_CLOCK_BPM:
				g_fixed_clock.set_bpm(value);
				break;
			case P_CLOCK_SRC:
				set_source_mode((V_CLOCK_SRC)value);
				fire_event(EV_CLOCK_RESET, 0);
				break;
			case P_AUX_IN_MODE:
				m_cfg.m_aux_in_mode = (V_AUX_IN_MODE)value;
				break;
			case P_CLOCK_IN_RATE:
				g_pulse_clock_in.set_rate((V_CLOCK_IN_RATE)value);
				fire_event(EV_CLOCK_RESET, 0);
				break;
			case P_CLOCK_OUT_MODE:
				g_pulse_clock_out.set_mode((V_CLOCK_OUT_MODE)value);
				fire_event(EV_CLOCK_RESET, 0);
				break;
			case P_CLOCK_OUT_RATE:
				g_pulse_clock_out.set_rate((V_CLOCK_OUT_RATE)value);
				fire_event(EV_CLOCK_RESET, 0);
				break;
#ifndef NB_PROTOTYPE
			case P_AUX_OUT_MODE:
				g_pulse_aux_out.set_mode((V_CLOCK_OUT_MODE)value);
				fire_event(EV_CLOCK_RESET, 0);
				break;
			case P_AUX_OUT_RATE:
				g_pulse_aux_out.set_rate((V_CLOCK_OUT_RATE)value);
				fire_event(EV_CLOCK_RESET, 0);
				break;
#endif
			case P_MIDI_CLOCK_OUT:
				g_midi_clock_out.set_mode((V_MIDI_CLOCK_OUT)value);
				break;
		default:
			break;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	int get(PARAM_ID param) {
		switch(param) {
		case P_CLOCK_BPM: return g_fixed_clock.get_bpm();
		case P_CLOCK_SRC: return m_cfg.m_source_mode;
		case P_AUX_IN_MODE: return m_cfg.m_aux_in_mode;
		case P_CLOCK_IN_RATE: return g_pulse_clock_in.get_rate();
		case P_CLOCK_OUT_MODE: return g_pulse_clock_out.get_mode();
		case P_CLOCK_OUT_RATE: return g_pulse_clock_out.get_rate();
#ifndef NB_PROTOTYPE
		case P_AUX_OUT_MODE: return g_pulse_aux_out.get_mode();
		case P_AUX_OUT_RATE: return g_pulse_aux_out.get_rate();
#endif
		case P_MIDI_CLOCK_OUT: return g_midi_clock_out.get_mode();
		default: return 0;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	int is_valid_param(PARAM_ID param) {
		switch(param) {
		case P_CLOCK_BPM: return !!(m_cfg.m_source_mode == V_CLOCK_SRC_INTERNAL);
		case P_CLOCK_IN_RATE: return !!(m_cfg.m_source_mode == V_CLOCK_SRC_EXTERNAL);
		case P_CLOCK_OUT_RATE: return !!(g_pulse_clock_out.get_mode() == V_CLOCK_OUT_MODE_CLOCK || g_pulse_clock_out.get_mode() == V_CLOCK_OUT_MODE_GATED_CLOCK);
#ifndef NB_PROTOTYPE
		case P_AUX_OUT_RATE: return !!(g_pulse_aux_out.get_mode() == V_CLOCK_OUT_MODE_CLOCK || g_pulse_aux_out.get_mode() == V_CLOCK_OUT_MODE_GATED_CLOCK);
#else
		case P_AUX_IN_MODE: return 0;
		case P_AUX_OUT_MODE: return 0;
		case P_AUX_OUT_RATE: return 0;
#endif
		default: return 1;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	// handle sequencer events
	void event(int event, uint32_t param) {
		switch(event) {
		case EV_SEQ_STOP:
		case EV_SEQ_CONTINUE:
			break;
		case EV_CLOCK_RESET:
		case EV_SEQ_RESTART:
			m_ticks = 0;
			m_ticks_remainder = 0;
			break;
		case EV_REAPPLY_CONFIG:
			set_source_mode(m_cfg.m_source_mode);
			break;
		}
		g_fixed_clock.event(event, param);
		g_pulse_clock_in.event(event, param);
		g_pulse_clock_out.event(event, param);
#ifndef NB_PROTOTYPE
		g_pulse_aux_out.event(event, param);
#endif
		g_midi_clock_in.event(event, param);
		g_midi_clock_out.event(event, param);
	}

	///////////////////////////////////////////////////////////////////////////////
	// Set accent gates if according to mode
	void set_accent(int value) {
		g_pulse_clock_out.set_accent(value);
#ifndef NB_PROTOTYPE
		g_pulse_aux_out.set_accent(value);
#endif
	}

	///////////////////////////////////////////////////////////////////////////////
	// Return incrementing number of ticks (256 per 24PPQN period)
	inline TICKS_TYPE get_ticks() {
		return m_ticks;
	}

	///////////////////////////////////////////////////////////////////////////////
	// Return incrementing number of ms
	inline uint32_t get_ms() {
		return m_ms;
	}

	///////////////////////////////////////////////////////////////////////////////
	// Checks if its been 1ms since previous call
	inline byte is_ms_tick() {
		if(m_ms_tick) {
			m_ms_tick = 0;
			return 1;
		}
		return 0;
	}

	///////////////////////////////////////////////////////////////////////////////
	// utility function to wait a number of ms (blocking)
	void wait_ms(int ms) {
		while(ms) {
			m_ms_tick = 0;
			while(!m_ms_tick);
			--ms;
		}
	}

	///////////////////////////////////////////////////////////////////////////////
	// Based on current clock rate, convert PP24 to ms
	inline int get_ms_for_pp24(int pp24) {
		return (int)(pp24_to_ticks(pp24)/m_source->ticks_per_ms());
	}

	///////////////////////////////////////////////////////////////////////////////
	// Based on current clock rate, convert V_SQL_STEP_RATE to ms
	inline int get_ms_per_measure(V_SQL_STEP_RATE step_rate) {
		return get_ms_for_pp24(pp24_per_measure(step_rate));
	}

	///////////////////////////////////////////////////////////////////////////////
	// Method called approx once per ms
	void run() {
		g_pulse_clock_in.run(m_ms);
		g_pulse_clock_out.run();
#ifndef NB_PROTOTYPE
		g_pulse_aux_out.run();
#endif
	}

	///////////////////////////////////////////////////////////////////////////////
	// method called as fast as poss
	inline void poll_aux_in() {
#ifndef NB_PROTOTYPE
		if(m_aux_in_state) {
			if(!READ_GPIOA(BIT_AUXIN)) { // falling edge
				switch(m_cfg.m_aux_in_mode) {
				case V_AUX_IN_MODE_RUN_STOP:
					fire_event(EV_SEQ_RUN_STOP, 0);
					break;
				case V_AUX_IN_MODE_CONT:
					fire_event(EV_SEQ_CONTINUE, 0);
					break;
				case V_AUX_IN_MODE_RESTART:
					fire_event(EV_SEQ_RESTART, 0);
					break;
				}
				m_aux_in_state = 0;
			}
		}
		else {
			if(READ_GPIOA(BIT_AUXIN)) {
				m_aux_in_state = 1;
			}
		}
#endif
	}

	///////////////////////////////////////////////////////////////////////////////
	// Interrupt service routine called exactly once per millisecond
	void per_ms_isr() {

		// maintain a millisecond counter for general
		// timing purposes
		++m_ms;

		// set a tick flag which is used for general
		// ting purposes
		m_ms_tick = 1;

		TICKS_TYPE prev_ticks = m_ticks;

		// update the tick counter used for scheduling the sequencer
		if(m_ticks < m_source->min_ticks()) {
			m_ticks = m_source->min_ticks();
			m_ticks_remainder = 0;
		}
		else {
			// count the appropriate number of ticks for this millisecond
			// but don't yet save the values
			double ticks_remainder = m_ticks_remainder + m_source->ticks_per_ms();
			int whole_ticks = (int)ticks_remainder;
			ticks_remainder -= whole_ticks;
			TICKS_TYPE ticks = m_ticks + whole_ticks;

			if(ticks < m_source->max_ticks()) {
				m_ticks = ticks;
				m_ticks_remainder = ticks_remainder;
			}
		}

		// check for a rollover into the next 24PPQN tick
		if((m_ticks ^ prev_ticks)&~0xFF) {
			int pp24 = m_ticks>>8;
			g_pulse_clock_out.on_pp24(pp24);
#ifndef NB_PROTOTYPE
			g_pulse_aux_out.on_pp24(pp24);
#endif
			g_midi_clock_out.on_pp24();
			g_beat_led_out.on_pp24(pp24);
		}

	}
	inline void ext_clock_isr() {
		g_pulse_clock_in.on_pulse(m_ms);
	}

	static int get_cfg_size() {
		return sizeof(m_cfg) +
			CFixedClockSource::get_cfg_size() +
			CPulseClockSource::get_cfg_size() +
			CPulseClockOut::get_cfg_size() +
#ifndef NB_PROTOTYPE
			CPulseClockOut::get_cfg_size() +
#endif
			CMidiClockOut::get_cfg_size();
	}
	void get_cfg(byte **dest) {
		memcpy(*dest, &m_cfg, sizeof m_cfg);
		(*dest)+=sizeof(m_cfg);
		g_fixed_clock.get_cfg(dest);
		g_pulse_clock_in.get_cfg(dest);
		g_pulse_clock_out.get_cfg(dest);
#ifndef NB_PROTOTYPE
		g_pulse_aux_out.get_cfg(dest);
#endif
		g_midi_clock_out.get_cfg(dest);
	}
	void set_cfg(byte **src) {
		memcpy(&m_cfg, *src, sizeof m_cfg);
		(*src)+=sizeof(m_cfg);
		g_fixed_clock.set_cfg(src);
		g_pulse_clock_in.set_cfg(src);
		g_pulse_clock_out.set_cfg(src);
#ifndef NB_PROTOTYPE
		g_pulse_aux_out.set_cfg(src);
#endif
		g_midi_clock_out.set_cfg(src);
	}


};


}; // namespace

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// define the clock instance
clock::CClock g_clock;

// ISR for the millisecond timer
extern "C" void PIT_CH0_IRQHandler(void) {
	PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);
	g_clock.per_ms_isr();
}

// ISR for the KBI interrupt (SYNC IN)
extern "C" void KBI0_IRQHandler(void)
{
    if (KBI_IsInterruptRequestDetected(KBI0)) {
        uint32_t keys = KBI_GetSourcePinStatus(KBI0);
        KBI_ClearInterruptFlag(KBI0);
        if(keys & clock::KBI0_BIT_CLOCKIN) {
        	g_clock.ext_clock_isr();
        }
    }
}

#endif // CLOCK_H_
