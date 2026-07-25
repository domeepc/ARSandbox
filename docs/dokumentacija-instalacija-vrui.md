# Rješavanje problema pri instalaciji biblioteke Vrui na sustavu Linux Mint 22.3

Ovaj dokument opisuje probleme uočene pri instalaciji biblioteke Vrui, verzija 8.0-002,
na sustavu Linux Mint 22.3 (temeljenom na distribuciji Ubuntu 24.04 LTS), te postupke
njihova rješavanja.

---

# 1. Nedostupan paket `libdc1394-22-dev`

## Opis problema

Prilikom pokretanja skripte `Build-Ubuntu.sh`, namijenjene za automatiziranu instalaciju
biblioteke Vrui (VR Toolkit), došlo je do pogreške tijekom instalacije potrebnih paketa
putem alata `apt-get`:

```
E: Unable to locate package libdc1394-22-dev
```

Skripta unaprijed definira popis paketa potrebnih za kompajliranje Vrui-a, među kojima se
nalazio i paket `libdc1394-22-dev`, korišten za podršku IEEE 1394 (FireWire) digitalnih
kamera.

## Analiza uzroka

Provjerom dostupnih paketa u repozitorijima sustava (naredba `apt-cache search libdc1394`)
utvrđeno je da paket `libdc1394-22-dev` više ne postoji pod tim imenom. Razlog tomu je
promjena verzije SONAME dijeljene biblioteke `libdc1394`, koja je u novijim izdanjima
Ubuntua (22.04 i novijim, na kojima se temelji korištena distribucija Linux Mint 22.3)
prešla s verzije 22 na verziju 25. Posljedično, i naziv razvojnog (`-dev`) paketa
promijenjen je iz `libdc1394-22-dev` u generički naziv `libdc1394-dev`, dok se sama
dijeljena biblioteka sada distribuira kao `libdc1394-25`.

Dostupni paketi vezani uz tu biblioteku na testiranom sustavu bili su:

- `libdc1394-25` – dijeljena biblioteka (runtime)
- `libdc1394-dev` – razvojni paket (zaglavlja i statičke datoteke)
- `libdc1394-doc` – dokumentacija
- `libdc1394-utils` – pomoćni alati

## Rješenje

U skripti `Build-Ubuntu.sh` unutar varijable `PREREQUISITE_PACKAGES` izvršena je izmjena
naziva paketa:

```diff
- libdc1394-22-dev
+ libdc1394-dev
```

Nakon navedene izmjene, skripta je uspješno mogla nastaviti s instalacijom preostalih
predefiniranih paketa te s daljnjim koracima preuzimanja, kompajliranja i instalacije
biblioteke Vrui.

## Zaključak

Slučaj ilustrira uobičajen problem prenosivosti instalacijskih skripti pisanih za starije
verzije distribucija temeljenih na Debianu/Ubuntuu: nazivi razvojnih paketa koji sadrže
broj verzije dijeljene biblioteke (SONAME) u imenu paketa (npr. `libime-22-dev`,
`libdc1394-22-dev`) podložni su promjeni pri svakoj promjeni glavne verzije te biblioteke.
Prilikom pokretanja starijih skripti na novijim izdanjima distribucije preporučljivo je
prije izvođenja provjeriti dostupnost navedenih paketa naredbom `apt-cache search <naziv>`
te po potrebi ažurirati popis potrebnih paketa u skripti.

---

# 2. Sukob deklaracija tipova `ALCdevice` i `ALCcontext` (OpenAL)

## Opis problema

Nakon uspješne instalacije potrebnih paketa, tijekom kompajliranja izvornog koda Vrui-a
prevoditelj `g++` prijavio je pogrešku pri prevođenju datoteke `Vrui/SoundContext.cpp`:

```
/usr/include/AL/alc.h:34:16: error: using typedef-name ‘ALCdevice’ after ‘struct’
   34 | typedef struct ALCdevice ALCdevice;
      |                ^~~~~~~~~
/home/dome/src/Vrui-8.0-002/Vrui/SoundContext.h:35:33: note: ‘ALCdevice’ has a previous
declaration here
   35 | typedef struct ALCdevice_struct ALCdevice;
      |                                 ^~~~~~~~~
/usr/include/AL/alc.h:34:26: error: conflicting declaration ‘typedef int ALCdevice’
```

Istovjetna pogreška prijavljena je i za tip `ALCcontext`.

## Analiza uzroka

Zaglavlje `Vrui/SoundContext.h` ne uključuje izravno zaglavlje OpenAL-a, već koristi tzv.
unaprijednu deklaraciju (engl. *forward declaration*) tipova `ALCdevice` i `ALCcontext`
kako bi se izbjeglo nepotrebno uključivanje sistemskog zaglavlja u datoteku zaglavlja.
Te su deklaracije bile napisane prema starijoj konvenciji referentne implementacije
OpenAL-a, u kojoj su navedeni tipovi bili definirani kao:

```c
typedef struct ALCdevice_struct  ALCdevice;
typedef struct ALCcontext_struct ALCcontext;
```

Međutim, implementacija OpenAL Soft, koja se u novijim distribucijama koristi kao zadana
implementacija OpenAL-a, počevši od verzije 1.20 mijenja te definicije te uklanja sufiks
`_struct`:

```c
typedef struct ALCdevice  ALCdevice;
typedef struct ALCcontext ALCcontext;
```

Budući da su tako `ALCdevice` i `ALCcontext` u dvama zaglavljima definirani kao aliasi
dviju različitih (nepotpunih) strukturnih tipova, prevoditelj prijavljuje sukob
deklaracija (*conflicting declaration*) i prevođenje se prekida.

## Rješenje

U datoteci `Vrui/SoundContext.h` unaprijedne deklaracije usklađene su s definicijama iz
sistemskog zaglavlja `/usr/include/AL/alc.h`:

```diff
- struct ALCdevice_struct;
- typedef struct ALCdevice_struct ALCdevice;
- struct ALCcontext_struct;
- typedef struct ALCcontext_struct ALCcontext;
+ struct ALCdevice;
+ typedef struct ALCdevice ALCdevice;
+ struct ALCcontext;
+ typedef struct ALCcontext ALCcontext;
```

Provjerom cjelokupnog stabla izvornog koda (naredbom `grep -rn "ALCdevice_struct"`)
utvrđeno je da su se zastarjele deklaracije nalazile isključivo u navedenoj datoteci, pa
dodatne izmjene nisu bile potrebne.

Nakon izmjene datoteka `Vrui/SoundContext.cpp` uspješno je prevedena, a dijeljena
biblioteka `libALSupport.so.8.0` uspješno je povezana.

## Zaključak

