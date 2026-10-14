#pragma once

// Standalone diagnostic vocabulary. The full reader will use CrossInk's I18n
// catalog when its HAL is connected; no ESP/Arduino dependency in this tool.
enum DiagnosticString { STR_TITLE, STR_SUBTITLE, STR_INSTRUCTION, STR_CENTER, STR_EXIT, STR_TIMEOUT };
inline const char* tr(DiagnosticString message) {
  switch (message) {
    case STR_TITLE:
      return "CROSSINK PW1";
    case STR_SUBTITLE:
      return "TESTE DE TELA E TOQUE";
    case STR_INSTRUCTION:
      return "TOQUE NOS CANTOS 1 2 3 4";
    case STR_CENTER:
      return "DEPOIS TOQUE NO CENTRO";
    case STR_EXIT:
      return "SAIR";
    case STR_TIMEOUT:
      return "SAIDA AUTOMATICA EM 90 SEGUNDOS";
  }
  return "";
}
