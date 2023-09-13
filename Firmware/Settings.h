// (c) 2013 Artjom Eske
// This code is licensed under MIT license (see LICENSE.txt for details)
// Version 1.1 - Min HW: 1.3

#pragma once

enum SETTINGS_STATE {
  OFF = 0,
  ON = 1,
  ENABLE_FOR_HD = 2,
  DISABLE_FOR_HD = 3
};

class Setting {

public:

  Setting(){}

  Setting(const SETTINGS_STATE* states, SETTINGS_STATE state) 
    : m_states(states), m_state(state)
  {

  }

  const SETTINGS_STATE* GetStates(){ return m_states; } 
  SETTINGS_STATE GetState() { return m_state; }
  void SetState(SETTINGS_STATE state) { m_state = state; }

private:
  SETTINGS_STATE m_state = SETTINGS_STATE::OFF;
  const SETTINGS_STATE* m_states;
};

class Settings {

public:

  Settings(){
    m_DisAmp = Setting((SETTINGS_STATE*)(const SETTINGS_STATE[2]){SETTINGS_STATE::ON, SETTINGS_STATE::OFF}, SETTINGS_STATE::OFF);
    m_SDFilter = Setting((SETTINGS_STATE*)(const SETTINGS_STATE[3]){SETTINGS_STATE::ON, SETTINGS_STATE::DISABLE_FOR_HD, SETTINGS_STATE::OFF}, SETTINGS_STATE::OFF);
    m_SyncFilter = Setting((SETTINGS_STATE*)(const SETTINGS_STATE[3]){SETTINGS_STATE::ON, SETTINGS_STATE::ENABLE_FOR_HD, SETTINGS_STATE::OFF}, SETTINGS_STATE::ENABLE_FOR_HD);
  }

private:

    // Settings bit map
    // 00000000

    // Disable Amp when no sync -> ON, OFF
    // SD Filter amp -> Always ON, Disable for HD, Always OFF
    // Sync filter -> Always ON, Enable for HD, Always OFF

    Setting m_DisAmp;
    Setting m_SDFilter;
    Setting m_SyncFilter;

};