Uzrok problema jest nepodudarnost između unaprijednih deklaracija u izvornom kodu
aplikacije i stvarnih definicija tipova u sistemskom zaglavlju. Ovakav se problem ne bi
pojavio da izvorni kod izravno uključuje zaglavlje `<AL/alc.h>`, no takav je pristup
autor biblioteke izbjegao radi smanjenja ovisnosti među zaglavljima i bržeg prevođenja.
Slučaj ukazuje na rizik koji nosi ručno ponavljanje deklaracija iz vanjskih biblioteka:
pri promjeni sučelja biblioteke takve deklaracije postaju neispravne, a pogreška se
otkriva tek u fazi prevođenja.

---

# 3. Nerazriješene reference na `FancyTextNode` i `FancyFontStyleNode`

## Opis problema

Nakon otklanjanja prethodne pogreške prevođenje je uspješno dovršeno, no pri povezivanju
dijeljene biblioteke `libSceneGraph.so.8.0` povezivač (`ld`) prijavio je nerazriješene
reference:

```
/usr/bin/ld: o/.../SceneGraph/NodeCreator.o: in function
`SceneGraph::GenericNodeFactory<SceneGraph::FancyTextNode>::getClassName() const':
NodeCreator.cpp: undefined reference to `SceneGraph::FancyTextNode::className'
NodeCreator.cpp: undefined reference to `SceneGraph::FancyTextNode::FancyTextNode()'
NodeCreator.cpp: undefined reference to `SceneGraph::FancyFontStyleNode::className'
NodeCreator.cpp: undefined reference to `SceneGraph::FancyFontStyleNode::FancyFontStyleNode()'
```

## Analiza uzroka

Vrui podržava tzv. napredno iscrtavanje teksta (*fancy text rendering*) u svojem
podsustavu za prikaz scenskog grafa, implementirano razredima `FancyTextNode` i
`FancyFontStyleNode`. Ta je funkcionalnost uvjetna i u datoteci `makefile` upravlja se
varijablom `SCENEGRAPH_USE_FANCYTEXT`, koja se postavlja na 1 samo ako su ispunjena dva
uvjeta:

1. na sustavu je prisutna biblioteka FreeType (`SYSTEM_HAVE_FREETYPE`), i
2. pronađen je točno jedan direktorij koji sadrži TrueType fontove obitelji GNU FreeFont
   (`FreeSans.ttf`, `FreeSerif.ttf`, `FreeMono.ttf`).

Provjerom sustava utvrđeno je da navedeni fontovi nisu bili instalirani, zbog čega je
varijabla `SCENEGRAPH_USE_FANCYTEXT` poprimila vrijednost 0, a datoteke
`SceneGraph/FancyFontStyleNode.cpp` i `SceneGraph/FancyTextNode.cpp` bile su isključene
iz popisa izvornih datoteka za prevođenje.

Međutim, datoteka `SceneGraph/NodeCreator.cpp` uključuje zaglavlja navedenih razreda i
registrira odgovarajuće tvorničke objekte **bezuvjetno**, tj. bez zaštitne pretprocesorske
direktive `#if SCENEGRAPH_CONFIG_HAVE_FANCYTEXT`:

```cpp
#include <SceneGraph/FancyFontStyleNode.h>
#include <SceneGraph/FancyTextNode.h>
/* ... */
registerNodeType(new GenericNodeFactory<FancyFontStyleNode>());
registerNodeType(new GenericNodeFactory<FancyTextNode>());
```

Riječ je o propustu u izvornom kodu inačice Vrui 8.0-002: prevoditelj generira reference
na simbole razreda čije definicije nikada nisu prevedene, pa povezivanje ne uspijeva.

## Rješenje

Problem je moguće riješiti na dva načina:

**a) Instalacijom nedostajućih fontova (odabrani pristup).** Instalacijom paketa
`fonts-freefont-ttf` ispunjava se drugi uvjet za aktivaciju napredne obrade teksta:

```bash
sudo apt install fonts-freefont-ttf
```

Nakon toga sustav za izgradnju postavlja `SCENEGRAPH_USE_FANCYTEXT = 1`, obje se izvorne
datoteke prevode, a nerazriješene reference nestaju. Prednost je ovog pristupa što se
funkcionalnost naprednog iscrtavanja teksta zadržava.

**b) Uvjetnim prevođenjem u `NodeCreator.cpp`.** Alternativno, navedena se uključivanja
zaglavlja i pozivi `registerNodeType()` mogu obuhvatiti direktivom
`#if SCENEGRAPH_CONFIG_HAVE_FANCYTEXT`, čime se ispravlja sam propust u kodu, ali se
funkcionalnost naprednog iscrtavanja teksta trajno onemogućuje.

U ovom je radu odabran pristup (a) kao rješenje koje odgovara namjeri autora sustava za
izgradnju i ne zahtijeva izmjenu izvornog koda.

## Zaključak

Uzrok problema jest nedosljednost između sustava za izgradnju, koji uvjetno isključuje
određene izvorne datoteke, i izvornog koda, koji na njih referira bezuvjetno. Takve se
nedosljednosti ne očituju na razvojnom sustavu autora (na kojemu su svi neobavezni uvjeti
ispunjeni), nego isključivo na sustavima s drukčijim skupom instaliranih paketa, što ih
čini teško uočljivima tijekom razvoja.

---

# 4. Nepodudarnost TLS i ne-TLS referenci pri povezivanju

## Opis problema

Nakon instalacije fontova obje su datoteke uspješno prevedene, no povezivanje biblioteke
`libSceneGraph.so.8.0` ponovno nije uspjelo, ovaj put uz poruku:

```
/usr/bin/ld: _ZN23GLARBVertexBufferObject7currentE: TLS reference in
o/.../SceneGraph/FancyFontStyleNode.o mismatches non-TLS reference in
o/.../SceneGraph/CurveSetNode.o
/usr/bin/ld: o/.../SceneGraph/FancyFontStyleNode.o: error adding symbols: bad value
collect2: error: ld returned 1 exit status
```

## Analiza uzroka

Statički član `GLARBVertexBufferObject::current` deklariran je pomoću makronaredbe
`GL_THREAD_LOCAL`, definirane u zaglavlju `GL/TLSHelper.h`, čije proširenje ovisi o
konfiguracijskoj konstanti `GLSUPPORT_CONFIG_USE_TLS` iz datoteke `GL/Config.h`:

```c
#if GLSUPPORT_CONFIG_USE_TLS
  #define GL_THREAD_LOCAL(VariableType) __thread VariableType   /* dretveno lokalna varijabla */
#else
  #define GL_THREAD_LOCAL(VariableType) VariableType            /* obična varijabla */
#endif
```

