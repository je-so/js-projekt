/* title: Uniform-Resource-Identifier impl

   Implements <Uniform-Resource-Identifier>.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2017 Jörg Seebohn

   file: C-kern/api/io/www/uri.h
    Header file <Uniform-Resource-Identifier>.

   file: C-kern/io/www/uri.c
    Implementation file <Uniform-Resource-Identifier impl>.
*/

#include "C-kern/konfig.h"
#include "C-kern/api/io/www/uri.h"
#include "C-kern/api/err.h"
#include "C-kern/api/memory/memblock.h"
#include "C-kern/api/memory/wbuffer.h"
#include "C-kern/api/memory/mm/mm_macros.h"
#include "C-kern/api/test/validate.h"
#ifdef KONFIG_UNITTEST
#include "C-kern/api/test/unittest.h"
#endif

// private types

/* struct: normalize_path_r
 * Returntyp der Funktion normalize_path. */
typedef struct normalize_path_r {
   size_t pathlen;
   size_t prefixlen;
} normalize_path_r;

typedef struct parse_offsets_r {
   size_t  nr_percent_encoded;   // nr of percent encoded characters
   size_t  nr_need_encoding;     // number space in path which need to be encdoed as %20 ...
} parse_offsets_r;

typedef enum uri_type_e {
   type_DECODED,
   type_ENCODED
} uri_type_e;


// section: uri_t

// group: helper

/* function: hexvalue
 * Konvertiert Hexadezimalziffer digit in Dezimalwert 0 bis 15.
 * Ist digit nicht im Bereich [0..9a-fA..F], wird ein Wert > 15 zurückgegeben.
 *
 * Returns:
 * 0-9:     '0' <= digit && digit <= '9'
 * 10-15:   'A' <= digit && digit <= 'F'
 * 10-15:   'a' <= digit && digit <= 'f'
 * > 15:    digit not in [0..9a-fA..F]
 * */
static inline unsigned hexvalue(unsigned digit)
{
   return digit<='9' ? digit-'0' : (digit|0x20)>='a' ? (digit|0x20)-('a'-10) : digit;
}

static inline int ishex(unsigned value)
{
   return value<=0xf;
}

/* function: merge_path
 * Hängt den Pfad rel_path an den absoluten Pfad base an und gibt das Ergebnis in str zurück.
 *
 * Parameter:
 * base      - Absolute path (already normalized).
 * rel_path  - Relative path appended to base (already normalized).
 * prefixlen - The size of the prefix ("../../.."") (prefixlen==0||prefixlen>=2) of rel_path.
 * stroff    - The in/out value which acts as the current writing offset into str.
 *             The first byte written by this function is str[*stroff]. The returned value
 *             is computed by the old one plus the number of bytes written to str.
 * size      - The maximum number of bytes writable to str.
 * str       - The start address of the result string.
 *
 * Returns:
 * 0         - OK
 * EOVERFLOW - The written bytes would exceed size.
 *
 * Unchecked Precondition:
 * - base->size >= 1 && base->addr[0] == '/'          // base path is absolute
 * - rel_path->size == 0 || rel_path->addr[0] != '/'  // rel_path is relative
 * */
static int merge_path(uri_part_t* base, uint16_t prefixlen, uri_part_t* rel_path, size_t size, /*eout*/uint8_t str[size], /*out*/size_t* nrbytes)
{
   size_t   off  = 0;
   uint8_t* slash= memrchr(base->addr, '/', base->size);
   size_t baselen= (size_t) (slash-base->addr+1);
   unsigned prelen=prefixlen-2u;
   while (prelen<prefixlen && baselen>1) {
      slash  = memrchr(base->addr, '/', baselen-1);
      baselen= (size_t) (slash-base->addr+1);
      prelen-= 3u;
   }
   if (off+baselen<=size) {
      memcpy(str+off,base->addr,baselen);
      off+=baselen;
   } else {
      return EOVERFLOW;
   }
   size_t rellen= (size_t)(rel_path->size-prefixlen);
   if (off+rellen<=size) {
      memcpy(str+off,rel_path->addr+prefixlen,rellen);
      off+=rellen;
   } else {
      return EOVERFLOW;
   }

   // set out
   *nrbytes=off;
   return 0;
}

/* function: normalize_path
 * Normalisiert den Pfad path[0..size-1] und gibt den normalisierten Pfad in path zurück.
 * Der Rückgabewert pathlen enthält die neue Länge.
 * Ist der Pfad absolut, enthält der Rückgabewert prefixlen den Wert 1.
 * Ist der Pfad relativ, so gibt der Wert prefixlen die Länge in Bytes aller "../"
 * am Anfang des normalisierten Pfades wieder. Ist dieser Wert nicht durch drei teilbar,
 * endet der gesamte Pfad in einem "/..".
 *
 * Normalizations:
 * 1. "[ANY]//[ANY]" will be transformed into "[ANY]/[ANY]"
 * 2. "[ANY]/path/../[ANY]" will be transformed into "[ANY]/[ANY]"
 * 2. "[ANY]/path/..[END]" will be transformed into "[ANY]/[END]"
 * 3. "[START]/../[ANY]" will be transformed into "[START]/[ANY]"
 * 4. "[START]../../../[ANY]" the prefix [START]../../../ will not be changed.
 * 4.1 "[START]../../../[END]" will not be changed.
 * 4.2 "[START]../../..[END]" will not be changed.
 * 5. "[ANY]/./[ANY]" will be transformed into "[ANY]/[ANY]"
 * 5.1 "[ANY]/.[END]" will be transformed into "[ANY]/[END]"
 * 5.2 "[START]./[ANY]" will be transformed into "[ANY]"
 * */
static inline normalize_path_r normalize_path(size_t size, uint8_t path[size])
{
   uint8_t* rpos=path; // start of next path component
   uint8_t* wpos=path; // normalized output path
   uint8_t* prefixpos=path; // relative or absolute prefix which can not be removed by ../
   uint8_t* end=path+size;

   // Invariant:
   // wpos<=rpos && rpos<=end
   // wpos==c || wpos[-1]=='/'  ; examples: ../[wpos], a/b/c/[wpos]
   // rpos==path || rpos[-1]=='/'  ; examples: a/./[rpos], a/b/c/[wpos]
   // prefixpos==path || (prefixpos==path+1 && path[0]=='/') || (prefixpos>=path+3 && path[0..2]=='../') ; examples: [prefixpos], /[prefixpos], ../../[prefixpos]

#define SKIP_PREV_PATH() \
         do { wpos=memrchr(path, '/', (size_t)(wpos-path-1)); wpos=wpos?wpos+1:path; } while(0)

   while (rpos!=end) {
      uint8_t* slash=memchr(rpos, '/', (size_t)(end-rpos));
      if (!slash) slash=end;
      if (slash == rpos) {
         if (path==rpos) { prefixpos=wpos=path+1; } // do not skip / at beginning (absolute path)
         ++rpos; // else skipreduce "//" to "/"
      } else if (rpos+1 == slash && '.'==rpos[0]) {
         rpos+=1+(slash!=end); // skip "/./"
      } else if (rpos+2 == slash && '.'==rpos[0] && '.'==rpos[1]) {
         if (wpos!=prefixpos) {
            rpos+=2+(slash!=end); // reduce "/path/../" to "/"
            SKIP_PREV_PATH();
         } else if (prefixpos == path+1) {
            rpos+=2+(slash!=end); // reduce "/../" to "/"
         } else {
            prefixpos+=2+(slash!=end);  // append ../
            goto APPEND_PATH;
         }
      } else {
         APPEND_PATH: ;
         if (slash<end) { ++slash; }
         size_t append_len=(size_t)(slash-rpos);
         if (wpos != rpos) { memmove(wpos, rpos, append_len); }
         wpos+=append_len;
         rpos=slash;
      }
   }

#undef SKIP_PREV_PATH

   return (normalize_path_r) { (size_t)(wpos-path), (size_t)(prefixpos-path) };
}

/* define: is_percent_encoded
 * Gibt true zurück, wenn der ASCII-Buchstabencode als "%[HEXDIGIT][HEXDIGIT]" kodiert werden sollte.
 *
 * Arguments:
 * part - The part of the uri the parsing takes place. See <uri_part_e> for a list of values.
 * chr  - The unsigned ASCII code of a character which is checked if it should be represented with percent encoding.
 *        The character code 32 (space ' ') for uri_part_QUERY must be handled before this function is used.
 *        This expression returns always true for space but in case of uri_part_QUERY it has to be encoded as '+' and
 *        therefore '+' as percent encoded character. */
#define is_percent_encoded(part, chr) \
         ((uri_part_AUTHORITY==(part) && ( (chr)<=32 || (chr)>126 || '%'==(chr) || '#'==(chr) || '?'==(chr) || '/'==(chr)))            \
         ||(uri_part_PATH==(part) && ( (chr)<=32 || (chr)>126 || '%'==(chr) || '#'==(chr) || '?'==(chr) || ':'==(chr)))                \
         ||(uri_part_QUERY==(part) && ( (chr)<=32 || (chr)>126 || '%'==(chr) || '#'==(chr) || '+'==(chr) || '='==(chr) || '&'==(chr))) \
         ||(uri_part_FRAGMENT==(part) && ( (chr)<=32 || (chr)>126 || '%'==(chr)))                                                      \
         )

/* function: parse_offsets
 * Berechnet die Offsets der einzelnen Teile einer encodierten URI. */
static parse_offsets_r parse_offsets(/*out*/uri_encoded_t* uri, uint16_t size, const uint8_t str[size]/*encoded*/)
{
   uint8_t c=0;   // next read char
   size_t  i=0;   // index into str
   size_t  path_start=0;
   size_t  nr_percent_encoded=0;
   size_t  nr_need_encoding=0;
   size_t  nr_param=0;

   // SCHEME

   static_assert(uri_part_SCHEME==0, "first part");
   uri->offset[uri_part_SCHEME]=0;

   for (; i<size; ++i) {
      c = str[i];
      if ((c|0x20) < 'a' || (c|0x20) > 'z') { break; }
   }

   if (':' == c) {
      path_start= ++i;
   }

   // AUTHORITY

   static_assert(uri_part_AUTHORITY==1, "2nd part");
   uri->offset[uri_part_AUTHORITY]=(uint16_t) path_start;
   if (  i == path_start && i + 2 <= size
         && '/' == str[i] && '/' == str[1+i]) {
      i += 2;
      for (; i<size; ++i) {
         c = str[i];
         if ('/' == c || '?' == c || '#' == c) break;
         if ('%' == c) { ++nr_percent_encoded; i+=2; }
         nr_need_encoding += (c<=32)||(c>126);
      }
      if (i>size) i=size;
      path_start=i;
   }

   // PATH

   static_assert(uri_part_PATH==2, "3d part");
   uri->offset[uri_part_PATH]=(uint16_t) path_start;
   for (; i<size; ++i) {
      c = str[i];
      if ('?' == c || '#' == c) { break; }
      if ('%' == c) { ++nr_percent_encoded; i+=2; }
      nr_need_encoding += (c<=32)||(c>126);
   }
   if (i>size) i=size;

   // QUERY

   static_assert(uri_part_QUERY==3, "4th part");
   uri->offset[uri_part_QUERY]=(uint16_t) i;
   for (; i<size; ++i) {
      c = str[i];
      if ('#' == c) { break; }
      nr_param += ('&'==c);
      if ('%' == c) { ++nr_percent_encoded; i+=2; }
      nr_need_encoding += (c<32)||(c>126);
   }
   if (i>size) i=size;
   if (i > uri->offset[uri_part_QUERY]) {
      ++nr_param; // count last missing '&'
   }
   uri->nrparam=(uint16_t) nr_param;

   // FRAGMENT

   static_assert(uri_part_FRAGMENT==4, "5th part");
   uri->offset[uri_part_FRAGMENT]=(uint16_t) i;
   uri->offset[uri_part_FRAGMENT+1]=(uint16_t) size;
   for (; i<size; ++i) {
      c = str[i];
      if ('%' == c) { ++nr_percent_encoded; i+=2; }
      nr_need_encoding += (c<=32)||(c>126);
   }

   return (parse_offsets_r) { .nr_percent_encoded=nr_percent_encoded, .nr_need_encoding=nr_need_encoding };
}

