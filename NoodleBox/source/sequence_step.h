///////////////////////////////////////////////////////////////////////////////////
//
//                                  ~~  ~~             ~~
//  ~~~~~~    ~~~~~    ~~~~~    ~~~~~~  ~~     ~~~~~   ~~~~~~    ~~~~~   ~~    ~~
//  ~~   ~~  ~~   ~~  ~~   ~~  ~~   ~~  ~~    ~~   ~~  ~~   ~~  ~~   ~~   ~~  ~~
//  ~~   ~~  ~~   ~~  ~~   ~~  ~~   ~~  ~~    ~~~~~~~  ~~   ~~  ~~   ~~     ~~
//  ~~   ~~  ~~   ~~  ~~   ~~  ~~   ~~  ~~    ~~       ~~   ~~  ~~   ~~   ~~  ~~
//  ~~   ~~   ~~~~~    ~~~~~    ~~~~~~   ~~~   ~~~~~   ~~~~~~    ~~~~~   ~~    ~~
//
//  Serendipity Sequencer                                   CC-NC-BY-SA
//  hotchk155/2018                                          Sixty-four pixels ltd
//
//  SEQUENCER STEP
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef SEQUENCE_STEP_H_
#define SEQUENCE_STEP_H_

// this class represents a single step in the sequence, both "data points" (as entered
// by the user) and any automatically generated points. It also stores the associated
// gate information
//
// the user enters "data points" and other values are extrapolated depending on the
// layer type
//
class CSequenceStep {
	byte m_gate:1;
	byte m_tie:1;
	byte m_prob:2;			// probabitity 0=Always, 1,2,3 = high medium low
	byte m_retrig:2;			// Retrig 0=Never, 1,2,3 = high medium low
	byte m_is_data_point:1; // is the CV value user defined rather than auto filled
	byte m_value;	// CV value
public:
	enum {
		PROB_OFF,
		PROB_HIGH,
		PROB_MED,
		PROB_LOW,
		PROB_MAX = PROB_LOW
	};
	enum {
		RETRIG_OFF,
		RETRIG_SLOW,
		RETRIG_MED,
		RETRIG_FAST,
		RETRIG_MAX = RETRIG_FAST
	};

	///////////////////////////////////////////////////////////////////////////////////
	inline byte get_value() {
		return m_value;
	}

	///////////////////////////////////////////////////////////////////////////////////
	inline void set_value(byte value) {
		m_value = value;
	}

	///////////////////////////////////////////////////////////////////////////////////
	inline byte is_data_point() {
		return m_is_data_point;
	}

	///////////////////////////////////////////////////////////////////////////////////
	inline void set_data_point(byte value) {
		m_is_data_point = !!value;
	}

	///////////////////////////////////////////////////////////////////////////////////
	inline void set_gate(int gate) {
		m_gate = !!gate;
	}

	///////////////////////////////////////////////////////////////////////////////////
	inline int is_gate() {
		return m_gate;
	}

	///////////////////////////////////////////////////////////////////////////////////
	inline byte is_tied() {
		return m_tie;
	}

	///////////////////////////////////////////////////////////////////////////////////
	inline byte get_prob() {
		return m_prob;
	}

	///////////////////////////////////////////////////////////////////////////////////
	inline byte get_retrig() {
		return m_retrig;
	}

	///////////////////////////////////////////////////////////////////////////////////
	void copy_data_point(CSequenceStep &other) {
		m_value = other.m_value;
		m_is_data_point = other.m_is_data_point;
	}

	///////////////////////////////////////////////////////////////////////////////////
	void copy_gate(CSequenceStep &other) {
		m_gate = other.m_gate;
		m_tie = other.m_tie;
		m_prob = other.m_prob;
	}

	///////////////////////////////////////////////////////////////////////////////////
	// clear data point and gates
	void clear() {
		m_gate = 0;
		m_tie = 0;
		m_prob = 0;
		m_is_data_point = 0;
		m_value = 0;
	}

	///////////////////////////////////////////////////////////////////////////////////
	// clear data point but preserve gate
	void clear_data_point() {
		m_is_data_point = 0;
		m_value = 0;
	}

	///////////////////////////////////////////////////////////////////////////////////
	void toggle_gate() {
		m_gate = !m_gate;
	}

	///////////////////////////////////////////////////////////////////////////////////
	// glide is simply a toggle
	void toggle_tie() {
		m_tie = !m_tie;
	}

	///////////////////////////////////////////////////////////////////////////////////
	// Prob order is OFF->HIGH->MED->LOW->OFF
	void inc_prob() {
		if(++m_prob>PROB_MAX) {
			m_prob=0;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////
	// Prob order is OFF->HIGH->MED->LOW->OFF
	void inc_retrig() {
		if(++m_retrig>RETRIG_MAX) {
			m_retrig=0;
		}
	}
};

#endif /* SEQUENCE_STEP_H_ */