Ovisno o vrijednosti navedene konstante, isti se simbol prevodi ili kao dretveno lokalna
varijabla (engl. *thread-local storage*, TLS) ili kao obična globalna varijabla. Budući da
se radi o dvama različitim modelima pristupa simbolu na razini objektnog koda, povezivač
odbija povezati objektne datoteke prevedene s različitim vrijednostima te konstante.

Pregledom stanja radnog direktorija utvrđeno je da datoteka `GL/Config.h` nije bila
obrađena konfiguracijskim korakom sustava za izgradnju, nego je ostala u izvornom obliku
iz preuzetog arhiva. Na to nedvojbeno upućuje sadržaj konstante s putanjom do direktorija
fontova, koja je i dalje sadržavala apsolutnu putanju s razvojnog računala autora
biblioteke:

```c
#define GLSUPPORT_CONFIG_GL_FONT_DIR "/home/okreylos/Share/GLFonts"
#define GLSUPPORT_CONFIG_USE_TLS 1
```

Istodobno, datoteka `makefile` propisuje suprotnu vrijednost:

```makefile
GLSUPPORT_USE_TLS = 0
```

Uzrok neprovođenja konfiguracijskog koraka jest mehanizam tzv. datoteka-oznaka
(engl. *stamp files*) u direktoriju `d/`: postojanje datoteke `d/Configure-GLSupport`
sustavu za izgradnju signalizira da je konfiguracija podsustava GLSupport već obavljena,
pa se pripadajuće naredbe preskaču. Uslijed toga dio je objektnih datoteka preveden prema
jednoj, a dio prema drugoj vrijednosti konstante `GLSUPPORT_CONFIG_USE_TLS`.

## Rješenje

Konfiguracijski korak prisilno je ponovno pokrenut brisanjem datoteka-oznaka, uz
istodobno uklanjanje svih prethodno prevedenih objektnih datoteka kako bi cjelokupan
projekt bio preveden prema jedinstvenoj konfiguraciji:

```bash
rm -f d/Configure-*
rm -rf o lib
make -j$(nproc)
```

Nakon ponovnog pokretanja konfiguracije datoteka `GL/Config.h` ispravno je generirana:

```c
#define GLSUPPORT_CONFIG_USE_TLS 0
#define GLSUPPORT_CONFIG_GL_FONT_DIR "/usr/local/share/Vrui-8.0/GLFonts"
```

Time je otklonjena i nepodudarnost TLS referenci i, usputno, neispravna putanja do
direktorija fontova, koja bi inače uzrokovala pogrešku pri izvođenju aplikacija.

## Zaključak

Problem proizlazi iz nepotpune inkrementalne konfiguracije sustava za izgradnju: oznake
dovršenosti pojedinih konfiguracijskih koraka ostale su valjane iako pripadajuće
konfiguracijske datoteke nisu bile obrađene. U slučaju nedosljednih ili neočekivanih
pogrešaka pri povezivanju preporučuje se stoga potpuno ponovno prevođenje projekta uz
prethodno brisanje svih međurezultata izgradnje.

---

# 5. Ponovna pojava nepodudarnosti TLS referenci pri izgradnji primjera

## Opis problema

Pri izgradnji primjera aplikacija iz direktorija `ExamplePrograms` povezivanje programa
`VisionTest` nije uspjelo uz pogrešku istovjetnu onoj opisanoj u poglavlju 4, ovaj put
između novoprevedene objektne datoteke i **instalirane** biblioteke:

```
/usr/bin/ld: _ZN22GLEXTFramebufferObject7currentE: TLS reference in o/.../VisionTest.o
mismatches non-TLS reference in /usr/local/lib/x86_64-linux-gnu/Vrui-8.0/libVrui.so
/usr/bin/ld: /usr/local/lib/x86_64-linux-gnu/Vrui-8.0/libVrui.so: error adding symbols: bad value
```

## Analiza uzroka

Za razliku od same biblioteke, primjeri aplikacija ne prevode se prema konfiguracijskim
datotekama iz radnog direktorija, nego prema **instaliranim** zaglavljima u direktoriju
`/usr/local/include/Vrui-8.0`. Usporedbom instaliranih datoteka utvrđeno je da je
instalirano stanje bilo interno nedosljedno:

| Sastavnica | `GLSUPPORT_CONFIG_USE_TLS` | Podrijetlo |
|------------|----------------------------|------------|
| `/usr/local/lib/.../Vrui-8.0/libVrui.so.8.0` | 0 (bez TLS-a) | ispravno prevedena biblioteka |
| `/usr/local/include/Vrui-8.0/GL/Config.h` | 1 (s TLS-om) | neispravna datoteka iz arhiva |

Uzrok ovakvog stanja jest međudjelovanje dvaju činitelja. Ponovnim pokretanjem skripte
`Build-Ubuntu.sh` izvorni je arhiv iznova raspakiran, čime je datoteka `GL/Config.h`
vraćena u početno stanje (`USE_TLS 1`). Pritom je bitno da program `tar` pri raspakiravanju
zadržava izvorna vremena izmjene datoteka zapisana u arhivu (u ovom slučaju iz 2020.
godine). Novoraspakirane izvorne datoteke stoga su bile *starije* od već prevedenih
objektnih datoteka, pa ih alat `make` nije smatrao zastarjelima i nije pokrenuo ponovno
prevođenje. Posljedično je biblioteka povezana iz ranije, ispravno prevedenih objektnih
datoteka (bez TLS-a), dok je korak `make install` instalirao vraćena, neispravna
zaglavlja (s TLS-om).

Prevođenjem programa `VisionTest` prema tako instaliranim zaglavljima nastala je dretveno
lokalna referenca na simbol `GLEXTFramebufferObject::current`, koja se ne podudara s
običnom referencom u instaliranoj biblioteci.

## Rješenje

U radnom je direktoriju ponovno pokrenut konfiguracijski korak, uklonjeni su međurezultati
izgradnje primjera te je biblioteka ponovno prevedena i instalirana, čime su instalirana
zaglavlja usklađena s instaliranom bibliotekom:

```bash
rm -f d/Configure-*
rm -rf ExamplePrograms/o ExamplePrograms/bin
make -j$(nproc)
sudo make INSTALLDIR=/usr/local install
```

Ponovno je bilo potrebno primijeniti i ispravak iz poglavlja 2, budući da je raspakiravanje
arhiva poništilo i njega.

## Zaključak

