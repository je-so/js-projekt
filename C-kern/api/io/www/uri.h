/* title: Uniform-Resource-Identifier

   Eine URI ist eine Zeichenfolge, die eine abstrakte oder physische Ressource
   bezeichnet. Eine URI dient als eindeutiger Name oder bezeichnet die Lokation,
   wo die Ressource zu finden ist und wie auf sie zugegriffen werden kann
   (Protokoll oder Schema).

   URI Komponenten als regulärer Ausdruck:
   ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?
     |         |   |          | |      ||         | |     |
     [S]       :   [A]          [P]     [Q]         [F]

   [S]: (optional) Schema/Protokollname (scheme) und Endemarke ':', z.B. "http:".
   [A]: (optional) Beginmarke '//' und Standortname (authority) des Servers (DNS-Name).
                   Der Name endet mit dem ersten '/', '?' oder '#' oder dem Ende der URI.
                   Beispiel '//www.wikipedia.de'.
   [P]: (optional) Pfad (path) einer Ressource bezogen auf den Standort. Ist [A] vorhanden,
                   muss der Pfad mit einem '/' beginnen oder leer sein. Ist [A] nicht vorhanden,
                   kann der Pfad auch relativ sein (kein '/' am Anfang). Jedenfalls darf er nicht
                   mit '//' beginnen, falls [A] weggelassen wurde. Der Pfad endet mit dem ersten
                   '?', '#' oder dem Ende der URI. Beispiel '/angebote/maenner/index.html'
                   oder nur 'angebote', falls [A] fehlt.
   [Q]: (optional) Zusätzliche(r) Suchparameter (query), beginnt mit '?'.
                   Die Begriffe werden uri-enkodiert, z.B. '?suche=begriff&sortierung=aufsteigend'.
                   Der Name des Parameter wird gefolgt von '=' und seinem Wert.
                   Mehrere Parameter werden mittels '& aneinandergekettet und Leerzeichen werden
                   als '+' kodiert. Der Wert des letzten Suchparameters endet mit dem ersten '#'
                   oder dem Ende der URI.
   [F]: (optional) Fragmentname (fragment), der einen Teil innerhalb der gesamten Ressource
                   identifiziert, beginnt mit einem '#'.


   Prozent Enkodierung:
   Der reguläre Ausdruck für eine Zeichenkodierung lautet %[0-9a-fA-F][0-9a-fA-F].
   Ein einzelnes Zeichen, das seine besondere Bedeutung innerhalb einer URI verlieren soll,
   z.B. falls '?' oder ':' innerhalb eines Pfadnames vorkommt, wird als '%' Zeichen gefolgt
   von einer zweistelligen Hexadezimalzahl dargestellt, ausgehend von seinem ASCII-Code.

   Copyright:
   This program is free software. See accompanying LICENSE file.

   Author:
   (C) 2017 Jörg Seebohn

   file: C-kern/api/io/www/uri.h
    Header file <Uniform-Resource-Identifier>.

   file: C-kern/io/www/uri.c
    Implementation file <Uniform-Resource-Identifier impl>.
*/
#ifndef CKERN_IO_WWW_URI_HEADER
#define CKERN_IO_WWW_URI_HEADER

// forward
struct wbuffer_t;

// === exported types
struct uri_encoded_t;
struct uri_decoded_t;

/* enum: uri_scheme_e
 * Angabe der vom Parser unterstützten URI-Schemas (Protokollen). */
typedef enum uri_scheme_e {
  uri_scheme_UNKONWN,
  uri_scheme_HTTP,
} uri_scheme_e;

/* enum: uri_part_e
 * Dient der Benennung eines bestimmten Teils der URI. */
typedef enum uri_part_e {
  uri_part_SCHEME,         // encoded example: http:
  uri_part_AUTHORITY,      // encoded example: //www.server.com
  uri_part_PATH,           // encoded example: /1/2/3.html
  uri_part_QUERY,          // encoded example: ?p1=v1&p2=v2
  uri_part_FRAGMENT,       // encoded example: #title1
} uri_part_e;

