
#ifndef _PS2KEYBOARD_H
#define _PS2KEYBOARD_H

class PS2Keyboard {    
  public:
    void begin(uint16_t pClockLine, uint16_t pDataLine);
	void end();                         
    int waitkey();
	int getkey();
};

#endif /* _PS2KEYBOARD_H */