Slučaj pokazuje da pri izgradnji aplikacija koje se oslanjaju na instaliranu biblioteku
nije dovoljno osigurati unutarnju dosljednost radnog direktorija, nego i podudarnost
između instaliranih zaglavlja i instaliranih binarnih datoteka. Posebno je zanimljivo to
što uobičajeni mehanizam alata `make`, zasnovan na usporedbi vremena izmjene datoteka,
u kombinaciji s raspakiravanjem arhiva koji zadržava izvorna vremena izmjene, može dovesti
do toga da izmijenjene izvorne datoteke uopće ne budu ponovno prevedene.

---

# 6. Napomena: gubitak izmjena pri ponovnom pokretanju skripte

Tijekom postupka opetovano je uočeno da ponovno pokretanje skripte `Build-Ubuntu.sh`
poništava sve ručne izmjene izvornog koda. Razlog je taj što skripta u svakom pokretanju
iznova preuzima i raspakirava izvorni arhiv u isti direktorij:

```bash
wget -O - http://web.cs.ucdavis.edu/~okreylos/ResDev/Vrui/Vrui-$VRUI_VERSION-$VRUI_RELEASE.tar.gz | tar xfz -
```

Time se prepisuju sve prethodno izmijenjene datoteke, uključujući ispravke opisane u
poglavljima 2 i 4, zbog čega se već otklonjene pogreške ponovno pojavljuju.

Kako bi se to spriječilo, skripta je izmijenjena tako da preuzimanje provodi samo ako
direktorij s izvornim kodom još ne postoji:

```bash
if [ -d Vrui-$VRUI_VERSION-$VRUI_RELEASE ]; then
	echo "Source directory Vrui-$VRUI_VERSION-$VRUI_RELEASE already exists; skipping download"
	echo "Delete that directory first if you want a pristine copy"
else
	echo "Downloading Vrui-$VRUI_VERSION-$VRUI_RELEASE into $HOME/src"
	wget -O - http://web.cs.ucdavis.edu/~okreylos/ResDev/Vrui/Vrui-$VRUI_VERSION-$VRUI_RELEASE.tar.gz | tar xfz -
fi
```

Time skripta postaje idempotentna, tj. njezino ponovno pokretanje više ne uništava
prethodno primijenjene ispravke. Neovisno o tome, preporučuje se sljedeći redoslijed rada:

1. skriptu `Build-Ubuntu.sh` pokrenuti samo jedanput, radi instalacije potrebnih paketa i
   preuzimanja izvornog koda;
2. sve daljnje ispravke i ponovna prevođenja provoditi izravno u direktoriju
   `$HOME/src/Vrui-<verzija>-<izdanje>` naredbama `make` i `sudo make install`, bez
   ponovnog pokretanja skripte.

---

# 7. Provjera ispravnosti instalacije

Nakon usklađivanja instaliranih zaglavlja i binarnih datoteka (poglavlje 5) provedena je
provjera ispravnosti instalacije u tri koraka.

## 7.1. Provjera dosljednosti instaliranog stanja

Najprije je provjereno podudaraju li se konfiguracijske vrijednosti u instaliranim
zaglavljima s onima prema kojima je prevedena instalirana biblioteka:

```bash
grep -n "USE_TLS\|GL_FONT_DIR" /usr/local/include/Vrui-8.0/GL/Config.h
```

Ispis potvrđuje ispravno stanje:

```c
#define GLSUPPORT_CONFIG_USE_TLS 0
#define GLSUPPORT_CONFIG_GL_FONT_DIR "/usr/local/share/Vrui-8.0/GLFonts"
```

Usporedbom vremena izmjene utvrđeno je i da su zaglavlje
`/usr/local/include/Vrui-8.0/GL/Config.h` te biblioteka
`/usr/local/lib/x86_64-linux-gnu/Vrui-8.0/libVrui.so.8.0` instalirani istodobno, čime je
isključena mogućnost ranije opisane nedosljednosti.

## 7.2. Izgradnja primjera aplikacija

Primjeri aplikacija prevedeni su prema instaliranoj biblioteci:

```bash
cd $HOME/src/Vrui-8.0-002/ExamplePrograms
make -j$(nproc) VRUI_MAKEDIR=/usr/local/share/Vrui-8.0/make INSTALLDIR=/usr/local
```

Prevođenje i povezivanje dovršeni su bez pogrešaka (izlazni kod 0), pri čemu je izgrađeno
25 izvršnih datoteka, među kojima i program `VisionTest`, čije povezivanje prethodno nije
uspijevalo. Provjerom razrješavanja dijeljenih biblioteka naredbom `ldd` utvrđeno je da su
sve ovisnosti uspješno razriješene.

## 7.3. Pokretanje aplikacije

Kao završna provjera pokrenuta je demonstracijska aplikacija `ShowEarthModel`, koja
prikazuje trodimenzionalni model Zemlje:

```bash
./bin/ShowEarthModel
```

Aplikacija se uspješno pokrenula, otvorila prikazni prozor sustava X11 te ispravno
prikazala model Zemlje. Pri izlasku iz programa zabilježen je uredan završetak rada
(izlazni kod 0).

Tijekom pokretanja aplikacija ispisuje obavijest:

```
CommandDispatcher: Command file 0 was closed; not accepting further commands
```

Riječ je o uobičajenoj obavijesti, a ne o pogrešci: Vrui na standardnom ulazu očekuje
mogućnost primanja upravljačkih naredbi tijekom izvođenja, pa pri pokretanju aplikacije
bez pridruženog terminala javlja da takve naredbe neće biti prihvaćane. Na sam prikaz i
rad aplikacije to nema utjecaja.

## 7.4. Zaključak provjere

Uspješnim pokretanjem i ispravnim prikazom demonstracijske aplikacije potvrđeno je da su
svi opisani problemi otklonjeni te da su biblioteka Vrui 8.0-002 i pripadajuće aplikacije
ispravno prevedene i instalirane na sustavu Linux Mint 22.3.

---

# Sažetak provedenih izmjena

| Br. | Problem | Rješenje |
|-----|---------|----------|
| 1 | Paket `libdc1394-22-dev` nije dostupan | U `Build-Ubuntu.sh` naziv paketa promijenjen u `libdc1394-dev` |
| 2 | Sukob deklaracija tipova `ALCdevice` / `ALCcontext` | U `Vrui/SoundContext.h` uklonjen sufiks `_struct` iz unaprijednih deklaracija |
| 3 | Nerazriješene reference na `FancyTextNode` i `FancyFontStyleNode` | Instaliran paket `fonts-freefont-ttf` |
| 4 | Nepodudarnost TLS i ne-TLS referenci unutar biblioteke | Obrisane datoteke-oznake `d/Configure-*` i međurezultati `o/`, `lib/` te provedeno potpuno ponovno prevođenje |
| 5 | Nepodudarnost TLS referenci prema instaliranoj biblioteci | Ponovno pokrenuta konfiguracija te ponovno prevedena i instalirana biblioteka radi usklađivanja instaliranih zaglavlja i binarnih datoteka |
| 6 | Ponovno pokretanje skripte poništava ispravke | Preuzimanje arhiva u `Build-Ubuntu.sh` učinjeno uvjetnim (idempotentnost) |

