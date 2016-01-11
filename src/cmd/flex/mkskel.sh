cat <<!
/* File created from flex.skl via mkskel.sh */

#include "flexdef.h"

const char *skel[] = {
!

sed -e 's/\\/&&/g' -e 's/"/\\"/g' -e 's/.*/  "&",/' "$@"

cat <<!
  0
};
!
