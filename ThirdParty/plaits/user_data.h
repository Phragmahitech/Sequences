// Plaits user data (wavetables / patches chargés en flash sur le module) : désactivé dans Sequences.
// Remplace le fichier d'origine, qui dépend du matériel STM32.
#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_

#include "stmlib/stmlib.h"

namespace plaits {

class UserData {
 public:
  enum { SIZE = 0x1000 };
  UserData() { }
  inline const uint8_t* ptr (int) const { return NULL; }
};

}  // namespace plaits

#endif  // PLAITS_USER_DATA_H_