Nakon provedbe navedenih izmjena prevođenje i instalacija biblioteke Vrui 8.0-002
dovršeni su uspješno, bez pogrešaka, što je potvrđeno provjerom opisanom u poglavlju 7.

Analizirani problemi mogu se svrstati u tri kategorije, od kojih svaka ima drukčiji uzrok:

1. **Zastarjelost u odnosu na okolinu** (poglavlje 1) — promjena naziva paketa u
   distribuciji te promjena sučelja sistemske biblioteke (poglavlje 2). Ti se problemi
   pojavljuju zato što je programska podrška razvijena i ispitana na starijim inačicama
   operacijskog sustava.
2. **Nedosljednost sustava za izgradnju** (poglavlja 3, 4 i 5) — uvjetno isključivanje
   izvornih datoteka bez odgovarajuće zaštite u kodu te nepotpuna inkrementalna
   konfiguracija, uslijed koje su pojedini dijelovi projekta prevedeni prema različitim
   konfiguracijskim vrijednostima.
3. **Neidempotentnost instalacijskog postupka** (poglavlje 6) — ponovno pokretanje
   instalacijske skripte poništava prethodno primijenjene ispravke.

Navedeno upućuje na općenit zaključak da pri prevođenju starije znanstvene programske
podrške na suvremenim operacijskim sustavima treba očekivati probleme koji nisu ograničeni
na sam izvorni kod, nego obuhvaćaju i sustav za izgradnju te instalacijski postupak, a
čije rješavanje zahtijeva razumijevanje međuovisnosti konfiguracijskih datoteka,
međurezultata prevođenja i instaliranih datoteka.

---

# Dodatak A: Postupak kalibracije sustava SARndbox

Ovaj se dodatak, za razliku od prethodnih poglavlja, ne odnosi na pogreške pri prevođenju,
nego na **postupak kalibracije** aplikacije SARndbox (*Augmented Reality Sandbox*, inačica
2.8) — interaktivnog sustava proširene stvarnosti izgrađenog nad bibliotekom Vrui, koji
projektorom na površinu pijeska u sanduku projicira topografsku kartu i simulaciju
tečenja vode, a promjene reljefa mjeri kamerom dubine Microsoft Kinect.

## A.1. Uočena obavijest

Pri pokretanju aplikacije ispisuje se sljedeća obavijest:

```
./bin/SARndbox -uhm -fpv
Unable to load projector transformation from file
/home/dome/src/SARndbox-2.8/etc/SARndbox-2.8/ProjectorMatrix.dat due to exception
IO::StandardFile: Unable to open file ... for reading due to error 2
(No such file or directory)
```

## A.2. Analiza

Iako je formulacija poruke naizgled ozbiljna, **ne radi se o pogrešci koja prekida rad
aplikacije**. Pregledom izvornog koda utvrđeno je da je pristup datoteci obuhvaćen
mehanizmom obrade iznimaka (datoteka `Sandbox.cpp`, metoda
`RenderSettings::loadProjectorTransform()`):

```cpp
catch(const std::runtime_error& err)
	{
	/* Print an error message and disable calibrated projections: */
	std::cerr<<"Unable to load projector transformation from file "<<...<<std::endl;
	projectorTransformValid=false;
	}
```

Nakon neuspješnog čitanja datoteke zastavica `projectorTransformValid` postavlja se na
`false`, a pri određivanju projekcijske matrice provjeravaju se oba uvjeta:

```cpp
if(rs.fixProjectorView&&rs.projectorTransformValid)
```

Aplikacija se stoga vraća na uobičajenu projekciju biblioteke Vrui i nastavlja s radom.
Vrijedi napomenuti i da se navedena obavijest ispisuje **neovisno o naredbenom prekidaču
`-fpv`**, budući da se učitavanje datoteke poziva bezuvjetno u konstruktoru razreda
`RenderSettings`. Prekidač `-fpv` (*fix projector view*) određuje samo hoće li se učitana
matrica koristiti, pa je u nedostatku kalibracije bez djelovanja.

Ključno je da datoteka `ProjectorMatrix.dat` **nije konfiguracijska datoteka koju je moguće
ručno napisati ili preuzeti, nego rezultat mjernog postupka** kalibracije projektora, koji
zahtijeva fizičku postavu sustava. Njezino nepostojanje znači da taj postupak još nije
proveden.

## A.3. Kalibracijski niz i utvrđeno stanje

Postavljanje sustava SARndbox obuhvaća tri uzastopna kalibracijska koraka, od kojih svaki
proizvodi datoteku potrebnu sljedećem koraku:

| Korak | Alat | Rezultat | Stanje |
|-------|------|----------|--------|
| 1. Unutarnji parametri kamere dubine | `KinectUtil getCalib` | `IntrinsicParameters-<serijski broj>.dat` | proveden |
| 2. Mjerenje geometrije sanduka | `RawKinectViewer` | `BoxLayout.txt` | proveden |
| 3. Kalibracija projektora | `CalibrateProjector` | `ProjectorMatrix.dat` | **nije proveden** |

Prvi korak preuzima tvorničke unutarnje parametre kamere iz njezine ugrađene memorije.
Drugi korak služi za određivanje temeljne ravnine (izravnate prosječne površine pijeska)
te položaja četiriju kutova radne površine sanduka; rezultat je zapisan u datoteci
`BoxLayout.txt`, u kojoj prvi red sadrži jednadžbu temeljne ravnine (vektor normale i
udaljenost od ishodišta), a preostala četiri reda koordinate kutova radne površine.

Provjerom vremena izmjene datoteka utvrđeno je da su prva dva koraka provedena (datoteke
su nastale tijekom postavljanja sustava, a ne preuzete iz arhiva, za razliku od datoteka
`SARndbox.cfg` i `HeightColorMap.cpt` koje su ostale u izvornom obliku iz 2016. i 2012.
godine). Nedostaje isključivo rezultat trećega koraka.

### A.3.1. Mjerenje temeljne ravnine

Temeljna se ravnina određuje alatom `RawKinectViewer`, u kojemu se nad prosječnom slikom
dubine označava područje nepomućene površine pijeska, a alat metodom najmanjih kvadrata
prilagođava ravninu izmjerenim točkama te ispisuje njezinu jednadžbu u koordinatnom
sustavu kamere:

