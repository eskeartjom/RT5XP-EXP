#pragma once

struct States {
  bool Init = false;

  bool LMH1251_SELECT;
  bool LMH1251_AUTOMANUAL;
  bool LMH1251_SDHD;
  bool LMH1251_POWERSAVER;

  bool THS7374_DISABLE;
  bool THS7374_FILTERBYPASS;

  bool MAX4887_1_SELECT;
  bool MAX4887_1_ENABLE;

  bool MAX4887_2_SELECT;
  bool MAX4887_2_ENABLE;

  bool MAX4908_CB1;
  bool MAX4908_CB2;

  bool LMH1980_CVBS_FILTER;
  bool LMH1980_CVBS_HD;

  bool LMH1980_SOGY_FILTER;
  bool LMH1980_SOGY_HD;

  bool LMH1980_MCU_HD;

  bool LVC1G3_IN;
  bool LVC1G3_OUT;

  bool TS5A3357_1;
  bool TS5A3357_2;

  unsigned long MCU_LOOP = 0;

  float H_FRQ = 0.0f;
  float V_FRQ = 0.0f;
  float O_FRQ = 0.0f;

  int32_t LINES = 0;

};
