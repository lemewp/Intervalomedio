/*
 *  LCD Menu System
 *  Peter Hinson / 2011
 *	mewp.net
 *
 *  Provides a basic, navigatable menu system.
 *	Works as is with Sparkfun serial enabled LCD screens.
 *	All model specific code is contained within the LCDMenu class.
 */

#ifndef LCDMenu_h
#define LCDMenu_h

#include <stdlib.h>
#include "WProgram.h"
#include "Event.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * LCDMenuParameter
 * *  ---------------------------------------------------------
 * *	Single menu items. Stores values, display names, etc. 
 * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef void (*SetValueCallback)(Event);
		
class LCDMenuParameter {
	protected:
		char					*_name;
		int						_id;					// For identifying events...
		float					_value;
		float					_inc;
		bool					_display_float;
		
		SetValueCallback		_setValueCallback;
		
	public:
		LCDMenuParameter() { }
		
		LCDMenuParameter(char in_name[], int id_tag, float in_value, float in_inc, bool in_display_float, SetValueCallback setValueCallback = NULL) 
		{
			init(in_name, id_tag, setValueCallback);
			_value				= in_value;
			_inc 				= in_inc;
			_display_float		= in_display_float;
		}
		
		void init(char in_name[], int id_tag, SetValueCallback setValueCallback) 
		{
			_name				= in_name;
			_id					= id_tag;
			_setValueCallback	= setValueCallback;
		}
		
		float getValue()
		{
			return _value;
		}
		
		virtual char* getDisplayValue()
		{
			char string[10];
	//		String f = (int)getValue();
			
			snprintf(string, 10, "%f",getValue());
			return string;
		}
		
		char* getName()
		{
			return _name;
		}
		
		void setValue(float new_value)
		{
			if (_value != new_value) {
				_value = new_value;
				if (_setValueCallback) { // If a callback is set for this value, create an event and call it.
					Event event;
					event.source	= _id;
					event.time		= millis();
					event.value		= new_value;
					event.object	= this;
					_setValueCallback(event);
				}
							
			}
		}
		
		void incValue(float steps)
		{
			setValue(_value + (_inc*steps));
		}
		
		void registerSetValueCallback(SetValueCallback newCallback)
		{
			this->_setValueCallback = newCallback;
		}
		
		virtual bool isFloatValue() { return true; }
};

class LCDMenuButton : public LCDMenuParameter {
	protected:
		int						_num_states;
		int						_state;
		char**					_state_values;
		
	public:
		LCDMenuButton() { }
		
		LCDMenuButton(char in_name[], int id_tag, char* state_values[], int num_states=1, int init_state = 0, SetValueCallback setValueCallback = NULL) 
		{
			init(in_name, id_tag, setValueCallback);
			_state_values		= state_values;
			_num_states			= num_states;
			_state				= init_state;
		}
		
		char* getDisplayValue()
		{
			return _state_values[_state];
		}
		
		bool validState(int state) {
			return (state >= 0 && state < _num_states) ? true : false;
		}
		
		void setValue(int new_value)
		{
			if (_state != new_value && validState(new_value)) {
				_state = new_value;
				if (_setValueCallback) { // If a callback is set for this value, create an event and call it.
					Event event;
					event.source	= _id;
					event.time		= millis();
					event.value		= _state;
					event.object	= this;
					_setValueCallback(event);
				}			
			} else if (!validState(_state)) _state = 0;
		}
		
		void incValue(int steps)
		{
			if (_state + steps >= _num_states) incValue(_state + steps - _num_states);
			else if (_state+steps < 0) setValue(_num_states - 1 - (_state + steps));
			else setValue(_state + steps);
		}
		