#define uri_part__NROF (uri_part_FRAGMENT+1)


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_www_uri
 * Test <uri_t> functionality. */
int unittest_io_www_uri(void);
#endif


/* struct: uri_part_t
 * Beschreibt den Wert eines URI-Teils (siehe <uri_part_e>). */
typedef struct uri_part_t {
   uint16_t       size; // length of string
   const uint8_t* addr; // *no* \0-terminated string, *but* could contain \0-bytes !
} uri_part_t;

#define uri_part_FREE  { 0, 0 }


/* struct: uri_param_t
 * Trägt die Stringwerte eines Parameternamens und dessen zugewiesenen Wertes. */
typedef struct uri_param_t {
   union {
      uri_part_t name_value[2]; // name: name_value[0], value: name_value[1]
      struct {
         uri_part_t name;  // name string of parameter (no \0-terminating byte, could contain \0-bytes)
         uri_part_t value; // assigned string value of parameter (no \0-terminating byte, could contain \0-bytes)
      };
   };
} uri_param_t;

#define uri_param_FREE  { {{ uri_part_FREE, uri_part_FREE }} }


/* struct: uri_encoded_t
 * Speichert eine (http-protocol-specific) URI in einem neu
 * allokierten Speicherblock.
 * Die URI kann als ganzer String abgefragt werden, oder einzelne
 * Abschnitte (<uri_part_e>) daraus. Die zurückgegebenen Werte enthalten
 * Prefixe wie '?', '#', und "//" oder das Suffix ':'. Die Werte sind
 * nötigenfalls URL-kodiert (ein "\n" wird als URL-kodiert zu "%0A").
 * Die URI kann von einem String geparst oder aus einzelnen Teilen
 * zusammengebaut werden (<initparse_uriencoded>, <initbuild_uriencoded>). */
typedef struct uri_encoded_t {
   uint8_t* mem_addr;   // start addr of allocated memory
   size_t   mem_size;   // size in bytes of allocated memory
   uint16_t offset[uri_part__NROF+1]; // offsets of all uri parts relative to mem_addr, len of part is determined by offset[part+1]-offset[part]
   uint16_t prefixlen;  // 1: absolute path, 3n(n>0): prefix is n times "../", 3n-1(n>0): prefix is (n-1) times "../" plus following ".."
   uint16_t nrparam;    // number of (encoded/decoded) parameter (encoded "?a=1&b=2...", decoded "a1b2..."
} uri_encoded_t;

/* define: uri_encoded_FREE
 * Static initializer. */
#define uri_encoded_FREE \
         { .mem_addr = 0, .mem_size = 0 }

/* function: init_uriencoded
 * Verwandelt eine <uri_decoded_t> um in eine <uri_encoded_t>.
 * Die einzelnen Abschnitte einer URI werden vorher normalisiert. Siehe auch <initparse_uriencoded>. */
int init_uriencoded(/*out*/uri_encoded_t* uri, const struct uri_decoded_t* fromuri);

/* function: initbuild_uriencoded
 * Baut eine URI zusammen aus ihren einzelnen Bestandteilen. Die nötigenn Suffixe bzw. Prefixe
 * werden automatisch hinzugefügt. Unbenötigte Teile können als 0(NULL) übergeben werden.
 * Die einzelnen Abschnitte einer URI werden vorher normalisiert. Siehe auch <initparse_uriencoded>.
 *
 * Example:
 * - Für die URI "http://server.de?p1=v1#title" erhält initbuild_uriencoded folgende Parameter: (&uri,"http","server.de",0,1,{ "p1", "v1" },"title").
 * - Für die URI "../image.png" erhält initbuild_uriencoded folgende Parameter: (&uri,0,0,"../image.png",0,0,0).
 */
int initbuild_uriencoded(/*out*/uri_encoded_t* uri, const uri_part_t* pScheme, const uri_part_t* pAuthority, const uri_part_t* pPath, uint16_t nrparam, const uri_param_t params[nrparam], const uri_part_t* pFragment);

