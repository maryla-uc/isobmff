/*
Copyright 2026 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "MP4Atoms.h"
#include <stdlib.h>
#include <stdint.h>

static void destroy(MP4AtomPtr s)
{
  MP4HumanReadableStreamDescriptionAtomPtr self;
  self = (MP4HumanReadableStreamDescriptionAtomPtr)s;
  if(self == NULL) return;

  if(self->description)
  {
    free(self->description);
    self->description = NULL;
  }

  if(self->super) self->super->destroy(s);
}

static MP4Err serialize(struct MP4Atom *s, char *buffer)
{
  MP4Err err                                    = MP4NoErr;
  MP4HumanReadableStreamDescriptionAtomPtr self = (MP4HumanReadableStreamDescriptionAtomPtr)s;
  size_t len;

  err = MP4SerializeCommonBaseAtomFields(s, buffer);
  if(err) goto bail;
  buffer += self->bytesWritten;
  assert(self->type == MP4HumanReadableStreamDescriptionAtomType);

  len = strlen(self->description);
  if(self->description == NULL || len == 0 || len >= UINT32_MAX - 1)
  {
    err = MP4BadParamErr;
    goto bail;
  }
  /* Write description as null-terminated UTF-8 string */
  PUTBYTES(self->description, (u32)len + 1); /* Include null terminator */

  assert(self->bytesWritten == self->size);
bail:
  TEST_RETURN(err);

  return err;
}

static MP4Err calculateSize(struct MP4Atom *s)
{
  MP4Err err;
  MP4HumanReadableStreamDescriptionAtomPtr self = (MP4HumanReadableStreamDescriptionAtomPtr)s;
  err                                           = MP4NoErr;

  err = MP4CalculateBaseAtomFieldSize(s);
  if(err) goto bail;

  if(self->description == NULL || strlen(self->description) == 0)
  {
    err = MP4BadParamErr;
    goto bail;
  }
  /* Add description size (null-terminated string) */
  self->size += (u32)strlen(self->description) + 1; /* Including '\0' */

bail:
  TEST_RETURN(err);

  return err;
}

static MP4Err createFromInputStream(MP4AtomPtr s, MP4AtomPtr proto, MP4InputStreamPtr inputStream)
{
  MP4Err err;
  MP4HumanReadableStreamDescriptionAtomPtr self = (MP4HumanReadableStreamDescriptionAtomPtr)s;
  s64 bytesToRead;
  u32 nullPos;
  u32 descLen;
  u8 *buf = NULL;
  u32 i;

  if(self == NULL) BAILWITHERROR(MP4BadParamErr)
  err = self->super->createFromInputStream(s, proto, (char *)inputStream);
  if(err) goto bail;

  /* Read all remaining bytes into a flat buffer, then split on the first null byte.
   * Scanning byte-by-byte and "rewinding" by adjusting only bytesRead (while leaving
   * the stream cursor in place) does not work — readData always reads from the current
   * stream position. */
  bytesToRead = (s64)(self->size - self->bytesRead);
  if(bytesToRead < 0) BAILWITHERROR(MP4BadDataErr);

  if(bytesToRead > 0)
  {
    buf = (u8 *)calloc((u32)bytesToRead, 1);
    TESTMALLOC(buf);
    err = inputStream->readData(inputStream, (u32)bytesToRead, (char *)buf, NULL);
    if(err) goto bail;
    self->bytesRead += (u32)bytesToRead;

    /* Find the null terminator that ends the description field */
    nullPos = (u32)bytesToRead; /* default: no null found */
    for(i = 0; i < (u32)bytesToRead; i++)
    {
      if(buf[i] == 0)
      {
        nullPos = i;
        break;
      }
    }

    /* Description: bytes [0 .. nullPos] (including the null terminator) */
    descLen           = nullPos + 1; /* length including null */
    self->description = (char *)calloc(descLen, 1);
    TESTMALLOC(self->description);
    memcpy(self->description, buf, descLen);
  }

bail:
  free(buf);
  TEST_RETURN(err);
  return err;
}

MP4Err
MP4CreateHumanReadableStreamDescriptionAtom(MP4HumanReadableStreamDescriptionAtomPtr *outAtom)
{
  MP4Err err;
  MP4HumanReadableStreamDescriptionAtomPtr self;

  self = (MP4HumanReadableStreamDescriptionAtomPtr)calloc(
    1, sizeof(MP4HumanReadableStreamDescriptionAtom));
  TESTMALLOC(self);

  err = MP4CreateBaseAtom((MP4AtomPtr)self);
  if(err) goto bail;
  self->type                  = MP4HumanReadableStreamDescriptionAtomType;
  self->name                  = "HumanReadableStreamDescription";
  self->createFromInputStream = (cisfunc)createFromInputStream;
  self->destroy               = destroy;
  self->calculateSize         = calculateSize;
  self->serialize             = serialize;

  self->description = NULL;

  *outAtom = self;
bail:
  TEST_RETURN(err);

  return err;
}