/* function: parse_uri
 * Bestimmt die einzelnen Abschnitte einer URI (siehe uri_part_e) aus dem String str[size] und enkodiert
 * oder dekodiert diese dann in einen für das Ergebnis genügend groß allokierten Speicherbereich.
 * Das Ergebnis wird in uri zurückgegeben und auch die Offsets der einzelnen Teile,
 * so dass die Werte aller einzelnen Abschnitte von uri erfragt werden können. */
static int parse_uri(/*out*/uri_encoded_t* uri, uri_type_e type, uint16_t size, const uint8_t str[size])
{
   int err;
   unsigned d;    // value decoded from %XY where d = hexvalue(X) * 16 + hexvalue(Y)
   uint8_t c;     // next read char
   size_t  i;     // index into str
   parse_offsets_r stats=parse_offsets(uri, size, str);
   const unsigned PARAM_SIZE = sizeof(((uri_decoded_t*)0)->param[0]);

   // alloc memory for parsed url

   size_t uri_size_max;
   size_t param_array_size;

   if (type_DECODED!=type) {
      // determine maximum possible size for encoding
      param_array_size = 0;
      uri_size_max = (size_t)1/*\0 at end*/ + (2*stats.nr_need_encoding) + size;
   } else {
      // ... decoding (delete sequence #QQ in case of any Q not in range [0..9a..fA..F])
      param_array_size = (2u+uri->nrparam)*PARAM_SIZE; // why (2u+uri->nrparam)? +1: end offset for nparam-1 is stored in nparam, another +1: used for possible memory alignment of struct
      uri_size_max = (size_t)1/*\0 at end*/ - (2*stats.nr_percent_encoded) + size;
   }

   if (uri_size_max>UINT16_MAX+1u) {
      err=EOVERFLOW;
      goto ONERR;
   }

   err = ALLOC_MM(uri_size_max+param_array_size, cast_memblock(uri, mem_));
   if (err) goto ONERR;

   // copy & normalize

   size_t iparam=0;     // index into ((uri_decoded_t*)uri)->param
   size_t wmemoff=0;    // write offset into uri->mem_addr
   size_t wmem_start;   // start write offset into uri->mem_addr of a parsed component

   if (type_DECODED==type) {
      uintptr_t array=((uintptr_t)uri->mem_addr + uri_size_max);
      if ((array & (PARAM_SIZE-1))/*not aligned*/) { array+=PARAM_SIZE-(array & (PARAM_SIZE-1)); /*aligned to PARAM_SIZE*/}
      ((uri_decoded_t*)uri)->param = (void*) array;
   }

#define copy_char(c)       uri->mem_addr[wmemoff++] = (c)
#define overwrite_char(c)  uri->mem_addr[wmemoff-1] = (c)
#define _param(i)          ((uri_decoded_t*)uri)->param[i]

#define encode_part(part)  copy_part(true, part)

#define decode_part(part)  copy_part(false, part)

#define copy_part(isEncode, part) \
         do {                                                           \
            wmem_start = wmemoff;                                       \
            i=uri->offset[part];                                        \
            size_t component_end=uri->offset[(part)+1];                 \
            uri->offset[part]=(uint16_t) wmemoff;                       \
            int isParamValue;                                           \
            if (!(isEncode)) {                                          \
               i+=(part == uri_part_AUTHORITY)*2                        \
                +(part == uri_part_QUERY)                               \
                +(part == uri_part_FRAGMENT);                           \
                if (part == uri_part_QUERY) {                           \
                  _param(iparam).nameoff=(uint16_t)wmemoff;             \
                  isParamValue=0;                                       \
                }                                                       \
            }                                                           \
            for (; i<component_end; ++i) {                              \
               c=str[i];                                                \
               copy_char(c);                                            \
               if ('%' == c) {                                          \
                  --wmemoff; /* remove '%' */                           \
                  i+=2;                                                 \
                  if (i < component_end) {                              \
                     unsigned d1 = hexvalue(str[i-1]);                  \
                     unsigned d2 = hexvalue(str[i]);                    \
                     if (ishex(d1) && ishex(d2)) {                      \
                        d = (d1<<4) | d2;                               \
                        if ((isEncode) && is_percent_encoded(part,d)) { \
                           if ((part == uri_part_QUERY) && (' '==d)) {  \
                              copy_char('+');                           \
                           } else {                                     \
                              ++wmemoff; /* add '%' */                     \
                              copy_char((uint8_t)(d1+(d1<10?'0':'A'-10))); \
                              copy_char((uint8_t)(d2+(d2<10?'0':'A'-10))); \
                           }                                            \
                        } else {                                        \
                           copy_char((uint8_t)d);                       \
                        }                                               \
                     }                                                  \
                  }                                                     \
               } else if ((isEncode)) {                                 \
                  if ((part == uri_part_QUERY) && (' ' == c)) {         \
                     overwrite_char('+');                               \
                  } else if (c<=32 || c>126) {                          \
                     unsigned d1 = (unsigned)c >> 4;                    \
                     unsigned d2 = (unsigned)c & 0x0f;                  \
                     overwrite_char('%');                               \
                     copy_char((uint8_t)(d1+(d1<10?'0':'A'-10)));       \
                     copy_char((uint8_t)(d2+(d2<10?'0':'A'-10)));       \
                  }                                                     \
               } else if (!(isEncode)) {                                \
                  if (part == uri_part_QUERY) {                         \
                     if ('+' == c) {                                    \
                        overwrite_char(' ');                            \
                     } else if ('&' == c) {                             \
                        --wmemoff; /* skip '&' */                       \
                        if (!isParamValue) {                            \
                           _param(iparam).valueoff=(uint16_t)wmemoff;   \
                        }                                               \
                        _param(++iparam).nameoff=(uint16_t)wmemoff;     \
                        isParamValue=0;                                 \
                     } else if ('=' == c && !isParamValue) {            \
                        _param(iparam).valueoff=(uint16_t)--wmemoff;    \
                        isParamValue=1;                                 \
                     }                                                  \
                  }                                                     \
               }                                                        \
            }                                                           \
            if (!(isEncode) && part==uri_part_QUERY && uri->nrparam) {  \
               if (!isParamValue) {                                     \
                  _param(iparam).valueoff=(uint16_t)wmemoff;            \
               }                                                        \
               _param(++iparam).nameoff=(uint16_t)wmemoff;              \
               assert(iparam==uri->nrparam);                            \
            }                                                           \
         } while(0)

   if (uri->offset[uri_part_SCHEME+1]) {
      for (i=0; i<uri->offset[uri_part_SCHEME+1]; ++i) {
         copy_char((str[i]|0x20)); // lowercase
      }
      wmemoff-=(type_DECODED==type); /* skip ':' */
   }

   if (type_DECODED!=type) {
      encode_part(uri_part_AUTHORITY);
      encode_part(uri_part_PATH);
      normalize_path_r norm = normalize_path(wmemoff-uri->offset[uri_part_PATH], &uri->mem_addr[wmem_start]);
      wmemoff = wmem_start + norm.pathlen;
      uri->prefixlen=(uint16_t) norm.prefixlen;
      encode_part(uri_part_QUERY);
      encode_part(uri_part_FRAGMENT);
      uri->offset[uri_part_FRAGMENT+1]=(uint16_t) wmemoff;

   } else {
      // decoding

      decode_part(uri_part_AUTHORITY);
      decode_part(uri_part_PATH);
      normalize_path_r norm = normalize_path(wmemoff-uri->offset[uri_part_PATH], &uri->mem_addr[wmem_start]);
      wmemoff = wmem_start + norm.pathlen;
      uri->prefixlen=(uint16_t) norm.prefixlen;
      decode_part(uri_part_QUERY);
      decode_part(uri_part_FRAGMENT);
      uri->offset[uri_part_FRAGMENT+1]=(uint16_t) wmemoff;
   }

   copy_char(0);
   assert(wmemoff <= uri_size_max);

#undef _param
#undef copy_char
#undef copy_part
#undef encode_part
#undef decode_part
#undef overwrite_char

   return 0;
ONERR:
   return err;
}


// section: uri_encoded_t

// group: lifetime

