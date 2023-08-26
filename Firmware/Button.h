#pragma once

class Button{
public:
  Button(int pin) : m_pin(pin) {}

private:
  int32_t m_pin;
  bool m_pressed;
  uint64_t m_elapsedTime = 0;

  uint64_t m_start = 0;
  uint64_t m_stop = 0;
  bool m_wasPressed = false;

public:

  uint64_t ElapsedTime(){
    return m_elapsedTime;
  }

  void Read(){
      if(digitalRead(m_pin) == HIGH){
        if(m_pressed == false){
            m_start = millis();
            m_stop = 0;
            m_elapsedTime = 0;
        }
        m_pressed = true;
        m_elapsedTime = millis() - m_start;
      }
      else{
        if(m_pressed == true){
            m_pressed = false;
            m_stop = millis();
            m_elapsedTime = m_stop - m_start;
            m_wasPressed = true;
        }
      }
  }

  bool WasPressed(){
      if(m_wasPressed){
          m_wasPressed = false;
          return true;
      }
      
      return false;
  }

  bool Reset(){
      m_pressed = false;
      m_start = 0;
      m_stop = 0;
      m_elapsedTime = 0;
      m_wasPressed = false;
  }
};