/* function: initparse_uriencoded
 * Zerlegt eine kodierten URI-String in seine (kodierten) Bestandteile und speichert sie kodiert.
 * Die einzelnen Abschnitte einer URI werden vorher normalisiert. Falls Pfade. "../" enthalten,
 * werden diese aufgelöst oder an die Spitze des Pfades verschoben und überflüssige "/./" werden
 * gelöscht. ASCII-Wert zwischen [0..32] und [127..255] werden in Prozentdarstellung kodiert (%1A). */
int initparse_uriencoded(/*out*/uri_encoded_t* uri, uint16_t size, const uint8_t str[size]);

// group: query

/* function: isabsolute_uriencoded
 * Gibt true zurück, falls der Pfad mit einem '/' beginnt.
 * Eine uri_part_AUTHORITY enhaltende URI muss entweder einen leeren oder
 * absoluten Pfad enthalten. */
static inline int isabsolute_uriencoded(const uri_encoded_t* uri);

/* function: getpart_uriencoded
 * Gibt den String eines bestimmten Teils einer URI zurück.
 * Siehe <uri_part_e> für eine Liste aller Teile. */
uri_part_t getpart_uriencoded(const uri_encoded_t* uri, uri_part_e part);

/* function: nrparam_uriencoded
 * Gibt die Anzahl Parameter einer URI zurück. Die Parameter werden
 * im Abschnitt uri_part_QUERY kodiert. */
static inline uint16_t nrparam_uriencoded(const uri_encoded_t* uri);

/* function: str_uriencoded
 * Gibt die Startadresse des die URI beschreibenden Strings zurück.
 * Dieser kann per HTTP übertragen werden. */
static inline const uint8_t* str_uriencoded(const uri_encoded_t* uri);

/* function: size_uriencoded
 * Liefert die Länge des von <str_uriencoded> zurückgegebnen Strings. */
static inline uint16_t size_uriencoded(const uri_encoded_t* uri);

/* function: resolve_uriencoded
 * Liefert den kodierten String einer zusammengesetzten URI zurück.
 * Der URI-String wird aus einer absoluten URI base und der relativen,
 * unvollständigen URI uri zusammengesetzt und in str zurückgegeben.
 * Die Länge der URI wird in nrbytes geliefert.
 *
 * Returns:
 * 0         - OK.
 * EOVERFLOW - ERROR cause of: (used_size_of(base) + used_size_of(uri) > size)
 * EINVAL    - ERROR cause of: !isabsolute(base) && getpart(base,uri_part_PATH).size > 0
 * */
int resolve_uriencoded(const uri_encoded_t* uri, const uri_encoded_t* base, uint16_t size, /*eout*/uint8_t str[size], /*out*/uint16_t* nrbytes);


/* struct: uri_decoded_t
 * Speichert die aus einer kodierten Repräsentation einer URI dekodierten Werte.
 * Dafür wird eigenes ein neuer Speicherblock allokiert.
 * */
typedef struct uri_decoded_t {
   uri_encoded_t  uri;
   struct {
      uint16_t nameoff;    // offset into uri.mem_addr of string describing parameter name
      uint16_t valueoff;   // offset into uri.mem_addr of string describing parameter value (valueoff>=nameoff)
      // uint16_t namelen  == param[current].valueoff-param[current].nameoff
      // uint16_t valuelen == param[current+1].nameoff-param[current].valueoff
   }           *  param;   // param[uri.nrparam+1]
} uri_decoded_t;

// group: lifetime

/* define: uri_decoded_FREE
 * Static initializer. */
#define uri_decoded_FREE \
         { uri_encoded_FREE, 0 }

/* function: init_uridecoded
 * Gibt den von init allokierten Speicherblock wieder frei. */
int free_uridecoded(/*out*/uri_decoded_t* uri);

/* function: init_uridecoded
 * Verwandelt eine <uri_encoded_t> um in eine <uri_decoded_t>.
 * URL-kodierte Werte werden dekodiert und in einem neu dafür allokierten
 * Speicherblock gespeichert. */
int init_uridecoded(/*out*/uri_decoded_t* uri, const uri_encoded_t* fromuri);

