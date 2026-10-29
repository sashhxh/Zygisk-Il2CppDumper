#include "model/avatar_bundle_info.h"
.section .rodata
.balign 16
.global cfesp_avatar_bundle
.hidden cfesp_avatar_bundle
cfesp_avatar_bundle:
.incbin "model/custom-avatar.bundle"
.global cfesp_avatar_bundle_end
.hidden cfesp_avatar_bundle_end
cfesp_avatar_bundle_end:
.section .note.GNU-stack,"",%progbits
