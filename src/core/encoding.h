/*
 *  encoding.h
 *  MacBlueTelnet
 *
 *  Created by Yung-Luen Lan on 9/11/07.
 *  Copyright 2007 yllan.org. All rights reserved.
 *
 */
#ifndef ENCODING_H_
#define ENCODING_H_
#ifdef __GNUG__
  #pragma interface "encoding.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "gemanx_utils.h"
#include <glib.h>

X_EXPORT void init_encoding_table();
X_EXPORT gchar * my_convert(const gchar *str, gssize len, 
        const gchar *to_codeset,
        const gchar *from_codeset, gsize * writelen);

#ifdef __cplusplus
}
#endif

#endif //ENCODING_H_
