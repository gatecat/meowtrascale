#ifndef CONSTIDS_H
#define CONSTIDS_H

#include "idstring.h"
#include "preface.h"

MEOW_NAMESPACE_BEGIN

#ifndef Q_MOC_RUN
enum ConstIds
{
    ID_NONE
#define X(t) , ID_##t
#include "constids.inc"
#undef X
};

#define X(t) static constexpr auto id_##t = IdString(ID_##t);
#include "constids.inc"
#undef X
#endif

MEOW_NAMESPACE_END

#endif
