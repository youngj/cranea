#ifndef _STDAFX_H
#define _STDAFX_H

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#include <stdio.h>

#define OFFSET_SIZE sizeof(size_t)

#define FORCED_COMMAND "@!forced!@"
#define FIRST_VISIT_COMMAND "@!firstvisit!@"
#define RETURN_VISIT_COMMAND "@!returnvisit!@"

enum ObjectType
{
    ObjectTypeLocation = 0,
    ObjectTypeItem,
    ObjectTypeFile,
    ObjectTypeAction
};
#define NUM_GLOBAL_MAPS ((int)ObjectTypeFile + 1)

enum ActionType
{
    ActionTypeDefault,
    ActionTypeLook,
    ActionTypeTake,
    ActionTypeDrop,
    ActionTypeInventory,
    ActionTypeQuit,
    ActionTypeReturn,
    ActionTypeCall,
    ActionTypeSave,
    ActionTypeLoad
};

#include "aes.h"
#include "sha.h"
#define KEY_SIZE CryptoPP::AES::DEFAULT_KEYLENGTH
#define KEYHASH_SIZE CryptoPP::SHA1::DIGESTSIZE

#define DEBUG_MAGIC 9

#endif