```
Camera-space plane equation: x * (0.10452, -0.0356638, 0.993883) = -150.685
```

Ispis valja tumačiti kao jednadžbu ravnine u obliku **n · x = d**, pri čemu je
**n** = (0,10452, −0,0356638, 0,993883) jedinični vektor normale, a *d* = −150,685
udaljenost ravnine od ishodišta. Sve su duljinske mjere u sustavu SARndbox izražene u
**centimetrima**, pa navedena vrijednost odgovara udaljenosti od približno 1,5 m, što je
u skladu s visinom na kojoj je kamera dubine postavljena iznad sanduka. Takav se zapis u
datoteku `BoxLayout.txt` prenosi kao prvi red, u obliku `(nx, ny, nz) , d`.

Vrijedi napomenuti da vrijednost *d* ujedno određuje razinu nulte elevacije, tj. razinu
koju upotrijebljena karta boja tumači kao razinu mora. Budući da se temeljna ravnina u
praksi često izmjeri neznatno iznad izravnate površine pijeska, tu je razinu moguće
naknadno pomaknuti izmjenom same vrijednosti *d* u datoteci `BoxLayout.txt`: povećanje
vrijednosti pomiče razinu mora prema gore, a smanjenje prema dolje, pri čemu promjena od
1 odgovara pomaku od 1 cm.

Budući da je kamera dubine postavljena iznad sanduka i usmjerena približno okomito na
površinu pijeska, normala ispravno izmjerene temeljne ravnine mora biti približno paralelna
s optičkom osi kamere, tj. njezina *z*-komponenta mora biti blizu jedinice. Ta činjenica
omogućuje brzu provjeru vjerodostojnosti pojedinog mjerenja izračunom otklona normale od
optičke osi kamere:

| Mjerenje | Vektor normale | *d* | Otklon od optičke osi |
|----------|----------------|-----|-----------------------|
| Gore navedeno | (0,10452, −0,0356638, 0,993883) | −150,685 | 6,3° |
| Prethodno zapisano u `BoxLayout.txt` | (−0,543801, −0,0517966, 0,837614) | −107,486 | 33,1° |

Otklon od približno 6° odgovara očekivanom, blagom odstupanju od okomitosti pri ručnom
postavljanju kamere, dok otklon od približno 33° upućuje na neispravno prilagođenu ravninu
— najvjerojatnije zbog toga što je pri označavanju područja obuhvaćen i dio stranice
sanduka ili neka druga površina koja ne pripada dnu sanduka. Mjerenje s manjim otklonom
stoga treba smatrati mjerodavnim.

Budući da se svaki od tri kalibracijska koraka oslanja na rezultat prethodnoga, neispravno
izmjerena temeljna ravnina prenosi pogrešku na kalibraciju projektora i na naknadno
mjerenje visina, a da pritom ni u jednom trenutku ne uzrokuje poruku o pogrešci. Provjera
vjerodostojnosti svakog pojedinog mjernog rezultata prije prelaska na sljedeći korak zato
je bitan dio postupka.

### A.3.2. Mjerenje prostornih koordinata kutova radne površine

Osim temeljne ravnine, sustav zahtijeva i poznavanje bočnih granica vidljive površine
pijeska. One se određuju mjerenjem prostornih koordinata četiriju kutova izravnate
prosječne površine pijeska, također alatom `RawKinectViewer`, pomoću alata za mjerenje
prostornih položaja. Izmjerene su sljedeće vrijednosti (u centimetrima, u koordinatnom
sustavu kamere):

```
(-23.3573, -31.4472, -150.601)
( 43.3822, -30.6687, -159.682)
(-23.2273,  50.1974, -147.133)
( 41.5816,  56.6409, -154.631)
```

Redoslijed zapisa je obvezujući: točke se navode kao donja lijeva, donja desna, gornja
lijeva i gornja desna, gledano iz perspektive projicirane slike. Zamjena redoslijeda
uzrokuje zrcaljenje ili zakretanje projicirane karte u odnosu na stvarnu površinu pijeska.
Navedene četiri točke čine drugi do peti red datoteke `BoxLayout.txt`.

Nad izmjerenim je vrijednostima provedena dvostruka provjera vjerodostojnosti.

**Podudarnost s temeljnom ravninom.** Kutovi po definiciji leže u izravnatoj površini
pijeska, pa uvrštavanjem njihovih koordinata u jednadžbu temeljne ravnine iz odjeljka
A.3.1 rezidual **n · x − d** mora biti blizu nule:

| Kut | **n · x** | Rezidual |
|-----|-----------|----------|
| donji lijevi | −151,000 | −0,32 cm |
| donji desni | −153,077 | −2,39 cm |
| gornji lijevi | −150,451 | +0,23 cm |
| gornji desni | −151,359 | −0,67 cm |

Odstupanja reda veličine nekoliko milimetara u skladu su s točnošću ručnog označavanja
točaka i mjernom nesigurnošću kamere dubine prve generacije, čime je potvrđeno da izmjereni
kutovi i izmjerena temeljna ravnina opisuju istu fizičku površinu.

**Geometrijska pravilnost.** Kako radna površina sanduka ima pravokutni oblik, nasuprotne
stranice četverokuta određenog izmjerenim kutovima moraju biti približno jednakih duljina:

| Stranica | Duljina |
|----------|---------|
| donja | 67,36 cm |
| gornja | 65,56 cm |
| lijeva | 81,72 cm |
| desna | 87,47 cm |

Dobivene dimenzije, približno 66 × 85 cm, odgovaraju stvarnim dimenzijama radne površine
sanduka. Duljine se nasuprotnih vodoravnih stranica razlikuju za oko 2,7 %, što je u
granicama očekivane mjerne nesigurnosti, dok razlika okomitih stranica iznosi oko 7 %.
Potonje odstupanje, zajedno s najvećim rezidualom u prethodnoj tablici, upućuje na to da su
koordinate donjeg desnog i gornjeg desnog kuta izmjerene s nešto manjom točnošću, pa bi ih
pri zahtjevu za većom točnošću kalibracije trebalo ponovno izmjeriti.

## A.4. Provedba kalibracije projektora

Treći korak uspostavlja projekcijsku preobrazbu između prostora kamere dubine i prostora
slike projektora, tj. omogućuje da se projicirana slika točno poklopi s površinom pijeska.
Postupak se pokreće naredbom:

```bash
./bin/CalibrateProjector -s <širina> <visina>
```

pri čemu se za razlučivost projektora korištenog u ovom radu (izlaz `HDMI-0`, 1920×1080
slikovnih elemenata) koristi:

```bash
./bin/CalibrateProjector -s 1920 1080
```