int free_uriencoded(uri_encoded_t* uri)
{
   int err;

   err = FREE_MM(cast_memblock(uri, mem_));

   *uri = (uri_encoded_t) uri_encoded_FREE;
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_uriencoded(/*out*/uri_encoded_t* uri, const struct uri_decoded_t* fromuri)
{
   int err;
   memblock_t   mblock;
   uri_param_t* params=0;
   uri_part_t   parts[uri_part__NROF];

   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      parts[part]=getpart_uridecoded(fromuri, part);
   }

   unsigned nrparam=nrparam_uridecoded(fromuri);

   if (nrparam) {
      size_t bytes = nrparam * sizeof(uri_param_t);

      err = ALLOC_MM(bytes, &mblock);
      if (err) goto ONERR;
      params = (uri_param_t*)mblock.addr;

      for (unsigned iparam=0; iparam<nrparam; ++iparam) {
         params[iparam] = getparam_uridecoded(fromuri, iparam);
      }
   }

   err = initbuild_uriencoded(uri, &parts[uri_part_SCHEME], &parts[uri_part_AUTHORITY],
                           &parts[uri_part_PATH], (uint16_t)nrparam, params, &parts[uri_part_FRAGMENT]);
   if (err) goto ONERR;

   if (params) {
      err = FREE_MM(&mblock);
      if (err) {
         (void) free_uriencoded(uri);
         goto ONERR;
      }
   }

   return 0;
ONERR:
   if (params) {
      (void) FREE_MM(&mblock);
   }
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initbuild_uriencoded(/*out*/uri_encoded_t* uri, const uri_part_t* pScheme, const uri_part_t* pAuthority, const uri_part_t* pPath, uint16_t nrparam, const uri_param_t params[nrparam], const uri_part_t* pFragment)
{
   int err;
   size_t bytes = 0;
   if (pScheme) {
      bytes = (pScheme->size ? pScheme->size+1u/*':'*/ : 0u);
      for (unsigned i=0; i<pScheme->size; ++i) {
         uint8_t c=(0x20|pScheme->addr[i]);
         if (c<'a' || 'z'<c) {
            err=EINVAL;
            goto ONERR;
         }
      }
   }

   if (pAuthority && pPath && pAuthority->size && pPath->size) {
      if (pPath->addr[0] != '/') {
         err=EINVAL;
         goto ONERR;  // need absolute path in case of authority
      }
   }

   bytes += 2u*nrparam; // count '?', '&' and '='
   for (unsigned iparam=0; iparam<nrparam; ++iparam) {
      for (unsigned nv=0; nv<2; ++nv) {
         bytes += params[iparam].name_value[nv].size;
         for (unsigned i=0; i<params[iparam].name_value[nv].size; ++i) {
            uint8_t c=params[iparam].name_value[nv].addr[i];
            if (c!=' '/*encoded as '+'*/ && is_percent_encoded(uri_part_QUERY,c)) {
               bytes+=2; // need percent encoding
            }
         }
      }
   }

   const uri_part_t* parts[]  = { pAuthority, pPath, pFragment };
   const uri_part_e  partnr[] = { uri_part_AUTHORITY, uri_part_PATH, uri_part_FRAGMENT };

   for (unsigned part=0; part<lengthof(parts); ++part) {
      if (parts[part] && parts[part]->size) {
         bytes += (size_t)parts[part]->size + (part!=1/*'#','/'*/) + (part==0/*'/'*/);
         for (unsigned i=0; i<parts[part]->size; ++i) {
            uint8_t c=parts[part]->addr[i];
            if (is_percent_encoded(partnr[part],c)) {
               bytes+=2; // need percent encoding
            }
         }
      }
   }

   ++bytes; // \0-byte

   if (bytes > UINT16_MAX) {
      err=EOVERFLOW;
      goto ONERR;
   }

   err = ALLOC_MM(bytes, cast_memblock(uri, mem_));
   if (err) goto ONERR;

   uri->nrparam=nrparam;

   size_t wmemoff=0; // index into uri->mem_addr

   uri->offset[uri_part_SCHEME]=0;
   if (pScheme && pScheme->size) {
      for (unsigned i=0; i<pScheme->size; ++i) {
         uint8_t c=(0x20|pScheme->addr[i]);
         uri->mem_addr[wmemoff++]=c;
      }
      uri->mem_addr[wmemoff++]=':';
   }

   for (uri_part_e part=uri_part_SCHEME+1, ipart=0; part<uri_part__NROF; ++part) {
      uri->offset[part]=(uint16_t)wmemoff;
      if (uri_part_QUERY==part) {
         if (nrparam) {
            uri->mem_addr[wmemoff++]='?';
            for (unsigned iparam=0; iparam<nrparam; ++iparam) {
               if (iparam) { uri->mem_addr[wmemoff++]='&'; }
               for (unsigned nv=0; nv<2; ++nv) {
                  if (nv==1 && params[iparam].name_value[nv].size) { uri->mem_addr[wmemoff++]='='; }
                  for (unsigned i=0; i<params[iparam].name_value[nv].size; ++i) {
                     uint8_t c=params[iparam].name_value[nv].addr[i];
                     if (c==' ') {
                        uri->mem_addr[wmemoff++]='+';
                     } else if (is_percent_encoded(uri_part_QUERY,c)) {
                        unsigned d1=((unsigned)c)>>4;
                        unsigned d2=((unsigned)c)&0x0f;
                        uri->mem_addr[wmemoff++]='%';
                        uri->mem_addr[wmemoff++]=(uint8_t)(d1+(d1<10?'0':'A'-10));
                        uri->mem_addr[wmemoff++]=(uint8_t)(d2+(d2<10?'0':'A'-10));
                     } else {
                        uri->mem_addr[wmemoff++]=c;
                     }
                  }
               }
            }
         }
      } else {
         if (parts[ipart] && parts[ipart]->size) {
            if (uri_part_AUTHORITY==part) {
               uri->mem_addr[wmemoff++]='/'; uri->mem_addr[wmemoff++]='/';
            } else if (uri_part_FRAGMENT==part) {
               uri->mem_addr[wmemoff++]='#';
            }
            for (unsigned i=0; i<parts[ipart]->size; ++i) {
               uint8_t c=parts[ipart]->addr[i];
               if (is_percent_encoded(part,c)) {
                  unsigned d1=((unsigned)c)>>4;
                  unsigned d2=((unsigned)c)&0x0f;
                  uri->mem_addr[wmemoff++]='%';
                  uri->mem_addr[wmemoff++]=(uint8_t)(d1+(d1<10?'0':'A'-10));
                  uri->mem_addr[wmemoff++]=(uint8_t)(d2+(d2<10?'0':'A'-10));
               } else {
                  uri->mem_addr[wmemoff++]=c;
               }
            }
         }
         if (uri_part_PATH==part) {
            normalize_path_r norm=normalize_path(wmemoff-uri->offset[part], uri->mem_addr+(size_t)uri->offset[part]);
            wmemoff       = norm.pathlen+uri->offset[part];
            uri->prefixlen= (uint16_t) norm.prefixlen;
         }
         ++ipart;
      }
   }
   uri->offset[uri_part__NROF]=(uint16_t)wmemoff;
   uri->mem_addr[wmemoff++]=0;

   assert(wmemoff <= uri->mem_size);

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

int initparse_uriencoded(/*out*/uri_encoded_t* uri, uint16_t size, const uint8_t str[size])
{
   int err;
   uri_encoded_t new_uri;

   err = parse_uri(&new_uri, type_ENCODED, size, str);
   if (err) goto ONERR;

   // set out
   *uri = new_uri;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}

// group: query

static inline uint16_t get_part_size(const uri_encoded_t* uri, uri_part_e part)
{
   return (uint16_t) (uri->offset[part+1] - uri->offset[part]);
}

static inline uri_part_t get_part(const uri_encoded_t* uri, uri_part_e part)
{
   return (uri_part_t) { .size = get_part_size(uri,part), .addr = uri->mem_addr + uri->offset[part] };
}

uri_part_t getpart_uriencoded(const uri_encoded_t* uri, uri_part_e part)
{
   if (part >= uri_part__NROF) {
      return (uri_part_t) { 0, 0 };
   }
   return get_part(uri,part);
}

int resolve_uriencoded(const uri_encoded_t* uri, const uri_encoded_t* base, uint16_t size, uint8_t str[size], /*eout*/uint16_t* nrbytes)
{
   int err;
   uri_part_e part;
   size_t part_size;
   size_t stroff=0;

   VALIDATE_INPARAM_TEST(base && (isabsolute_uriencoded(base) || 0==get_part_size(base,uri_part_PATH)),ONERR,);

   for (part=0; part<uri_part_FRAGMENT; ++part) {
      if (get_part_size(uri,part)) break;
   }

   part_size= (size_t)base->offset[part]-base->offset[0];
   if (part_size) {
      if (stroff+part_size <= size) {
         memcpy(str+stroff,base->mem_addr+base->offset[0],part_size);
         stroff += part_size;
      } else {
         return EOVERFLOW;
      }
   }

   if (part==uri_part_PATH && !isabsolute_uriencoded(uri)) {
      ++part;
      uri_part_t uripart=get_part(base,uri_part_PATH);
      uri_part_t uripart2=get_part(uri,uri_part_PATH);
      if (!uripart.size) {
         uripart.size=1;
         uripart.addr=(const uint8_t*)"/";
      }
      size_t pathlen;
      err=merge_path(&uripart, uri->prefixlen, &uripart2, size-stroff, str+stroff, &pathlen);
      if (err) return err;
      stroff+= pathlen;
   }

   part_size= (size_t)uri->offset[uri_part__NROF]-uri->offset[part];
   if (part_size) {
      if (stroff+part_size <= size) {
         memcpy(str+stroff,uri->mem_addr+uri->offset[part],part_size);
         stroff += part_size;
      } else {
         return EOVERFLOW;
      }
   }

   // set additional out
   *nrbytes=(uint16_t)stroff;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return err;
}


// section: uri_decoded_t

// group: lifetime

int free_uridecoded(uri_decoded_t* uri)
{
   int err;

   err = free_uriencoded(&uri->uri);
   uri->param = 0;
   if (err) goto ONERR;

   return 0;
ONERR:
   TRACEEXITFREE_ERRLOG(err);
   return err;
}

int init_uridecoded(/*out*/uri_decoded_t* uri, const uri_encoded_t* fromuri)
{
   int err;
   uri_decoded_t  new_uri;
   uint16_t       size= size_uriencoded(fromuri);
   const uint8_t* str = str_uriencoded(fromuri);

   err = parse_uri(&new_uri.uri, type_DECODED, size, str);
   if (err) goto ONERR;

   // set out
   *uri = new_uri;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return EINVAL;
}

int initparse_uridecoded(/*out*/uri_decoded_t* uri, uint16_t size, const uint8_t str[size])
{
   int err;
   uri_decoded_t new_uri;

   err = parse_uri(&new_uri.uri, type_DECODED, size, str);
   if (err) goto ONERR;

   // set out
   *uri = new_uri;

   return 0;
ONERR:
   TRACEEXIT_ERRLOG(err);
   return EINVAL;
}

// group: query

uri_param_t getparam_uridecoded(const uri_decoded_t* uri, unsigned iparam)
{
   if (iparam >= uri->uri.nrparam) {
      return (uri_param_t) uri_param_FREE;
   }
   uint16_t name_size = (uint16_t) (uri->param[iparam].valueoff-uri->param[iparam].nameoff);
   uint16_t val_size  = (uint16_t) (uri->param[iparam+1].nameoff-uri->param[iparam].valueoff);

   return (uri_param_t) {
      .name =  {  .size = name_size,
                  .addr = uri->uri.mem_addr + (size_t) uri->param[iparam].nameoff },
      .value = {  .size = val_size,
                  .addr = uri->uri.mem_addr + (size_t) uri->param[iparam].valueoff },
  };
}


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST

#define SETPATH(str)   memcpy(path, str, strlen(str)+1)

static int check_prefix(size_t prefixlen, uint8_t* path)
{
   TEST( 0==prefixlen || 1==prefixlen || (prefixlen>=2 && (prefixlen%3==0||prefixlen%3==2)));
   if (1==prefixlen) {
      TEST( '/'==path[0]);
   } else if (prefixlen>=3) {
      for (unsigned i=0; i<prefixlen; i+=3) {
         TEST(0 == memcmp(&path[i], "../", prefixlen>=i+3?3:2));
      }
      if (prefixlen%3 != 0) {
         TEST( 0 == path[prefixlen]);
      } else {
         TEST( 0 != memcmp(&path[prefixlen], "../", 3));
         TEST( 0 != memcmp(&path[prefixlen], "..", 2) || 0!=path[prefixlen+2]);
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int need_percent(uri_part_e part, unsigned c)
{
   switch(part) {
   case uri_part_SCHEME:      break;
   case uri_part_AUTHORITY:   return c<=32 || c>126 || (c=='%') || (c=='/') || (c=='?') || (c=='#');
   case uri_part_PATH:        return c<=32 || c>126 || (c=='%') || (c=='?') || (c=='#') || (c==':');
   case uri_part_QUERY:       return c<=32 || c>126 || (c=='%') || (c=='#') || (c=='=') || (c=='+') || (c=='&');
   case uri_part_FRAGMENT:    return c<=32 || c>126 || (c=='%');
   }
   return 0;
}

static int test_helper(void)
{
   uint8_t     path[256];
   uint8_t     value[256];
   size_t      S;
   normalize_path_r norm;
   parse_offsets_r offsets;
   uri_encoded_t uri = uri_encoded_FREE;

   // TEST hexvalue
   for (unsigned i=0; i<=255; ++i) {
      unsigned v=hexvalue(i);
      if ('0'<=i && i<='9') {
         TEST( v == (i-'0'));
      } else if ('A'<=i && i<='F') {
         TEST( v == (i-'A'+10));
      } else if ('a'<=i && i<='f') {
         TEST( v == (i-'a'+10));
      } else {
         TEST( v > 15); // invalid
      }
   }

   const char* test_merge_path[][3] = {
         { "/", "", "/"}, { "/a/b/c", "../../x.html", "/x.html"}, { "/a/b/c/", "../../x.html", "/a/x.html"},
         { "/", "../x", "/x"}, { "/", "../..", "/"}, { "/a/b/c/d/", "../../..", "/a/"},
         { "/a/b/c/a.css", "b.html", "/a/b/c/b.html"}, { "/a/b/c/", "f.html", "/a/b/c/f.html"},
   };

   // TEST merge_path: examples
   for (unsigned i=0; i<lengthof(test_merge_path); ++i) {
      uri_part_t base  = { .size=(uint16_t)strlen(test_merge_path[i][0]), .addr=(const uint8_t*)test_merge_path[i][0] };
      uri_part_t rel   = { .size=(uint16_t)strlen(test_merge_path[i][1]), .addr=(const uint8_t*)test_merge_path[i][1] };
      uri_part_t expect= { .size=(uint16_t)strlen(test_merge_path[i][2]), .addr=(const uint8_t*)test_merge_path[i][2] };
      size_t     prefixlen=0;
      while (0==strncmp(test_merge_path[i][1]+prefixlen,"../",3)) {
         prefixlen+=3;
      }
      if (0==strcmp(test_merge_path[i][1]+prefixlen,"..")) {
         prefixlen+=2;
      }
      TEST( 0 == merge_path(&base, (uint16_t)prefixlen, &rel, sizeof(path), path, &S));
      TEST( S == expect.size);
      TEST( 0 == memcmp(expect.addr, path, expect.size));
   }

   // TEST merge_path: EOVERFLOW
   {
      // test rel.size too large
      for (unsigned size=sizeof(path)-4; size<=1024; ++size) {
         uri_part_t base  = { .size=6, .addr=(const uint8_t*)"/a/b/c" };
         uri_part_t rel   = { .size=(uint16_t)size, .addr=0 };
         TEST( EOVERFLOW == merge_path(&base, 0, &rel, sizeof(path), path, &S));
      }
      // test base.size too large
      memset(path,'/',sizeof(path));
      for (unsigned size=1; size<=10; ++size) {
         uri_part_t base  = { .size=(uint16_t)sizeof(path), .addr=path };
         uri_part_t rel   = { .size=0, .addr=(const uint8_t*)"" };
         TEST( EOVERFLOW == merge_path(&base, 0, &rel, sizeof(path)-size, path, &S));
      }
   }

   const char* keepsame[] = { ".g", "g.", "..g", "g..", "/.g", "/g.", "/..g", "/g..",
                              "", "..", "../..", "../../a/b/c", "../../..g"
                              "aaa/bbb/ccc", "/aaa/bbb/ccc",
                              "/1./.b/.c", "/1../..b/..c", "..1/22../333./.444/5.."
                           };

   // TEST normalize_path: path does not change
   for (unsigned i=0; i<lengthof(keepsame); ++i) {
      size_t len=strlen(keepsame[i]);
      // test without ending in '/'
      memcpy(path, keepsame[i], len);
      norm = normalize_path(len, path);
      path[norm.pathlen < sizeof(path) ? norm.pathlen : sizeof(path)-1]=0;
      TESTP( len == norm.pathlen, "path:'%s' expect:'%s'",path, keepsame[i]);
      TESTP( 0 == memcmp(path, keepsame[i], len), "path:'%s' expect:'%s'", path, keepsame[i]);
      TEST( 0 == check_prefix(norm.prefixlen, path));
      // test ending in '/'
      memcpy(path, keepsame[i], len); path[len]='/';
      norm = normalize_path(len+1, path);
      path[norm.pathlen < sizeof(path) ? norm.pathlen : sizeof(path)-1]=0;
      TESTP( len+1 == norm.pathlen, "path:'%s' expect:'%s'", path, keepsame[i]);
      TESTP( 0 == memcmp(path, keepsame[i], len), "path:'%s' expect:'%s'", path, keepsame[i]);
      TEST( '/' == path[len]);
      TEST( 0 == check_prefix(norm.prefixlen, path));
   }

   const char* change[][2] = {
         { ".", ""}, { "./", ""}, { ".//", ""}, { ".///", ""}, { "././/.///", ""},
         { "/.", "/"}, { "/./", "/"}, { "/.//", "/"}, { "/.///", "/"}, { "/././/.///", "/"},
         { "/..", "/"}, { "/../", "/"}, { "/../..", "/"}, { "/../../", "/"},
         { "1/2/3/../../../../../../", "../../../" }, { "1/2/3/4/../../../../../..", "../.." },
         { "1/2/../../", "" }, { "1/2/../..", "" },
         { "/1/../..", "/" }, { "/1/../../", "/" },
         { "/456/../../../../a.html", "/a.html" },
         { "1/2/../456/../../../../a.html", "../../a.html" }, { "1/2/../3/../../../4/", "../4/" },
         { "//.//1//2//3//.//./.", "/1/2/3/"},
         { "//.//1//2//3//.//./.", "/1/2/3/"},
         { "/11/222/3333/..", "/11/222/" }, { "/11/222/3333/../", "/11/222/" },
         { "11/222/3333/../4", "11/222/4" }, { "11/222/../3333/../4/./5/", "11/4/5/" },
         { "/11/222/3333/./.././/.././/../4/5/../../6", "/6" }, { "//1//2//3//..//.//.././/..//4//5/../../6//", "/6/" },
         { "b/c/d;x", "b/c/d;x"},
   };

   // TEST normalize_path: normailze path
   for (unsigned i=0; i<lengthof(change); ++i) {
      size_t len=strlen(change[i][0]);
      size_t newlen=strlen(change[i][1]);
      memcpy(path, change[i][0], len);
      norm = normalize_path(len, path);
      path[norm.pathlen < sizeof(path) ? norm.pathlen : sizeof(path)-1]=0;
      TESTP( newlen == norm.pathlen, "path:'%s' expect:'%s'", path, change[i][1]);
      TESTP( 0 == memcmp(path, change[i][1], newlen), "path:'%s' expect:'%s'", path, change[i][1]);
      TEST( 0 == check_prefix(norm.prefixlen, path));
   }

   // TEST is_percent_encoded
   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      for (unsigned i=0; i<256; ++i) {
         int expect=need_percent(part,i);
         TEST( expect == is_percent_encoded(part,i));
      }
   }

   // TEST is_percent_encoded: uri_part_QUERY && ' ' ==> ' ' must be handled explicitly
   TEST( 1 == is_percent_encoded(uri_part_QUERY,' '));

   // == EXAMPLES ==

   const char* tp1_scheme[]={ "", ":", "http:" };
   const char* tp1_authority[]={ "", "//~\x7F", "// w w w" };
   const char* tp1_path[]={ "", "/",  "/pa th/../../a", "a/../../../b ", "..", "../../.." };
   const char* tp1_query[]={ "", "?",  "?a", "?=b &",  "?a==?b&&&", "?a%23=%23", "?a\x01=\x02\x03", "?a%%%%x==%%%x&b%3z=y%3!", "?a=1&b=2&c=3" };
   uint16_t    tp1_nrparam[]={ 0, 1,   1,    2,        4,           1,           1,                 2,                         3};
   const char* tp1_fragment[]={ "", "#", "##", "#%41%42%43", "#\x01\x02\x7E\x7F", "#\x01%41\x02%42\x03%43\x04" };
   const char**tp1_parts[uri_part__NROF]={ tp1_scheme, tp1_authority, tp1_path, tp1_query, tp1_fragment };
   unsigned tp1_i[uri_part__NROF];
   for (tp1_i[0]=0; tp1_i[0]<lengthof(tp1_scheme); ++tp1_i[0]) {
      for (tp1_i[1]=0; tp1_i[1]<lengthof(tp1_authority); ++tp1_i[1]) {
         for (tp1_i[2]=0; tp1_i[2]<lengthof(tp1_path); ++tp1_i[2]) {
            // skip authority and relative path
            if (tp1_path[tp1_i[2]][0]!='/' && tp1_authority[tp1_i[1]]!=0) continue;
            for (tp1_i[3]=0; tp1_i[3]<lengthof(tp1_query); ++tp1_i[3]) {
               for (tp1_i[4]=0; tp1_i[4]<lengthof(tp1_fragment); ++tp1_i[4]) {
                  size_t size=0;
                  size_t nrenc=0;   // bytes needed to by percent encoded
                  size_t nrperc=0;  // percent encoded chars
                  for (uri_part_e part=0; part<uri_part__NROF; ++part) {
                     const char* partval=tp1_parts[part][tp1_i[part]];
                     size_t partlen=strlen(partval);
                     memcpy(value+size, partval, partlen);
                     size+=partlen;
                     assert(size<sizeof(value));
                     for (size_t i=0,i2=0; i<partlen; ++i) {
                        uint8_t c=(uint8_t)partval[i];
                        nrenc+=(unsigned)(c<32||c>126)+(c==32&&part!=uri_part_QUERY);
                        if (c=='%' && i>=i2) { ++nrperc; i2=i+3; }
                     }
                  }
#if (0) // DEBUG
                  printf("(");
                  for (uri_part_e part=0; part<uri_part__NROF-1; ++part) {
                     printf("%d,", tp1_i[part]);
                  }
                  printf("%d)\n", tp1_i[uri_part__NROF-1]);
                  if (tp1_i[1]==1 && tp1_i[2]==3)
                      printf("OK\n");
#endif
                  // TEST parse_offsets: examples
                  offsets = parse_offsets(&uri, (uint16_t)size, value);
                  // check offsets
                  TEST( offsets.nr_percent_encoded == nrperc);
                  TEST( offsets.nr_need_encoding   == nrenc);
                  // check uri (unchanged parts)
                  TEST( 0 == uri.mem_addr);
                  TEST( 0 == uri.mem_size);
                  TEST( 0 == uri.prefixlen);
                  // check uri (set parts)
                  TEST( tp1_nrparam[tp1_i[uri_part_QUERY]] == uri.nrparam);
                  size_t partoff=0;
                  for (uri_part_e part=0; part<uri_part__NROF; ++part) {
                     TEST( partoff == uri.offset[part])
                     partoff += strlen(tp1_parts[part][tp1_i[part]]);
                  }
               }
            }
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

#define URISTR(str)   strlen(str), (const uint8_t*)str

static int check_isfree(uri_encoded_t* uri)
{
   TEST( 0 == uri->mem_addr);
   TEST( 0 == uri->mem_size);
   TEST( 0 == uri->offset[0]);
   TEST( 0 == uri->offset[uri_part__NROF]);
   TEST( 0 == uri->prefixlen);
   TEST( 0 == uri->nrparam);
   return 0;
ONERR:
   return EINVAL;
}

static int check_isfree2(uri_decoded_t* uri)
{
   TEST( 0 == check_isfree(&uri->uri));
   TEST( 0 == uri->param);
   return 0;
ONERR:
   return EINVAL;
}

static int check_string(const uri_encoded_t* uri, const uint8_t* str)
{
   size_t len=strlen((const char*)str);
   TEST( uri->mem_size == len+1);
   for (size_t i=0; i<=len; ++i) {
      TESTP( str[i] == uri->mem_addr[i], "i:%d %x!=%x", (int)i, str[i], uri->mem_addr[i]);
   }
   return 0;
ONERR:
   return EINVAL;
}

static const uint8_t* partprefix(uri_part_e part)
{
   return (part == uri_part_AUTHORITY) ? (const uint8_t*)"//" :
          (part == uri_part_QUERY)     ? (const uint8_t*)"?" :
          (part == uri_part_FRAGMENT)  ? (const uint8_t*)"#" : (const uint8_t*)"";
}

static const uint8_t* partsuffix(uri_part_e part)
{
   return (part == uri_part_SCHEME) ? (const uint8_t*)":" : (const uint8_t*)"";
}

static int check_encoded_part(const uri_encoded_t* uri, uri_part_e part, uint16_t len, const uint8_t* value)
{
   size_t i2=0;
   const uint8_t* prefix=partprefix(part);
   const uint8_t* suffix=partsuffix(part);

   for (size_t i=0; len && i<strlen((const char*)prefix); ++i, ++i2) {
      TEST( i2+1 <= uri->mem_size);
      TESTP( uri->mem_addr[i2] == prefix[i], "i:%d;i2:%d; 0x%x(%c)!=0x%x(%c)", (int)i, (int)i2, uri->mem_addr[i2], uri->mem_addr[i2], prefix[i], prefix[i]);
   }
   for (unsigned nv=0; nv<2; ++nv) {
      if (nv) {
         TESTP( uri->mem_addr[i2++] == (uint8_t)'=', "uri:%x(%c) hex:%x(%c)", uri->mem_addr[i2-1], uri->mem_addr[i2-1], '=', '=');
      }
      for (size_t i=0; i<len; ++i) {
         uint8_t c=value[i];
         if (need_percent(part, c) && (part!=uri_part_QUERY || c!=' ')) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            TEST( i2+3 <= uri->mem_size);
            TESTP( 0 == memcmp(uri->mem_addr+i2, hex, 3), "i:%d;i2:%d; uri:%.3s hex:%s", (int)i, (int)i2, uri->mem_addr+i2, hex);
            i2+=3;
         } else {
            c = (part==uri_part_QUERY && c==' ' ? '+' : c);
            TESTP( uri->mem_addr[i2] == c, "uri:%x(%c) hex:%x(%c)", uri->mem_addr[i2], uri->mem_addr[i2], c, c);
            i2++;
         }
      }
      if (part!=uri_part_QUERY || len==0) break;
   }
   for (size_t i=0; len && i<strlen((const char*)suffix); ++i) {
      TEST( i2+1 <= uri->mem_size);
      TEST( uri->mem_addr[i2++] == suffix[i]);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int check_decoded_part(const uri_decoded_t* uri, uri_part_e part, uint16_t len, const uint8_t* value)
{
   if (part==uri_part_QUERY) {
      uri_param_t param=getparam_uridecoded(uri, 0);
      TEST( 1 == nrparam_uridecoded(uri));
      for (unsigned nv=0; nv<2; ++nv) {
         TESTP( len == param.name_value[nv].size, "len:%d uri.size:%d", len, param.name_value[nv].size);
         TEST( param.name_value[nv].addr)
         if (len) {
            TESTP( 0 == memcmp(param.name_value[nv].addr, value, len), "part:'%s' expect:'%s'", param.name_value[nv].addr, value);
         }
      }
   } else {
      uri_part_t up=getpart_uridecoded(uri, part);
      TESTP( len == up.size, "len:%d uri.size:%d", len, up.size);
      TEST(    0 != up.addr);
      if (len) {
         TESTP( 0 == memcmp(up.addr, value, len), "part:'%s' expect:'%s'", up.addr, value);
      }
   }
   return 0;
ONERR:
   return EINVAL;
}

static int check_encoded_parts(const uri_encoded_t* uri, unsigned partbits, uint16_t len, const uint8_t* value)
{
   size_t i2=0;

   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      if ((partbits & (1u<<part))) {
         const uint8_t* prefix=partprefix(part);
         const uint8_t* suffix=partsuffix(part);

         for (size_t i=0; len && i<strlen((const char*)prefix); ++i, ++i2) {
            TEST( i2+1 <= uri->mem_size);
            TESTP( uri->mem_addr[i2] == prefix[i], "i:%d;i2:%d; 0x%x(%c)!=0x%x(%c)", (int)i, (int)i2, uri->mem_addr[i2], uri->mem_addr[i2], prefix[i], prefix[i]);
         }
         for (unsigned iparam=0; iparam<len; ++iparam) {
            if (iparam) { TESTP( uri->mem_addr[i2++] == (uint8_t)'&', "uri:%x(%c) hex:%x(%c)", uri->mem_addr[i2-1], uri->mem_addr[i2-1], '&', '&'); }
            for (unsigned nv=0; nv<2; ++nv) {
               if (nv) { TESTP( uri->mem_addr[i2++] == (uint8_t)'=', "uri:%x(%c) hex:%x(%c)", uri->mem_addr[i2-1], uri->mem_addr[i2-1], '=', '='); }
               for (size_t i=0; i<len; ++i) {
                  uint8_t c=value[i+30u*(part==uri_part_SCHEME)];
                  if (need_percent(part, c) && (part!=uri_part_QUERY || c!=' ')) {
                     char hex[4];
                     snprintf(hex, sizeof(hex), "%%%02X", c);
                     TEST( i2+3 <= uri->mem_size);
                     TESTP( 0 == memcmp(uri->mem_addr+i2, hex, 3), "i:%d;i2:%d; uri:%.3s hex:%s", (int)i, (int)i2, uri->mem_addr+i2, hex);
                     i2+=3;
                  } else {
                     c = (part==uri_part_QUERY && c==' ' ? '+' : c);
                     TESTP( uri->mem_addr[i2] == c, "uri:%x(%c) hex:%x(%c)", uri->mem_addr[i2], uri->mem_addr[i2], c, c);
                     i2++;
                  }
               }
               if (part!=uri_part_QUERY) break;
            }
            if (part!=uri_part_QUERY) break;
         }
         for (size_t i=0; len && i<strlen((const char*)suffix); ++i) {
            TEST( i2+1 <= uri->mem_size);
            TEST( uri->mem_addr[i2++] == suffix[i]);
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int check_parts(const uri_encoded_t* uri, uri_type_e utype, const char* partval[uri_part__NROF], uint16_t nrparam, uri_param_t params[nrparam])
{
   size_t size=0;
   const uint8_t* str=str_uriencoded(uri);

   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      size_t partlen=strlen(partval[part]);
      // check str
      TEST( 0 == memcmp(&str[size], partval[part], partlen));
      size+=partlen;
      TEST( size <= uri->mem_size);

      // check part
      uri_part_t uripart=getpart_uriencoded(uri, part);
      if (utype == type_DECODED) {
         uri_part_t part2=getpart_uridecoded((const uri_decoded_t*)uri, part);
         TEST( uripart.size == part2.size);
         TEST( uripart.addr == part2.addr);
      }
      TESTP( partlen == uripart.size, "part:%d size:%d expect:%zd", part, uripart.size, partlen);
      TEST( 0 == memcmp(uripart.addr, partval[part], partlen));
   }
   TEST( size == size_uriencoded(uri));

   // check params
   TEST( nrparam == nrparam_uriencoded(uri));
   if (utype == type_DECODED) {
      TEST( nrparam == nrparam_uridecoded((const uri_decoded_t*)uri));
      for (unsigned ip=0; ip<nrparam; ++ip) {
         uri_param_t param=getparam_uridecoded((const uri_decoded_t*)uri, ip);
         TEST( params[ip].name.size == param.name.size);
         TEST( params[ip].value.size == param.value.size);
         TEST( 0 == memcmp(params[ip].name.addr, param.name.addr, param.name.size));
         TEST( 0 == memcmp(params[ip].value.addr, param.value.addr, param.value.size));
      }
   }

   // check len of path prefix
   if (partval[uri_part_PATH][0]=='/') {
      TEST( 1 == uri->prefixlen);
   } else {
      size_t partlen=strlen(partval[uri_part_PATH]);
      size_t len=0;
      while (len+3<=partlen && strncmp(partval[uri_part_PATH]+len,"../",3)) {
         len+=3;
      }
      if (len+2==partlen && strncmp(partval[uri_part_PATH]+len,"..",2)) {
         len+=2;
      }
      TEST( len == uri->prefixlen);
   }

   return 0;
ONERR:
   return EINVAL;
}

static int check_decoded_parts(const uri_decoded_t* uri, unsigned partbits, uint16_t len, const uint8_t* value)
{
   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      if ((partbits & (1u<<part))) {
         uri_part_t up=getpart_uridecoded(uri,part);
         for (unsigned iparam=0; iparam<len; ++iparam) {
            uri_param_t param=getparam_uridecoded(uri,iparam);
            for (unsigned nv=0; nv<2; ++nv) {
               if (part==uri_part_QUERY) {
                  up = param.name_value[nv];
               }
               TEST( up.size == len);
               for (size_t i=0; i<len; ++i) {
                  uint8_t c=value[i+30u*(part==uri_part_SCHEME)];
                  TESTP( up.addr[i] == c, "uri:%x(%c) c:%x(%c)", up.addr[i], up.addr[i], c, c);
               }
               if (part!=uri_part_QUERY) break;
            }
            if (part!=uri_part_QUERY) break;
         }
      }
   }

   return 0;
ONERR:
   return EINVAL;
}

static int do_free(size_t size, uri_encoded_t* uri[size])
{
   for (size_t i=0; i<size; ++i) {
      TEST(0 == free_uriencoded(uri[i]));
      TEST(0 == check_isfree(uri[i]));
   }

   return 0;
ONERR:
   return EINVAL;
}


static int test_initfree(void)
{
   uri_encoded_t  uri  = uri_encoded_FREE;
   uri_encoded_t  uri2 = uri_encoded_FREE;
   uri_decoded_t  uri3 = uri_decoded_FREE;
   uri_encoded_t* freeuri[] = { &uri, &uri2, &uri3.uri };
   uri_part_t     parts[uri_part__NROF];
   uint8_t      * value;
   uri_param_t    params[256];
   memblock_t     mblock=memblock_FREE;

   // prepare
   TEST(0 == ALLOC_MM(65536, &mblock));
   value = mblock.addr;

   // TEST uri_encoded_FREE
   TEST( 0 == check_isfree(&uri));

   // TEST free_uriencoded
   for (unsigned init=0; init<5; ++init) {
      const char* url;
      switch (init) {
      case 0:  url="http://www/path/?1#1";
               TEST(0 == initparse_uridecoded(&uri3, (uint16_t)strlen(url), (const uint8_t*)url));
               TEST(0 == init_uriencoded(&uri, &uri3));
               break;
      case 1:  url="http:?1=2&3=4#title";
               TEST(0 == initparse_uriencoded(&uri, (uint16_t)strlen(url), (const uint8_t*)url));
               break;
      case 2:  TEST(0 == initbuild_uriencoded(&uri, &(uri_part_t){4,(const uint8_t*)"http"}, &(uri_part_t){6,(const uint8_t*)"server"}, &(uri_part_t){5,(const uint8_t*)"/path"},
                               1, (const uri_param_t[]) { { .name={4,(const uint8_t*)"name"}, .value={5,(const uint8_t*)"value"} } },
                               &(uri_part_t){8,(const uint8_t*)"fragment"}));
               break;
      }
      for (unsigned i=0; i<2; ++i) {
         TEST( 0 == free_uriencoded(&uri));
         TEST( 0 == check_isfree(&uri));
         TEST( 0 == free_uridecoded(&uri3));
         TEST( 0 == check_isfree2(&uri3));
      }
   }

   // TEST initbuild_uriencoded: NULL
   TEST( 0 == initbuild_uriencoded(&uri, 0, 0, 0, 0, 0, 0));
   TEST( 0 != str_uriencoded(&uri));
   TEST( 0 == size_uriencoded(&uri));
   TEST( 0 == nrparam_uriencoded(&uri));
   TEST( 0 == free_uriencoded(&uri));
   TEST( 0 == check_isfree2(&uri3));

   // == uri_part_SCHEME / EINVAL ==

   memset(parts, 0, sizeof(parts));
   for (unsigned i=0; i<256; ++i) {
      parts[uri_part_SCHEME].size=1;
      parts[uri_part_SCHEME].addr=value;
      value[0]=(uint8_t)i;
      value[1]=0;
      uint8_t* logbuffer;
      size_t   logsize, logsize2;
      if (('A'<=i && i<='Z') || ('a'<=i && i<='z')) {
         // TEST initbuild_uriencoded: uri_part_SCHEME && "all valid chars"
         TEST( 0 == initbuild_uriencoded(&uri, &parts[uri_part_SCHEME], 0, 0, 0, 0, 0));
         snprintf((char*)value, 10, "%c:", (i|0x20));
         TEST( 0 == check_string(&uri, value));
         TEST( 0 == free_uriencoded(&uri));
         TEST( 0 == check_isfree(&uri));
      } else {
         // TEST initbuild_uriencoded: uri_part_SCHEME && EINVAL
         GETBUFFER_ERRLOG(&logbuffer, &logsize);
         TEST( EINVAL == initbuild_uriencoded(&uri, &parts[uri_part_SCHEME], 0, 0, 0, 0, 0));
         GETBUFFER_ERRLOG(&logbuffer, &logsize2);
         TRUNCATEBUFFER_ERRLOG(logsize);
         TEST(logsize2 > logsize);
      }
   }

   // == SINGLE PART ===

   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      uint16_t nrparam=0;
      memset(parts, 0, sizeof(parts));
      for (unsigned len=0; len<512; ++len) {
         for (unsigned i=0; i<len; ++i) {
            value[i]=(uint8_t)(part==uri_part_SCHEME?(i%26)+'a':i);
         }
         if (part==uri_part_QUERY) {
            nrparam=1;
            for (unsigned nv=0; nv<2; ++nv) {
               params[0].name_value[nv].size=(uint16_t)len;
               params[0].name_value[nv].addr=value;
            }
         } else {
            parts[part].size=(uint16_t)len;
            parts[part].addr=value;
         }

         // TEST initbuild_uriencoded: single part
         TEST( 0 == initbuild_uriencoded(&uri, &parts[0], &parts[1], &parts[2], nrparam, params, &parts[4]));
         TESTP( 0 == check_encoded_part(&uri, part, (uint16_t)len, value), "part:%d len:%d\n",part,len);

         // TEST initbuild_uriencoded: single part (NULL param)
         TEST( 0 == do_free(lengthof(freeuri), freeuri));
         TEST( 0 == initbuild_uriencoded(&uri, part==0?&parts[0]:0, part==1?&parts[1]:0, part==2?&parts[2]:0, nrparam, params, part==4?&parts[4]:0));
         TESTP( 0 == check_encoded_part(&uri, part, (uint16_t)len, value), "part:%d len:%d\n",part,len);

         // TEST initparse_uriencoded: single part
         TEST( 0 == initparse_uriencoded(&uri2, uri.offset[uri_part__NROF], uri.mem_addr));
         TESTP( 0 == check_encoded_part(&uri2, part, (uint16_t)len, value), "part:%d len:%d, uri:%s got:%s\n",part,len,uri.mem_addr,uri2.mem_addr);

         // TEST init_uridecoded: single part
         TEST( 0 == init_uridecoded(&uri3, &uri));
         TESTP( 0 == check_decoded_part(&uri3, part, (uint16_t)len, value), "part:%d len:%d, uri:%s\n",part,len,uri.mem_addr);

         // TEST initparse_uridecoded: single part
         TEST(0 == free_uridecoded(&uri3));
         TEST( 0 == initparse_uridecoded(&uri3, uri.offset[uri_part__NROF], uri.mem_addr));
         TESTP( 0 == check_decoded_part(&uri3, part, (uint16_t)len, value), "part:%d len:%d, uri:%s\n",part,len,uri.mem_addr);

         // TEST init_uriencoded: single part
         TEST(0 == free_uriencoded(&uri2));
         TEST(  0 == init_uriencoded(&uri2, &uri3));
         TESTP( 0 == check_encoded_part(&uri2, part, (uint16_t)len, value), "part:%d len:%d, uri:%s got:%s\n",part,len,uri.mem_addr,uri2.mem_addr);

         // reset
         TEST(0 == do_free(lengthof(freeuri), freeuri));
      }
   }

   // === ANY COMBINATION of PARTS ===

   for (unsigned partbits=1; partbits<(2<<uri_part__NROF); ++partbits) {
      static_assert(30<lengthof(params), "len parameters supported");
      for (unsigned len=0; len<30; len=len+(len>3?13:1)) {
         for (unsigned i=0; i<len; ++i) {
            value[i]=(uint8_t)('0'+i);
            value[30+i]=(uint8_t)('a'+i);
         }
         uint16_t nrparam=0;
         memset(parts, 0, sizeof(parts));
         for (uri_part_e part=0; part<uri_part__NROF; ++part) {
            if ((partbits & (1u<<part))) {
               if (part==uri_part_QUERY) {
                  nrparam=(uint16_t)len;
                  for (unsigned i=0; i<nrparam; ++i) {
                     for (unsigned nv=0; nv<2; ++nv) {
                        params[i].name_value[nv].size=(uint16_t)len;
                        params[i].name_value[nv].addr=value;
                     }
                  }
               } else {
                  parts[part].size=(uint16_t)len;
                  parts[part].addr=(part==uri_part_SCHEME?value+30:value);
               }
            }
         }

         if (parts[uri_part_PATH].size && parts[uri_part_AUTHORITY].size) {
            value[0]='/'; // path needs to absolute in case of authority
         }

         // TEST initbuild_uriencoded
         TEST( 0 == initbuild_uriencoded(&uri, &parts[0], &parts[1], &parts[2], nrparam, params, &parts[4]));
         TESTP( 0 == check_encoded_parts(&uri, partbits, (uint16_t)len, value), "partbits:%x len:%d\n",partbits,len);

         // TEST initparse_uriencoded
         TEST( 0 == initparse_uriencoded(&uri2, uri.offset[uri_part__NROF], uri.mem_addr));
         TESTP( 0 == check_encoded_parts(&uri2, partbits, (uint16_t)len, value), "partbits:%x len:%d, uri:%s got:%s\n",partbits,len,uri.mem_addr,uri2.mem_addr);

         // TEST init_uridecoded: single part
         TEST( 0 == init_uridecoded(&uri3, &uri));
         TESTP( 0 == check_decoded_parts(&uri3, partbits, (uint16_t)len, value), "partbits:%x len:%d, uri:%s\n",partbits,len,uri.mem_addr);

         // TEST initparse_uridecoded: single part
         TEST(0 == free_uridecoded(&uri3));
         TEST( 0 == initparse_uridecoded(&uri3, uri.offset[uri_part__NROF], uri.mem_addr));
         TESTP( 0 == check_decoded_parts(&uri3, partbits, (uint16_t)len, value), "partbits:%x len:%d, uri:%s\n",partbits,len,uri.mem_addr);

         // TEST init_uriencoded
         TEST(0 == free_uriencoded(&uri2));
         TEST(  0 == init_uriencoded(&uri2, &uri3));
         TESTP( 0 == check_encoded_parts(&uri2, partbits, (uint16_t)len, value), "partbits:%x len:%d, uri:%s got:%s\n",partbits,len,uri.mem_addr,uri2.mem_addr);

         // reset
         TEST(0 == do_free(lengthof(freeuri), freeuri));
      }
   }

   // == PERCENT ENCODED ===

   for (uri_part_e part=uri_part_SCHEME+1; part<uri_part__NROF; ++part) {
      for (unsigned chr=0; chr<256; ++chr) {
         size_t pl=strlen((const char*)partprefix(part));
         memcpy(value+50, partprefix(part), pl);
         for (unsigned i=0; i<3; ++i) {
            value[i]=(uint8_t)chr;
            snprintf((char*)&value[50+pl+3*i], 4, "%%%02x", chr);
         }
         size_t len=(part==uri_part_PATH && '/'==chr)?1/*'///'=>'/'*/:3;
         size_t len_percent=9+pl;
         if (part==uri_part_QUERY) {
            value[50+len_percent]='=';
            memcpy(value+51+len_percent, value+50+pl, 3*3);
            len_percent+=10;
         }
         value[len]=0;
         value[50+len_percent]=0;

         // TEST initparse_uriencoded: percent normalization
         TEST( 0 == initparse_uriencoded(&uri, (uint16_t)len_percent, value+50));
         TESTP( 0 == check_encoded_part(&uri, part, (uint16_t)len, value), "part:%d chr:0x%x, uri:%s got:%s\n",part,chr,value+50,uri.mem_addr);

         // TEST initparse_uridecoded: decoding of unmormalized percent encoding
         TEST( 0 == initparse_uridecoded(&uri3, (uint16_t)len_percent, value+50));
         TESTP( 0 == check_decoded_part(&uri3, part, (uint16_t)len, value), "part:%d chr:0x%x, uri:%s got:%s\n",part,chr,value+50,uri.mem_addr);

         // reset
         TEST(0 == do_free(lengthof(freeuri), freeuri));

         // prepare invalid percent encoding
         if (!(('0'<=chr && chr<='9') || ('A'<=chr && chr<='F') || ('a'<=chr && chr<='f'))) {
            memcpy(value+50, partprefix(part), pl);
            snprintf((char*)&value[50+pl], 5, "0%%%c0", chr);
            snprintf((char*)&value[50+pl+4], 5, "1%%0%c", chr);
            len_percent=8+pl;
            if (part==uri_part_QUERY) {
               value[50+len_percent]='=';
               memcpy(value+51+len_percent, value+50+pl, 4*2);
               len_percent+=9;
            }
            value[3]=0;
            value[50+len_percent]=0;

            // TEST initparse_uriencoded: invalid percent encoding is skipped
            TEST( 0 == initparse_uriencoded(&uri, (uint16_t)len_percent, value+50));
            TESTP( 0 == check_encoded_part(&uri, part, 2, (const uint8_t*)"01"), "part:%d chr:0x%x, uri:%s got:%s\n",part,chr,value+50,uri.mem_addr);
            if (part==uri_part_QUERY) {
               TEST( 18+pl == uri.mem_size);
            } else {
               TEST( 9+pl == uri.mem_size);
            }

            // TEST initparse_uridecoded: invalid percent encoding is skipped
            TEST( 0 == initparse_uridecoded(&uri3, (uint16_t)len_percent, value+50));
            TESTP( 0 == check_decoded_part(&uri3, part, 2, (const uint8_t*)"01"), "part:%d chr:0x%x, uri:%s got:%s\n",part,chr,value+50,uri.mem_addr);
            if (part==uri_part_QUERY) {
               TEST( 10+pl+12 == uri3.uri.mem_size);
            } else {
               TEST( 5+pl+8 == uri3.uri.mem_size);
            }

            // reset
            TEST(0 == do_free(lengthof(freeuri), freeuri));
         }
      }
   }

   // TEST initparse_uriencoded: EOVERFLOW (percent encoding needs 3 times the size)
   memset(mblock.addr, 1, mblock.size); // will be percent encoded
   TEST( EOVERFLOW == initparse_uriencoded(&uri, UINT16_MAX, mblock.addr));

   // === MAX SIZE ===

   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      for (unsigned q=0; q<(part==uri_part_QUERY?3:1); ++q) {
         switch(part) {
         case uri_part_SCHEME:   memset(mblock.addr, 'h', mblock.size); mblock.addr[65534] = ':'; break;
         case uri_part_AUTHORITY:memset(mblock.addr, 'w', mblock.size); mblock.addr[0] = '/'; mblock.addr[1] = '/'; break;
         case uri_part_PATH:     memset(mblock.addr, 'p', mblock.size); mblock.addr[0] = '/'; break;
         case uri_part_QUERY:    if (q==0) { memset(mblock.addr, '&', mblock.size); mblock.addr[0] = '?'; break; }
                                 else if (q==1) { memset(mblock.addr, 'n', mblock.size); mblock.addr[0] = '?'; break; }
                                 else { memset(mblock.addr, 'v', mblock.size); mblock.addr[0] = '?'; mblock.addr[1] = '='; break; }
         case uri_part_FRAGMENT: memset(mblock.addr, '_', mblock.size); mblock.addr[0] = '#'; break;
         }

         // TEST initparse_uriencoded: UINT16_MAX
         TEST( 0 == initparse_uriencoded(&uri, UINT16_MAX, mblock.addr));
         uri_part_t uripart=getpart_uriencoded(&uri, part);
         switch(part) {
         case uri_part_SCHEME:   break;
         case uri_part_AUTHORITY:break;
         case uri_part_PATH:     TEST( 1 == uri.prefixlen); break;
         case uri_part_QUERY:    if (q==0) { TEST( UINT16_MAX == nrparam_uriencoded(&uri)); break; }
                                 else      { TEST( 1 == nrparam_uriencoded(&uri)); break; }
         case uri_part_FRAGMENT: break;
         }
         TEST( UINT16_MAX == size_uriencoded(&uri));
         TEST( 0 == memcmp(str_uriencoded(&uri), mblock.addr, UINT16_MAX));
         TEST( UINT16_MAX == uripart.size);
         TEST( 0 == free_uriencoded(&uri));
         TEST( 0 == check_isfree(&uri));

         // TEST initparse_uridecoded: UINT16_MAX
         TEST( 0 == initparse_uridecoded(&uri3, UINT16_MAX, mblock.addr));
         uripart=getpart_uridecoded(&uri3, part);
         switch(part) {
         case uri_part_SCHEME:   break;
         case uri_part_AUTHORITY:break;
         case uri_part_PATH:     TEST( 1 == uri3.uri.prefixlen); break;
         case uri_part_QUERY:    TEST( (q==0?UINT16_MAX:1) == nrparam_uridecoded(&uri3));
                                 for (unsigned i=0; i<nrparam_uridecoded(&uri3); ++i) {
                                    uri_param_t param=getparam_uridecoded(&uri3, i);
                                    TEST(0 == uri3.param[i].nameoff);
                                    for (unsigned nv=0; nv<2; ++nv) {
                                       if (q==1 && nv==0) {
                                          TEST( 65534 == param.name_value[nv].size);
                                          TEST( 0 == memcmp(param.name_value[nv].addr, value+1, param.name_value[nv].size));
                                       } else if (q==2 && nv==1) {
                                          TEST( 65533 == param.name_value[nv].size);
                                          TEST( 0 == memcmp(param.name_value[nv].addr, value+2, param.name_value[nv].size));
                                       } else {
                                          TEST( 0 == param.name_value[nv].size);
                                          TEST( 0 != param.name_value[nv].addr);
                                       }
                                    }
                                 }
                                 break;
         case uri_part_FRAGMENT: break;
         }
         if (part != uri_part_QUERY) {
            TEST( UINT16_MAX == uripart.size +strlen((const char*)partprefix(part)) +strlen((const char*)partsuffix(part)));
            TEST( 0 == memcmp(uripart.addr, mblock.addr+strlen((const char*)partprefix(part)), uripart.size));
         }
         TEST( 0 == free_uridecoded(&uri3));
         TEST( 0 == check_isfree2(&uri3));
      }
   }

   // == PARAMETER PARSING ==

   const char* tp1_prefix[]={ "?", "//www?", "http:?", "http://www?", "/path?", "http:/path?", "http://www/path?" };
   const char* tp1_param[]={ "", "=", "=2", "==2", "1", "1=", "1=2", "1==2" };
   for (unsigned nrparam=1; nrparam<=256; ++nrparam) {
      for (unsigned ipre=0; ipre<lengthof(tp1_prefix); ++ipre) {
         size_t presize=strlen(tp1_prefix[ipre]);
         for (unsigned ifirst=0; ifirst<lengthof(tp1_param); ++ifirst) {
            size_t size=presize;
            memcpy(value, tp1_prefix[ipre], presize);
            for (unsigned iparam=0; iparam<nrparam; ++iparam) {
               const char* param=tp1_param[(ifirst+iparam)%lengthof(tp1_param)];
               if (iparam) { value[size++]='&'; }
               memcpy(value+size, param, strlen(param));
               size += strlen(param);
            }
            unsigned isPath=(presize>5 && 0==strncmp("/path",tp1_prefix[ipre]+presize-6,5));

            // TEST initparse_uriencoded: parameter
            TEST( 0 == initparse_uriencoded(&uri, (uint16_t)size, value));
            TEST( isPath == uri.prefixlen);
            TEST( nrparam == nrparam_uriencoded(&uri));
            TEST( size == size_uriencoded(&uri));
            TEST( 0 == memcmp(str_uriencoded(&uri), value, size));

            // TEST initparse_uridecoded: parameter
            TEST( 0 == initparse_uridecoded(&uri3, (uint16_t)size, value));
            TEST( isPath == uri.prefixlen);
            TEST( nrparam == nrparam_uridecoded(&uri3));
            // check param
            for (unsigned iparam=0; iparam<nrparam; ++iparam) {
               const char* param=tp1_param[(iparam+ifirst)%lengthof(tp1_param)];
               int         isName  = (param[0]=='1');
               int         isValue = (strlen(param)?param[strlen(param)-1]=='2':0);
               int         isBig   = ((iparam+ifirst)%4 == 3);
               uri_param_t up=getparam_uridecoded(&uri3, iparam);
               TEST( isName         == up.name.size);
               TEST( isValue+isBig  == up.value.size);
               TEST( (isName?'1':0) == (isName?up.name.addr[0]:0));
               TEST( 0 == strncmp(isBig?"=2":"2", (const char*)up.value.addr, (size_t)(isValue+isBig)));
            }

            // reset
            TEST(0 == do_free(lengthof(freeuri), freeuri));
         }
      }
   }

   // === TEST EXAMPLES ===

   const char* tp2_scheme[]={ "", ":", "http:" };
   const char* tp2_scheme3[]={ "", "", "http" };
   const char* tp2_authority[]={ "", "//~\x7F", "// w w w" };
   const char* tp2_authority2[]={ "", "//~%7F", "//%20w%20w%20w" };
   const char* tp2_authority3[]={ "", "~\x7F", " w w w" };
   const char* tp2_path[]={ "", "/",  "/pa th/../../a", "a/../../../b ", "..", "../../.." };
   const char* tp2_path2[]={ "", "/", "/a",             "../../b%20",    "..", "../../.." };
   const char* tp2_path3[]={ "", "/", "/a",             "../../b   ",    "..", "../../.." };
   const char* tp2_query[]={ "", "?",  "?a", "?=b ",  "?a==?b", "?a%23=%23", "?a\x01=\x02\x03",             "?a%%%%x==%%%x&b%3z=y%3!" };
   const char* tp2_query2[]={ "", "?",  "?a", "?=b+", "?a==?b", "?a%23=%23", "?a\x25""01=\x25""02\x25""03", "?a=x&b=y" };
   const char* tp2_query3[]={ "", "",   "a",  "b ",   "a=?b",   "a##",       "a\001\002\003",               "axby" };
   uint16_t    tp2_nrpar[]={  0,  1,    1,    1,      1,        1,          1,                              2 };
   uri_param_t* tp2_params[]={ 0, (uri_param_t[]){{.name={.size=0},.value={.size=0}}},
                                  (uri_param_t[]){{.name={.size=1,.addr=(const uint8_t*)"a"},.value={.size=0}}},
                                  (uri_param_t[]){{.name={.size=0,},.value={.size=2,.addr=(const uint8_t*)"b "}}},
                                  (uri_param_t[]){{.name={.size=1,.addr=(const uint8_t*)"a"},.value={.size=3,.addr=(const uint8_t*)"=?b"}}},
                                  (uri_param_t[]){{.name={.size=2,.addr=(const uint8_t*)"a#"},.value={.size=1,.addr=(const uint8_t*)"#"}}},
                                  (uri_param_t[]){{.name={.size=2,.addr=(const uint8_t*)"a\x01"},.value={.size=2,.addr=(const uint8_t*)"\x02\x03"}}},
                                  (uri_param_t[]){{.name={.size=1,.addr=(const uint8_t*)"a"},.value={.size=1,.addr=(const uint8_t*)"x"}},{.name={.size=1,.addr=(const uint8_t*)"b"},.value={.size=1,.addr=(const uint8_t*)"y"}}},
                              };
   const char* tp2_fragment[]={ "", "#", "##", "#%41%42%43", "#\x01\x02\x7E\x7F", "#\x01%41\x02%42\x03%43\x04" };
   const char* tp2_fragment2[]={ "", "#", "##", "#ABC",      "#%01%02~%7F",     "#%01A%02B%03C%04" };
   const char* tp2_fragment3[]={ "", "",  "#",  "ABC",       "\x01\x02~\x7F",   "\001A\002B\003C\x04" };
   const char**tp2_parts[uri_part__NROF]={ tp2_scheme, tp2_authority, tp2_path, tp2_query, tp2_fragment };
   const char**tp2_parts2[uri_part__NROF]={ tp2_scheme, tp2_authority2, tp2_path2, tp2_query2, tp2_fragment2 };
   const char**tp2_parts3[uri_part__NROF]={ tp2_scheme3, tp2_authority3, tp2_path3, tp2_query3, tp2_fragment3 };
   unsigned tp2_i[uri_part__NROF];
   for (tp2_i[0]=0; tp2_i[0]<lengthof(tp2_scheme); ++tp2_i[0]) {
      for (tp2_i[1]=0; tp2_i[1]<lengthof(tp2_authority); ++tp2_i[1]) {
         for (tp2_i[2]=0; tp2_i[2]<lengthof(tp2_path); ++tp2_i[2]) {
            // skip authority and relative path
            if (tp2_path[tp2_i[2]][0]!='/' && tp2_authority[tp2_i[1]]!=0) continue;
            for (tp2_i[3]=0; tp2_i[3]<lengthof(tp2_query); ++tp2_i[3]) {
               for (tp2_i[4]=0; tp2_i[4]<lengthof(tp2_fragment); ++tp2_i[4]) {
                  size_t size=0;
                  size_t esize=0;   // bytes needed to by percent encoded
                  size_t nrperc=0;  // percent encoded chars
                  const char* partval2[uri_part__NROF];
                  const char* partval3[uri_part__NROF];
                  for (uri_part_e part=0; part<uri_part__NROF; ++part) {
                     const char* partval=tp2_parts[part][tp2_i[part]];
                     size_t partlen=strlen(partval);
                     memcpy(value+size, partval, partlen);
                     size+=partlen;
                     for (size_t i=0,i2=0; i<partlen; ++i) {
                        uint8_t c=(uint8_t)partval[i];
                        esize+=2u*(c<32||c>126)+2u*(c==32&&part!=uri_part_QUERY);
                        if (c=='%' && i>=i2) { ++nrperc; i2=i+3; }
                     }
                     partval2[part]=tp2_parts2[part][tp2_i[part]];
                     partval3[part]=tp2_parts3[part][tp2_i[part]];
                  }
#if (0) // DEBUG
                  printf("(");
                  for (uri_part_e part=0; part<uri_part__NROF-1; ++part) {
                     printf("%d,", tp2_i[part]);
                  }
                  printf("%d)\n", tp2_i[uri_part__NROF-1]);
                  if (tp2_i[1]==1 && tp2_i[2]==1)
                     printf("OK\n");
#endif
                  // TEST initparse_uriencoded: examples
                  TEST( 0 == initparse_uriencoded(&uri, (uint16_t)size, value));
                  TEST( size+esize+1/*\0-byte*/ == uri.mem_size);
                  TEST( 0 == check_parts(&uri, type_ENCODED, partval2, tp2_nrpar[tp2_i[3]], tp2_params[tp2_i[3]]));

                  // TEST initparse_uriencoded: examples
                  TEST( 0 == initparse_uridecoded(&uri3, (uint16_t)size, value));
                  TESTP( size-2u*nrperc+1/*\0-byte*/+4u*(2u+tp2_nrpar[tp2_i[3]]) == uri3.uri.mem_size, "uri.size:%zd expected:%zd", uri3.uri.mem_size, size-2u*nrperc+1);
                  TEST( 0 == check_parts(&uri3.uri, type_DECODED, partval3, tp2_nrpar[tp2_i[3]], tp2_params[tp2_i[3]]));

                  if (  1!=strlen(tp2_scheme[tp2_i[uri_part_SCHEME]])      // can not build empty ':'
                     && 1!=strlen(tp2_fragment[tp2_i[uri_part_FRAGMENT]])  // can not build empty '#'
                     && 0==strstr(tp2_query[tp2_i[uri_part_QUERY]],"=="))  // '=' would be percent encoded
                  {
                     uri_part_t build[uri_part__NROF];
                     for (uri_part_e part=0; part<uri_part__NROF; ++part) {
                        build[part].size =(uint16_t)strlen(tp2_parts3[part][tp2_i[part]]);
                        build[part].addr =(const uint8_t*)tp2_parts3[part][tp2_i[part]];
                     }

                     // TEST initbuild_uridecoded: examples
                     TEST( 0 == initbuild_uriencoded(&uri2, build+0, build+1, build+2, tp2_nrpar[tp2_i[3]], tp2_params[tp2_i[3]], build+4));
                     TEST( size+esize+1/*\0-byte*/ == uri.mem_size);
                     TEST( 0 == check_parts(&uri2, type_ENCODED, partval2, tp2_nrpar[tp2_i[3]], tp2_params[tp2_i[3]]));

                     // TEST init_uriencoded
                     TEST( 0 == free_uriencoded(&uri2));
                     TEST( 0 == init_uriencoded(&uri2, &uri3))
                     TEST( 0 == check_parts(&uri2, type_ENCODED, partval2, tp2_nrpar[tp2_i[3]], tp2_params[tp2_i[3]]));
                  }

                  // TEST init_uridecoded: examples
                  TEST( 0 == free_uridecoded(&uri3));
                  TEST( 0 == init_uridecoded(&uri3, &uri));
                  TEST( 0 == check_parts(&uri3.uri, type_DECODED, partval3, tp2_nrpar[tp2_i[3]], tp2_params[tp2_i[3]]));

                  // reset
                  TEST(0 == do_free(lengthof(freeuri), freeuri));
               }
            }
         }
      }
   }

   // reset
   TEST(0 == do_free(lengthof(freeuri), freeuri));
   TEST(0 == FREE_MM(&mblock));

   return 0;
ONERR:
   do_free(lengthof(freeuri), freeuri);
   FREE_MM(&mblock);
   return EINVAL;
}

static int test_query(void)
{
   uri_encoded_t uri = uri_encoded_FREE;
   uri_decoded_t uri2 = uri_decoded_FREE;
   uint32_t      parambuffer[257];

   // TEST isabsolute_uriencoded
   for (uint16_t pl=0; pl<=10; ++pl) {
      uri.prefixlen=pl;
      TEST( (pl==1) == isabsolute_uriencoded(&uri));
   }

   // TEST isabsolute_uridecoded
   for (uint16_t pl=0; pl<=10; ++pl) {
      uri2.uri.prefixlen=pl;
      TEST( (pl==1) == isabsolute_uridecoded(&uri2));
   }

   // TEST getpart_uriencoded
   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      for (uintptr_t addr=1; addr; addr<<=1) {
         for (unsigned off=0; off<100; off+=15) {
            for (unsigned size=0; size<30; size+=3) {
               uri.mem_addr    = (uint8_t*)addr;
               uri.offset[part]= (uint16_t)(off);
               uri.offset[part+1]= (uint16_t)(off+size);
               uri_part_t uripart= getpart_uriencoded(&uri, part);
               TEST( uripart.size == size);
               TEST( uripart.addr == (uint8_t*)(addr+off));
            }
         }
      }
   }

   // TEST getpart_uridecoded
   for (uri_part_e part=0; part<uri_part__NROF; ++part) {
      for (uintptr_t addr=1; addr; addr<<=1) {
         for (unsigned off=0; off<100; off+=15) {
            for (unsigned size=0; size<30; size+=3) {
               uri2.uri.mem_addr    = (uint8_t*)addr;
               uri2.uri.offset[part]= (uint16_t)(off);
               uri2.uri.offset[part+1]= (uint16_t)(off+size);
               uri_part_t uripart= getpart_uridecoded(&uri2, part);
               TEST( uripart.size == size);
               TEST( uripart.addr == (uint8_t*)(addr+off));
            }
         }
      }
   }

   // TEST getparam_uridecoded
   for (unsigned iparam=1; iparam<256; ++iparam) {
      uri2.uri.nrparam= (uint16_t)(iparam+1);
      uri2.param= (void*)parambuffer;
      for (uintptr_t addr=1; addr; addr<<=1) {
         for (unsigned off=0; off<100; off+=15) {
            for (unsigned nsize=0; nsize<10; nsize+=3) {
               for (unsigned vsize=0; vsize<10; vsize+=3) {
                  uri2.uri.mem_addr = (uint8_t*)addr;
                  uri2.param[iparam].nameoff = (uint16_t)(off);
                  uri2.param[iparam].valueoff= (uint16_t)(off+nsize);
                  uri2.param[iparam+1].nameoff= (uint16_t)(off+nsize+vsize);
                  uri_param_t param= getparam_uridecoded(&uri2, iparam);
                  TEST( param.name_value[0].size == nsize);
                  TEST( param.name_value[0].addr == (uint8_t*)(addr+off));
                  TEST( param.name_value[1].size == vsize);
                  TEST( param.name_value[1].addr == (uint8_t*)(addr+off+nsize));
                  TEST( param.name.size == nsize);
                  TEST( param.name.addr == (uint8_t*)(addr+off));
                  TEST( param.value.size == vsize);
                  TEST( param.value.addr == (uint8_t*)(addr+off+nsize));
               }
            }
         }
      }
   }

   // TEST getparam_uridecoded: iparam>nrparam
   for (unsigned iparam=0; iparam<256; ++iparam) {
      uri2.uri.nrparam = (uint16_t)(iparam+1);
      uri_param_t param= getparam_uridecoded(&uri2, iparam+1);
      TEST( param.name.size == 0);
      TEST( param.name.addr == 0);
      TEST( param.value.size == 0);
      TEST( param.value.addr == 0);
   }

   // TEST nrparam_uriencoded
   for (uint16_t p=1; p<16; ++p) {
      uri.nrparam=p;
      TEST( p == nrparam_uriencoded(&uri));
   }

   // TEST nrparam_uridecoded
   for (uint16_t p=1; p<16; ++p) {
      uri2.uri.nrparam=p;
      TEST( p == nrparam_uridecoded(&uri2));
   }

   // TEST str_uriencoded
   for (uintptr_t p=1; p; p<<=1) {
      uri.mem_addr=(uint8_t*)p;
      for (unsigned off=0; off<100; off+=15) {
         uri.offset[0] = (uint16_t)off;
         TEST( p+off == (uintptr_t)str_uriencoded(&uri));
      }
   }

   // TEST size_uriencoded
   for (unsigned s=1; s<=UINT16_MAX; s<<=1) {
      uri.offset[uri_part__NROF]=(uint16_t)s;
      TEST( s == size_uriencoded(&uri));
   }

   return 0;
ONERR:
   return EINVAL;
}

static int test_resolve(void)
{
   uri_encoded_t uri = uri_encoded_FREE;
   uri_encoded_t uri2 = uri_encoded_FREE;
   uri_encoded_t*freeuri[]={&uri, &uri2};
   const char  * str;
   const char  * str2;
   uint8_t buffer[512];
   uint16_t bytes;

   const char* tp1_resolve[][3] = {
         { "http://www.de/Path/file?X#Y", "", "http://www.de/Path/file?X" },
         { "http://www.de/Path/file?X#Y", "../x.html", "http://www.de/x.html" },
         { "http://www.de/Path/file?X#Y", "x.html", "http://www.de/Path/x.html" },
         { "http://www.de/Path/file?X#Y", "?n1=v1&n2=v2", "http://www.de/Path/file?n1=v1&n2=v2" },
         { "http://www.de/Path/file?X#Y", "#fragment", "http://www.de/Path/file?X#fragment" },
         { "http://www.de/Path/file?X#Y", "?p1#fragment", "http://www.de/Path/file?p1#fragment" },
         { "http://www.de/Path/file?X#Y", "F?p1#fragment", "http://www.de/Path/F?p1#fragment" },
         { "http://www.de/Path/file?X#Y", "//WWW/path/f?p1#fragment", "http://WWW/path/f?p1#fragment" },
         { "http://www.de/Path/file?X#Y", "http://1/2/3?4#5", "http://1/2/3?4#5" },
         { "http://www.de/Path/file?X#Y", "http:1/2/3?4#5", "http:1/2/3?4#5" }, // strict ==> keeps relative path if http: set
         { "http://www.de/Path/file?X#Y", "1/2/3?4#5", "http://www.de/Path/1/2/3?4#5" },
   };

   // TEST resolve_uriencoded
   for (unsigned i=0; i<lengthof(tp1_resolve); ++i) {
      size_t S=strlen(tp1_resolve[i][2]);
      str =tp1_resolve[i][1];
      str2=tp1_resolve[i][0];
      TEST( 0 == initparse_uriencoded(&uri, (uint16_t)strlen(str), (const uint8_t*)str));
      TEST( 0 == initparse_uriencoded(&uri2, (uint16_t)strlen(str2), (const uint8_t*)str2));
      TEST( 0 == resolve_uriencoded(&uri, &uri2, sizeof(buffer), buffer, &bytes));
      TEST( S == bytes);
      TEST( 0 == memcmp(buffer, tp1_resolve[i][2], bytes));
      TEST( 0 == do_free(lengthof(freeuri), freeuri));
   }

   // TEST resolve_uriencoded: EOVERFLOW
   for (uri_part_e part=0; part<=uri_part__NROF; ++part) {
      str ="http://YYY/PATH?P1=V#2345";
      str2="http://yyy/path?p1=v#____";
      str += part*5u;
      TEST( 0 == initparse_uriencoded(&uri, (uint16_t)strlen(str), (const uint8_t*)str));
      TEST( 0 == initparse_uriencoded(&uri2, (uint16_t)strlen(str2), (const uint8_t*)str2));
      // test absolute path
      TEST( 0 == resolve_uriencoded(&uri, &uri2, 5u*uri_part__NROF, buffer, &bytes));
      for (unsigned m=1; m<5; ++m) {
         TEST( EOVERFLOW == resolve_uriencoded(&uri, &uri2, (uint16_t)((4u+(part!=5))*uri_part__NROF-m), buffer, &bytes));
      }
      TEST( 0 == do_free(lengthof(freeuri), freeuri));
      // test relative path
      for (unsigned off=0; off<=6; off+=6) {
         str ="../../x.html"+off;
         str2="http://server/a/b/c/"; // 16 bytes ".../a/" total 20 bytes
         TEST( 0 == initparse_uriencoded(&uri, (uint16_t)strlen(str), (const uint8_t*)str));
         TEST( 0 == initparse_uriencoded(&uri2, (uint16_t)strlen(str2), (const uint8_t*)str2));
         TEST( 0 == resolve_uriencoded(&uri, &uri2, off?20+6:16+6, buffer, &bytes));
         for (unsigned m=1; m<5; ++m) {
            TEST( EOVERFLOW == resolve_uriencoded(&uri, &uri2, (uint16_t)((off?26u:22u)-m), buffer, &bytes));
         }
         TEST( 0 == do_free(lengthof(freeuri), freeuri));
      }
   }

   // TEST resolve_uriencoded: EINVAL
   str ="http://www/path/file";
   str2="http:../path/file";
   TEST( 0 == initparse_uriencoded(&uri, (uint16_t)strlen(str), (const uint8_t*)str));
   TEST( 0 == initparse_uriencoded(&uri2, (uint16_t)strlen(str2), (const uint8_t*)str2));
   TEST( EINVAL == resolve_uriencoded(&uri, 0, sizeof(buffer), buffer, &bytes));
   TEST( EINVAL == resolve_uriencoded(&uri, &uri2, sizeof(buffer), buffer, &bytes));
   TEST( 0 == do_free(lengthof(freeuri), freeuri));

   // TEST resolve_uriencoded: empty path allowd (replaced by '/')
   str ="F?v1";
   str2="http://www?v2";
   TEST( 0 == initparse_uriencoded(&uri, (uint16_t)strlen(str), (const uint8_t*)str));
   TEST( 0 == initparse_uriencoded(&uri2, (uint16_t)strlen(str2), (const uint8_t*)str2));
   TEST( 0 == resolve_uriencoded(&uri, &uri2, sizeof(buffer), buffer, &bytes));
   TEST( 15== bytes);
   TEST( 0 == memcmp(buffer, "http://www/F?v1", bytes));
   TEST( 0 == do_free(lengthof(freeuri), freeuri));

   return 0;
ONERR:
   do_free(lengthof(freeuri), freeuri);
   return EINVAL;
}

int unittest_io_www_uri()
{
   if (test_helper())      goto ONERR;
   if (test_initfree())    goto ONERR;
   if (test_query())       goto ONERR;
   if (test_resolve())     goto ONERR;

   return 0;
ONERR:
   return EINVAL;
}

#endif