/* function: initparse_uridecoded
 * Zerlegt eine kodierte (http) URI ihre Teile und dekodiert alle URL-kodierte Werte.
 * Die reinene Werte ohne Prefixe wie '//','?','#' oder Suffixe wie ':' können dann
 * mittels <getparam_uridecoded> erfragt werden. Für die dekodierten Werte wird eigens
 * eine neuer Speicherblock allokiert. */
int initparse_uridecoded(/*out*/uri_decoded_t* uri, uint16_t size, const uint8_t str[size]);

// group: query

/* function: isabsolute_uridecoded
 * Gibt true zurück, falls der Pfad mit einem '/' beginnt. */
static inline int isabsolute_uridecoded(const uri_decoded_t* uri);

/* function: getpart_uridecoded
 * Gibt den dekodierten String eines bestimmten Teils einer URI zurück.
 * Siehe <uri_part_e> für eine Liste aller Teile. Suffixe und Präfixe, die
 * für die Kodierung verwendet werden, wurden schon entfernt.
 * Der Rückgabewert für part==uri_part_QUERY ist nicht sinnvoll. Verwenden
 * sie deshalb <getparam_uridecoded> zum Abfragen der einzelnen Parameterwerte. */
static inline uri_part_t getpart_uridecoded(const uri_decoded_t* uri, uri_part_e part);

/* function: getparam_uridecoded
 * Gibt den dekodierten Wert eines URI-Parameters zurück.
 * Siehe <uri_param_t> für den Aufbau der zurückgegeben Struktur.
 * Ist iparam nicht kleiner als der Rückgabewert von <nrparam_uridecoded>,
 * wird der Wert <uri_param_FREE> zurückgegeben. Die Parameter werden
 * im Abschnitt uri_part_QUERY gespeichert. Deshalb ersetzt diese Funtkion
 * die Funktion <getpart_uridecoded> für den Abschnitt <uri_part_QUERY>. */
uri_param_t getparam_uridecoded(const uri_decoded_t* uri, unsigned iparam);

/* function: nrparam_uridecoded
 * Gibt die Anzahl Parameter einer URI zurück. Die Parameter werden
 * im Abschnitt uri_part_QUERY gespeichert. */
static inline uint16_t nrparam_uridecoded(const uri_decoded_t* uri);


// section: inline implementation

// group: uri_encoded_t

/* define: isabsolute_uriencoded
 * Implements <uri_encoded_t.isabsolute_uriencoded>. */
static inline int isabsolute_uriencoded(const uri_encoded_t* uri)
{
         return 1==uri->prefixlen;
}

/* define: nrparam_uriencoded
 * Implements <uri_encoded_t.nrparam_uriencoded>. */
static inline uint16_t nrparam_uriencoded(const uri_encoded_t* uri)
{
         return uri->nrparam;
}

/* define: str_uriencoded
 * Implements <uri_encoded_t.str_uriencoded>. */
static inline const uint8_t* str_uriencoded(const uri_encoded_t* uri)
{
         return uri->mem_addr + uri->offset[uri_part_SCHEME];
}

/* define: size_uriencoded
 * Implements <uri_encoded_t.size_uriencoded>. */
static inline uint16_t size_uriencoded(const uri_encoded_t* uri)
{
         return uri->offset[uri_part__NROF];
}

// group: uri_decoded_t

/* define: isabsolute_uridecoded
 * Implements <uri_decoded_t.isabsolute_uridecoded>. */
static inline int isabsolute_uridecoded(const uri_decoded_t* uri)
{
         return isabsolute_uriencoded(&uri->uri);
}

/* define: nrparam_uridecoded
 * Implements <uri_decoded_t.nrparam_uridecoded>. */
static inline uint16_t nrparam_uridecoded(const uri_decoded_t* uri)
{
         return nrparam_uriencoded(&uri->uri);
}

/* define: getpart_uridecoded
 * Implements <uri_decoded_t.getpart_uridecoded>. */
static inline uri_part_t getpart_uridecoded(const uri_decoded_t* uri, uri_part_e part)
{
         return getpart_uriencoded(&uri->uri, part);
}

#endif