Kalibracija se provodi pomoću posebnog mjernog cilja — plosnate kružne ploče s jasno
označenim središtem, za što se prema uputama autora preporučuje stariji optički disk (CD)
s prilijepljenim bijelim papirnatim krugom i dvjema ortogonalnim linijama kroz središte.

Alat najprije snima pozadinsku sliku nepomućene površine pijeska (tijekom snimanja zaslon
je crven), nakon čega prikazuje niz parova sjecišnih linija. Korisnik za svaku veznu točku
(engl. *tie point*) postavlja mjerni cilj tako da se projicirane linije sijeku točno u
njegovu središtu, uz površinu ploče paralelnu s temeljnom ravninom. Po prikupljanju
cijeloga niza veznih točaka izračunava se kalibracijska matrica i zapisuje u datoteku
`ProjectorMatrix.dat`; matrica se osvježava nakon svake dodatno prikupljene točke, čime je
moguće postupno poboljšavati točnost.

Pri provedbi je nužno poštovati tri uvjeta, jer njihovo nepoštivanje daje neispravnu
kalibraciju:

1. **Točna razlučivost projektora.** Zadana je vrijednost u programu 1024×768 slikovnih
   elemenata; izostavljanje prekidača `-s` pri drukčijoj razlučivosti daje neispravnu
   matricu bez ikakve obavijesti o pogrešci.
2. **Prikaz preko cijelog zaslona projektora.** Kalibracija provedena u prozoru daje
   neispravan rezultat. Cjeloekranski se prikaz uključuje tipkom F11 ili odgovarajućom
   postavkom položaja i veličine prozora u konfiguraciji biblioteke Vrui.
3. **Nepomućena površina pijeska tijekom snimanja pozadine**, bez ikakvih predmeta između
   kamere dubine i površine.

Isti uvjet cjeloekranskog prikaza vrijedi i pri kasnijem pokretanju same aplikacije
SARndbox, jer se kalibracija odnosi na cjelovitu sliku projektora.

## A.5. Zaključak dodatka

Uočena obavijest nije posljedica pogreške u programskoj podršci ni u postupku instalacije,
nego pokazatelj nedovršenog kalibracijskog niza. Slučaj ujedno ilustrira da poruke o
iznimkama ne treba tumačiti isključivo prema njihovoj formulaciji: pregledom izvornog koda
utvrđeno je da je iznimka obuhvaćena obradom te da aplikacija nastavlja s radom uz
smanjenu funkcionalnost, što se iz same poruke ne može zaključiti.

---

# Dodatak B: Poboljšanja kvalitete prikaza

Nakon uspješne instalacije provedena je analiza kvalitete prikaza sustava SARndbox te je
provedeno šest izmjena. Polazište analize bilo je zapažanje da je grafički podsustav
računala (NVIDIA GeForce RTX 5060 Ti, OpenGL 4.6) neusporedivo snažniji od zahtjeva
aplikacije, koja je pisana za sklopovlje iz razdoblja oko 2012. godine i koristi proširenja
ARB tada aktualne inačice OpenGL-a 2.x, dok ulazni tok podataka ograničava kamera dubine
Kinect v1 na 640 × 480 elemenata pri 30 slika u sekundi. Budući da brzina prikaza stoga
nije ograničavajući čimbenik, raspoloživa je računalna moć usmjerena na povećanje kvalitete.

## B.1. Zaglađivanje rubova izohipsi

Izvorna implementacija u sjenčaru `SurfaceAddContourLines.fs` za svaki je slikovni element
donosila binarnu odluku te ga bojila neprozirnom crnom bojom. Posljedično su izohipse bile
tvrde, široke točno jedan slikovni element i s izrazito nazubljenim rubovima. Dodatno je
primijenjen heuristički postupak stanjivanja zasnovan na parnosti koordinata (uzorak
šahovnice), koji je izohipse na blagim nagibima razlamao u isprekidane crte.

Postupak je zamijenjen mjerenjem udaljenosti od središta slikovnog elementa do najbliže
izohipse, izražene u zaslonskim slikovnim elementima, koja se zatim pretvara u vrijednost
pokrivenosti. Gradijent visinskog polja računa se analitički iz istih četiriju kutnih
uzoraka koje je dohvaćala i izvorna implementacija, pa nisu potrebne dodatne instrukcije
za računanje derivacija:

```glsl
float e =(c00+c10+c01+c11)*0.25;
float gx=((c10+c11)-(c00+c01))*0.5;
float gy=((c01+c11)-(c00+c10))*0.5;
float f=e*contourLineFactor;
float grad=length(vec2(gx,gy))*contourLineFactor;
float dPix=abs(f-floor(f+0.5))/max(grad,1.0e-8);
float cov=clamp((w*0.5+0.5)-dPix,0.0,1.0);
```

Uvedene su i naglašene izohipse (svaka peta), u skladu s kartografskom praksom, te
postupno iščezavanje izohipsi na mjestima gdje se međusobno približe na svega nekoliko
slikovnih elemenata. Potonje istodobno otklanja dvije postojeće nepravilnosti: na strmim
stijenkama pijeska izohipse su se stapale u crnu mrlju, a na vanjskom rubu radne površine
pojavljivao se lažni crni prsten, budući da se ondje očišćena vrijednost međuspremnika
visina dodiruje s rekonstruiranom površinom pijeska.

## B.2. Anizotropija mreže simulacije vode

Utvrđeno je da simulacijska mreža nije bila razmjerna stvarnim dimenzijama sanduka.
Prema algoritmu u datoteci `WaterTable2.cpp` os *x* domene postavlja se duž kraćeg brida
sanduka, pa je pri izmjerenoj domeni od 69,64 × 87,43 cm i zadanoj mreži od 640 × 480
ćelija veličina ćelije iznosila 0,1088 × 0,1821 cm. Ćelije su dakle bile 1,674 puta dulje
duž jedne osi, što je uzrokovalo lošije razlučivanje toka vode u tom smjeru i o smjeru
ovisno numeričko rasipanje valova.

Mreža je promijenjena na 640 × 804 ćelije, čime veličina ćelije postaje 0,1088 × 0,1087 cm.
Bitno je da ta promjena gotovo ne povećava broj potrebnih vremenskih koraka: ograničenje
duljine koraka prema uvjetu CFL određuje **manja** dimenzija ćelije, koja je ostala
nepromijenjena.

## B.3. Filtriranje dubinske slike