		bool isFloatValue() { return false; }
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * LCDMenuSection
 * *  ---------------------------------------------------------
 * *	A section or submenu. Currently can only contain
 * *	parameters as items, but could be expanded for nesting.
 * *	
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class LCDMenuSection {
	private:
		//LCDMenuItem[]		submenus;
		LCDMenuParameter	*_params[8];			// TODO: make this a linked list.
		int					_num_params;
		int					_index;				// Currently selected param/submenu
		
	public:
		LCDMenuSection()
		{
			_num_params	= 0;
			_index		= 0;
		}
		
		LCDMenuParameter* getCurrentParameter()
		{
			if (_index >= 0 && _index < _num_params)
				return _params[_index];
			else
				return new LCDMenuParameter();
		}
		
		void addParameter(LCDMenuParameter *new_param)
		{
			if (_num_params < 8) {
				_params[_num_params++] = new_param;
			}	// TODO: Should fail gracefully... or create a new array!
		}
		
		void nextItem() { if (_index < _num_params-1) _index++; else _index = 0; }
		void prevItem() { if (_index > 0) _index--; else _index = _num_params-1; }
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * * * LCDMenu
 * *  ---------------------------------------------------------
 * *	Contains the display logic on the menu system, interfaces
 * *	with the hardware. Somewhat model specific.
 * *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class LCDMenu {
	private:
		int					_index;
		bool				_dirty;
		bool				_dirt[2];
		bool				_is_asleep;
		int					_sleep_timeout;				// Milliseconds of inactivity before the display is put to sleep
		unsigned long		_last_activity_time;		// Time of last activity (redraw)
		LCDMenuSection		*_root;
		LCDMenuSection		*_cur_section;
		
	public:
		LCDMenu()
		{			
			_root			= NULL;
			_cur_section	= NULL;
			_is_asleep		= false;
			_sleep_timeout	= 30*1000;
			
			clearLCD();
			backlightOn();
			screenSize(5);
			setDirty(true);
		};
		
		void printMenu()
		{
			if (_dirty) {					// If marked for redraw...
				_dirty = false;
				if (_dirt[0] && _dirt[1]) clearLCD();
				
				LCDMenuParameter *cur_param = _cur_section->getCurrentParameter();
				
				if (_dirt[0]) {
					selectLineOne();
					Serial.print(cur_param->getName());
					_dirt[0] = false;
				}
				if (_dirt[1]) {
					selectLineTwo();
					if (cur_param->isFloatValue()) 	// A hack to avoid float->string formating
						Serial.print(cur_param->getValue());	
					else
						Serial.print(cur_param->getDisplayValue());
					_dirt[1] = false;
				}
			} else if (millis() - _last_activity_time > _sleep_timeout)
				sleep();	// Put the screen to sleep after a bit of inactivity
		}
		
		void nextItem() 
		{
			_cur_section->nextItem();
			setDirty(true);
		}
		
		void prevItem() 
		{
			_cur_section->prevItem();
			setDirty(true);
		}
		
		void incCurrentParam(float inc) 
		{
			_cur_section->getCurrentParameter()->incValue(inc);
			setDirty(true, 2);
		}
		
		void addSection(LCDMenuSection *section, LCDMenuSection *parent = NULL) 
		{
			// Add a submenu or group of parameters... LCDMenu only works with one section currently.
			_cur_section = section;		// Set this to the active section 
			if (_root == NULL)
				_root = section;		// Temp. Should only be for the first item.
			
			setDirty(true);
		}
		
		LCDMenuSection * getCurrentSection() { return _cur_section; }
		
		void setDirty(bool is_dirty, int row = 0) 
		{			
			_dirty = is_dirty;	// Mark LCD for refresh
			if (row <= 0) {
				for (int i = 0; i < 2; i++ ) _dirt[i] = is_dirty;
			}
			else _dirt[row <= 2 ? row-1 : 1] = is_dirty;
			
			stayAwake();
		}
		
		bool isDirty() { return _dirty; }
		
		void stayAwake()
		{
			if (_is_asleep) {
				backlightOn();
				_is_asleep = false;
			}
			_last_activity_time = millis();
		}
		
		void sleep()
		{
			backlightOff();
			_is_asleep = true;
		}
		
		/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
			Danger...
			Model specific code follows. Override?
		/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
		
		void selectLineOne()
		{	// Puts the cursor at line 0 char 0.
			Serial.print(0xFE, BYTE);   //command flag
			Serial.print(128, BYTE);    //position
			delay(10);
		}

		void selectLineTwo()
		{	// Puts the cursor at line 0 char 0.
			Serial.print(0xFE, BYTE);   //command flag
			Serial.print(192, BYTE);    //position
			delay(10);
		}
		
		void goTo(int position)
		{	// Position = line 1: 0-15, line 2: 16-31, 31+ defaults back to 0
			if (position<16) {
				Serial.print(0xFE, BYTE);   				//command flag
				Serial.print((position+128), BYTE);    		//position
			} else if (position<32) {
				Serial.print(0xFE, BYTE);  					//command flag
				Serial.print((position+48+128), BYTE);		//position 
			} else { goTo(0); }
			delay(10);
		}

		void clearLCD()
		{	// Clears the LCD. 
			LCDCommand();   			// command flag
	 		Serial.print(0x01, BYTE);   // clear command.
			delay(10);
		}
		
		void backlightOn()
		{	// Turns on the backlight
		    Serial.print(0x7C, BYTE);   // command flag for backlight stuff
		    Serial.print(157, BYTE);    // light level.
			delay(10);
		}
		void backlightOff()
		{	// Turns off the backlight
			Serial.print(0x7C, BYTE);   // command flag for backlight stuff
			Serial.print(128, BYTE);	// light level for off.
			delay(10);
		}
		
		void screenSize(int size)		
		{	// This can be 3-6, controls the resolution
			Serial.print(0x7C, BYTE);
			Serial.print(size, DEC);
			delay(10);
		}
		
		void LCDCommand()
		{   // A general function to call the command flag for issuing all other commands   
			Serial.print(0xFE, BYTE);
		}
};

#endif