Zadane vrijednosti filtra (30 spremišnih mjesta, histereza 0,1 cm) davale su mirnu, ali
tromu površinu: 30 slika pri 30 Hz odgovara punoj sekundi kašnjenja, a budući da se
nestabilni slikovni elementi zamrzavaju na prethodnoj vrijednosti umjesto da se postupno
mijenjaju, preoblikovanje pijeska rezultiralo je sekundom zastarjelog prikaza nakon koje
bi uslijedio skok. Broj spremišnih mjesta smanjen je na 15, čime kašnjenje pada na
približno 0,5 s, a histereza na 0,05 cm.

Pritom je utvrđeno ograničenje koje je potrebno poštovati: filtar zbraja kvadrate
vrijednosti u 32-bitnom cijelom broju bez predznaka, pa test stabilnosti prelijeva
raspon iznad približno 32 spremišna mjesta, nakon čega se stabilnost pojedinih elemenata
utvrđuje nasumično.

## B.4. Sjenčanje reljefa

Sjenčanje reljefa u izvornoj je inačici onemogućeno, a pripadajući izvor svjetla isključen
pretprocesorskom direktivom `#if 0`. Njegovim bi uključivanjem bez daljnjih izmjena prikaz
postao lošiji, jer bi jedini izvor svjetla bio onaj pridružen promatraču, koji je pri
geometriji "projektor iznad sanduka" gotovo okomit na približno vodoravnu površinu, pa
skalarni umnožak normale i smjera svjetla svugdje iznosi približno jedan i sjenčanje ne
prenosi nikakvu informaciju o nagibu.

Provedene su tri izmjene:

1. Izvor svjetla stvara se kada je sjenčanje uključeno, a smjer mu se zadaje azimutom i
   elevacijom u stupnjevima (zadano 315° i 45°, prema kartografskoj praksi) umjesto
   nepromjenjivim vektorom, pa ga je moguće uskladiti s fizičkom postavom bez ponovnog
   prevođenja.
2. Normala površine računa se središnjom razlikom preko ±2 umjesto ±1 slikovnog elementa
   dubinske slike. Razlika preko jednog elementa djeluje kao visokopropusni filtar nad
   signalom čiji je preostali šum upravo ono što filtar dubinske slike nije uklonio, i
   glavni je uzrok zrnatosti sjenčanja.
3. Sjenčanje se primjenjuje kao djelomično stapanje prema osvijetljenoj boji
   (`mix(baseColor, litColor, reliefStrength)`) umjesto potpunog Lambertovog osvjetljenja,
   čime se očuvava zasićenost visinske karte boja, što je bitno pri projekciji gdje je
   kontrast već ograničen okolnom rasvjetom i vlastitim albedom pijeska.

## B.5. Ispravak prekoračenja međuspremnika

Pregledom izvornog koda utvrđeno je prekoračenje granica polja. Član
`SurfaceRenderer::DataItem::heightMapShaderUniforms` deklariran je s 16 elemenata, no
metoda `createSinglePassSurfaceShader` u njega upisuje 17 vrijednosti kada su istodobno
uključeni visinska karta boja, izohipse, geološki sloj, sjenčanje reljefa i simulacija
vode. Sedamnaesti upis pada na susjedni član `surfaceSettingsVersion`, koji upravlja
ponovnom izgradnjom sjenčara. Polje je prošireno na 20 elemenata.

Riječ je o latentnoj pogrešci koja se ne očituje pri uobičajenoj uporabi, budući da
geološki sloj nije moguće uključiti naredbenim retkom, nego isključivo upravljačkim
kanalom, pa se navedena kombinacija postavki rijetko ostvaruje.

## B.6. Upravljačka ploča izvedena u okviru Qt

Za podešavanje parametara tijekom rada izrađena je zasebna aplikacija u tehnologiji
Qt Quick. Razmatrana je i mogućnost zamjene ugrađenoga grafičkog sučelja (GLMotif) sučeljem
Qt, no ta je zamisao odbačena: biblioteka Vrui upravlja glavnom petljom programa,
stvaranjem prozora i kontekstom OpenGL-a, pa bi njezino ugnježđivanje u drugi okvir
zahtijevalo opsežne izmjene, i to za sučelje koje se projektorom ionako ne prikazuje.

Umjesto toga iskorišten je mehanizam koji SARndbox već posjeduje: imenovani kanal
(engl. *named pipe*, FIFO) zadan prekidačem `-cp`, iz kojega aplikacija u svakoj slici
čita naredbene retke. Upravljačka ploča stoga u taj kanal samo upisuje naredbe i ne
zahtijeva nikakvu izmjenu koda za prikaz. Budući da se izvodi kao zaseban proces, njezin
prekid ne može poremetiti rad sanduka, a oba se programa mogu neovisno ponovno pokretati.

Pri izvedbi su bitna dva svojstva imenovanih kanala. Otvaranje kanala za pisanje blokira
dok ne postoji čitatelj, pa se koristi zastavica `O_NONBLOCK`, uz ponovne pokušaje
povezivanja; stanje "sanduk nije pokrenut" time postaje uobičajeno i oporavljivo stanje,
a ne pogreška. Nadalje, opisnik datoteke mora ostati otvoren tijekom cijele sjednice, jer
bi otvaranje i zatvaranje pri svakoj naredbi čitatelju izgledalo kao opetovani kraj
datoteke.

Komunikacija je jednosmjerna: sanduk ne vraća svoje stanje, pa ploča prikazuje posljednje
poslane, a ne stvarne vrijednosti. To je svjesno prihvaćeno ograničenje, budući da bi
dvosmjerna izvedba zahtijevala drugi kanal, a odstupanje nastaje samo ako se istodobno
koriste i ugrađeni dijalozi aplikacije.

Upravljačkom su pločom dostupni parametri vode (brzina, najveći broj koraka, prigušenje),
izohipsi (uključivanje, razmak, debljina) i sjenčanja reljefa (jačina, azimut i elevacija
izvora svjetla). Parametri koji se čitaju samo pri pokretanju, poput veličine simulacijske
mreže i postavki filtra dubinske slike, namjerno nisu izloženi, jer bi ploča inače
sugerirala mogućnost izmjene koja tijekom rada ne postoji.

## B.7. Zaključak dodatka

Provedene izmjene pokazuju da se pri prilagodbi starije znanstvene programske podrške
suvremenom sklopovlju najveći dobitak u kvaliteti ne postiže nužno povećanjem razlučivosti
ili složenosti prikaza, nego uklanjanjem pretpostavki ugrađenih u kod u vrijeme kada su
računalni resursi bili ograničeni. Binarna odluka pri iscrtavanju izohipsi, središnja
razlika preko jednog slikovnog elementa i razmjerno grubo vremensko usrednjavanje
dubinske slike sve su redom razumni kompromisi za sklopovlje iz 2012. godine, koji na
suvremenom sklopovlju postaju nepotrebni.
