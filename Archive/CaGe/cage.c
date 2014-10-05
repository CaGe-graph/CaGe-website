/*****************************************************************************
*   CaGe.c  -  Program for generation and output of graphs                   *
*                                                                            *
*   original version:                                                        *
*     Thomas Harmuth  -  September 1996                                      *
*                                                                            *
*   latest modification:                                                     *
*     Olaf Delgado    -     August 2000                                      *
*                                                                            *
*   This program uses several graph generation programs developed in         *
*   Bielefeld. Furthermore, it uses "tcl" and "tk". Parts of the files       *
*   "tkMain.c" and "tkAppInit.c" are used in this program                    *
*****************************************************************************/

/*************/
/* includes: */
/*************/
#ifdef __linux			/* Linux Parameter fuer ps */
#define SYS "LINUX"
#endif

#ifdef __sgi			/* Silicon Graphics */
#define SYS "SGI"
#endif

#ifdef __osf__			/* OSF auf den alphas */
#define SYS "OSF1"
#endif

#ifndef SYS
#define SYS "MSDOOF"
#endif
/*
   #include <dlfcn.h>          
 */
#ifdef NO_STDLIB_H
#include "compat/stdlib.h"
#else
#include <stdlib.h>
#endif
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<ctype.h>
#include<math.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<limits.h>
#include <errno.h>
#ifndef TCLTK_INCLUDES_WITHOUT_VERSION
#include <tcl8.0.h>
#include <tk8.0.h>
#else
#include <tcl.h>
#include <tk.h>
#endif
#include<sys/signal.h>


/***********************/
/* public definitions: */
/***********************/

#define MAXN 2000		/* maximal vertex number in input graphs */
#define MAXENTRIES (MAXN*4)+1	/* Maximum code length for input graphs.
				   (MAXN*4)+1 is sufficient for 3-regular graphs. It is also sufficient for
				   the dual of 3-regular graphs. */
#define NL      "\n"		/* newline-symbol */
#define RED #900

/***************************************************************************/
/*  Arbeitsweise des Programms:

   - Die zentrale Funktion des Programms ist die Funktion "event_loop".
   In ihr werden die waehrend eines Generierungsprozess auftretenden
   Events verarbeitet, z.B. Druecken von Buttons, Laden und Speichern
   von Graphen, Entgegennahme von Einbettungen.
   Umhuellt wird die Funktion "event_loop" von der Funktion
   "fullgen_handling". Diese Funktion stellt die Optionsfenster bereit
   und startet und stoppt die Generierungsprogramme. Die Funktion
   ist fuer jedes Generierungsprogramm (und auch fuer das Einlesen von
   Graphen direkt aus Files) verantwortlich, nicht nur fuer "fullgen".
   Ihren Namen traegt die Funktion aus historischen Gruenden. Aber 
   insbesondere sind alle Generierungsprogramme in ihrer Struktur
   aehnlich zu "fullgen", was folgende Aspekte betrifft:
   - alle Graphen koennen nur einmal ausgegeben werden, ein Ruecksprung
   ist nicht moeglich
   - vor der Ausgabe des ersten Graphen ist eine kleine Wartezeit
   einzukalkulieren
   - bis auf die Uebergabeparameter beim Programmaufruf erwarten die 
   Generierungsprogramme keine weiteren Eingaben 
   (es gibt keine "On-Line-Veraenderungen")

   - Die in der Anleitung beschriebene Vorschau arbeitet intern wie folgt:
   Sei x der angezeigte Graph. Zuerst wird Graph x+1 eingebettet, dann
   x+10, dann x+2, x+3, ... x+9. Auf die Weise steht sowohl fuer den
   Button (+1) als auch fuer den Button (+10) der verlangte Graph
   schnellstmoeglich zur Verfuegung. 

   Die gelesenen Graphen werden zunaechst zwischengespeichert (in den Arrays
   graph, graphnv, graphnr. etc.). Dieser Zwischenspeicher wird staendig
   ueberschrieben. Die Graphen, die auf dem Display angezeigt wurden,
   muessen fuer die Review-Funktion jedoch auf Dauer gespeichert werden.
   Dies wiederum geschieht in einer doppelt verketteten Liste.
   Der Zwischenspeicher muss Platz fuer mindestens 12 Graphen bieten,
   naemlich fuer den aktuellen Graphen (er trage die Nummer x), fuer die
   Graphen x+1 und x+10 wegen der Vorausberechnung von Einbettungen und
   damit auch fuer die Graphen x+2 bis x+9, weil man an den (x+10)-ten
   Graphen nicht herankommt, ohne die Graphen x+2 bis x+9 zu laden. Diese
   Graphen duerfen natuerlich nicht verworfen werden, denn der (x+10)-te
   Graph wird bei der Vorausschau ja nur "auf Verdacht" geladen, und 
   vielleicht moechte sich der Benutzer zuerst die Graphen x+2 bis x+9  
   anschauen. Als zwoelfter Graph wird eventuell der Graph gespeichert, der
   gerade im Review-Modus angezeigt wird.
   Nicht zwischengespeichert werden muessen alle Graphen mit einer kleineren
   Nummer als x:  Alle bereits gezeigten Graphen sind in der doppelt
   verketteten Liste gespeichert, die nicht gezeigten Graphen sind verloren
   und koennen nicht mehr angeschaut werden. Ferner brauchen alle Graphen 
   mit einer groesseren Nummer als x+10 nicht zwischengespeichert zu werden:
   CaGe schaut nicht so weit voraus. Alle Graphen mit einer groesseren
   Nummer als x+10 werden erst dann geladen, wenn sich x vergroessert.

   - In diesem C-Programm befinden sich einige Tcl-Routinen. Insbesondere
   werden auch einige Tcl-Variablen benutzt. Manche Informationen befinden
   sich nur in C-Variablen (z.B. gelesene Graphen), manche Informationen
   befinden sich nur in Tcl-Variablen (z.B. Button-Zustaende). Manche
   C- und Tcl-Variablen sind miteinander gekoppelt, so dass sie stets den
   gleichen Wert besitzen, und manche Tcl-Variablenwerte werden bei Bedarf
   an C-Variablen uebergeben und umgekehrt, ohne dass die beteiligten
   Variablen miteinander gekoppelt sind.
   Miteinander gekoppelte oder in loser Verbindung stehende C- und Tcl-
   Variablen muessen nicht immer denselben Namen tragen. Deshalb an dieser
   Stelle eine Auffuehrung der einander zugeordneten Variablen:

   Tcl                    C                      *            **
   -------------------------------------------------------------------
   bigface2               bigface_2              *
   cancel                 cancel                              **
   emb_a5                 emb_a                  *
   emb_b5                 emb_b                  *
   emb_c5                 emb_c                  *
   emb_d5                 emb_d                  *
   outcancel              ende
   global_background      global_background
   global_end             global_end
   new_embed5             new_embed              *
   newschlegel            newschlegel
   old_embed5             old_embed              *
   outnr                  outnr
   p3                     p_3                    *
   *** transfer_p3                                                                          
   programm               programm
   rasmol_id              rasmol_id
   reviewstep             reviewstep
   save_2d                save_s2d
   save_3d                save_s3d
   save_pdb               save_spdb
   skip                   skip
   start                  start                               **
   woh3                   woh_3                  *
   *** transfer_woh3
   graphnr3               you_see_nr1            *
   graphnr2               you_see_nr2            *

   (*)  Die Variablen sind nicht durch einen Link miteinander gekoppelt.
   Jedoch werden die Werte regelmaessig einander angepasst.
   (Der Link will aus unverstaendlichen Gruenden nicht funktionieren
   oder ein Link ist nicht notwendig, da die Werte nach einmaliger
   Zuordnung konstant bleiben).
   (**) Die C-Variable ist keine globale Variable, sondern kommt in einer
   der Funktionen "fullgen_handling" oder "event_loop" vor. 
   (***) Auf Linux-Rechnern funktioniert der Tcl-Befehl
   'Tcl_GetVar' nicht fuer array's. Daher werden die Werte
   ueber die Variablen 'transfer_*' ausgelesen.         

   Weitere Relationen (aber keine direkte Korrespondenz):
   graphnr              <-- highnr/outnr
   dateiname_s2d        --> file_s2d
   dateiname_s3d        --> file_s3d
   dateiname_spdb       --> file_spdb
   topipe($programm)    --> outpipe
   r_or_g($programm)    --> d3torasmol, rasmol, file_3d
   file_2d($programm)   --> tkview, file_2d
   tofile($programm)    --> raw
   filecode($programm)  --> ascfile

   - Um mit Tcl kompatibel zu sein, sind viele C-Variablen, die vom Typ BOOL
   sein koennten, vom Typ int. Sie enthalten nur die Werte 0 oder 1.

   - Aus historischen Gruenden tragen einige Konstanten oder Variablen 
   unverstaendliche oder gar unlogische Bezeichnungen. Im einzelnen:
   - 3reg, 3reggen:  Variablen, in denen diese Kuerzel auftauchen, beziehen
   sich auf das Programm CPF, das frueher "3reggen" hiess. Da ein
   Konstanten- oder Variablenname nicht mit einer Zahl beginnen darf,
   ist bei Bedarf die 3 durch ein D ersetzt worden (z.B. bei der
   Konstante DREGGEN)
   - rasmol:  Frueher war nur das Programm "RasMol" vorgesehen, um drei-
   dimensionale Einbettungen auf dem Bildschirm auszugeben. Deshalb war
   die Aussage "bitte 3D-Ausgabe" gleichbedeutend mit "bitte RasMol
   aufrufen". Wenn die Variable einen Wert ungleich 0 hatte, so wurde 
   beides getan. Mittlerweile kann die 3D-Ausgabe aber auch ueber 
   "GeomView" erfolgen. Deshalb bedeutet ein Variablenwert ungleich 0 
   zunaechst nur noch "bitte 3D-Ausgabe", wohin diese Ausgabe erfolgen 
   soll, wird jedoch in der Variablen "d3torasmol" festgehalten. Wenn die
   Ausgabe nach "RasMol" geleitet werden soll, so ist der Inhalt der
   Variablen "rasmol" gleichzeitig der Zeiger auf den Dateinamen der 
   Temporaerdatei, in die jene Einbettung geschrieben wird, die wiederum
   von "RasMol" ausgelesen und angezeigt wird. 
   Einige andere Variablen, die das Wort "rasmol" in ihren Namen tragen,
   beziehen sich ebenfalls auf die 3D-Ausgabe generell und nicht auf die
   Ausgabe nach RasMol im speziellen, so z.B. "rasmol_coords".
   - tkview:  Frueher wurde das Programm "tkviewgraph" aufgerufen, um
   Schlegel-Diagramme darzustellen, so dass die Aussage "bitte 2D-Ausgabe"
   gleichbedeutend war mit "bitte tkviewgraph aufrufen". Mittlerweile ist 
   "tkviewgraph" ein Bestandteil von "CaGe", so dass der Variablenwert
   1 einfach so zu interpretieren ist, dass Schlegel-Diagramme auf dem
   Bildschirm angezeigt werden sollen (also "bitte 2D-Ausgabe"). 
   Mit dem Programm "tkviewgraph" hat diese Ausgabe aber nichts mehr zu 
   tun.

   - Case-Anweisungen:  Manche Case-Anweisungen koennten sich problemlos
   zusammenfassen lassen (z.B. solche, in denen eine Fallunterscheidung
   nach dem Inhalt der Variablen "programm" gefuehrt wird). Die 
   Einzelfaelle sind nur deshalb gesondert aufgefuehrt, um eventuelle
   Erweiterungen, die nur einzelne Programme betreffen, problemlos
   einfuehren zu koennen.

   - _old-Variablen:  Manche Variablen gibt es in zwei Ausfuehrungen: Als
   "var" und als "var_old". In diesem Fall ist "var" an eine Tcl-Variable
   gekoppelt, und durch Vergleich von "var" und "var_old" kann man erkennen,
   ob sich der Wert von "var" veraendert hat. Normalerweise stimmen die 
   Werte von "var" und "var_old" naemlich ueberein.

 */


/*************************/
/* internal definitions: */
/*************************/

#ifndef BIG_ENDIAN
#define BIG_ENDIAN 0
#endif
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1
#endif

#ifdef __sgi
#define ENDIAN_OUT BIG_ENDIAN
#define ENDIAN_IN  BIG_ENDIAN
#else
  /* BIG_ENDIAN list is probably not complete */
#define ENDIAN_OUT LITTLE_ENDIAN
#define ENDIAN_IN  LITTLE_ENDIAN
#endif

#define FALSE 0
#define TRUE  1
#define nil   0
#define TOLERANZ 0.000001	/* double-Variablen mit hoechstens diesem 
				   Unterschied werden als gleich angesehen */
#define LINELEN 2000		/* maximale Laenge eines Steuerkommandos oder einer 
				   Kommandozeile oder einer Outer-Cycle-Codierung, also irgendeines Strings.
				   Wird diese Laenge ueberschritten, gibt's Riesenprobleme! */
#define ANZ_GRAPH 11		/* Anzahl der gleichzeitig im Speicher gehaltenen 
				   Input-Graphen (muss >10 sein, weil 10er-Sprung im Voraus berechnet wird).
				   Hinzu kommt ein Platz fuer den aktuellen Reviewgraphen. */

#define KEINES 0		/* Programm, das gestartet werden soll */
#define FULLGEN 1
#define DREGGEN 2		/* CPF (ehemals 3reggen) */
#define HCGEN   3
#define TUBETYPE 4
#define INPUTFILE 5		/* einfache Datei mit Header als Input
				   (Inputdatei wird wie ein Prozess behandelt) */
#define ANZ_PRG 5		/* Anzahl der verfuegbaren Programme */

/* Status-Defines waehrend eines Durchlaufs von "event_loop" 
   (OR-verknuepfbar): */
#define NONE                   0
#define LIES_GRAPH             1	/* ein neuer Inputgraph wird gebraucht */
#define GRAPH_IST_DA           2	/* Graph vom Inputprozess ist da */
#define BETTE_SPRING_EIN       4	/* aktueller Graph soll eingebettet werden */
#define BETTE_SCHLEGEL_EIN     8	/* aktueller Graph soll eingebettet werden */
#define SPRING_IST_AN         16	/* Spring laeuft */
#define SCHLEGEL_IST_AN       32	/* Schlegel laeuft */
#define SPRING_IST_DA         64	/* Springeinbettung ist vorhanden */
#define SCHLEGEL_IST_DA      128	/* Schlegeleinbettung ist vorhanden */
#define SPRING_IN_LISTE      256	/* eine Einbettung soll in die Liste */
#define SCHLEGEL_IN_LISTE    512	/* eine Einbettung soll in die Liste */
#define GIB_SPRING_AUS      1024	/* Graph (aus Liste) soll ausgegeben werden */
#define GIB_SCHLEGEL_AUS    2048	/* Graph (aus Liste soll ausgegeben werden */
#define STOP_SPRING         4096	/* Spring soll gestoppt werden */
#define STOP_SCHLEGEL       8192	/* Schlegel soll gestoppt werden */
#define REVIEWMODUS        16384	/* Reviewmodus ist an */
#define SKIPMODUS          32768	/* Skipmodus laeuft */
#define SOFTENDE           65536	/* Inputprozess ist zu Ende */
#define SCHREIBE_OUTNR   (1<<17)	/* Schreibt Nummer des gesuchten Graphen */
#define NEWSCHLEGEL      (1<<18)	/* Neue Umrandungsflaeche bei Schlegeldiag. */
#define SAVE_2D          (1<<19)	/* aktuelle 2D-Einbettung abspeichern */
#define SAVE_3D          (1<<20)	/* aktuelle 3D-Einbettung abspeichern */
#define SAVE_PDB         (1<<21)	/* aktuelle 3D-Einbettung abspeichern als pdb */

/* Graphstates */
#define LEER 0			/* zugehoeriger Speicherplatz ist unbesetzt */
#define ROH  1			/* Graph ist noch nicht eingebettet */
#define SPRING_IN_ARBEIT 2	/* Graph wird gerade eingebettet */
#define SCHLEGEL_IN_ARBEIT 4	/* Graph wird gerade eingebettet */
#define SPRING_EINGEBETTET 8	/* Spring-Daten liegen vor */
#define SCHLEGEL_EINGEBETTET 16	/* Schlegel-Daten liegen vor */
#define SPRING_SAVED 32		/* Spring-Daten wurden in .s3d gespeichert */
#define SPRING_SAVED_PDB 64	/* Spring-Daten wurden in .spdb gespeichert */

/**********************/
/* type declarations: */
/**********************/

typedef char BOOL;		/* !=0 => TRUE (use only values 0 and 1) */

typedef struct liste {		/* Element der doppelt verketteten Liste mit den
				   bereits angezeigten Graphen */
  struct liste *next;
  struct liste *prev;
  int nr;			/* Nummer im Generierungsprozess */
  int nv;			/* Anzahl der Knoten */
  int ne;			/* Anzahl der Kanten */
  unsigned short *planarcode;	/* Zeiger auf Code */
  double *rasmol_coords;	/* Zeiger auf 3D-Koordinaten */
  double *schlegel_coords;	/* Zeiger auf 2D-Koordinaten */
  unsigned int graphstate;	/* moegliche Zustaende siehe obige Konstanten */
} LISTE;


/*********************************/
/* global variables and defines: */
/*********************************/

/* --- Konstanten fuer die "dumme" Berechnung der Sechseckmuster --- */

const double pi = 3.1415926535897932384626433;
double cospi6;			/* muss in main-Funktion initialisiert werden */


/* --- Zwischenspeicherung von Graphen ---- */
/* ANZ_GRAPH+1:  letztes Feld fuer Reviewgraph */

unsigned short newgraph[MAXENTRIES];	/* Platz fuer unnuetzen Planarcode */
double newcoords[MAXN * 3];	/* enthaelt nur Nullen */

unsigned short graph[ANZ_GRAPH + 1][MAXENTRIES];	/* Graphen im Planarcode */
unsigned long graphnr[ANZ_GRAPH + 1];	/* zugehoerige Nummern im 
					   Generierungsprozess (0 = unbelegt) */
unsigned int graphstate[ANZ_GRAPH + 1];		/* zugehoerige Zustaende */
double rasmol_coords[ANZ_GRAPH + 1][MAXN * 3];	/* zugehoerige 3D-Koordinaten, 
						   falls ausgerechnet */
double schlegel_coords[ANZ_GRAPH + 1][MAXN * 2];	/* zugehoerige 2D-Koordinaten, 
							   falls ausgerechnet */
int graphnv[ANZ_GRAPH + 1];	/* zugehoerige Knotenzahlen */
int graphne[ANZ_GRAPH + 1];	/* zugehoerige Kantenzahlen */


/* --- Listenspeicherung von bereits gezeigten Graphen --- */

LISTE *first_g = nil, *curr_g = nil, *last_g = nil, *dummy_g;
    /* Zeiger auf den ersten, den aktuell angezeigten und den letzten Graphen
       in der Liste. In der Liste sind die Graphen nach aufsteigenden Nummern
       (innerhalb des Generierungsprozesses) geordnet.
       Der letzte Zeiger wird fuer Schleifendurchlaeufe verwendet.  */


/* --- Aufruf und Durchlauf der Funktionen "Fullgen_handling" sowie
   "event_loop": Die Variablen sind global, da sonst eine Unmenge an
   Variablen zwischen den beiden Funktionen uebergeben werden
   muessten.                                                      --- */

unsigned long status = NONE;	/* Programmstatus im event-loop */
int programm = KEINES;		/* Programm, das gestartet werden soll bzw. laeuft */
unsigned int prg_used[ANZ_PRG] =
{0, 0, 0, 0, 0};
 /* "prg_used[i]" ist 0, wenn Programm mit der Nummer i+1 noch nicht 
    aufgerufen wurde. In diesem Fall erfolgt zu Beginn der Funktion
    "fullgen_handling" eine Initialisierung der zugehoerigen Tcl-Variablen. */


/* PIDs von Programmen, die nebenher laufen koennen: */
pid_t spring_id = 0, schlegel_id = 0;	/* zugehoerige PIDs */
pid_t inputfile_id = 0;		/* ID des InputPROZESSES (=0, falls Input aus Datei
				   kommt) */
pid_t rasmol_id = 0;		/* PID des 3D-Ausgabeprogramms, also
				   RasMol ODER GEOMVIEW */

/* File-Deskriptoren von Programmen, die nebenher laufen koennen: */
FILE *inputfile = nil;		/* Inputpipe vom Inputprozess oder Inputfile */
FILE *raw = nil;		/* zeigt auf Ausgabedatei fuer Adjazenzinfos */
FILE *outpipe = nil;		/* zeigt auf Ausgabepipe fuer Adjazenzinfos */
FILE *spring = nil;		/* Outputpipe zu "spring" */
FILE *schlegel = nil;		/* Outputpipe zu "schlegel" */
BOOL d3torasmol;		/* TRUE = rasmol,  FALSE = geomview fuer 3D-Output */
FILE *geompipe = nil;		/* Outputpipe nach GeomView, falls gewuenscht */
char *rasmol = nil;		/* != nil  => 3D-Einbettungen auf Bildschirm ausgeben
				   (siehe Kommentar in den Erlaeuterungen) */
BOOL tkview = False;		/* True => 2D-Einbettungen auf Bildschirm ausgeben */
BOOL filehandler;		/* True => File-Handler fuer Inputfile ist aktiv */

int global_end = 0;		/* 1 => Quit-Button wurde gedrueckt */
int global_background = 0;	/* 1 => Background-Button wurde gedrueckt */
			    /* 2 => Background-Button wurde verarbeitet */

/* weitere Variablen fuer einen Durchlauf: */
int spr_ebnr;			/* Nummer des Graphen, der spring-eingebettet werden
				   soll  (0  =>  kein Graph wird eingebettet) */
int sl_ebnr;			/* dasselbe fuer schlegel */
int outnr;			/* Nummer des anzuzeigenden Graphen (sofern es kein
				   Review-Graph ist) */
int outnr_old;			/* alter Wert */
int you_see_nr1;		/* Nummer des Graphen, dessen 3D-Einbettung gerade 
				   auf dem Bildschirm zu sehen ist */
int you_see_nr2;		/* Nummer des Graphen, dessen 2D-Einbettung gerade
				   auf dem Bildschirm zu sehen ist */
int current_vertex_number;	/* Knotenzahl fuer Outputbedienfenster.
				   Existiert auch als TCL Variable */
signed int spr_ebindex;		/* Index des Graphen im Zwischenspeicher-Array, der
				   gerade spring-eingebettet wurde oder wird 
				   im Array, also graphnr[spr_ebindex] = spr_ebnr 
				   (-1  =>  kein Graph wird eingebettet) */
signed int sl_ebindex;		/* dasselbe fuer Schlegel */
signed int spr_in_liste_idx;	/* Index desjenigen Graphen, dessen 
				   3D-Einbettung in die Liste der gezeigten Graphen aufgenommen werden soll */
signed int sl_in_liste_idx;	/* Index desjenigen Graphen, dessen
				   2D-Einbettung in die Liste der gezeigten Graphen aufgenommen werden soll */
int highnr;			/* Anzahl der bisher vom Generierungsprogramm bzw. der 
				   Inputdatei eingelesenen Graphen. Geplant ist, dass diese 
				   Zahl eines Tages fallen koennen wird, wenn das Generierungs-
				   programm ein Signal bekommt, so dass es den Generierungspro-
				   zess (teilweise) wiederholt. */
int absolutehighnr;		/* Anzahl der bisher eingelesenen verschiedenen Graphen
				   (diese Zahl faellt nie, mit ihrer Hilfe werden
				   eingelesene Graphen nicht doppelt gespeichert) */
BOOL reviewmode;		/* TRUE => es werden gerade alte Graphen
				   ausgegeben ("outnr" wird nicht veraendert) */
signed int reviewstep;		/* ==x  => x review-Graphen weiter bzw. zurueck */
int skip;			/* 1 => eingelesene Graphen werden uebersprungen */
int skip_old;			/* alter skip-Wert */
int newschlegel;		/* True => Benutzer hat Flaeche im Schlegel-Diagramm
				   angeklickt */
int n, ne;			/* Knoten- und Kantenzahl eines eingelesenen Graphen (wird nur
				   lokal benutzt) */
int ende;			/* TRUE => event-loop beenden */
BOOL ascfile, ascpipe;		/* TRUE => Ausgabe der Adjazenzinformationen im 
				   "writegraph2d"-code anstelle des "planar_code"
				   (je eine Variable fuer Datei- und Pipeausgabe) */
int bigface_2;			/* groesste erlaubte Flaeche bei einem konkreten CPF-Aufruf */
int p_3;			/* wird nur bei HCgen gebraucht: Anzahl der Fuenfecke
				   bei einem konkreten HCgen-Aufruf */
int woh_3;			/* wird nur bei HCgen gebraucht: mit/ohne Wasserstoffen
				   generieren, einbetten und ausgeben */

FILE *file_2d;			/* != nil  => alle Graphen 2d-einbetten und speichern */
FILE *file_3d;			/* != nil  => alle Graphen 3d-einbetten und speichern */
int save_s2d;			/* True => angezeigte 2d-Einbettungen abspeichern */
int save_s3d;			/* True => angezeigte 3d-Einbettungen abspeichern */
int save_spdb;			/* True => angezeigte 3d-Einbettungen abspeichern als pdb */
int save_s2d_old;		/* alter Wert */
int save_s3d_old;		/* alter Wert */
int save_spdb_old;		/* alter Wert */
FILE *file_s2d;			/* Zielfile fuer die 2d-Einbettungen von "save_s2d" */
FILE *file_s3d;			/* Zielfile fuer die 3d-Einbettungen von "save_s3d" */
FILE *file_spdb;		/* Zielfile fuer die 3d-Einbettungen von "save_spdb" */
int pdb_file;			/* ob file_3d im pdb Format gespeichert werden soll */

int old_embed;			/* entspricht old_embed5 bei Tcl */
int new_embed;			/* entspricht new_embed5 bei Tcl */
int emb_a, emb_b, emb_c, emb_d;	/* emb_x entspricht emb_x5 bei Tcl */
int inputcode;			/* Code, der in die Inputpipe geschrieben wird: */
		    /* 0 = planar_code, 1 = planar_code_old, 2 = writegraph2d,
		       3 = writegraph3d */


/* --- sonstiges --- */

int endian_in = ENDIAN_IN;	/* Inputmodus (default) */


/****************************************/
/*  more global variables and defines:  */
/****************************************/

#ifndef lint
static char sccsid[] = "@(#) tkAppInit.c 1.15 95/06/28 13:14:28";
#endif /* not lint */

/* The following variable is a special hack that is needed in order for
   Sun shared libraries to be used for Tcl. */

extern int matherr ();
int *tclDummyMathPtr = (int *) matherr;

/* Used by the Tk_Main function: */

static Tk_Window mainWindow;	/* The main window for the application.  If
				 * NULL then the application no longer
				 * exists. */
static Tcl_Interp *interp;	/* Interpreter for this application. */
static Tcl_DString command;	/* Used to assemble lines of terminal input
				 * into Tcl commands. */
static int tty;			/* Non-zero means standard input is a
				 * terminal-like device.  Zero means it's
				 * a file. */
static char errorExitCmd[] = "exit 1";

/* Command-line options: */

static int synchronize = 0;
static char *fileName = NULL;
static char *name = NULL;	/* name of application */
static char *display = NULL;
static char *geometry = NULL;

static Tk_ArgvInfo argTable[] =
{
  {"-display", TK_ARGV_STRING, (char *) NULL, (char *) &display,
   "Display to use"},
  {"-geometry", TK_ARGV_STRING, (char *) NULL, (char *) &geometry,
   "Initial geometry for window"},
  {"-name", TK_ARGV_STRING, (char *) NULL, (char *) &name,
   "Name to use for application"},
  {"-sync", TK_ARGV_CONSTANT, (char *) 1, (char *) &synchronize,
   "Use synchronous mode for display server"},
  {(char *) NULL, TK_ARGV_END, (char *) NULL, (char *) NULL,
   (char *) NULL}
};

/****************************************/
/* vs global variables and defines:  */
/****************************************/

static int Tcl_last_graph = 0;
int no_output = 0;
int wp_last_graph = 0;		/* == last_graph (last_graph wird immer wieder
				   ueberschrieben */
int last_skip = 0;
/*******************/
/*  declarations:  */
/*******************/

/*
 * Declarations for various library procedures and variables (don't want
 * to include tkInt.h or tkPort.h here, because people might copy this
 * file out of the Tk source directory to make their own modified versions).
 * Note: don't declare "exit" here even though a declaration is really
 * needed, because it will conflict with a declaration elsewhere on
 * some systems.
 */

extern int isatty _ANSI_ARGS_ ((int fd));
extern int read _ANSI_ARGS_ ((int fd, char *buf, size_t size));
extern char *strrchr _ANSI_ARGS_ ((CONST char *string, int c));

/* Forward declarations for procedures defined later in this file: */

/*int Tcl_AppInit(Tcl_Interp *interp); */
void event_loop ();


/******************/
/*  Tcl-Scripts:  */
/******************/

/* Die Skripte koennen mit dem Befehl 
   fprintf(stdout,"%s",Tcl_skript_XXX)
   ausgegeben werden */

char Tcl_Skript_Initialisierung[] =	/* fuer globale Variablen */
"# Programmende - Flags:\n"
/* "set at_work [image create photo -file under_const.gif -format gif]\n" */
/* "set at_work [image create  -file at_work.xbm -background #eeeeee000]\n" */
"set global_end 0\n"
"set global_background 0\n"
"set outcancel 0                  ;# fuer Output-Bedienfenster\n"
"\n"
"# Flags, die angeben, ob ein Programm existiert:\n"
"set isFullgen [file exists \"fullgen\"]\n"
"set is3Reggen [file exists \"CPF\"]\n"
"set isHCgen [file exists \"HCgen\"]\n"
"set isTubetype [file exists \"tubetype\"]\n"
"\n"
"# Programm, das gestartet werden soll:\n"
"set programm 0\n"
"# Kuerzel der Programmnamen (tauchen in Widgetnamen auf):\n"
"set prg(1) full\n"
"set prg(2) r3\n"
"set prg(3) hc\n"
"set prg(4) tube\n"
"set prg(5) file\n"
"\n"
"# PIDs der Child-Prozesse:\n"
"set inputfile_id 0\n"
"set rasmol_id 0\n"
"set spring_id 0\n"
"set schlegel_id 0\n"
"\n"
"# Outputbedienfenster-Variablen einfuehren:\n"
"set graphnr2 0\n"
"set graphnr  0\n"
"set outnr    0\n"
"set current_vertex_number 0       ;# evs Existiert auch als C Variable\n"
"\n"
"# Funktionen fuer die Zeichnung von Schlegeldiagrammen:\n"
"set newschlegel 0           ;#  1 --> es wurde Feld angeklickt\n"
"proc elements array {\n"
"	upvar $array a\n"
"\n"
"	set list {}\n"
"	foreach i [array names a] {\n"
"		lappend list $a($i)\n"
"	}\n"
"	return $list\n"
"}\n"
"\n"
"\n"
"proc min list {\n"
"	set min [lindex $list 1]\n"
"\n"
"	foreach x $list {\n"
"		if {$x < $min} {\n"
"			set min $x\n"
"		}\n"
"	}\n"
"	return $min\n"
"}\n"
"\n"
"\n"
"proc max list {\n"
"	set max [lindex $list 1]\n"
"\n"
"	foreach x $list {\n"
"		if {$x > $max} {\n"
"			set max $x\n"
"		}\n"
"	}\n"
"	return $max\n"
"}\n"
"\n"
"\n"
"proc drawGraph {} {\n"
"    global xpos ypos zpos adj shownumbers\n"
"\n"
"    set border 20\n"
"\n"
"    set cwidth [winfo width .schlegel.c]\n"
"    set cheight [winfo height .schlegel.c]\n"
"\n"
"    set xmin [min [elements xpos]]\n"
"    set xmax [max [elements xpos]]\n"
"    set ymin [min [elements ypos]]\n"
"    set ymax [max [elements ypos]]\n"
"\n"
"    # abbrechen, falls Aufruf vor dem ersten Graph kam\n"
"    # (das kann durch den bind-Befehl fuer .schlegel.c passieren)\n"
"    if {$xmax=={} || $ymax=={}} {return}\n"
"\n"
"    set dx [expr ($xmax-$xmin)/2]\n"
"    if {$dx < 1} { set dx 1 }\n"
"    set dy [expr ($ymax-$ymin)/2]\n"
"    if {$dy < 1} { set dy 1 }\n"
"    set xscale [expr ($cwidth/2 - $border) / $dx]\n"
"    set yscale [expr ($cheight/2 - $border) / $dy]\n"
"    if {$xscale < $yscale} {\n"
"	set scale $xscale\n"
"    } else {\n"
"	set scale $yscale\n"
"    }\n"
"\n"
"    set xmid [expr $cwidth/2]\n"
"    set ymid [expr $cheight/2]\n"
"    set cx [expr ($xmax+$xmin)/2]\n"
"    set cy [expr ($ymax+$ymin)/2]\n"
"    set xshift [expr $xmid-($cx*$scale)]\n"
"    set yshift [expr $ymid-($cy*$scale)]\n"
"\n"
"    .schlegel.c delete all\n"
"\n"
"    foreach n [array names xpos] {\n"
"	set x [expr $xpos($n)*$scale+$xshift]\n"
"	set y [expr $ypos($n)*$scale+$yshift]\n"
"	if {$shownumbers == 1} {\n"
"	    .schlegel.c create oval [expr $x-10] [expr $y-10] \\\n"
"		    [expr $x+10] [expr $y+10] \\\n"
"		    -outline black -fill white\n"
"	    .schlegel.c create text $x $y -text $n\n"
"	} else {\n"
"	    .schlegel.c create oval [expr $x-2] [expr $y-2] \\\n"
"		    [expr $x+2] [expr $y+2] \\\n"
"		    -outline black -fill black\n"
"	}\n"
"	\n"
"	foreach m $adj($n) {\n"
"	    if {$m > $n} {\n"
"		set x1 [expr $xpos($m)*$scale+$xshift]\n"
"		set y1 [expr $ypos($m)*$scale+$yshift]\n"
"		set line [.schlegel.c create line $x $y $x1 $y1]\n"
"		.schlegel.c lower $line\n"
"	    }\n"
"	}\n"
"    }\n"
"}\n"
"\n"
"\n"
"proc switchNumbers {} {\n"
"	global shownumbers\n"
"\n"
"	if {$shownumbers == 0} {\n"
"		set shownumbers 1\n"
"		.schlegel.num configure -text \"Hide numbers\"\n"
"		drawGraph\n"
"	} else {\n"
"		set shownumbers 0\n"
"		.schlegel.num configure -text \"Show numbers\"\n"
"		drawGraph\n"
"	}\n"
"}\n"
"\n"
"\n"
"proc printCanvas {} {\n"
"	global psfile\n"
"	.schlegel.c postscript -pagewidth 16c -file $psfile\n"
"}\n"
"\n"
"# Variablen-Defaults fuer Schlegeldiagramm:\n"
"set psfile unnamed.ps\n"
"set shownumbers 0\n"
"\n"
"# Programmunabhaengiges automatisches Erstellen von Dateinamen:\n"
"proc fuelle_dateifeld {} {\n"
"  global prg programm\n"
"  set prog $prg($programm)\n"
"  fuelle_${prog}_dateifeld\n"
"}\n"
"\n"
"# Gemeinsamer Teil aller Optionsfenster:  Output - Optionen\n"
"proc zeichne_outputoptionen {} {\n"
"  global programm   ;# enthaelt Nummer des zu startenden Programms\n"
"  global prg        ;# enthaelt Namenskuerzel\n"
"  set prog $prg($programm)   ;# aktuelle Kuerzel\n"
"  frame .${prog}top.block8 -relief groove -borderwidth 4  ;# Rahmen\n"
"  frame .${prog}top.block2\n"
"  frame .${prog}top.block10\n"
"  frame .${prog}top.block11\n"
"  frame .${prog}top.block12\n"
"  frame .${prog}top.block13\n"
"  frame .${prog}top.block16\n"
"  frame .${prog}top.block17\n"
"  frame .${prog}top.block18\n"
"  \n"
"  # Block 8\n"
"  label .${prog}top.outputlabel -text \"Output\"\n"
"  frame .${prog}top.block9        ;# Rahmeninhalt: Output-Optionen\n"
"  pack .${prog}top.outputlabel .${prog}top.block9 -in .${prog}top.block8 "
"-side top -pady 1m -fill x\n"
"  \n"
"  # Block 12\n"
"  checkbutton .${prog}top.adjacency\\\n"
"    -text \"Adjacency information:    \"\\\n"
"    -variable adjacency($programm) -command {\n"
"      if {$adjacency($programm)==0} {\n"
"        set tofile($programm) 0;  set topipe($programm) 0\n"
"        set filecode($programm) 2; set automatic_f($programm) 0\n"
"      }\\\n"
"      else {set tofile($programm) 1; set filecode($programm) 0\n"
"            set automatic_f($programm) 1; fuelle_dateifeld}\n"
"    }\n"
"  checkbutton .${prog}top.tofile -anchor w -variable tofile($programm)\\\n"
"    -text File -command {\n"
"      set automatic_f($programm) $tofile($programm)\n"
"      fuelle_dateifeld\n"
"      if {$tofile($programm)==0 && $topipe($programm)==0}\\\n"
"         {set adjacency($programm) 0; set filecode($programm) 2}\\\n"
"      else {\n"
"        set adjacency($programm) 1\n"
"        if {$filecode($programm)==2} {set filecode($programm) 0}\n"
"      }\n"
"    }\n"
"  checkbutton .${prog}top.topipe -anchor w -variable topipe($programm)\\\n"
"    -text Pipe -command {\n"
"      if {$tofile($programm)==0 && $topipe($programm)==0}\\\n"
"         {set adjacency($programm) 0; set filecode($programm) 2}\\\n"
"      else {\n"
"        set adjacency($programm) 1\n"
"        if {$filecode($programm)==2} {set filecode($programm) 0}\n"
"      }\n"
"    }\n"
"  pack .${prog}top.adjacency -side left -in .${prog}top.block12\n"
"  pack .${prog}top.tofile .${prog}top.topipe -side left\\\n"
"    -in .${prog}top.block12 -padx 2m\n"
"  \n"
"  # Block 11\n"
"  label .${prog}top.code -text \"      Code:\"\n"
"  radiobutton .${prog}top.code1asc\\\n"
"    -text \"ASCII (\\\"writegraph2d\\\")\"\\\n"
"    -variable filecode($programm) -value 0 -anchor w\\\n"
"    -command {\n"
"      if {$tofile($programm)==0 && $topipe($programm)==0}\\\n"
"          {set tofile($programm) 1; set adjacency($programm) 1\n"
"           set automatic_f($programm) 1; fuelle_dateifeld}\n"
"    }\n"
"  radiobutton .${prog}top.code1bin\\\n"
"    -text \"binary (\\\"planar_code\\\")\"\\\n"
"    -variable filecode($programm) -value 1 -anchor w\\\n"
"    -command {\n"
"      if {$tofile($programm)==0 && $topipe($programm)==0}\\\n"
"         {set tofile($programm) 1; set adjacency($programm) 1\n"
"          set automatic_f($programm) 1; fuelle_dateifeld}\n"
"    }\n"
"  pack .${prog}top.code .${prog}top.code1asc .${prog}top.code1bin\\\n"
"    -side left -in .${prog}top.block11 -padx 2m\n"
"  \n"
"  # Block 13\n"
"  checkbutton .${prog}top.torasmol -text \"3D representation:    \"\\\n"
"    -anchor w -variable torasmol($programm)\\\n"
"    -command {\n"
"      if {$torasmol($programm)==0}\\\n"
"           {set r_or_g($programm) 2\n"
"            set pdb_file($programm) 0\n"
"            set automatic_3d($programm) 0}\\\n"
"      elseif {$file2d($programm)==0} {set r_or_g($programm) 3\n"
"         set automatic_3d($programm) 1; fuelle_dateifeld}\\\n"
"      else {set r_or_g($programm) 0}\n"
"      if {$programm==2} {dual_conn}\n"
"    }\n"
"  radiobutton .${prog}top.rasmol -text RasMol -anchor w -variable\\\n"
"    r_or_g($programm) -value 0 -command {set torasmol($programm) 1\n"
"      set automatic_3d($programm) 0\n"
"      set pdb_file($programm) 0\n"
"      if {$programm==2} {dual_conn}\n"
"      if {$file2d($programm)==0}\\\n"
"         {set file2d($programm) 2; set automatic_2d($programm) 0\n"
"          set totkview($programm) 0}\n"
"    }\n"
"  radiobutton .${prog}top.geomview -text GeomView -anchor w -variable\\\n"
"    r_or_g($programm) -value 1 -command {set torasmol($programm) 1\n"
"      set automatic_3d($programm) 0\n"
"      set pdb_file($programm) 0\n"
"      if {$programm==2} {dual_conn}\n"
"      if {$file2d($programm)==0}\\\n"
"         {set file2d($programm) 2; set automatic_2d($programm) 0\n"
"          set totkview($programm) 0}\n"
"    }\n"
"  radiobutton .${prog}top.file3d -text \"File (\\\"writegraph3d\\\")\"\\\n"
"    -anchor w -variable r_or_g($programm) -value 3 -command {\n"
"      set automatic_3d($programm) 1; set pdb_file($programm) 0; set torasmol($programm) 1\n"
"      fuelle_dateifeld\n"
"      if {$file2d($programm)==1}\\\n"
"         {set totkview($programm) 0; set file2d($programm) 2}\n"
"    }\n"
"#---------------------------\n"
"  radiobutton .${prog}top.filepdb -text \"File (\\\"PDB\\\")\"\\\n"
"    -anchor w -variable pdb_file($programm) -value 1 -command {\n"
"      set automatic_3d($programm) 1; set r_or_g($programm) 2; set torasmol($programm) 1\n"
"      fuelle_dateifeld\n"
"      if {$file2d($programm)==1}\\\n"
"         {set totkview($programm) 0; set file2d($programm) 2}\n"
"    }\n"
"  pack .${prog}top.torasmol -side left -in .${prog}top.block13\n"
"  pack .${prog}top.rasmol .${prog}top.geomview .${prog}top.file3d .${prog}top.filepdb\\\n"
"    -side left -in .${prog}top.block13 -padx 2m\n"
"  \n"
"  # Block 16\n"
"  checkbutton .${prog}top.totkview "
"    -text \"2D representation (Schlegel diagrams):    \"\\\n"
"    -anchor w -variable totkview($programm) -command {\\\n"
"      if {$totkview($programm)==0}\\\n"
"         {set file2d($programm) 2\n"
"          set automatic_2d($programm) 0}\\\n"
"      elseif {$r_or_g($programm)==3} {set file2d($programm) 0\n"
"        set automatic_2d($programm) 1;fuelle_dateifeld}\\\n"
"      else {set file2d($programm) 1}\n"
"      if {$programm==2} {dual_conn}\n"
"    }\n"
"  radiobutton .${prog}top.display2d -text Display -anchor w -variable\\\n"
"    file2d($programm) -value 1 -command {set totkview($programm) 1\n"
"      set automatic_2d($programm) 0\n"
"      if {$programm==2} {dual_conn}\n"
"      if {$r_or_g($programm)==3 && $programm!=3}\\\n"
"         {set r_or_g($programm) 2; set automatic_3d($programm) 0\n"
"          set torasmol($programm) 0}\n"
"    }\n"
"  radiobutton .${prog}top.file2d\\\n"
"    -text \"File (\\\"writegraph2d\\\")\"\\\n"
"    -anchor w -variable file2d($programm) -value 0 -command {\n"
"      set automatic_2d($programm) 1\n"
"      fuelle_dateifeld\n"
"      set totkview($programm) 1\n"
"      if {$r_or_g($programm)<2 && $programm!=3 }\\\n"
"         {set torasmol($programm) 0; set r_or_g($programm) 2}\n"
"    }\n"
"  pack .${prog}top.totkview -side left -in .${prog}top.block16\n"
"  pack .${prog}top.display2d .${prog}top.file2d -side left\\\n"
"    -in .${prog}top.block16 -padx 2m\n"
"  \n"
"  # Block 2\n"
"  label .${prog}top.filelabel_f -text \"      Filename:\"\n"
"  entry .${prog}top.filename_f -width 40 -relief sunken\\\n"
"    -textvariable dateiname_f($programm) -highlightcolor #d9d9d9\n"
"  checkbutton .${prog}top.automatic_f -text automatic -variable\\\n"
"    automatic_f($programm) -anchor w -command {\n"
"      if {$automatic_f($programm)} {fuelle_dateifeld\n"
"          set tofile($programm) 1\n"
"          set adjacency($programm) 1\n"
"          set filecode($programm) 0}\n"
"    }\n"
"  pack .${prog}top.filelabel_f .${prog}top.filename_f\\\n"
"    .${prog}top.automatic_f -side left -padx 2m -pady 1m\\\n"
"    -in .${prog}top.block2\n"
"  bind .${prog}top.filename_f <Button> {set tofile($programm) 1\n"
"    set adjacency($programm) 1; set automatic_f($programm) 0}\n"
"  \n"
"  # Block 17\n"
"  label .${prog}top.filelabel_2d -text \"      Filename:\"\n"
"  entry .${prog}top.filename_2d -width 40 -relief sunken\\\n"
"    -textvariable dateiname_2d($programm) -highlightcolor #d9d9d9\n"
"  checkbutton .${prog}top.automatic_2d -text automatic -variable\\\n"
"    automatic_2d($programm) -anchor w -command {\n"
"      if {$automatic_2d($programm)&&$programm!=3} {fuelle_dateifeld\n"
"          set file2d($programm) 0; set totkview($programm) 1}\n"
"    }\n"
"  pack .${prog}top.filelabel_2d .${prog}top.filename_2d\\\n"
"    .${prog}top.automatic_2d -side left -padx 2m -pady 1m\\\n"
"    -in .${prog}top.block17\n"
"  bind .${prog}top.filename_2d <Button> {\n"
"      set file2d($programm) 0\n"
"      set totkview($programm) 1; set automatic_2d($programm) 0\n"
"  }\n"
"  \n"
"  # Block 18\n"
"  label .${prog}top.filelabel_3d -text \"      Filename:\"\n"
"  entry .${prog}top.filename_3d -width 40 -relief sunken\\\n"
"    -textvariable dateiname_3d($programm) -highlightcolor #d9d9d9\n"
"  checkbutton .${prog}top.automatic_3d -text automatic -variable\\\n"
"    automatic_3d($programm) -anchor w -command {\n"
"      if {$automatic_3d($programm)} {fuelle_dateifeld\n"
"          if {!$pdb_file($programm)} {\n"
"             set r_or_g($programm) 3\n"
"          }\n"
"          set torasmol($programm) 1}\n"
"    }\n"
"  pack .${prog}top.filelabel_3d .${prog}top.filename_3d\\\n"
"    .${prog}top.automatic_3d -side left -padx 2m -pady 1m\\\n"
"    -in .${prog}top.block18\n"
"  bind .${prog}top.filename_3d <Button> {set r_or_g($programm) 3\n"
"    set torasmol($programm) 1; set automatic_3d($programm) 0}\n"
"  \n"
"  # Block 10\n"
"  label .${prog}top.pipelabel -text \"     Program call:\"\n"
"  entry .${prog}top.pipename -width 38 -relief sunken\\\n"
"    -textvariable pipename($programm) -highlightcolor #d9d9d9\n"
"  pack .${prog}top.pipelabel .${prog}top.pipename -side left\\\n"
"    -padx 2m -pady 1m -in .${prog}top.block10\n"
"  \n"
"  pack .${prog}top.block13 .${prog}top.block18 .${prog}top.block16\\\n"
"       .${prog}top.block17 .${prog}top.block12 .${prog}top.block2\\\n"
"       .${prog}top.block10 .${prog}top.block11 -in .${prog}top.block9\\\n"
"       -side top -fill x -padx 2m -anchor w -pady 1m\n"
"}\n"
"\n";


char Tcl_Skript_Titelfenster[] =
"wm title . \"CaGe V0.3\"\n"
"wm geometry . 480x180\n"
"wm geometry . +10+10\n"
"frame .hauptframe\n"
"frame .hauptframe.buttonframe\n"
"pack .hauptframe -pady 3m\n"
"label .hauptframe.hauptlabel -text CaGe -font "
"*-*-bold-r-*--*-240-*-*-*-*-*-*\n"
"button .hauptframe.buttonframe.ende -text \"         Quit         \" "
"-command {set global_end 1}\n"
"button .hauptframe.buttonframe.background -text \"Go to background\" "
"-command {set global_background 1; destroy .;}\n"
"frame .hauptframe.peopleframe\n"
"button .hauptframe.peopleframe.people -text People -command {wm deiconify .people}\n"
"pack .hauptframe.peopleframe.people"
" -in .hauptframe.peopleframe\n"
"frame .hauptframe.statusframe -width 12c\n"
"pack .hauptframe.buttonframe.ende .hauptframe.buttonframe.background "
"-side left -expand 1 -padx 4m\n"
"pack .hauptframe.hauptlabel .hauptframe.buttonframe .hauptframe.peopleframe .hauptframe.statusframe"
" -side top -padx 5m -pady 1m\n"
"label .hauptframe.statusframe.bitmap\n"
"message .hauptframe.statusframe.msg -width 10c -justify center\n"
"pack .hauptframe.statusframe.bitmap .hauptframe.statusframe.msg "
"-side left -padx 2m\n"
"pack .hauptframe.statusframe -side top -padx 5m -pady 1m\n"
"# bind .hauptframe.buttonframe.ende <Destroy> {set global_end 1}\n"
"# wenn .hauptframe.ende kaputt ist, ist das ganze Fenster kaputt\n"
"update idletasks\n"
"toplevel .people\n"
"wm withdraw .people\n"
"wm title .people \"People\"\n"
"frame .people.out\n"
"text .people.text -relief ridge -bd 4 -yscrollcommand \".people.scroll set\"\n"
"  scrollbar .people.scroll -command \".people.text yview\"\n"
"button .people.cancel -text Cancel -command {wm withdraw .people}\n"
" .people.text insert end \" Gunnar     Brinkmann      gunnar@Mathematik.Uni-Bielefeld.DE\n\"\n"
" .people.text insert end \" Olaf       Delgado        delgado@Mathematik.Uni-Bielefeld.DE\n\"\n"
" .people.text insert end \" Andreas    Dress          dress@Mathematik.Uni-Bielefeld.DE\n\"\n"
" .people.text insert end \" Thomas     Harmuth        harmuth@Mathematik.Uni-Bielefeld.DE\n\"\n"
" .people.text insert end \" Brendan    McKay          bdm@cs.anu.edu.au\n\"\n"
" .people.text insert end \" Ulrike     von Nathusius  unathusi@Mathematik.Uni-Bielefeld.DE\n\"\n"
" .people.text insert end \" Tomasz     Pisanski       tomaz.pisanski@fmf.uni-lj.si\n\"\n"
" .people.text insert end \" Volker     Senkel         senkel@Mathematik.Uni-Bielefeld.DE\n\"\n"
" .people.text insert end \" Sebastian  Lisken         lisken@Mathematik.Uni-Bielefeld.DE\n\"\n"
"pack .people.text .people.scroll -side left -fill y -in .people.out\n"
"pack .people.out .people.cancel -padx 5m -pady 5m\n";

char Tcl_Skript_Schlegelfenster[] =
"toplevel .schlegel\n"
"wm withdraw .schlegel\n"
"wm title .schlegel \"Schlegel diagram\"\n"
"canvas .schlegel.c -relief groove -borderwidth 4\n"
"\n"
"frame .schlegel.block1\n"
"frame .schlegel.block2\n"
"pack .schlegel.c -fill both -expand 1 -padx 2m -pady 2m\n"
"pack .schlegel.block1 .schlegel.block2 -side top"
" -fill x -padx 2m -pady 2m\n"
"button .schlegel.num -text \"Show numbers\" -command switchNumbers "
"-relief raised\n"
"button .schlegel.print -text \"Save as PostScript file\" -command "
"printCanvas -relief raised\n"
"label .schlegel.filelabel -text \"Filename:\"\n"
"entry .schlegel.filename -width 40 -relief sunken "
"-textvariable psfile -highlightcolor #d9d9d9\n"
"pack .schlegel.num .schlegel.print -in .schlegel.block1 -side left "
"-expand 1 -padx 2m\n"
"pack .schlegel.filelabel -in .schlegel.block2 -side left -padx 1m\n"
"pack .schlegel.filename -in .schlegel.block2 -side right "
" -fill x -expand 1 -padx 1m\n"
"\n"
"bind .schlegel.c <Button-1> {\n"
"set newschlegel 1\n"
"  set border 20\n"
"\n"
"  set cwidth [winfo width .schlegel.c]\n"
"  set cheight [winfo height .schlegel.c]\n"
"\n"
"  set xmin [min [elements xpos]]\n"
"  set xmax [max [elements xpos]]\n"
"  set ymin [min [elements ypos]]\n"
"  set ymax [max [elements ypos]]\n"
"\n"
"  set dx [expr ($xmax-$xmin)/2]\n"
"  set dy [expr ($ymax-$ymin)/2]\n"
"  set xscale [expr ($cwidth/2 - $border) / $dx]\n"
"  set yscale [expr ($cheight/2 - $border) / $dy]\n"
"  if {$xscale < $yscale} {\n"
"    set scale $xscale\n"
"  } else {\n"
"    set scale $yscale\n"
"  }\n"
"\n"
"  set xmid [expr $cwidth/2]\n"
"  set ymid [expr $cheight/2]\n"
"  set cx [expr ($xmax+$xmin)/2]\n"
"  set cy [expr ($ymax+$ymin)/2]\n"
"  set xshift [expr $xmid-($cx*$scale)]\n"
"  set yshift [expr $ymid-($cy*$scale)]\n"
"\n"
"  set sx [format \"%%.1f\" %x]\n"
"  set sy [format \"%%.1f\" %y]\n"
"  set s  [format \"%%.3f\" $scale]\n"
"  set schlegelx [expr ($sx - $xshift) / $s]\n"
"  set schlegely [expr ($sy - $yshift) / $s]\n"
"}\n";


char Tcl_Skript_Hauptfenster[] =
"toplevel .haupt\n"
"wm title .haupt CaGe\n"
"frame .haupt.hauptframe\n"
"pack .haupt.hauptframe -in .haupt -pady 1m\n"
"label .haupt.hauptlabel -text \"Please choose generation program:\"\n"
"pack .haupt.hauptlabel -in .haupt.hauptframe -padx 2m -pady 1m"
" -anchor center\n"
"if {$isFullgen==1} {\n"
"  button .haupt.fullgen -text \"Fullerenes (fullgen)\""
"    -command {wm withdraw .haupt;  set programm 1}\n"
"  pack .haupt.fullgen -fill x -padx 2m -pady 1m -in .haupt.hauptframe\n"
"}\n"
"if {$isTubetype==1} {\n"
"  button .haupt.tubetype -text \"Tube-type fullerenes (tubetype)\""
"    -command {wm withdraw .haupt;  set programm 4}\n"
"  pack .haupt.tubetype -fill x -padx 2m -pady 1m -in .haupt.hauptframe\n"
"}\n"
"if {$isHCgen==1} {\n"
"  button .haupt.hcgen -text \"Hydrocarbons (HCgen)\""
"    -command {wm withdraw .haupt;  set programm 3}\n"
"  pack .haupt.hcgen -fill x -padx 2m -pady 1m -in .haupt.hauptframe\n"
"}\n"
"#frame .haupt.dummyframe1     ;# Abstandhalter\n"
"#pack .haupt.dummyframe1 -in .haupt.hauptframe -padx 2m -pady 2m\n"
"if {$is3Reggen==1} {\n"
"  button .haupt.3reggen -text \"3-regular planar maps\""
"    -command {wm withdraw .haupt;  set programm 2}\n"
"  pack .haupt.3reggen -fill x -padx 2m -pady 1m -in .haupt.hauptframe\n"
"}\n"
"button .haupt.inputfile -text \"Input file\""
"  -command {wm withdraw .haupt;  set programm 5}\n"
"pack .haupt.inputfile -fill x -padx 2m -pady 1m -in .haupt.hauptframe\n"
"update idletasks\n";

char Tcl_Skript_Fullgen_Init[] =	/* vor dem ersten fullgen-Aufruf */
  /* die "1" als Arrayelement steht fuer "fullgen" (= Programm Nr.1) */
"# Knotenzahlen:\n"
"set n_anf(1) 20\n"
"set n_end(1) 20\n"
"\n"
"# alle Symmetrien einschalten:\n"
"for {set i 1} {$i<=28} {incr i 1} {set symm(1,$i) 1}\n"
"\n"
"# Defaults fuer Optionen:\n"
"set cases(1) 0\n"
"set ipr(1) 0\n"
"set dual(1) 0\n"
"set fix(1) 1\n"
"set spistat(1) 0\n"
"set symstat(1) 0\n"
"\n"
"# Variablen fuer die Ausgabe:\n"
"set torasmol(1) 1      ;# Graphen zu rasmol bzw. geomview schicken\n"
"set totkview(1) 0      ;# Graphen zu tkview (intern) schicken\n"
"set adjacency(1) 0     ;# Adjazenzinformation wird weitergeleitet\n"
"set tofile(1) 0        ;# Adjazenzen in File speichern\n"
"set topipe(1) 0        ;# Adjazenzen in eine Pipe leiten\n"
"set dateiname_f(1) {}\n"
"set dateiname_2d(1) {}\n"
"set dateiname_3d(1) {}\n"
"set automatic_f(1) 0\n"
"set automatic_2d(1) 0\n"
"set automatic_3d(1) 0\n"
"set pdb_file(1) 0\n"
"set pipename(1) cat\n"
"set filecode(1) 2\n"
"set r_or_g(1) 0        ;# RasMol (0) oder GeomView (1) oder keines (2)\n"
"                        # oder File (3)\n"
"set file2d(1) 2        ;# Schlegel-Diagramme in File (0) oder Bildschirm\n"
"                        # (1) oder keines (2) \n"
"\n"
"# Konstanten:\n"
"set N_MAX(1) 250             ;# maximale Knotenzahl\n"
"set symmname(1,1) C1\n"
"set symmname(1,2) C2\n"
"set symmname(1,3) Ci\n"
"set symmname(1,4) Cs\n"
"set symmname(1,5) C3\n"
"set symmname(1,6) D2\n"
"set symmname(1,7) S4\n"
"set symmname(1,8) C2v\n"
"set symmname(1,9) C2h\n"
"set symmname(1,10) D3\n"
"set symmname(1,11) S6\n"
"set symmname(1,12) C3v\n"
"set symmname(1,13) C3h\n"
"set symmname(1,14) D2h\n"
"set symmname(1,15) D2d\n"
"set symmname(1,16) D5\n"
"set symmname(1,17) D6\n"
"set symmname(1,18) D3h\n"
"set symmname(1,19) D3d\n"
"set symmname(1,20) T\n"
"set symmname(1,21) D5h\n"
"set symmname(1,22) D5d\n"
"set symmname(1,23) D6h\n"
"set symmname(1,24) D6d\n"
"set symmname(1,25) Td\n"
"set symmname(1,26) Th\n"
"set symmname(1,27) I\n"
"set symmname(1,28) Ih\n"
"\n"
"# Funktionen:\n"
"proc full_check_anf {} {\n"
"  global fix full_anf full_end n_end n_anf N_MAX\n"
"  scan $full_anf \"%d\" full_anf\n"
"  if {($full_anf !=\"\") && ($full_anf > 19) && ($full_anf <= $N_MAX(1)) && "
"     !([expr $full_anf%2]) && ($full_anf <= $full_end)} {\n"
"     if {$fix(1)} {\n"
"        set n_end(1) $full_anf\n"
"        set n_anf(1) $full_anf\n"
"        set full_end $full_anf\n"
"     } else {\n"
"        set n_anf(1) $full_anf\n"
"     }\n"
"    .fulltop.start configure -text Start -fg #000 -command {set start 1}\n"
"    fuelle_full_dateifeld\n"
"  } else {\n"
"    .fulltop.start configure -text \"Input Error\" -fg #900 -command {}\n"
"  }\n"
"}\n"
"\n"
"proc full_check_end {} {\n"
"  global fix full_anf full_end n_end n_anf N_MAX\n"
"  scan $full_end \"%d\" full_end\n"
"  if {($full_end !=\"\") && ($full_end > 19) && ($full_end <= $N_MAX(1)) && "
"    !([expr $full_end%2]) && ($full_anf <= $full_end)} {\n"
"     if {$fix(1)} {\n"
"        set n_end(1) $full_end\n"
"        set n_anf(1) $full_end\n"
"        set full_anf $full_end\n"
"     } else {\n"
"        set n_end(1) $full_end\n"
"     }\n"
"    .fulltop.start configure -text Start -fg #000 -command {set start 1}\n"
"    fuelle_full_dateifeld\n"
"  } else {\n"
"    .fulltop.start configure -text \"Input Error\" -fg #900 -command {}\n"
"  }\n"
"}\n"
"\n"
"proc bilde_full_dateiname {} {\n"
"  global symm symmname          ;# Arrays mit 28 Eintraegen\n"
"  global n_anf n_end cases ipr  ;# Knotenzahlen, Faelle, ipr-Flag\n"
"  if {$ipr(1)} {set iprstring \"_ipr\"} else {set iprstring {}}\n"
"  if {$cases(1)>=1 && $cases(1)<=3} {set casestring \"_c$cases(1)\"} else "
"{set casestring {} }\n"
"  if {$n_end(1) != $n_anf(1)} {set n_start \"_start_$n_anf(1)\"} else "
"{set n_start \"\"}\n"
"  set symmstring {}\n"
"  for {set i 1} {$i<=28} {incr i} {\n"
"if {$symm(1,$i)==0} {set symmstring \"Ja\"}\n"
"  }\n"
"  # Falls nicht alle Symmetrien gesetzt => Symmetrien in Filenamen\n"
"  if {$symmstring == \"Ja\"} {\n"
"    set symmstring \"\"\n"
"    for {set i 1} {$i<=28} {incr i} {\n"
"	 if {$symm(1,$i)} "
"{set symmstring \"${symmstring}_$symmname(1,$i)\"}\n"
"    }\n"
"  }\n"
"  set dateiname "
"\"Full_codes_$n_end(1)$n_start$iprstring$casestring$symmstring\"\n"
"  return $dateiname\n"
"}\n"
"\n"
"# default-Dateinamen ermitteln:\n"
"proc fuelle_full_dateifeld {} {\n"
"  global dateiname_f dateiname_2d dateiname_3d pdb_file\n"
"  global automatic_f automatic_2d automatic_3d\n"
"  global symm symmname cases\n"
"  set symmstring {}\n"
"  for {set i 1} {$i<=28} {incr i} {\n"
"if {$symm(1,$i)==0} {set symmstring \"Ja\"}\n"
"  }\n"
"  # Buttons aufleuchten lassen\n"
"  if {$symmstring == \"Ja\"} {.fulltop.symm configure -bg #b03060} "
" else {.fulltop.symm configure -bg #d9d9d9}\n"
"  if {$cases(1)==0} {.fulltop.cases configure -bg #d9d9d9} "
" else {.fulltop.cases configure -bg #b03060}\n"
"  set dateiname [bilde_full_dateiname]\n"
"  if {$automatic_f(1)}  {set dateiname_f(1) $dateiname}\n"
"  if {$automatic_2d(1)} {set dateiname_2d(1) \"${dateiname}.2d\"}\n"
"  if {$automatic_3d(1) && !$pdb_file(1)} {set dateiname_3d(1) \"${dateiname}.3d\"}\n"
"  if {$automatic_3d(1) && $pdb_file(1)} {set dateiname_3d(1) \"${dateiname}.pdb\"}\n"
"#----------------\n"
"}\n"
"\n"
"proc full_slider1_command {value} {    ;# min-vertex-number\n"
"  global fix full_anf full_end n_end n_anf\n"
"  if {$fix(1) == 1} {\n"
"    set n_end(1) $value\n"
"    set full_anf $value\n"
"    set full_end $value\n"
"  } else {\n"
"    set full_anf $value\n"
"  }\n"
"  if {$n_anf(1) > $n_end(1)} {\n"
"    set n_anf(1) $n_end(1)\n"
"    set full_anf $n_end(1)\n"
"   }\n"
"  fuelle_full_dateifeld\n"
"  .fulltop.start configure -text Start -fg #000 -command {set start 1}\n"
"}\n"
"proc full_slider2_command {value} {    ;# max-vertex-number\n"
"  global fix full_anf full_end n_end n_anf\n"
"  if {$fix(1) == 1} {\n"
"    set n_anf(1) $value\n"
"    set full_anf $value\n"
"    set full_end $value\n"
"  } else {\n"
"    set full_end $value\n"
"  }\n"
"  if {$n_anf(1) > $n_end(1)} {\n"
"    set n_end(1) $n_anf(1)\n"
"    set full_end $n_anf(1)\n"
"  }\n"
"  fuelle_full_dateifeld\n"
"  .fulltop.start configure -text Start -fg #000 -command {set start 1}\n"
"}\n";

/*hchchchcchchchchchchchchc */
char Tcl_Skript_HCgen_Init[] =	/* vor dem ersten hcgen-Aufruf */
  /* die "3" als Arrayelement steht fuer "hcgen" (= Programm Nr.3) */
"# Konstanten:\n"
"set N_MAX(3) 450             ;# maximale Knotenzahl\n"
"\n"
"# Zahlen:\n"
"set c(3) 6\n"
"set h(3) 6\n"
"set p3 0\n"
"set gap(3) 4\n"
"set c_max $N_MAX(3)\n"
"set c_min 5\n"
"\n"
"# Wurde zuletzt ein scale-Wert nach oben oder unten korrigiert?\n"
"set corr(3) 0          ;# 0 = nach oben, 1 = nach unten\n"
"\n"
"# Defaults fuer Optionen:\n"
"set ipr(3) 0\n"
"set woh3 1           ;# 0 = OHNE, 1 = MIT H-ATOMEN\n"
"set peric(3) 0\n"
"set automatic(3) 1\n"
"\n"
"# Variablen fuer die Ausgabe:\n"
"set torasmol(3) 1      ;# Graphen zu rasmol bzw. geomview schicken\n"
"set totkview(3) 0      ;# Graphen zu tkview (intern) schicken\n"
"set adjacency(3) 0     ;# Adjazenzinformation wird weitergeleitet\n"
"set tofile(3) 0        ;# Adjazenzen in File speichern\n"
"set topipe(3) 0        ;# Adjazenzen in eine Pipe leiten\n"
"set dateiname_f(3) {}\n"
"set dateiname_2d(3) {}\n"
"set dateiname_3d(3) {}\n"
"set automatic_f(3) 0\n"
"set automatic_2d(3) 0\n"
"set automatic_3d(3) 0\n"
"set pdb_file(3) 0\n"
"set pipename(3) cat\n"
"set filecode(3) 2\n"
"set r_or_g(3) 0        ;# RasMol (0) oder GeomView (1) oder keines (2)\n"
"                        # oder File (3)\n"
"set file2d(3) 2        ;# Schlegel-Diagramme in File (0) oder Bildschirm\n"
"                        # (1) oder keines (2) \n"
"\n"
"# Funktionen:\n"
"proc bilde_HCgen_dateiname {} {\n"
"  global c h p3 ipr woh3 peric\n"
"  if {$ipr(3)} {set iprstring \"_ipr\"} else {set iprstring {}}\n"
"  if {$woh3==0} {set wohstring \"_woh\"} else {set wohstring {}}\n"
"  if {$peric(3)} {set pcstring \"_pc\"} else {set pcstring {}}\n"
"  set dateiname \"\"\n"
"  append dateiname \"c\" $c(3) \"h\" $h(3) \"_\" $p3 \"pent\" $iprstring $pcstring $wohstring \n"
"\n"
"  return $dateiname        ;# dies ist keine globale Variable\n"
"}\n"
"\n"
"#-------------------------\n"
"proc find_c_bounds {} {\n"
"  global c_max c_min c h p3 N_MAX\n"
"  set c_max [expr $N_MAX(3) - $h(3)]\n"
"  set c_cur [expr 2*$h(3)-6+$p3]\n"
"  if {$c_cur >= 5} {\n"
"    set c_min $c_cur\n"
"  } else {\n"
"    set c_min 5\n"
"  }\n"
"  set c_max [maximum_c $h(3) $p3]\n"
"  if {[expr $h(3)%2] == 0} {\n"
"    if { [expr $c_min%2]==1} {\n"
"      incr c_min 1\n"
"    }\n"
"  } else {\n"
"    if { [expr $c_min%2]==0} {\n"
"      incr c_min 1\n"
"    }\n"
"  }\n"
"  if {$c_min > $N_MAX(3)} {\n"
"    set c_max 0\n"
"    set c_min 0\n"
"  }\n"
"  if {[expr $c_max+$h(3)]>$N_MAX(3)} {\n"
"    set c_max [expr $N_MAX(3)-$h(3)]\n"
"  }\n"
"  if {$c_min > $c_max} {\n"
"    set c_max 0\n"
"    set c_min 0\n"
"  }\n"
"  if {$p3==5 && $c_max==16} {\n"
"    set c_min 14\n"
"  }\n"
"  if {$p3==4 && $c_max==12} {\n"
"    set c_min 12\n"
"  }\n"
"}\n"
"\n"
"proc find_h_bounds {} {\n"
"  global h_max h_min c h p3 N_MAX\n"
"  set h_max [expr $N_MAX(3) - $c(3)]\n"
"  set h_cur [expr ($c(3)+6-$p3)/2]\n"
"  if {$h_cur <= $h_max} {\n"
"    set h_max $h_cur\n"
"  }\n"
"  set h_min [minimum_h $c(3) $p3]\n"
"  if {[expr $c(3)%2] == 0} {\n"
"    if { [expr $h_max%2]==1} {\n"
"      incr h_max -1\n"
"    }\n"
"  } else {\n"
"    if { [expr $h_max%2]==0} {\n"
"      incr h_max -1\n"
"    }\n"
"  }\n"
"  if {$h_min>$N_MAX(3)} {\n"
"    set h_min 0\n"
"    set h_max 0\n"
"  }\n"
"  if {[expr $c(3)+$h_min]>$N_MAX(3)} {\n"
"    set h_min 0\n"
"    set h_max 0\n"
"  }\n"
"  if {!$h_min && $h_max} {\n"
"    set h_max 0\n"
"  }\n"
"  if {$h_min > $h_max} {\n"
"    set h_max 0\n"
"    set h_min 0\n"
"  }\n"
"}\n"
"\n"
"proc find_p_bounds {} {\n"
"  global p_max p_min c h p3\n"
"  set p_min -1\n"
"  set p_max -1\n"
"  for {set i 0} {$i < 6} {incr i} {\n"
"    set hexagons [expr ($c(3)-2*$i-$h(3)+2)/2]\n"
"    set boundlength [expr 2*$h(3)-6+$i]\n"
"    set inner [expr $c(3) - $boundlength]\n"
"    set min_rand [min_rand $hexagons $i]\n"
"    if {($p_min==-1) && ($inner>=0) && ($boundlength>=$min_rand)} {\n"
"      set p_min $i\n"
"      set p_max $i\n"
"    }\n"
"    if {!($p_max==-1) && ($inner>=0) && ($boundlength>=$min_rand)} {\n"
"       set p_max $i\n"
"    }\n"
"  }\n"
"}\n"
"proc reset_paint {} {\n"
"    .hctop.c_min configure -fg #000\n"
"    .hctop.c_num configure -fg #000\n"
"    .hctop.c_max configure -fg #000\n"
"    .hctop.h_min configure -fg #000\n"
"    .hctop.h_num configure -fg #000\n"
"    .hctop.h_max configure -fg #000\n"
"    .hctop.p_min configure -fg #000\n"
"    .hctop.p_num configure -fg #000\n"
"    .hctop.p_max configure -fg #000\n"
"}\n"
"proc paint_error {} {\n"
"  global c h p3 c_min c_max h_min h_max p_min p_max message N_MAX\n"
"  set end 0\n"
"  reset_paint\n"
"  set no_out 1\n"
"  if {$message == \"\"} {\n"
"    set no_out 0\n"
"  }\n"
"  if {$c_min>$c_max} {\n"
"    .hctop.c_min configure -fg #900\n"
"    .hctop.c_max configure -fg #900\n"
"    if {!$no_out} {\n"
"      set message \"$message No C boundary.\"\n"
"    }\n"
"    incr end 1\n"
"  }\n"
"  if {$h_min>$h_max} {\n"
"    .hctop.h_min configure -fg #900\n"
"    .hctop.h_max configure -fg #900\n"
"    if {!$no_out} {\n"
"      set message \"$message No H boundary.\"\n"
"    }\n"
"    incr end 2\n"
"  }\n"
"  if {[expr $c(3)+$h(3)]>$N_MAX(3)} {\n"
"    set message \"$message (C + H) <= $N_MAX(3). \"\n"
"  }\n"
"  if {!($p_min !=-1 && $p_max != -1)} {\n"
"    .hctop.p_min configure -fg #900\n"
"    .hctop.p_max configure -fg #900\n"
"    if {!$no_out} {\n"
"      set message \"$message No Pentagon boundary.\"\n"
"    }\n"
"    incr end 4\n"
"  }\n"
"  if {!($end&1)} {\n"
"    if {!(($c_min<=$c(3)) && ($c(3)<=$c_max))} {\n"
"      .hctop.c_num configure -fg #900\n"
"      if {!$no_out} {\n"
"        set message \"$message C atoms not correct.\"\n"
"      }\n"
"    }\n"
"  }\n"
"  if {!($end&2)} {\n"
"    if {!(($h_min<=$h(3)) && ($h(3)<=$h_max))} {\n"
"      .hctop.h_num configure -fg #900\n"
"      if {!$no_out} {\n"
"        set message \"$message H atoms not correct.\"\n"
"      }\n"
"    }\n"
"  }\n"
"  if {!($end&4)} {\n"
"    if {!(($p_min<=$p3) && ($p3<=$p_max))} {\n"
"      .hctop.p_num configure -fg #900\n"
"      if {!$no_out} {\n"
"        set message \"$message Number of pentagons not correct.\"\n"
"      }\n"
"    }\n"
"  }\n"
"  if {$message == \"\" && [expr $c(3)-$h(3)&1]} {\n"
"    set message \"P Number of C and H atoms have to be even or odd.\"\n"
"  }\n"
"  if {$message != \"\"} {\n"
"    .hctop.mes configure -text $message\n"
"    .hctop.start configure -fg #900 -text \"Input error.\"\n"
"  } else {\n"
"    .hctop.start configure -fg #000 -text \"Start\"\n"
"  }\n"
"}\n"
"proc entry_check {} {\n"
" global c h p3 c_entry h_entry p_entry message N_MAX\n"
"  set message \"\"\n"
"  .hctop.mes configure -text $message\n"
"  if {!(($c_entry !=\"\") && ($h_entry !=\"\") && ($p_entry !=\"\"))} {\n"
"    set message \"Input not compete\"\n"
"  } else {\n"
"    if {(([scan $c_entry \"%d\" c_entry]==0) ||($c_entry < 5)||($c_entry >  $N_MAX(3)))} {\n"
"      set message \"$message Input of C atoms not correct.\"\n"
"    }\n"
"    if {(([scan $h_entry \"%d\" h_entry]==0)||($h_entry < 5)||($h_entry > [expr $N_MAX(3)/2+3]))} {\n"
"      set message \"$message Input of H atoms not correct.\"\n"
"    }\n"
"    if {(($p_entry != 0 && [scan $p_entry \"%d\" p_entry]==0)||($p_entry < 0)||($p_entry > 5))} {\n"
"      set message \"$message Input of pentagons not correct.\"\n"
"    }\n"
"  }\n"
"  if {$message ==\"\"} {\n"
"    set c(3) $c_entry\n"
"    set h(3) $h_entry\n"
"    set p3 $p_entry\n"
"    find_bounds\n"
"  } else {\n"
"    .hctop.mes configure -text $message\n"
"  }\n"
"}\n"
"proc fuelle_hc_dateifeld {} {\n"
"  global dateiname_f dateiname_2d dateiname_3d pdb_file\n"
"  global automatic_f automatic_2d automatic_3d\n"
"  set dateiname [bilde_HCgen_dateiname]\n"
"  if {$automatic_f(3)} {set dateiname_f(3) $dateiname}\n"
"  if {$automatic_2d(3)} {set dateiname_2d(3) \"${dateiname}.2d\"}\n"
"  if {$automatic_3d(3) && !$pdb_file(3)} {set dateiname_3d(3) \"${dateiname}.3d\"}\n"
"  if {$automatic_3d(3) && $pdb_file(3)} {set dateiname_3d(3) \"${dateiname}.pdb\"}\n"
"}\n"
"\n"
"proc find_bounds {} {\n"
"  find_c_bounds\n"
"  find_h_bounds\n"
"  find_p_bounds\n"
"  paint_error\n"
"  fuelle_hc_dateifeld\n"
"}\n"
"proc hcslider_command {value} {\n"
"  global c_entry h_entry p_entry c h p3 message\n"
"  set c_entry $c(3)\n"
"  set h_entry $h(3)\n"
"  set p_entry $p3\n"
"  set message \"\"\n"
"  .hctop.mes configure -text $message\n"
"  find_bounds\n"
"}\n";


char Tcl_Skript_3reggen_Init[] =	/* vor dem ersten 3reggen-Aufruf */
  /* die "2" als Arrayelement und in "face2" steht fuer "CPF" (= Prg. Nr.2) */
"# Konstanten:\n"
"set N_MAX(2) 250             ;# maximale Knotenzahl\n"
"\n"
"# Knotenzahlen:\n"
"set n_anf(2) 4\n"
"set n_end(2) 4\n"
"\n"
"# Defaults fuer Optionen:\n"
"set case1(2) 1\n"
"set case2(2) 1\n"
"set case3(2) 1\n"
"set con1(2) 0\n"
"set con2(2) 0\n"
"set con3(2) 1\n"
"set dual(2) 0\n"
"set face2(3) 1              ;# Dreiecke sind erlaubt\n"
"set lim(2,3) 0              ;# keine Begrenzung fuer Flaechenzahl\n"
"set uplim(2,3) 0            ;# obere Begrenzung, falls gewaehlt\n"
"set lowlim(2,3) 0           ;# untere Begrenzung, falls gewaehlt\n"
"set automatic(2) 1\n"
"set fix(2) 1\n"
"set facestat(2) 0\n"
"set patchstat(2) 0\n"
"set pathface_max(2) 0\n"
"set priority(2) 123\n"
"set alt(2) 0\n"
"set newface(2) 4               ;# Knotenzahl der neuen Flaeche\n"
"set selectface(2) Empty        ;# String wird noch sinnvoll belegt\n"
"\n"
"# Variablen fuer die Ausgabe:\n"
"set torasmol(2) 1      ;# Graphen zu rasmol bzw. geomview schicken\n"
"set totkview(2) 0      ;# Graphen zu tkview (intern) schicken\n"
"set adjacency(2) 0     ;# Adjazenzinformation wird weitergeleitet\n"
"set tofile(2) 0        ;# Adjazenzen in File speichern\n"
"set topipe(2) 0        ;# Adjazenzen in eine Pipe leiten\n"
"set dateiname_f(2) {}\n"
"set dateiname_2d(2) {}\n"
"set dateiname_3d(2) {}\n"
"set automatic_f(2) 0\n"
"set automatic_2d(2) 0\n"
"set automatic_3d(2) 0\n"
"set pdb_file(2) 0\n"
"set pipename(2) cat\n"
"set filecode(2) 2\n"
"set r_or_g(2) 0        ;# RasMol (0) oder GeomView (1) oder keines (2)\n"
"                        # oder File (3)\n"
"set file2d(2) 2        ;# Schlegel-Diagramme in File (0) oder Bildschirm\n"
"                        # (1) oder keines (2) \n"
"\n"
"# Funktionen:\n"
"proc bilde_3reggen_dateiname {codes} {\n"
"  global n_anf n_end case1 case2 case3 face2 lim uplim\n"
"  global lowlim pathface_max priority alt\n"
"  global con1 con2 con3 dual\n"
"  if {$n_end(2) != $n_anf(2)} {set n_start \"_s$n_anf(2)\"} else "
"{set n_start {}}\n"
"  if {$case1(2)==0 || $case2(2)==0 || $case3(2)==0} {\n"
"    set casestring \"_t\"\n"
"    if {$case1(2)==1} {set casestring ${casestring}1}\n"
"    if {$case2(2)==1} {set casestring ${casestring}2}\n"
"    if {$case3(2)==1} {set casestring ${casestring}3}\n"
"  } else {set casestring {}}\n"
"  if {$con1(2)==0 || $con2(2)==0 || $con3(2)==0} {\n"
"    set constring \"_c\"\n"
"    if {$con1(2)==1} {set constring ${constring}1}\n"
"    if {$con2(2)==1} {set constring ${constring}2}\n"
"    if {$con3(2)==1} {set constring ${constring}3}\n"
"  } else {set constring {}}\n"
"  set facestring {}\n"
"  foreach f [lsort -integer -increasing [array names face2]] {\n"
"    # f-Ecke sind erlaubt\n"
"    set facestring ${facestring}_f$f\n"
"    if {$lim(2,$f)==1} {\n"
"      set facestring \"${facestring}+$lowlim(2,$f)-$uplim(2,$f)\"\n"
"    }\n"
"  }\n"
"  if {$pathface_max(2)==1} {set pfstring \"_pmax\"} else {set pfstring {}}\n"
"  if {$priority(2) != 123} {set priostring \"_p$priority(2)\"} "
"else {set priostring {}}\n"
"  if {$alt(2)==1} {set altstring \"_alt\"} else {set altstring {}}\n"
"  if {$dual(2)==1} {set dualstring \"_dual\"} else {set dualstring {}}\n"
"  set dateiname \"3reg_n$n_end(2)$n_start$facestring$casestring$priostring"
"$pfstring$altstring$constring$dualstring\"\n"
"  if {$codes==0} {set dateiname ${dateiname}.log}\n"
"  return $dateiname        ;# dies ist keine globale Variable\n"
"}\n"
"\n"
"proc fuelle_r3_dateifeld {} {\n"
"  global priority case1 case2 case3\n"
"  global con1 con2 con3 dual\n"
"  global dateiname_f dateiname_2d dateiname_3d pdb_file\n"
"  global automatic_f automatic_2d automatic_3d\n"
"  # Buttonfarben bestimmen\n"
"  if {$case1(2)==1 && $case2(2)==1 && $case3(2)==1 && $priority(2)==123} "
"{.r3top.cases configure -bg #d9d9d9} "
" else {.r3top.cases configure -bg #b03060}\n"
"  set dateiname [bilde_3reggen_dateiname 1]\n"
"  if {$automatic_f(2)} {set dateiname_f(2) $dateiname}\n"
"  if {$automatic_2d(2)} {set dateiname_2d(2) \"${dateiname}.2d\"}\n"
"  if {$automatic_3d(2) && !$pdb_file(2)} {set dateiname_3d(2) \"${dateiname}.3d\"}\n"
"  if {$automatic_3d(2) && $pdb_file(2)} {set dateiname_3d(2) \"${dateiname}.pdb\"}\n"
"}\n"
"\n"
"proc conn_dual {} {\n"
"  global con1 con2 con3 dual totkview torasmol\n"
"  if {$con1(2)==1 || $con2(2)==1} {\n"
"    if {$torasmol(2)==1 || $totkview(2)==1} {set dual(2) 0}\n"
"  }\n"
"  fuelle_r3_dateifeld\n"
"}\n"
"proc dual_conn {} {\n"
"  global totkview torasmol dual con1 con2 con3\n"
"  if {$dual(2)==1} {\n"
"    if {$totkview(2)==1 || $torasmol(2)==1} "
"{set con1(2) 0; set con2(2) 0; set con3(2) 1}\n"
"  }\n"
"  fuelle_r3_dateifeld\n"
"}\n"
"proc r3_slider1_command {value} {      ;# min-vertex-number\n"
"  global fix n_end n_anf\n"
"  if {$fix(2) == 1} {set n_end(2) $value}\n"
"  if {$n_anf(2) > $n_end(2)} {set n_anf(2) $n_end(2)}\n"
"  fuelle_r3_dateifeld\n"
"}\n"
"proc r3_slider2_command {value} {      ;# max-vertex-number\n"
"  global fix n_end n_anf\n"
"  if {$fix(2) == 1} {set n_anf(2) $value}\n"
"  if {$n_anf(2) > $n_end(2)} {set n_end(2) $n_anf(2)}\n"
"  fuelle_r3_dateifeld\n"
"}\n"
"proc r3_slider3_command {value} {      ;# lowlim\n"
"  global lowlim uplim face2\n"
"  foreach f [array names face2]\\\n"
"    {if {$lowlim(2,$f) > $uplim(2,$f)} {set lowlim(2,$f) $uplim(2,$f)}}\n"
"  fuelle_r3_dateifeld\n"
"}\n"
"proc r3_slider4_command {value} {      ;# uplim\n"
"  global lowlim uplim face2\n"
"  foreach f [array names face2]\\\n"
"    {if {$lowlim(2,$f) > $uplim(2,$f)} {set uplim(2,$f) $lowlim(2,$f)}}\n"
"  fuelle_r3_dateifeld\n"
"}\n"
"proc r3_slider5_command {value} {        ;# newface\n"
"  global newface face2 selectface\n"
"  if {[info exists face2($newface(2))]==0} \\\n"
"       {set selectface(2) \"Include selected size\"}\\\n"
"  else {set selectface(2) \"Discard selected size\"}\n"
"}\n"
"\n"
"proc r3_check_limitbuttons {} {     ;# Buttons setzen oder loeschen?\n"
"  global face2 lim uplim lowlim N_MAX\n"
"  foreach f [array names face2] {\n"
"    if {$lim(2,$f)==0 && \\\n"
"        [llength [pack slaves .r3faces.frame$f]]>5} {\n"
"      # Limits loeschen\n"
"      destroy .r3faces.uplim$f;  destroy .r3faces.lowlim$f\n"
"      destroy .r3faces.upliml$f; destroy .r3faces.lowliml$f\n"
"      destroy .r3faces.uplimv$f; destroy .r3faces.lowlimv$f\n"
"    } elseif {$lim(2,$f)==1 &&\\\n"
"               [llength [pack slaves .r3faces.frame$f]]<6} {\n"
"      # Limits setzen\n"
"      label .r3faces.upliml$f -text \"max:\"\n"
"      label .r3faces.uplimv$f -textvariable uplim(2,$f) -width 3\n"
"      scale .r3faces.uplim$f -from 0 -to $N_MAX(2) \\\n"
"        -orient horizontal -resolution 1 -variable uplim(2,$f)\\\n"
"        -length 3c -command {r3_slider4_command} -showvalue 0\n"
"      label .r3faces.lowliml$f -text \"min:\"\n"
"      label .r3faces.lowlimv$f -textvariable lowlim(2,$f) -width 3\n"
"      scale .r3faces.lowlim$f -from 0 -to $N_MAX(2) \\\n"
"        -orient horizontal -resolution 1 -variable lowlim(2,$f)\\\n"
"        -length 3c -command {r3_slider3_command} -showvalue 0\n"
"      pack .r3faces.lowliml$f .r3faces.lowlimv$f .r3faces.lowlim$f \\\n"
"           .r3faces.upliml$f  .r3faces.uplimv$f  .r3faces.uplim$f \\\n"
"           -side left -in .r3faces.frame$f -padx 2m\n"
"    }\n"
"  }\n"
"  fuelle_r3_dateifeld\n"
"}\n"
"\n"
"proc r3_newfacebuttons {f} {             ;# Buttons fuer Flaeche\n"
"  frame .r3faces.frame$f -width 18c -height 7m\n"
"  checkbutton .r3faces.dolim$f -text Limits -variable lim(2,$f)\\\n"
"    -anchor w -command {r3_check_limitbuttons}\n"
"  label .r3faces.f$f -text \"${f}-gons\" -anchor e -width 8\\\n"
"    -font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"  pack propagate .r3faces.frame$f 0\n"
"  pack .r3faces.f$f .r3faces.dolim$f -side left -padx 2m -in \\\n"
"    .r3faces.frame$f -fill x\n"
"  pack .r3faces.frame$f -in .r3faces.hauptframe\\\n"
"    -side top -pady 1m -expand 1 -fill x\n"
"}\n";

char Tcl_Skript_Tubetype_Init[] =	/* vor dem ersten tubetype-Aufruf */
  /* die "4" als Arrayelement steht fuer "tubetype" (= Programm Nr.4) */
"# Konstanten:\n"
"set S_MAX(4) 205             ;# maximale Sechseckanzahl\n"
"set N_MAX(4) [expr $S_MAX(4) * 4 + 20]       ;# maximale Knotenzahl\n"
"\n"
"# Wurde zuletzt ein scale-Wert nach oben oder unten korrigiert?\n"
"set corr(4) 0          ;# 0 = nach oben, 1 = nach unten\n"
"\n"
"# Defaults fuer Optionen:\n"
"set tubes(4) 1        ;# 1 = tubes, 0 = fullerenes\n"
"set tubelen(4) 4      ;# Anzahl der Sechseckringe\n"
"set default(4) 1      ;# 1 => default-tubelen\n"
"set perim(4) 0.0\n"
"set shift(4) 0\n"
"set offset1(4) 5\n"
"set offset2(4) 0\n"
"set ipr(4) 0\n"
"set automatic(4) 1\n"
"\n"
"# Variablen fuer die Ausgabe:\n"
"set torasmol(4) 1      ;# Graphen zu rasmol bzw. geomview schicken\n"
"set totkview(4) 0      ;# Graphen zu tkview (intern) schicken\n"
"set adjacency(4) 0     ;# Adjazenzinformation wird weitergeleitet\n"
"set tofile(4) 0        ;# Adjazenzen in File speichern\n"
"set topipe(4) 0        ;# Adjazenzen in eine Pipe leiten\n"
"set dateiname_f(4) {}\n"
"set dateiname_2d(4) {}\n"
"set dateiname_3d(4) {}\n"
"set automatic_f(4) 0\n"
"set automatic_2d(4) 0\n"
"set automatic_3d(4) 0\n"
"set pdb_file(4) 0\n"
"set pipename(4) cat\n"
"set filecode(4) 2\n"
"set r_or_g(4) 0        ;# RasMol (0) oder GeomView (1) oder keines (2)\n"
"                        # oder File (3)\n"
"set file2d(4) 2        ;# Schlegel-Diagramme in File (0) oder Bildschirm\n"
"                        # (1) oder keines (2) \n"
"\n"
"# Funktionen:\n"
"proc bilde_tubetype_dateiname {codes} {\n"
"  global tubes tubelen perim shift\n"
"  if {$tubes(4)} {set prgstring \"tubes\"} else "
"{set prgstring \"tt_fullerenes\"}\n"
"  set dateiname \"${prgstring}_p$perim(4)_s$shift(4)_l$tubelen(4)\"\n"
"  if {$codes==0} {set dateiname ${dateiname}.log}\n"
"  return $dateiname        ;# dies ist keine globale Variable\n"
"}\n"
"\n"
"proc tt_tubelen_default {} {\n"
"  global tubelen offset1 offset2\n"
"  if {$offset1(4)>$offset2(4)} {set tubelen(4) [expr $offset1(4) - 1]}\\\n"
"  else {set tubelen(4) [expr $offset2(4) - 1]}\n"
"  set tubelen(4) [format \"%.0f\" $tubelen(4)]\n"
"}\n"
"\n"
"proc fuelle_tube_dateifeld {} {\n"
"  global dateiname_f dateiname_2d dateiname_3d pdb_file\n"
"  global automatic_f automatic_2d automatic_3d\n"
"  global default\n"
"  if {$default(4)} {tt_tubelen_default}\n"
"  set dateiname [bilde_tubetype_dateiname 1]\n"
"  if {$automatic_f(4)} {set dateiname_f(4) $dateiname}\n"
"  if {$automatic_2d(4)} {set dateiname_2d(4) \"${dateiname}.2d\"}\n"
"  if {$automatic_3d(4) && !$pdb_file(4)} {set dateiname_3d(4) \"${dateiname}.3d\"}\n"
"  if {$automatic_3d(4) && $pdb_file(4)} {set dateiname_3d(4) \"${dateiname}.pdb\"}\n"
"}\n"
"\n"
"proc tt_shift_2_offset {value} {\n"
"  # wandelt shift und perimeter-Werte in offsets um\n"
"  global perim shift offset1 offset2\n"
"  if {$shift(4)==0} {set offset1(4) $perim(4); set offset2(4) 0}\\\n"
"  elseif {$shift(4)>0} {\n"
"    if {[expr $shift(4) % 2]==1} {\n"
"      set offset1(4) [format \"%.0f\" [expr ($perim(4) * 10 "
"- 10 * $shift(4) - 5) / 10]]\n"
"      set offset2(4) $shift(4)\n"
"    }\\\n"
"    else {\n"
"      set offset1(4) [format \"%.0f\" [expr ($perim(4) * 10 "
"- 10 * $shift(4)) / 10]]\n"
"      set offset2(4) $shift(4)\n"
"    }\n"
"  }\\\n"
"  else {   ;# eigentlich ueberfluessig\n"
"    if {[expr $shift(4) % 2]==1} {\n"
"      set offset2(4) [format \"%.0f\" [expr ($perim(4) * 10 "
"- 10 * $shift(4) - 5) / 10]]\n"
"      set offset1(4) $shift(4)\n"
"    }\\\n"
"    else {\n"
"      set offset2(4) [format \"%.0f\" [expr ($perim(4) * 10 "
"- 10 * $shift(4)) / 10]]\n"
"      set offset1(4) $shift(4)\n"
"    }\n"
"  }\n"
"  fuelle_tube_dateifeld\n"
"}\n"
"\n"
"proc tt_offset_2_shift {value} {\n"
"  # wandelt offsets in shift- und perimeter-Werte um\n"
"  global perim shift offset1 offset2\n"
"  if {$offset2(4)==0} {set perim(4) $offset1(4); set shift(4) 0}\\\n"
"  elseif {$offset1(4)==0} {set perim(4) $offset2(4); set shift(4) 0}\\\n"
"  elseif {$offset1(4)>$offset2(4)} {\n"
"    set shift(4) $offset2(4)\n"
"    if {[expr $shift(4) % 2]==1} {\n"
"      set perim(4) [expr [format \"%.1f\" [expr ($offset1(4) - 1) * 10 "
"+ 10 * $shift(4) + 5]] / 10]\n"
"    } else {\n"
"      set perim(4) [expr [format \"%.1f\" [expr ($offset1(4) - 1) * 10 "
"+ 10 * $shift(4)]] / 10]\n"
"    }\n"
"  }\\\n"
"  else {\n"
"    set shift(4) $offset1(4)\n"
"    if {[expr $shift(4) % 2]==1} {\n"
"      set perim(4) [expr [format \"%.1f\" [expr ($offset2(4) - 1) * 10 "
"+ 10 * $shift(4) + 5]] / 10]\n"
"    } else {\n"
"      set perim(4) [expr [format \"%.1f\" [expr ($offset2(4) - 1) * 10 "
"+ 10 * $shift(4)]] / 10]\n"
"    }\n"
"  }\n"
"  fuelle_tube_dateifeld\n"
"}\n"
"\n"
"proc tt_slider_command1 {value} {\n"
"  # Angleichung von perimeter an shift\n"
"  global shift perim corr\n"
"  if {[expr [format \"%.0f\" [expr $perim(4) * 2]] % 2 + \\\n"
"      [expr $shift(4) % 2]]==1} {\n"
"    if {$corr(4)==0 && $perim(4)>0} {\n"
"      set corr(4) 1;  set perim(4) [expr $perim(4) - 0.5]\n"
"    } else {\n"
"      set corr(4) 0;  set perim(4) [expr $perim(4) + 0.5]\n"
"    }\n"
"  }\n"
"  tt_shift_2_offset 0\n"
"}\n"
"\n"
"proc tt_slider_command2 {value} {\n"
"  # Angleichung von shift an perimeter\n"
"  global shift perim corr\n"
"  if {[expr [format \"%.0f\" [expr $perim(4) * 2]] % 2 + \\\n"
"      [expr $shift(4) % 2]]==1} {\n"
"    if {$corr(4)==0 && $shift(4)>0} {\n"
"      set corr(4) 1;  incr shift(4) -1\n"
"    } else {\n"
"      set corr(4) 0;  incr shift(4) 1\n"
"    }\n"
"  }\n"
"  tt_shift_2_offset 0\n"
"}\n"
"\n"
"proc unset_default {value} {\n"
"  global default\n"
"  set default(4) 0\n"
"fuelle_tube_dateifeld\n"
"}\n"
"\n";

char Tcl_Skript_Inputfile_Init[] =	/* vor dem ersten inputfile-Aufruf */
  /* die "5" als Arrayelement steht fuer "inputfile" (= Programm Nr.5) */
"# Inputfile:\n"
"set inputfilename5 \"unnamed\"\n"
"set inputpipename5 \"unnamed\"\n"
"set inputradio 1\n"
"\n"
"# Einbettungsparameter:\n"
"set old_embed5 0     ;# nimm alte Einbettung (falls vorhanden)\n"
"set new_embed5 1     ;# bette neu ein\n"
"set emb_a5 10\n"
"set emb_b5 200\n"
"set emb_c5 20\n"
"set emb_d5 0\n"
"\n"
"# Variablen fuer die Ausgabe:\n"
"set torasmol(5) 1      ;# Graphen zu rasmol bzw. geomview schicken\n"
"set totkview(5) 0      ;# Graphen zu tkview (intern) schicken\n"
"set adjacency(5) 0     ;# Adjazenzinformation wird weitergeleitet\n"
"set tofile(5) 0        ;# Adjazenzen in File speichern\n"
"set topipe(5) 0        ;# Adjazenzen in eine Pipe leiten\n"
"set dateiname_f(5) {}\n"
"set dateiname_2d(5) {}\n"
"set dateiname_3d(5) {}\n"
"set automatic_f(5) 0\n"
"set automatic_2d(5) 0\n"
"set automatic_3d(5) 0\n"
"set pdb_file(5) 0\n"
"set pipename(5) cat\n"
"set filecode(5) 2\n"
"set r_or_g(5) 0        ;# RasMol (0) oder GeomView (1) oder keines (2)\n"
"                        # oder File (3)\n"
"set file2d(5) 2        ;# Schlegel-Diagramme in File (0) oder Bildschirm\n"
"                        # (1) oder keines (2) \n"
"\n"
"# Konstanten:\n"
"set N_MAX(5) 450             ;# maximale Knotenzahl\n"
"\n"
"# Funktionen:\n"
"proc bilde_file_dateiname {} {\n"
"  global inputfilename5\n"
"  set dateiname \"\"\n"
"  append dateiname $inputfilename5\n"
"  append dateiname \"_new\"\n"
"  return $dateiname\n"
"}\n"
"\n"
"proc ex_datei {} {\n"
"  global inputfilename5\n"
"  if {[file exists $inputfilename5] == 0} {\n"
"     .filetop.start configure -text \"No File\"\n"
"     return 0\n"
"  } else {\n"
"     .filetop.start configure -text \"Start\"\n"
"     return 1\n"
"  }\n"
"}\n"
"\n"
"proc fuelle_file_dateifeld {} {\n"
"  global dateiname_f dateiname_2d dateiname_3d pdb_file\n"
"  global automatic_f automatic_2d automatic_3d\n"
"  set dateiname [bilde_file_dateiname]\n"
"  if {$automatic_f(5)}  {set dateiname_f(5) $dateiname}\n"
"  if {$automatic_2d(5)} {set dateiname_2d(5) \"${dateiname}.2d\"}\n"
"  if {$automatic_3d(5) && !$pdb_file(5)} {set dateiname_3d(5) \"${dateiname}.3d\"}\n"
"  if {$automatic_3d(5) && $pdb_file(5)} {set dateiname_3d(5) \"${dateiname}.pdb\"}\n"
"}\n"
"\n"
"proc file_slider_command {value} {    ;# Embedding-Parameter veraendert\n"
"  global new_embed\n"
"  set new_embed5 1\n"
"}\n";

char Tcl_Skript_Fullgen_Caseauswahl[] =
"toplevel .fullcases\n"
"wm title .fullcases Fullerenes\n"
"wm withdraw .fullcases\n"
"frame .fullcases.hauptframe\n"
"pack .fullcases.hauptframe -in .fullcases -pady 2m -side top\n"
"frame .fullcases.block5\n"
"button .fullcases.ok -text Done -command {wm withdraw .fullcases}\n"
"pack .fullcases.block5 .fullcases.ok -side top -padx 2m"
" -in .fullcases.hauptframe\n"
"\n"
"# Block 5\n"
"radiobutton .fullcases.allcases -text \"All cases\" -variable cases(1) "
"-value 0 -anchor w -command {fuelle_full_dateifeld}\n"
"radiobutton .fullcases.case1 -text \"Only case 1\" -variable cases(1) "
"-value 1 -anchor w -command {fuelle_full_dateifeld}\n"
"radiobutton .fullcases.case2 -text \"Only case 2\" -variable cases(1) "
"-value 2 -anchor w -command {fuelle_full_dateifeld}\n"
"radiobutton .fullcases.case3 -text \"Only case 3\" -variable cases(1) "
"-value 3 -anchor w -command {fuelle_full_dateifeld}\n"
"pack .fullcases.allcases .fullcases.case1 .fullcases.case2 "
".fullcases.case3 -in .fullcases.block5 -side top -fill x -anchor w\n";


char Tcl_Skript_Fullgen_Symmetrieauswahl[] =
"toplevel .fullsymm\n"
"wm title .fullsymm \"Symmetry filter for fullerenes\"\n"
"wm withdraw .fullsymm\n"
"frame .fullsymm.mainframe\n"
"label .fullsymm.label -text \"Please choose symmetries:\"\n"
"frame .fullsymm.block1    ;# Checkbuttons\n"
"frame .fullsymm.block2    ;# Buttons\n"
"pack .fullsymm.mainframe -in .fullsymm -pady 2m\n"
"pack .fullsymm.label .fullsymm.block1 .fullsymm.block2 "
"-in .fullsymm.mainframe -padx 4m -side top -expand 1 -fill x\n"
"pack .fullsymm.block2 -in .fullsymm.mainframe -padx 4m -side top"
" -expand 1\n"
"\n"
"# Block 1\n"
"frame .fullsymm.block3\n"
"frame .fullsymm.block4\n"
"frame .fullsymm.block5\n"
"frame .fullsymm.block6\n"
"frame .fullsymm.block7\n"
"frame .fullsymm.block8\n"
"frame .fullsymm.block9\n"
"pack .fullsymm.block3 .fullsymm.block4 .fullsymm.block5 .fullsymm.block6 "
".fullsymm.block7 .fullsymm.block8 .fullsymm.block9 -in .fullsymm.block1 "
"-pady 1m -padx 3m -side left -expand 1 -fill x\n"
"\n"
"#Block 2\n"
"button .fullsymm.clear -text \"Clear all\" "
"  -command {for {set i 1} {$i < 29} {incr i} {set symm(1,$i) 0}\n"
"            fuelle_full_dateifeld}\n"
"button .fullsymm.set   -text \"Select all\" "
"  -command {for {set i 1} {$i < 29} {incr i} {set symm(1,$i) 1}\n"
"            fuelle_full_dateifeld}\n"
"button .fullsymm.cont  -text Done -command {wm withdraw .fullsymm}\n"
"pack .fullsymm.clear .fullsymm.set .fullsymm.cont -in .fullsymm "
"   -expand 1 -pady 2m -side left\n"
"\n"
"# Bloecke 3 bis 9\n"
"set j 0\n"
"set bl 3\n"
"for {set i 1} {$i<=28} {incr i} {\n"
"  incr j 1\n"
"  set h($j) $i\n"
"  eval checkbutton .fullsymm.b$i -text $symmname(1,$i) -anchor w "
"    -variable symm(1,$i) -command {fuelle_full_dateifeld}\n"
"  if {$j==4} {\n"
"    pack .fullsymm.b$h(1) .fullsymm.b$h(2) .fullsymm.b$h(3) "
" .fullsymm.b$h(4) -in .fullsymm.block$bl -anchor w -side top -fill x "
"-expand 1\n"
"    set j 0\n"
"    incr bl 1\n"
"  }\n"
"}\n";


char Tcl_Skript_3reggen_Caseauswahl[] =
"toplevel .r3cases\n"
"wm title .r3cases \"3-regular planar maps\"\n"
"wm withdraw .r3cases\n"
"frame .r3cases.hauptframe\n"
"pack .r3cases.hauptframe -in .r3cases -pady 2m -side top -expand 1\n"
"frame .r3cases.block1\n"
"frame .r3cases.block2\n"
"button .r3cases.ok -text Done -command {wm withdraw .r3cases}\n"
"pack .r3cases.ok -side top -padx 2m -pady 2m -in .r3cases\n"
"pack .r3cases.block1 .r3cases.block2 -in .r3cases.hauptframe -side left"
" -expand 1 -fill x\n"
"\n"
"# Block 1      ;# Cases\n"
"checkbutton .r3cases.case1 -text \"Do case 1\" -variable case1(2) "
"-anchor w -command {fuelle_r3_dateifeld}\n"
"checkbutton .r3cases.case2 -text \"Do case 2\" -variable case2(2) "
"-anchor w -command {fuelle_r3_dateifeld}\n"
"checkbutton .r3cases.case3 -text \"Do case 3\" -variable case3(2) "
"-anchor w -command {fuelle_r3_dateifeld}\n"
"pack .r3cases.case1 .r3cases.case2 .r3cases.case3 -in .r3cases.block1"
" -side top -padx 3m -expand 1\n"
"\n"
"# Block 2      ;# Priority\n"
"label .r3cases.priolabel -text \"Priority:\"\n"
"radiobutton .r3cases.prio123 -text 123 -variable priority(2) -value 123"
" -anchor w -command {fuelle_r3_dateifeld}\n"
"radiobutton .r3cases.prio132 -text 132 -variable priority(2) -value 132"
" -anchor w -command {fuelle_r3_dateifeld}\n"
"radiobutton .r3cases.prio213 -text 213 -variable priority(2) -value 213"
" -anchor w -command {fuelle_r3_dateifeld}\n"
"radiobutton .r3cases.prio231 -text 231 -variable priority(2) -value 231"
" -anchor w -command {fuelle_r3_dateifeld}\n"
"radiobutton .r3cases.prio312 -text 312 -variable priority(2) -value 312"
" -anchor w -command {fuelle_r3_dateifeld}\n"
"radiobutton .r3cases.prio321 -text 321 -variable priority(2) -value 321"
" -anchor w -command {fuelle_r3_dateifeld}\n"
"pack .r3cases.priolabel .r3cases.prio123 .r3cases.prio132 .r3cases.prio213"
" .r3cases.prio231 .r3cases.prio312 .r3cases.prio321 -side top"
" -in .r3cases.block2 -padx 3m -expand 1 -fill x\n";


char Tcl_Skript_3reggen_Faceauswahl[] =
"toplevel .r3faces\n"
"wm title .r3faces \"3-regular planar maps\"\n"
"wm withdraw .r3faces\n"
"frame .r3faces.hauptframe -relief groove -borderwidth 2\n"
"label .r3faces.face -text \"Face sizes to be included:\"\n"
"pack .r3faces.face -in .r3faces.hauptframe -side top -pady 2m\n"
"pack .r3faces.hauptframe -in .r3faces -side top -padx 2m -pady 2m\n"
"foreach f [lsort -integer -increasing [array names face2]]"
" {r3_newfacebuttons $f}\n"
"r3_check_limitbuttons\n"
"r3_slider5_command $newface(2)\n"
"frame .r3faces.newface -relief groove -borderwidth 2\n"
"label .r3faces.select -text \"Selected face size\"\n"
"button .r3faces.new -textvariable selectface(2) -command {\n"
"  if {[info exists face2($newface(2))]==0} {   ;# neue Flaeche\n"
"    set face2($newface(2)) 1\n"
"    # Sind schon Werte aus einem alten Aufruf vorhanden?\n"
"    if {[info exists lim(2,$newface(2))]==0} {\n"
"      set lim(2,$newface(2)) 0\n"
"      set uplim(2,$newface(2)) 0\n"
"      set lowlim(2,$newface(2)) 0\n"
"    }\n"
"    r3_newfacebuttons $newface(2)\n"
"    r3_check_limitbuttons\n"
"  } else {  ;# Flaeche loeschen\n"
"    unset face2($newface(2))\n"
"    if {[llength [pack slaves .r3faces.frame$newface(2)]]>5} {\n"
"      # Limits loeschen\n"
"      destroy .r3faces.uplim$newface(2)\n"
"      destroy .r3faces.lowlim$newface(2)\n"
"      destroy .r3faces.upliml$newface(2)\n"
"      destroy .r3faces.lowliml$newface(2)\n"
"      destroy .r3faces.uplimv$newface(2)\n"
"      destroy .r3faces.lowlimv$newface(2)\n"
"    }\n"
"    destroy .r3faces.f$newface(2)\n"
"    destroy .r3faces.dolim$newface(2)\n"
"    destroy .r3faces.frame$newface(2)\n"
"  }\n"
"  r3_slider5_command $newface(2)\n"
"  fuelle_r3_dateifeld\n"
"}\n"
"scale .r3faces.newscale -label \"Face size (atoms):\"\\\n"
"  -from 3 -to $N_MAX(2) -orient horizontal -resolution 1 -variable \\\n"
"  newface(2) -length 174m -command {r3_slider5_command}\n"
"pack .r3faces.select .r3faces.newscale .r3faces.new -in \\\n"
"  .r3faces.newface -side top -padx 2m -pady 2m\n"
"pack .r3faces.newface -side bottom -in .r3faces -pady 2m -padx 2m\n";

char Tcl_Skript_Fullgenfenster[] =	/* Optionsfenster fuer Fullgen */
"toplevel .fulltop\n"
"wm title .fulltop \"Fullerenes (fullgen) - Main Options\"\n"
"frame .fulltop.block4                  ;#  fuer Knotenzahl\n"
"pack .fulltop.block4 -in .fulltop -side top -padx 2m -pady 1m "
" -fill x -expand 1\n"
"frame .fulltop.block4b                  ;#  fuer Knotenzahl\n"
"pack .fulltop.block4b -in .fulltop -side top -padx 2m -pady 1m "
" -fill x -expand 1\n"
"zeichne_outputoptionen\n"
"frame .fulltop.block7 -relief groove -borderwidth 4   ;# Extraoptionen\n"
"frame .fulltop.block3                  ;#  fuer Start und Cancel\n"
"pack .fulltop.block8 .fulltop.block7 .fulltop.block3 "
"-in .fulltop -side top -pady 2m -padx 4m -fill x -expand 1\n"
"\n"
"# Block 7\n"
"label .fulltop.optionslabel -text \"Extra options\"\n"
"frame .fulltop.block1                  ;#  fuer Optionen\n"
"pack .fulltop.optionslabel .fulltop.block1 -in .fulltop.block7 "
"-side top -pady 1m -fill x\n"
"\n"
"# Block 4\n"
"scale .fulltop.minvertexnumber -label \"Minimum number of atoms\""
" -from 20 -to $N_MAX(1) -orient horizontal -resolution 2 -variable n_anf(1)"
" -command {full_slider1_command}\n"
"scale .fulltop.maxvertexnumber -label \"Maximum number of atoms\""
" -from 20 -to $N_MAX(1) -orient horizontal -resolution 2 -variable n_end(1)"
" -command {full_slider2_command}\n"
"checkbutton .fulltop.fixvertexnumber -text \"Min=Max\" -variable fix(1)"
" -command {full_slider1_command $n_anf(1); fuelle_full_dateifeld\n"
" if {$spistat(1)==1} {set fix(1) 1; .fulltop.spistat flash}}\n"
"  # Wenn \"spistat\" gewaehlt ist, muessen die Anfangs- und\n"
"  # Endknotenzahlen gleich sein.\n"
"\n"
"pack .fulltop.fixvertexnumber -side right -fill x "
"-in .fulltop.block4 -anchor s\n"
"pack .fulltop.minvertexnumber .fulltop.maxvertexnumber "
"-side left -padx 1m -fill x -in .fulltop.block4 -expand 1\n"
"\n"
"#Block 4b\n"
"label .fulltop.min_text -text \"Minimum number of atoms\"\n"
"label .fulltop.max_text -text \"Maximum number of atoms\"\n"
"entry .fulltop.min_in -width 10 -relief sunken\\\n"
"  -textvariable full_anf -highlightcolor #d9d9d9\n"
"bind .fulltop.min_in <KeyRelease> {full_check_anf}\n"
"entry .fulltop.max_in -width 10 -relief sunken\\\n"
"  -textvariable full_end -highlightcolor #d9d9d9\n"
"bind .fulltop.max_in <KeyRelease> {full_check_end}\n"
"pack .fulltop.min_text .fulltop.min_in .fulltop.max_text .fulltop.max_in "
" -side left -padx 1m -fill x -in .fulltop.block4b\n"
"# Block 3\n"
"button .fulltop.start -text Start -command {set start 1}\n"
"button .fulltop.cancel -text Cancel -command {set cancel 1}\n"
"pack .fulltop.start .fulltop.cancel -side left -in .fulltop.block3 "
"-pady 1m -expand 1\n"
"\n"
"# Block 1\n"
"frame .fulltop.block5    ;#  fuer checkbutton-Optionen\n"
"frame .fulltop.block6    ;#  fuer button- Optionen\n"
"pack .fulltop.block5 .fulltop.block6 -in .fulltop.block1 -padx 2m "
"-side left -expand 1 -pady 1m -anchor center\n"
"\n"
"# Block 5\n"
"checkbutton .fulltop.ipr -text \"Isolated pentagons (ipr)\" -anchor w "
"-variable ipr(1) -command {fuelle_full_dateifeld}\n"
"checkbutton .fulltop.dual -text \"Dual output (triangulations)\" -anchor w "
"-variable dual(1) -command {fuelle_full_dateifeld}\n"
"checkbutton .fulltop.spistat -text \"Spiral statistics\" "
"-variable spistat(1) -anchor w "
"-command {if {$spistat(1)==1} "
"{set fix(1) 1; full_slider1_command $n_anf(1)}}\n"
"checkbutton .fulltop.symstat -text \"Symmetry statistics\"  -anchor w "
"-variable symstat(1)\n"
"pack .fulltop.ipr .fulltop.dual .fulltop.spistat .fulltop.symstat "
"-in .fulltop.block5 -side top -fill x\n"
"\n"
"# Block 6\n"
"button .fulltop.symm -text \"Symmetry filter\" "
"-command {wm deiconify .fullsymm}\n"
"button .fulltop.cases -text \"Select cases\" "
"-command {wm deiconify .fullcases}\n"
"pack .fulltop.symm .fulltop.cases "
"-in .fulltop.block6 -side top -fill x -pady 1m\n"
"\n"
"fuelle_full_dateifeld\n";



char Tcl_Skript_HCgenfenster[] =
"toplevel .hctop\n"
"wm title .hctop \"Hydrocarbons - options\"\n"
"frame .hctop.block4          ;# fuer Atomzahlen\n"
"frame .hctop.block4b         ;# direkte Eingabe\n"
"frame .hctop.c_direkt -relief groove -borderwidth 4\n"
"frame .hctop.c_direkt_num\n"
"frame .hctop.h_direkt -relief groove -borderwidth 4\n"
"frame .hctop.h_direkt_num\n"
"frame .hctop.p_direkt -relief groove -borderwidth 4\n"
"frame .hctop.p_direkt_num\n"
"frame .hctop.err_mes         ;# error message\n"
"frame .hctop.sum_box -relief groove -borderwidth 4\n"
"pack .hctop.block4 -in .hctop -side top -padx 2m -pady 1m \\\n"
"     -fill x -expand 1\n"
"pack .hctop.block4b -in .hctop -side top -padx 2m -pady 1m \\\n"
"     -fill x -expand 1\n"
"pack .hctop.err_mes -in .hctop -side top -padx 2m -pady 1m \\\n"
"     -fill x -expand 1\n"
"frame .hctop.block7 -relief groove -borderwidth 4  ;# Extraoptionen\n"
"frame .hctop.block3                  ;#  fuer Start und Cancel\n"
"zeichne_outputoptionen\n"
"pack .hctop.block8 .hctop.block7 .hctop.block3 \\\n"
"     -in .hctop -side top -pady 2m -padx 4m -fill x -expand 1\n"
"\n"
"# Block 4\n"
"scale .hctop.cnumber -label \"Number of C atoms\" -from 0 -to $N_MAX(3) \\\n"
"      -orient horizontal -resolution 1 -variable c(3) \\\n"
"      -command {hcslider_command}\n"
"scale .hctop.hnumber -label \"Number of H atoms\" -from 0 -to \\\n"
"      [expr $N_MAX(3)/2+3]\\\n"
"      -orient horizontal -resolution 1 -variable h(3) \\\n"
"      -command {hcslider_command}\n"
"scale .hctop.pnumber -label \"Pentagons\" -from 0 -to 5 \\\n"
"      -orient horizontal -resolution 1 -variable p3 \\\n"
"      -command {hcslider_command}\n"
"pack .hctop.cnumber .hctop.hnumber -side left -padx 1m -fill x \\\n"
"     -in .hctop.block4 -expand 1\n"
"pack .hctop.pnumber -side left -padx 1m -in .hctop.block4\n"
"\n"
"#Block4b\n"
"label .hctop.c_text -text \"C-Atoms\"\n"
"label .hctop.c_min_text -text \"min.\"\n"
"label .hctop.c_max_text -text \"max.\"\n"
"label .hctop.c_min -textvariable c_min -width 3\n"
"entry .hctop.c_num -width 8 -relief sunken\\\n"
"  -textvariable c_entry -highlightcolor #d9d9d9\n"
"bind .hctop.c_num <KeyRelease> {entry_check}\n"
"label .hctop.c_max -textvariable c_max -width 3\n"
"pack .hctop.c_min_text .hctop.c_min .hctop.c_num .hctop.c_max_text "
".hctop.c_max -side left -fill x -expand 1 -in .hctop.c_direkt_num\n"
"pack  .hctop.c_text .hctop.c_direkt_num -in .hctop.c_direkt\n"
"label .hctop.h_text -text \"H-Atoms\"\n"
"label .hctop.h_min_text -text \"min.\"\n"
"label .hctop.h_max_text -text \"max.\"\n"
"label .hctop.h_min -textvariable h_min -width 3\n"
"entry .hctop.h_num -width 8 -relief sunken\\\n"
"  -textvariable h_entry -highlightcolor #d9d9d9\n"
"bind .hctop.h_num <KeyRelease> {entry_check}\n"
"label .hctop.h_max -textvariable h_max -width 3\n"
"pack .hctop.h_min_text .hctop.h_min .hctop.h_num .hctop.h_max_text "
".hctop.h_max -side left -fill x -expand 1 -in .hctop.h_direkt_num\n"
"pack  .hctop.h_text .hctop.h_direkt_num -fill x -in .hctop.h_direkt\n"
"label .hctop.p_text -text \"Pentagons\"\n"
"label .hctop.p_min_text -text \"min.\"\n"
"label .hctop.p_max_text -text \"max.\"\n"
"label .hctop.p_min -textvariable p_min -width 3\n"
"entry .hctop.p_num -width 8 -relief sunken\\\n"
"  -textvariable p_entry -highlightcolor #d9d9d9\n"
"bind .hctop.p_num <KeyRelease> {entry_check}\n"
"label .hctop.p_max -textvariable p_max -width 3\n"
"pack .hctop.p_min_text .hctop.p_min .hctop.p_num .hctop.p_max_text "
".hctop.p_max -side left -fill x -expand 1 -in .hctop.p_direkt_num\n"
"pack  .hctop.p_text .hctop.p_direkt_num -in .hctop.p_direkt\n"
"pack .hctop.c_direkt  .hctop.h_direkt .hctop.p_direkt"
" -side left -fill x -expand 1 -in .hctop.block4b\n"
"\n"
"#Bolck err_mes\n"
"label .hctop.mes -fg #900 -text \"\"\n"
"pack .hctop.mes -fill x -in .hctop.err_mes\n"
"\n"
"# Block 7\n"
"label .hctop.optionslabel -text \"Extra options\"\n"
"frame .hctop.block1                  ;#  fuer Optionen\n"
"pack .hctop.optionslabel .hctop.block1 -in .hctop.block7 "
"-side top -pady 1m -fill x\n"
"\n"
"# Block 3\n"
"button .hctop.start -text Start -command {set start 1;\n"
"}\n"
"button .hctop.cancel -text Cancel -command {set cancel 1;\n"
"}\n"
"pack .hctop.start .hctop.cancel -side left -in .hctop.block3 "
"-pady 1m -expand 1\n"
"\n"
"# Block 1\n"
"checkbutton .hctop.ipr -text \"Isolated pentagons (ipr)\" -anchor w "
"-variable ipr(3) -command {fuelle_hc_dateifeld}\n"
"checkbutton .hctop.woh -text \"Include H atoms\" -anchor w "
"-variable woh3 -command {fuelle_hc_dateifeld}\n"
"checkbutton .hctop.peric -text \"Strictly peri-condensed\" -anchor w "
"-variable peric(3) -command {fuelle_hc_dateifeld}\n"
"scale .hctop.gap -label \"Maximum size of a gap\" -from 1 -to $N_MAX(3)\\\n"
"      -orient horizontal -resolution 1 -variable gap(3)\n"
"pack .hctop.ipr .hctop.woh .hctop.peric -in .hctop.block1 -padx 2m "
"-side left -expand 1\n"
"pack .hctop.gap -in .hctop.block7 -padx 2m -pady 2m -side top -fill x"
" -expand 1\n"
"\n"
"fuelle_hc_dateifeld\n";

/*vs iiiiiiiiiiii */
char Tcl_Skript_3reggenfenster[] =
"toplevel .r3top\n"
"wm title .r3top \"3-regular planar maps - Main Options\"\n"
"frame .r3top.block4                  ;#  fuer Knotenzahl\n"
"pack .r3top.block4 -in .r3top -side top -padx 2m -pady 1m "
" -fill x -expand 1\n"
"frame .r3top.block7 -relief groove -borderwidth 4   ;# Extraoptionen\n"
"frame .r3top.block3                  ;#  fuer Start und Cancel\n"
"zeichne_outputoptionen\n"
"pack .r3top.block8 .r3top.block7 .r3top.block3 "
"-in .r3top -side top -pady 2m -padx 4m -fill x -expand 1\n"
"\n"
"# Block 7\n"
"label .r3top.optionslabel -text \"Extra options\"\n"
"frame .r3top.block1                  ;#  fuer Optionen\n"
"pack .r3top.optionslabel .r3top.block1 -in .r3top.block7 "
"-side top -pady 1m -fill x\n"
"\n"
"# Block 4\n"
"scale .r3top.minvertexnumber -label \"Minimum number of atoms\"\\\n"
" -from 4 -to $N_MAX(2) -orient horizontal -resolution 2 -variable n_anf(2)"
" -command {r3_slider1_command}\n"
"scale .r3top.maxvertexnumber -label \"Maximum number of atoms\"\\\n"
" -from 4 -to $N_MAX(2) -orient horizontal -resolution 2 -variable n_end(2)"
" -command {r3_slider2_command}\n"
"checkbutton .r3top.fixvertexnumber -text \"Min=Max\" -variable fix(2)"
" -command {r3_slider1_command $n_anf(2)}\n"
"\n"
"pack .r3top.fixvertexnumber -side right -fill x "
"-in .r3top.block4 -anchor s\n"
"pack .r3top.minvertexnumber .r3top.maxvertexnumber "
"-side left -padx 1m -fill x -in .r3top.block4 -expand 1\n"
"\n"
"# Block 3\n"
"button .r3top.start -text Start -command {set start 1}\n"
"button .r3top.cancel -text Cancel -command {set cancel 1}\n"
"pack .r3top.start .r3top.cancel -side left -in .r3top.block3 "
"-pady 1m -expand 1\n"
"\n"
"# Block 1\n"
"frame .r3top.block5    ;#  fuer checkbutton-Optionen\n"
"frame .r3top.block6    ;#  fuer button-Optionen\n"
"pack .r3top.block5 .r3top.block6 -in .r3top.block1 -padx 2m "
"-side left -expand 1 -pady 1m -anchor center\n"
"\n"
"# Block 5\n"
"checkbutton .r3top.alt -text \"Use alternative ECC\" -anchor w "
"-variable alt(2) -command {fuelle_r3_dateifeld}\n"
"checkbutton .r3top.pmax -text \"Pathface maximal\" -anchor w "
"-variable pathface_max(2) -command {fuelle_r3_dateifeld}\n"
"checkbutton .r3top.dual -text \"Dual output (triangulations)\" -anchor w "
"-variable dual(2) -command dual_conn\n"
"checkbutton .r3top.facestat -text \"Face statistics\" "
"-variable facestat(2) -anchor w\n"
"checkbutton .r3top.patchstat -text \"Patch statistics\" -anchor w "
"-variable patchstat(2)\n"
"pack .r3top.alt .r3top.pmax .r3top.dual .r3top.facestat\\\n"
"  .r3top.patchstat -in .r3top.block5 -side top -fill x\n"
"\n"
"# Block 6\n"
"button .r3top.cases -text \"Select cases\\n and priority\" "
"-command {wm deiconify .r3cases}\n"
"frame .r3top.connframe\n"
"pack .r3top.cases .r3top.connframe -in .r3top.block6 -side top -fill x\\\n"
"  -pady 1m\n"
"checkbutton .r3top.con1 -text \"1-connected graphs\" -variable con1(2) "
"-anchor w -command conn_dual\n"
"checkbutton .r3top.con2 -text \"2-connected graphs\" -variable con2(2) "
"-anchor w -command conn_dual\n"
"checkbutton .r3top.con3 -text \"3-connected graphs\" -variable con3(2) "
"-anchor w -command conn_dual\n"
"pack .r3top.con1 .r3top.con2 .r3top.con3 -in .r3top.connframe"
" -side top -padx 3m -expand 1\n"
"\n"
"\n"
"fuelle_r3_dateifeld\n";

char Tcl_Skript_Tubetypefenster[] =
"toplevel .tubetop\n"
"wm title .tubetop \"Tube-type fullerenes (tubetype) - Main Options\"\n"
"frame .tubetop.block15 -relief groove -borderwidth 4  ;#  fuer Optionen\n"
"frame .tubetop.block4                  ;#  fuer Tubelen\n"
"frame .tubetop.block1                  ;#  fuer shift + perimeter\n"
"frame .tubetop.block7                  ;#  fuer Graphtypen\n"
"frame .tubetop.block3                  ;#  fuer Start und Cancel\n"
"zeichne_outputoptionen\n"
"pack .tubetop.block7 .tubetop.block4 .tubetop.block1 -in .tubetop.block15 "
"-side top -pady 1m -padx 2m -fill x -expand 1\n"
"pack .tubetop.block15 .tubetop.block8\\\n"
"  .tubetop.block3 -in .tubetop -side top -pady 2m -padx 4m -fill x -expand 1\n"
"\n"
"# Block 7\n"
"label .tubetop.label1 -text \"Generate:\"\n"
"radiobutton .tubetop.tubes -text \"tubes\" -variable tubes(4) -value 1\\\n"
"  -anchor w\n"
"radiobutton .tubetop.fulls -text \"fullerenes\" -variable tubes(4)\\\n"
"  -value 0 -anchor w\n"
"checkbutton .tubetop.ipr -text \"IPR (isolated pentagons)\"\\\n"
"  -variable ipr(4) -anchor w\n"
"pack .tubetop.label1 .tubetop.tubes .tubetop.fulls .tubetop.ipr\\\n"
"  -in .tubetop.block7 -side left -padx 1m -fill x -expand 1\n"
"\n"
"# Block 4\n"
"label .tubetop.label2 -text \"Tube length:\"\n"
"scale .tubetop.tubelen -from 0 -to [format \"%.0f\" "
"[expr sqrt($N_MAX(4))]]\\n"
"  -showvalue 0  -orient horizontal\\\n"
"  -resolution 1 -variable tubelen(4) -command unset_default\n"
"label .tubetop.label3 -textvariable tubelen(4) -width 3\n"
"checkbutton .tubetop.default -text \"default\" -anchor w\\\n"
"  -variable default(4) -command {tt_tubelen_default}\n"
"pack .tubetop.label2 .tubetop.label3\\\n"
"  -in .tubetop.block4 -side left -padx 1m -fill x\n"
"pack .tubetop.tubelen\\\n"
"  -in .tubetop.block4 -side left -padx 1m -fill x -expand 1\n"
"pack .tubetop.default\\\n"
"  -in .tubetop.block4 -side left -padx 1m -fill x\n"
"\n"
"# Block 1\n"
"frame .tubetop.block5    ;#  fuer perim-shift-Optionen\n"
"label .tubetop.label4 -text \"<=>\" "
"-font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"frame .tubetop.block6    ;#  fuer offset-Optionen\n"
"pack .tubetop.block5 .tubetop.label4 .tubetop.block6 -in .tubetop.block1\\\n"
"  -padx 8m -side left -expand 1 -anchor center\n"
"\n"
"# Block 5\n"
"scale .tubetop.perimeter -from 2 -to [format \"%.0f\" "
"[expr sqrt($N_MAX(4))]]\\\n"
"  -orient horizontal -resolution 0.5 -variable perim(4)\\\n"
"  -command tt_slider_command2 -label Perimeter\n"
"scale .tubetop.shift -from 0 -to [format \"%.0f\" "
"[expr sqrt($N_MAX(4))]]\\\n"
"  -orient horizontal -label Shift\\\n"
"  -resolution 1 -variable shift(4) -command tt_slider_command1\n"
"pack .tubetop.perimeter .tubetop.shift -in .tubetop.block5\\\n"
"  -side top -pady 1m -fill x -expand 1\n"
"\n"
"# Block 6\n"
"scale .tubetop.offset1 -from 2 -to [format \"%.0f\" "
"[expr sqrt($N_MAX(4))]]\\\n"
"  -orient horizontal -label \"Offset 1\"\\\n"
"  -resolution 1 -variable offset1(4) -command tt_offset_2_shift\n"
"scale .tubetop.offset2 -from 0 -to [format \"%.0f\" "
"[expr sqrt($N_MAX(4))]]\\\n"
"  -orient horizontal -label \"Offset 2\"\\\n"
"  -resolution 1 -variable offset2(4) -command tt_offset_2_shift\n"
"pack .tubetop.offset1 .tubetop.offset2 -in .tubetop.block6\\\n"
"  -side top -pady 1m -fill x -expand 1\n"
"\n"
"# Block 3\n"
"button .tubetop.start -text Start -command {set start 1}\n"
"button .tubetop.cancel -text Cancel -command {set cancel 1}\n"
"pack .tubetop.start .tubetop.cancel -side left -in .tubetop.block3 "
"-pady 1m -expand 1\n"
"\n"
"tt_offset_2_shift 0\n"
"fuelle_tube_dateifeld\n";


char Tcl_Skript_Inputfilefenster[] =
"toplevel .filetop\n"
"wm title .filetop \"Input file - options\"\n"
"frame .filetop.block5b         ;# fuer Klick-Fenster\n"
"frame .filetop.block4          ;# fuer Einbettungsparameter\n"
"frame .filetop.block19         ;# fuer Einbettungsparameter\n"
"frame .filetop.block5          ;# fuer Inputfilename\n"
"frame .filetop.block5c         ;# fuer Pipe\n"
"frame .filetop.block6          ;# fuer Einbettungsarten\n"
"frame .filetop.block7 -relief groove -borderwidth 4  ;# Input-Optionen\n"
"frame .filetop.block1          ;# fuer Input-Optionen\n"
"frame .filetop.block3                  ;#  fuer Start und Cancel\n"
"label .filetop.parlabel -text \"Parameters for 3D re-embedding:   \"\n"
"pack .filetop.block5 .filetop.block5b .filetop.block5c .filetop.block6"
" .filetop.parlabel\\\n"
" .filetop.block4 .filetop.block19\\\n"
" -in .filetop.block1 -side top -padx 2m -pady 1m -fill x\n"
"zeichne_outputoptionen\n"
"pack .filetop.block7 .filetop.block8 .filetop.block3 \\\n"
"     -in .filetop -side top -pady 2m -padx 2m -fill x -expand 1\n"
"\n"
"# Block 4\n"
"frame .filetop.block20\n"
"frame .filetop.block21\n"
"frame .filetop.block22\n"
"frame .filetop.block23\n"
"pack .filetop.block20 .filetop.block21 -in .filetop.block4 -side left\\\n"
"  -padx 2m -fill x -expand 1\n"
"pack .filetop.block22 .filetop.block23 -in .filetop.block19 -side left\\\n"
"  -padx 2m -fill x -expand 1\n"
"label .filetop.alabel -text \"a:\"\n"
"label .filetop.blabel -text \"b:\"\n"
"label .filetop.clabel -text \"c:\"\n"
"label .filetop.dlabel -text \"d:\"\n"
"label .filetop.avalue -textvariable emb_a5 -width 4\n"
"label .filetop.bvalue -textvariable emb_b5 -width 4\n"
"label .filetop.cvalue -textvariable emb_c5 -width 4\n"
"label .filetop.dvalue -textvariable emb_d5 -width 4\n"
"scale .filetop.anumber -from 0 -to 100 \\\n"
"      -orient horizontal -resolution 1 -variable emb_a5 \\\n"
"      -command {file_slider_command} -showvalue 0\n"
"scale .filetop.bnumber -from 0 -to 1000 \\\n"
"      -orient horizontal -resolution 1 -variable emb_b5 \\\n"
"      -command {file_slider_command} -showvalue 0\n"
"scale .filetop.cnumber -from 0 -to 100 \\\n"
"      -orient horizontal -resolution 1 -variable emb_c5 \\\n"
"      -command {file_slider_command} -showvalue 0\n"
"# im neuen spring ignoriert d\n" 
"scale .filetop.dnumber -from 0 -to 0 \\\n"
"      -orient horizontal -resolution 1 -variable emb_d5 \\\n"
"      -command {file_slider_command} -showvalue 0\n"
"pack .filetop.alabel .filetop.avalue -side left -in .filetop.block20\n"
"pack .filetop.anumber -side left -in .filetop.block20 -fill x -expand 1\n"
"pack .filetop.blabel .filetop.bvalue -side left -in .filetop.block21\n"
"pack .filetop.bnumber -side left -in .filetop.block21 -fill x -expand 1\n"
"pack .filetop.clabel .filetop.cvalue -side left -in .filetop.block22\n"
"pack .filetop.cnumber -side left -in .filetop.block22 -fill x -expand 1\n"
"pack .filetop.dlabel .filetop.dvalue -side left -in .filetop.block23\n"
"pack .filetop.dnumber -side left -in .filetop.block23 -fill x -expand 1\n"
"\n"
"# Block 5\n"
"radiobutton .filetop.inputfileradio -text \"Inputfile\""
" -variable inputradio -value 1\n"
"label .filetop.inputfilelabel -text \"Input filename:\"\n"
"entry .filetop.inputfilename -width 49 -relief sunken\\\n"
"  -textvariable inputfilename5 -highlightcolor #d9d9d9\n"
"label .filetop.text -text \"Files in directory :\"\n"
"button .filetop.file -text \"Files\" "
"  -command {read_dir;wm deiconify .filetip}\n"
"pack .filetop.inputfileradio .filetop.inputfilelabel "
".filetop.inputfilename -in .filetop.block5\\\n"
"  -side left -padx 1m -pady 1m -fill x -expand 1\n"
"pack .filetop.text .filetop.file -in .filetop.block5b"
"  -side left -padx 1m -pady 1m -fill x -expand 1\n"
"bind .filetop.inputfilename <KeyRelease> {fuelle_file_dateifeld}\n"
"\n"
"# Block 5c\n"
"radiobutton .filetop.inputpiperadio -text \"Pipe\""
" -variable inputradio -value 0\n"
"label .filetop.inputpipelabel -text \"Input pipename:\"\n"
"entry .filetop.inputpipename -width 49 -relief sunken\\\n"
"  -textvariable inputpipename5 -highlightcolor #d9d9d9\n"
"pack .filetop.inputpiperadio .filetop.inputpipelabel "
".filetop.inputpipename"
" -in .filetop.block5c\\\n"
"  -side left -padx 1m -pady 1m -fill x -expand 1\n"
"# Block 6\n"
"checkbutton .filetop.oldembed -variable old_embed5 -text\\\n"
"  \"Use old embedding (if provided)\" -command\\\n"
"  {if {$old_embed5==0} {set new_embed5 1}}\n"
"checkbutton .filetop.newembed -variable new_embed5 -text \"Re-embed\"\\\n"
"  -command {if {$new_embed5==0} {set old_embed5 1}}\n"
"pack .filetop.oldembed .filetop.newembed -side left -in .filetop.block6\\\n"
"  -padx 3m -expand 1 -pady 1m -fill x\n"
"\n"
"# Block 7\n"
"label .filetop.optionslabel -text \"Input\"\n"
"pack .filetop.optionslabel .filetop.block1 -in .filetop.block7 "
"-side top -pady 1m -fill x\n"
"\n"
"# Block 3\n"
"button .filetop.start -text Start -command {wm withdraw .filetip ;"
"   wm withdraw .filemess;\n"
"   wm withdraw .filequest;\n"
"   if {$inputradio == 1} {\n"
"      set start [ex_datei]\n"
"   } else {\n"
"      set start 1\n"
"   }\n"
"}\n"
"button .filetop.cancel -text Cancel -command {wm withdraw .filetip;"
"   wm withdraw .filemess;\n"
"   wm withdraw .filequest;\n"
"set cancel 1}\n"
"pack .filetop.start .filetop.cancel -side left -in .filetop.block3 "
"-pady 1m -expand 1\n"
"\n"
"fuelle_file_dateifeld\n";


char Tcl_Skript_File_Fenster[] =
"set mess_text \"\"\n"
"set command \"\"\n"
"set com_arg \"\"\n"
"proc read_dir {} {\n"
"  global index .filetip.file_dateien\n"
"  for {set j 0} {$j < $index} {incr j} {\n"
"     .filetip.file_dateien delete end \n"
"  }\n"
"  set all_datei [glob -nocomplain *]\n"
"  set index 0\n"
"  foreach i $all_datei {\n"
"     if {[file isfile $i]} {\n"
"     set current_datei [open $i r]\n"
"     set daten_da [gets $current_datei zeile]\n"
"     if {$daten_da>0} {\n"
"        if {[string match \">>writegraph2d planar*<<*\" $zeile]==1} {\n"
"            set datei($index) $i\n"
"            incr index\n"
"        }\n"
"        if {[string match \">>writegraph3d planar*<<*\" $zeile]==1} {\n"
"            set datei($index) $i\n"
"            incr index\n"
"        }\n"
"        if {[string match \">>planar_code*<<*\" $zeile]==1} {\n"
"            set datei($index) $i\n"
"            incr index\n"
"        }\n"
"     }\n"
"     close $current_datei\n"
"     }\n"
"  }\n"
"  for {set j 0} {$j < $index} {incr j} {\n"
"     lappend datei_list $datei($j)\n"
"  }\n"
"  set datei_list [lsort -dictionary $datei_list]\n"
"  for {set j 0} {$j < $index} {incr j} {\n"
"     .filetip.file_dateien insert end  [lindex $datei_list $j]\n"
"  }\n"
"}\n"
"proc check_file {name} {\n"
"  global inputfilename5\n"
"  set back 0\n"
"  if {$name  == \"\"} {\n"
"    set back 1\n"
"  }\n"
"  if {$back==0 && ![file isdirectory [file dirname $name]]} {\n"
"    set back 2\n"
"  }\n"
"  if {$back==0 && [file exists $name]} {\n"
"    set back 3\n"
"  }\n"
"  if {$back==3 && $name==$inputfilename5} {\n"
"    set back 4\n"
"  }\n"
"  return $back\n"
"}\n"
"proc rename_file {name} {\n"
"  global inputfilename5 quest_butt quest_text mess_text command com_arg\n"
"   wm withdraw .filemess\n"
"   wm withdraw .filequest\n"
"  if {[file exists $inputfilename5]==0} {\n"
"    set mess_text \"No file to rename.\"\n"
"    wm deiconify .filemess\n"
"  } else {\n"
"    set res [check_file $name]\n"
"    if {$res == 1} {\n"
"      set mess_text \"No file to rename to.\"\n"
"      wm deiconify .filemess\n"
"    }\n"
"    if {$res == 2} {\n"
"      set mess_text \"Not a directory.\"\n"
"      wm deiconify .filemess\n"
"    }\n"
"    if {$res == 3} {\n"
"      set quest_text \"File '$name' exists. Overwrite ?\"\n"
"      set quest_butt \"Yes, overwrite.\"\n"
"      set command \"mv\"\n"
"      set com_arg \"$inputfilename5 $name\"\n"
"      set quest_butt \"Yes, overwrite.\"\n"
"      wm deiconify .filequest\n"
"    }\n"
"    if {$res == 4} {\n"
"      set mess_text \"Files are identical.\"\n"
"      wm deiconify .filemess\n"
"    }\n"
"    if {$res == 0} {\n"
"      set command \"mv\"\n"
"      set com_arg \"$inputfilename5 $name\"\n"
"      exec_file\n"
"    }\n"
"  }\n"
"}\n"
"proc copy_file {name} {\n"
"  global inputfilename5 quest_butt quest_text mess_text command com_arg\n"
"   wm withdraw .filemess\n"
"   wm withdraw .filequest\n"
"  if {[file exists $inputfilename5]==0} {\n"
"    set mess_text \"No file to copy.\"\n"
"    wm deiconify .filemess\n"
"  } else {\n"
"    set res [check_file $name]\n"
"    if {$res == 1} {\n"
"      set mess_text \"No file to copy to.\"\n"
"      wm deiconify .filemess\n"
"    }\n"
"    if {$res == 2} {\n"
"      set mess_text \"Not a directory.\"\n"
"      wm deiconify .filemess\n"
"    }\n"
"    if {$res == 3} {\n"
"      set quest_text \"File '$name' exists. Overwrite ?\"\n"
"      set quest_butt \"Yes, overwrite.\"\n"
"      set command \"cp\"\n"
"      set com_arg \"$inputfilename5 $name\"\n"
"      set quest_butt \"Yes, overwrite.\"\n"
"      wm deiconify .filequest\n"
"    }\n"
"    if {$res == 4} {\n"
"      set mess_text \"Files are identical.\"\n"
"      wm deiconify .filemess\n"
"    }\n"
"    if {$res == 0} {\n"
"      set command \"cp\"\n"
"      set com_arg \"$inputfilename5 $name\"\n"
"      exec_file\n"
"    }\n"
"  }\n"
"}\n"
"proc delete_file {} {\n"
"  global inputfilename5 quest_butt quest_text mess_text command com_arg\n"
"   wm withdraw .filemess\n"
"   wm withdraw .filequest\n"
"  if {[file exists $inputfilename5] == 0} {\n"
"    set mess_text \"No File\"\n"
"    wm deiconify .filemess\n"
"  } else {\n"
"    set command \"rm\"\n"
"    set com_arg $inputfilename5\n"
"    set quest_text \"Delete : $inputfilename5\"\n"
"    set quest_butt \"Yes, delete.\"\n"
"    wm deiconify .filequest\n"
"  }\n"
"}\n"
"proc exec_file {} {\n"
"  global inputfilename5 rename_file copy_file command com_arg\n"
"  eval exec $command $com_arg\n"
"  read_dir\n"
"  if {$command == \"rm\"} {\n"
"    set inputfilename5 \"unnamed\"\n"
"    set copy_file \"\"\n"
"    set rename_file \"\"\n"
"  }\n"
"  if {$command == \"mv\"} {\n"
"    set inputfilename5 $rename_file\n"
"    set rename_file \"\"\n"
"    set copy_file \"\"\n"
"  }\n"
"  if {$command == \"cp\"} {\n"
"    set inputfilename5 $copy_file\n"
"    set rename_file \"\"\n"
"    set copy_file \"\"\n"
"  }\n"
"}\n"
"proc clear_com {} {\n"
"  global command com_arg\n"
"  set command \"\"\n"
"  set com_arg \"\"\n"
"}\n"
"toplevel .filemess\n"
"wm title .filemess \"Message\"\n"
"wm geometry .filemess 200x100\n"
"wm withdraw .filemess\n"
"frame .filemess.mess -relief groove -borderwidth 4\n"
"frame .filemess.cancel\n"
"label .filemess.mess.text -textvariable mess_text\n"
"pack .filemess.mess.text -in .filemess.mess\n"
"pack .filemess.mess -padx 3m -pady 3m -fill x\n"
"button .filemess.cancel.cancel -text Cancel -command {wm withdraw .filemess}\n"
"pack .filemess.cancel.cancel .filemess.cancel\n"
"pack .filemess.cancel\n"
"toplevel .filequest\n"
"wm title .filequest \"Question\"\n"
"wm geometry .filequest 250x100\n"
"wm withdraw .filequest\n"
"frame .filequest.quest -relief groove -borderwidth 4\n"
"frame .filequest.cancel\n"
"label .filequest.quest.text -textvariable quest_text\n"
"button .filequest.cancel.cancel -text Cancel -command {wm withdraw .filequest;clear_com}\n"
"button .filequest.cancel.ok -textvariable quest_butt -command {wm withdraw .filequest;exec_file}\n"
"pack .filequest.quest.text -in .filequest.quest\n"
"pack .filequest.quest  -padx 3m -pady 3m -fill x\n"
"pack  .filequest.cancel.ok .filequest.cancel.cancel -in .filequest.cancel -side left\n"
"pack  .filequest.cancel\n"
"set index 0\n"
"toplevel .filetip\n"
"wm title .filetip \"Files\"\n"
"wm geometry .filetip 680x500\n"
"wm withdraw .filetip\n"
"frame .filetip.text\n"
"frame .filetip.file\n"
"frame .filetip.select\n"
"frame .filetip.delete\n"
"frame .filetip.rename\n"
"frame .filetip.copy\n"
"frame .filetip.cancel\n"
"frame .filetip.left -relief sunken -borderwidth 4\n"
"frame .filetip.right -relief groove -borderwidth 4\n"
"pack .filetip.left -in .filetip -padx 2m -pady 1m \\\n"
"     -fill x -fill y -side left\n"
"pack .filetip.right -in .filetip -padx 2m -pady 1m \\\n"
"     -fill x -fill y -expand 1 -side right\n"
"#Block right\n"
"label .filetip.text_text -text \"Choose by Double-Click\"\n"
"pack .filetip.text_text -in .filetip.text\n"
"pack .filetip.text -fill x -in .filetip.right\n"	/*right */
"scrollbar .filetip.file_yscroll -command \".filetip.file_dateien yview\"\n"
"listbox .filetip.file_dateien -width 49 -yscrollcommand {.filetip.file_yscroll set}\n"
"pack .filetip.file_dateien -side left -expand true -fill both "
"-in .filetip.file\n"
"pack .filetip.file_yscroll -side right -fill y -in .filetip.file\n"
"pack .filetip.file -expand true -fill both -in .filetip.right\n"	/*right */
"bind .filetip.file_dateien <Double-1> {\n"
"     set inputfilename5 [selection get]\n"
"     .filetop.start configure -text \"Start\"\n"
"}\n"
"#Block left\n"
"entry .filetip.select_inputfilename -width 30 -relief sunken\\\n"
"  -textvariable inputfilename5 -highlightcolor #d9d9d9\n"
"label .filetip.select_text -text \"Selected File :\"\n"
"pack .filetip.select_text .filetip.select_inputfilename -in .filetip.select\n"
"pack .filetip.select -padx 3m -pady 3m -expand true -in .filetip.left\n"  /**/
  "label .filetip.delete_text -text \"Delete selected file !\"\n"
  "button .filetip.delete_delete -text \"Delete\" -command delete_file\n"
  "pack .filetip.delete_text -in .filetip.delete -side left -fill x\n"
  "pack .filetip.delete_delete -in .filetip.delete -side right -fill x\n"
  "pack .filetip.delete -padx 3m -pady 3m -fill x -expand true -in .filetip.left\n"/**/
"label .filetip.rename_text -text \"Rename selected file to :\"\n"
"entry .filetip.rename_file -width 30 -relief sunken\\\n"
"  -textvariable rename_file -highlightcolor #d9d9d9\n"
"bind .filetip.rename_file <Return> {rename_file $rename_file}\n"
"button .filetip.rename_rename -text \"Rename\" -command {rename_file $rename_file}\n"
"pack .filetip.rename_text .filetip.rename_file .filetip.rename_rename -in .filetip.rename\n"
"pack .filetip.rename -padx 3m -pady 3m -expand true -in .filetip.left\n"  /**/
  "label .filetip.copy_text -text \"Copy selected file to :\"\n"
  "entry .filetip.copy_file -width 30 -relief sunken\\\n"
  "  -textvariable copy_file -highlightcolor #d9d9d9\n"
  "bind .filetip.copy_file <Return> {copy_file $copy_file}\n"
  "button .filetip.copy_copy -text \"Copy\" -command {copy_file $copy_file}\n"
  "pack .filetip.copy_text .filetip.copy_file .filetip.copy_copy -in .filetip.copy\n"
  "pack .filetip.copy -padx 3m -pady 3m -in .filetip.left\n"/**/
"button .filetip.cancel_cancel -text \"Cancel\" -command "
"{wm withdraw .filetip}\n"
"pack .filetip.cancel_cancel -in .filetip.cancel\n"
"pack .filetip.cancel -expand true -in .filetip.left\n";

char Tcl_Skript_Fullgen_Programmaufruf[] =	/* "fullgen"-Aufruf vorbereiten */
"# Programm \"rasmol\" oder \"geomview\" starten:\n"
"if {$torasmol(1)==1 && $r_or_g(1)==0} {set rasmol_id [exec rasmol &]}\\\n"
"else {set rasmol_id 0}\n"
"\n"
"# Warten:\n"
".hauptframe.statusframe.bitmap configure -bitmap hourglass\n"
".hauptframe.statusframe.msg configure -text "
"\"Status: Preparing generation (may take some minutes for big vertex"
" numbers)\"\n"
"update idletasks\n"
"\n"
"# Kommandozeile fuer fullgen-Aufruf erzeugen (in \"fullcall\"):\n"
"set endstring \" $n_end(1)\"\n"
"if {$n_anf(1) != $n_end(1)} {set startstring \" start $n_anf(1)\"} else "
"{set startstring \"\"}\n"
"if {$ipr(1)} {set iprstring \" ipr\"} else {set iprstring \"\"}\n"
"if {$cases(1)>0} {set casestring \" case $cases(1)\"} else "
"{set casestring \"\"}\n"
"if {$symstat(1)} {set symstatstring \" symstat\"} else "
"{set symstatstring \"\"}\n"
"if {$spistat(1)} {set spistatstring \" spistat\"} else "
"{set spistatstring \"\"}\n"
"if {$dual(1)} {set dualstring \" code 7\"} else "
"{set dualstring \" code 1\"}\n"
"set symmstring \"\"\n"
"for {set i 1} {$i<=28} {incr i} {\n"
"if {$symm(1,$i)==0} {set symmstring \"Ja\"}\n"
"}\n"
"# Falls nicht alle Symmetrien gesetzt => Symmetrien in Aufruf\n"
"if {$symmstring == \"Ja\"} {\n"
"  set symmstring \"\"\n"
"  for {set i 1} {$i<=28} {incr i} {\n"
"    if {$symm(1,$i)} "
"{set symmstring \"${symmstring} symm $symmname(1,$i)\"}\n"
"  }\n"
"}\n"
"\n"
"set fullcall "
"fullgen$endstring$startstring$iprstring$casestring$spistatstring\n"
"set fullcall \"$fullcall$symstatstring$symmstring$dualstring stdout pid\"\n"
"\n"
"# Namen des rasmol-Prozesses erfragen:\n"
"if {$torasmol(1)==1 && $r_or_g(1)==0} {\n"
"  while {[lsearch -glob [winfo interps] rasmol*]==-1} {after 500}\n"
"  set procstring [winfo interps]\n"
"  set rasmol_name "
"[lindex $procstring [lsearch -glob $procstring rasmol*]]\n"
"  puts $rasmol_name\n"
"\n"
"  # rasmol_name enthaelt den Namen, unter dem der Prozess laeuft\n"
"}\n"
"\n"
"# Namen der save-Dateien festlegen:\n"
"set dateiname [bilde_full_dateiname]\n"
"set dateiname_s2d(1) \"${dateiname}.s2d\"\n"
"set dateiname_s3d(1) \"${dateiname}.s3d\"\n"
"set dateiname_spdb(1) \"${dateiname}.spdb\"\n";


char Tcl_Skript_HCgen_Programmaufruf[] =
"# Programm \"rasmol\" oder \"geomview\" starten:\n"
"if {$torasmol(3)==1 && $r_or_g(3)==0} {set rasmol_id [exec rasmol &]}\\\n"
"else {set rasmol_id 0}\n"
"\n"
"# Warten:\n"
".hauptframe.statusframe.bitmap configure -bitmap hourglass\n"
".hauptframe.statusframe.msg configure -text "
"\"Status: Preparing generation (may take some minutes for big atom"
" numbers)\"\n"
"update idletasks\n"
"\n"
"# Kommandozeile fuer HCgen-Aufruf erzeugen (in \"hccall\"):\n"
"if {$ipr(3)} {set iprstring \" ipr\"} else {set iprstring \"\"}\n"
"if {$woh3==0} {set wohstring \" without_H\"} else {set wohstring \"\"}\n"
"if {$peric(3)} {set pcstring \" peri_condensed\"} else {set pcstring \"\"}\n"
"set hccall \"HCgen $c(3) $h(3) $p3 gap $gap(3) $iprstring$pcstring"
"$wohstring stdout pid\"\n"
"\n"
"# Namen des rasmol-Prozesses erfragen:\n"
"if {$torasmol(3)==1 && $r_or_g(3)==0} {\n"
"  while {[lsearch -glob [winfo interps] rasmol*]==-1} {after 500}\n"
"  set procstring [winfo interps]\n"
"  set rasmol_name "
"[lindex $procstring [lsearch -glob $procstring rasmol*]]\n"
"  puts $rasmol_name\n"
"  # rasmol_name enthaelt den Namen, unter dem der Prozess laeuft\n"
"}\n"
"# Namen der save-Dateien festlegen:\n"
"set dateiname [bilde_HCgen_dateiname]\n"
"set dateiname_s2d(3) \"${dateiname}.s2d\"\n"
"set dateiname_s3d(3) \"${dateiname}.s3d\"\n"
"set dateiname_spdb(3) \"${dateiname}.spdb\"\n";

char Tcl_Skript_3reggen_Programmaufruf[] =
"# Programm \"rasmol\" oder \"geomview\" starten:\n"
"if {$torasmol(2)==1 && $r_or_g(2)==0} {set rasmol_id [exec rasmol &]}\\\n"
"else {set rasmol_id 0}\n"
"\n"
"# Warten:\n"
".hauptframe.statusframe.bitmap configure -bitmap hourglass\n"
".hauptframe.statusframe.msg configure -text "
"\"Status: Preparing generation (may take some minutes for big vertex"
" numbers)\"\n"
"update idletasks\n"
"\n"
/*vs */
"set limtest 1\n"
"foreach f [array names face2] {\n"
"  if {$lim(2,$f)==1} {set limtest 0 ; break}\n"
"}\n"
"set r3call \"CPF\"\n"
"if {$con3(2) && !$con2(2) && !$con1(2) && $n_anf(2)==$n_end(2)} {\n"
"  set face_list \"\"\n"
"  foreach f [array names face2] {\n"
"    lappend face_list $f\n"
"  }\n"
"  set face_list [lsort -integer $face_list]\n"
"  set face_llength [llength $face_list]\n"
"  if {$face_llength==4 && [lindex $face_list 0]==3"
" && [lindex $face_list 1]==4 && [lindex $face_list 2]==5"
" && [lindex $face_list 3] == 6 && $limtest} {\n"
"      set r3call \"plantri_md6 [expr $n_end(2)/2+2] -d\"\n"
"  } else {\n"
"    set i 4\n"
"    foreach f $face_list {\n"
"      if {$f == $i} {\n"
"        incr i 1\n"
"      } else {\n"
"        set i 0\n"
"        break \n"
"      }\n"
"    }\n"
"    if {$i && $face_llength>=4 && $limtest} {\n"
"       set r3call \"plantri -m4 [expr $n_end(2)/2+2] -d\"\n"
"    } else {\n"
"      if {[lindex $face_list [expr $face_llength-1]] <=6} {\n"
"         set r3call \"CPF\"\n"
"      } else {\n"
"        if {$face_llength>=4 && [lindex $face_list 1]==5"
" && [lindex $face_list 2]==6 && $limtest} {\n"
"         set r3call \"CPF\"\n"
"        } else {\n"
"          if {$face_llength==2 && [lindex $face_list 0]==5"
" && [lindex $face_list 1]==9 && $limtest} {\n"
"            set r3call \"CPF\"\n"
"          } else {\n"
"            if {$face_llength==3 && [lindex $face_list 0]==4"
" && [lindex $face_list 1]==5 && [lindex $face_list 2]<=9 && $limtest} {\n"
"              set r3call \"CPF\"\n"
"            } else {\n"
"              set face_call \"\"\n"
"                set t F\n"	/*dumm aber wahr */
"                set tt _\n"
"                set TT ^\n"
"                foreach f [array names face2] {\n"
"                  set face_call \"$face_call$t$f\"\n"
"                  if {$lim(2,$f)==1} {\n"
"           set face_call \"$face_call$tt$lowlim(2,$f)$TT$uplim(2,$f)\"}\n"
"                }\n"
"#              foreach f $face_list {\n"
"#                set face_call \"$face_call$t$f\"\n"
"#              }\n"
"               set r3call \"plantri_ad [expr $n_end(2)/2+2] -$face_call -d\"\n"
"            }\n"
"          }\n"
"        }\n"
"      }\n"
"    }\n"
"  }\n"
"}\n"				/*if {$con3(2) && !$con2(2) && !$con1(2)} */
"if {$r3call == \"CPF\"} {\n"
/* vs */
"# Kommandozeile fuer fullgen-Aufruf erzeugen (in \"r3call\"):\n"
"set r3call \"$r3call stdout pid n $n_end(2)\"\n"
"if {$n_anf(2) != $n_end(2)} {set r3call \"$r3call s $n_anf(2)\"}\n"
"foreach f [array names face2] {\n"
"  set r3call \"$r3call f $f\"\n"
"  if {$lim(2,$f)==1} {set r3call \"$r3call -$uplim(2,$f) "
"+$lowlim(2,$f)\"}\n"
"}\n"
"if {$case1(2)==0} {set r3call \"$r3call no_1\"}\n"
"if {$case2(2)==0} {set r3call \"$r3call no_2\"}\n"
"if {$case3(2)==0} {set r3call \"$r3call no_3\"}\n"
"if {$facestat(2)} {set r3call \"$r3call facestat\"}\n"
"if {$patchstat(2)} {set r3call \"$r3call patchstat\"}\n"
"if {$pathface_max(2)} {set r3call \"$r3call pathface_max\"}\n"
"if {$priority(2)!=123} {set r3call \"$r3call priority ${priority(2)}\"}\n"
"if {$alt(2)} {set r3call \"$r3call alt\"}\n"
"if {$dual(2)} {set r3call \"$r3call dual\"}\n"
"if {$con1(2)} {set r3call \"$r3call con 1\"}\n"
"if {$con2(2)} {set r3call \"$r3call con 2\"}\n"
"if {$con3(2)} {set r3call \"$r3call con 3\"}\n"
"}\n"
/*vs qqqqqqqqqqqqq CPF aufruf */
"\n"
"# Namen des rasmol-Prozesses erfragen:\n"
"if {$torasmol(2)==1 && $r_or_g(2)==0} {\n"
"  while {[lsearch -glob [winfo interps] rasmol*]==-1} {after 500}\n"
"  set procstring [winfo interps]\n"
"  set rasmol_name "
"[lindex $procstring [lsearch -glob $procstring rasmol*]]\n"
"  puts $rasmol_name\n"
"  # rasmol_name enthaelt den Namen, unter dem der Prozess laeuft\n"
"}\n"
"# Namen der save-Dateien festlegen:\n"
"set dateiname [bilde_3reggen_dateiname 1]\n"
"set dateiname_s2d(2) \"${dateiname}.s2d\"\n"
"set dateiname_s3d(2) \"${dateiname}.s3d\"\n"
"set dateiname_spdb(2) \"${dateiname}.spdb\"\n"
"puts $r3call\n";


char Tcl_Skript_Tubetype_Programmaufruf[] =
"# Programm \"rasmol\" oder \"geomview\" starten:\n"
"if {$torasmol(4)==1 && $r_or_g(4)==0} {set rasmol_id [exec rasmol &]}\\\n"
"else {set rasmol_id 0}\n"
"\n"
"# Warten:\n"
".hauptframe.statusframe.bitmap configure -bitmap hourglass\n"
".hauptframe.statusframe.msg configure -text "
"\"Status: Preparing generation (may take some minutes for big vertex"
" numbers)\"\n"
"update idletasks\n"
"\n"
"# Kommandozeile fuer tubetype-Aufruf erzeugen (in ttcall):\n"
"if {$ipr(4)} {set iprstring \" ipr\"} else {set iprstring \"\"}\n"
"if {$tubes(4)} {set tubestring \" dt\"}"
" else {set tubestring \" fullerenes\"}\n"
"set ttcall \"tubetype $offset1(4) $offset2(4) tube $tubelen(4) "
"$tubestring$iprstring pid\"\n"
"\n"
"# Namen des rasmol-Prozesses erfragen:\n"
"if {$torasmol(4)==1 && $r_or_g(4)==0} {\n"
"  while {[lsearch -glob [winfo interps] rasmol*]==-1} {after 500}\n"
"  set procstring [winfo interps]\n"
"  set rasmol_name "
"[lindex $procstring [lsearch -glob $procstring rasmol*]]\n"
"  puts $rasmol_name\n"
"  # rasmol_name enthaelt den Namen, unter dem der Prozess laeuft\n"
"}\n"
"# Namen der save-Dateien festlegen:\n"
"set dateiname [bilde_tubetype_dateiname 1]\n"
"set dateiname_s2d(4) \"${dateiname}.s2d\"\n"
"set dateiname_s3d(4) \"${dateiname}.s3d\"\n"
"set dateiname_spdb(4) \"${dateiname}.spdb\"\n";


char Tcl_Skript_File_Programmaufruf[] =
"# Programm \"rasmol\" oder \"geomview\" starten:\n"
"if {$torasmol(5)==1 && $r_or_g(5)==0} {set rasmol_id [exec rasmol &]}\\\n"
"else {set rasmol_id 0}\n"
"\n"
"# Namen des rasmol-Prozesses erfragen:\n"
"if {$torasmol(5)==1 && $r_or_g(5)==0} {\n"
"  while {[lsearch -glob [winfo interps] rasmol*]==-1} {after 500}\n"
"  set procstring [winfo interps]\n"
"  set rasmol_name "
"[lindex $procstring [lsearch -glob $procstring rasmol*]]\n"
"  puts $rasmol_name\n"
"  # rasmol_name enthaelt den Namen, unter dem der Prozess laeuft\n"
"}\n"
"\n"
"# Namen der save-Dateien festlegen:\n"
"set dateiname [bilde_file_dateiname]\n"
"set dateiname_s2d(5) \"${dateiname}.s2d\"\n"
"set dateiname_s3d(5) \"${dateiname}.s3d\"\n"
"set dateiname_spdb(5) \"${dateiname}.spdb\"\n";

/*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
char Tcl_Skript_Outputbedienfenster[] =		/* Output panel */
"toplevel .aufruf\n"
"wm withdraw .aufruf\n"
"wm title .aufruf \"Output panel\"\n"
"button .aufruf.cancel -text Cancel "
"-command {set outcancel 1;.aufruf.cancel configure -command {};"
"if {$programm == 5 && $inputradio == 0} {kill_child $inputpipename5}}\n"
"frame .aufruf.topframe\n"
"pack .aufruf.topframe -in .aufruf -pady 2m\n"
"frame .aufruf.current_frame -relief sunken -borderwidth 2        ;#vs\n"
"frame .aufruf.nrframe3 -relief sunken -borderwidth 2\n"
"frame .aufruf.nrframe2 -relief sunken -borderwidth 2\n"
"frame .aufruf.nrframe -relief sunken -borderwidth 2\n"
"frame .aufruf.fwdframe -width 50m -height 6m\n"
"frame .aufruf.reviewframe -borderwidth 2 -relief sunken\n"
"frame .aufruf.saveframe\n"
"\n"
"label .aufruf.save -text \"Save:\"\\\n"
"  -font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"set save2d 0;   set save3d 0 ;set savepdb 0\n"
"checkbutton .aufruf.save2d -anchor w -text 2d -variable save2d\\\n"
"  -font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"checkbutton .aufruf.save3d -anchor w -text 3d -variable save3d\\\n"
"  -font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"checkbutton .aufruf.savepdb -anchor w -text PDB -variable savepdb\\\n"
"  -font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"pack .aufruf.save -in .aufruf.saveframe -expand 1 -side left\n"
"if {$totkview($programm)}\\\n"
"   {pack .aufruf.save2d -in .aufruf.saveframe -side left -expand 1}\n"
"if {$torasmol($programm)}\\\n"
"   {pack .aufruf.save3d .aufruf.savepdb -in .aufruf.saveframe"
" -side left -expand 1}\n"
"\n"
";#bvs\n"
"label .aufruf.current -text \"Number of atoms :\"\n"
"label .aufruf.current_vertex_number -width 6 -relief flat -justify right -anchor e "
"-textvariable current_vertex_number -foreground red\n"
"pack .aufruf.current .aufruf.current_vertex_number -in .aufruf.current_frame "
"-padx 2m -pady 1m -expand 1 -fill x -side left\n"
"pack .aufruf.current_frame -in .aufruf.topframe\\\n"
"-side top -expand 1 -fill x -padx 2m -pady 1m \n"
"                                                                 ;#evs\n"
"checkbutton .aufruf.fffwd -text \"skip\" -variable skip "
"-font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"if {$torasmol($programm)} {pack .aufruf.nrframe3 -in .aufruf.topframe\\\n"
"   -side top -expand 1 -fill x -padx 2m -pady 2m}\n"
"if {$totkview($programm)} {pack .aufruf.nrframe2 -in .aufruf.topframe\\\n"
"   -side top -expand 1 -fill x -padx 2m -pady 2m}\n"
"label .aufruf.graph2 -text \"2D display: graph no.\"\n"
"label .aufruf.graphnr2 -width 6 -relief flat -justify right -anchor e "
"-textvariable graphnr2 -foreground red\n"
"label .aufruf.graph3 -text \"3D display: graph no.\"\n"
"label .aufruf.graphnr3 -width 6 -relief flat -justify right -anchor e "
"-textvariable graphnr3 -foreground red\n"
"label .aufruf.graph -text \"Show graph no.\"\n"
"entry .aufruf.graphnr -width 6 -relief flat -justify right "
"-textvariable graphnr -highlightcolor #d9d9d9\n"
"bind .aufruf.graphnr <Return> {set outnr $graphnr;}\n"
"pack .aufruf.graph3 .aufruf.graphnr3 -in .aufruf.nrframe3 "
"-padx 2m -pady 1m -expand 1 -fill x -side left\n"
"pack .aufruf.graph2 .aufruf.graphnr2 -in .aufruf.nrframe2 "
"-padx 2m -pady 1m -expand 1 -fill x -side left\n"
"pack .aufruf.graph .aufruf.graphnr -in .aufruf.nrframe "
"-padx 2m -expand 1 -fill x -side left\n"
"\n"
"button .aufruf.fwd -text \"+1\" -command {incr outnr 1} "
"-font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"button .aufruf.ffwd -text \"+10\" -command {incr outnr 10} "
"-font *-times-bold-r-normal--*-140-*-*-*-*-*-*\n"
"pack .aufruf.fwd .aufruf.ffwd .aufruf.fffwd -in .aufruf.fwdframe "
"-side left -fill x -expand 1 -padx 2m\n"
"\n"
"label .aufruf.review -text \"Review:  \"\n"
"pack .aufruf.review -in .aufruf.reviewframe -side left -pady 1m -padx 1m\n"
"button .aufruf.reviewbwd -text \"<\" -command {incr reviewstep -1}\n"
"button .aufruf.reviewfwd -text \">\" -command {incr reviewstep 1}\n"
"pack .aufruf.reviewfwd .aufruf.reviewbwd -in "
".aufruf.reviewframe -pady 1m -padx 1m -ipadx 3m -side right\n"
"pack .aufruf.cancel -in .aufruf -side top -padx 2m -pady 2m\\\n"
"  -expand 1\n"
"pack .aufruf.nrframe .aufruf.fwdframe .aufruf.saveframe\\\n"
"  .aufruf.reviewframe -in .aufruf.topframe -pady 2m\\\n"
"  -padx 2m -side top -expand 1 -fill x\n"
"button .aufruf.rasmoldefaults -text \"Rasmol defaults\" "
"  -command {wm deiconify .rasmoldefaults}\n"
"pack .aufruf.rasmoldefaults\n"
"\n";

/* Die eigentliche Init. findet in : Tcl_Script_load_rasmol_rc[] statt */

char Tcl_Skript_Rasmol_defaults_init[] =
"rasmol_init\n"
"write_win\n"
"\n"
"proc read_win {} {\n"
"   global red green blue\n"
"   global wframe ambient sfill dots\n"
"   global axis box slab rlabel\n"
"   global programm\n"
"   global trans_red trans_green trans_blue\n"
"   global trans_wframe trans_ambient trans_sfill trans_dots\n"
"   global trans_axis trans_box trans_slab trans_label\n"
"   set red($programm) $trans_red\n"
"   set green($programm) $trans_green\n"
"   set blue($programm) $trans_blue\n"
"   set ambient($programm) $trans_ambient\n"
"   set wframe($programm) $trans_wframe\n"
"   set sfill($programm) $trans_sfill\n"
"   set dots($programm) $trans_dots\n"
"   set axis($programm) $trans_axis\n"
"   set box($programm) $trans_box\n"
"   set slab($programm) $trans_slab\n"
"   set rlabel($programm) $trans_label\n"
"}\n"
"proc show {} {\n"
"  global red blue green\n"
"  global wframe ambient sfill dots\n"
"  global axis box slab rlabel\n"
"  global programm\n"
"  global rasmol_name\n"
"  set color_nachricht \"set background \\[$red($programm),$green($programm),$blue($programm)\\]\"\n"
"  set ambient_nachricht \"set ambient $ambient($programm)\"\n"
"  set wframe_nachricht \"wireframe $wframe($programm)\"\n"
"  set sfill_nachricht \"spacefill $sfill($programm)\"\n"
"  if {$dots($programm) == 0} {\n"
"     set dots_nachricht \"dots false\"\n"
"  } else {\n"
"     set dots_nachricht \"dots $dots($programm)\"\n"
"  }\n"
"  if {$axis($programm) == 1} {\n"
"     set axis_nachricht \"set axis true\"\n"
"  } else {\n"
"     set axis_nachricht \"set axis false\"\n"
"  }\n"
"  if {$box($programm) == 1} {\n"
"     set box_nachricht \"set boundbox true\"\n"
"  } else {\n"
"     set box_nachricht \"set boundbox false\"\n"
"  }\n"
"  if {$slab($programm) == 100} {\n"
"     set slab_nachricht \"slab false\"\n"
"  } else {\n"
"     set slab_nachricht \"slab $slab($programm)\"\n"
"  }\n"
"  if {$rlabel($programm) == 1} {\n"
"     set label_nachricht \"label true\"\n"
"  } else {\n"
"     set label_nachricht \"label false\"\n"
"  }\n"
"  send $rasmol_name $ambient_nachricht\n"
"  send $rasmol_name $wframe_nachricht\n"
"  send $rasmol_name $color_nachricht\n"
"  send $rasmol_name $sfill_nachricht\n"
"  send $rasmol_name $dots_nachricht\n"
"  send $rasmol_name $axis_nachricht\n"
"  send $rasmol_name $box_nachricht\n"
"  send $rasmol_name $slab_nachricht\n"
"  send $rasmol_name $label_nachricht\n"
"}\n"
"\n";

char Tcl_Script_mv_rasmolrc[] =
"if {[file exists .rasmolrc]} {\n"
"  if {[file exists .rasmolrc_tmp]} {\n"
"     exec rm .rasmolrc_tmp\n"
"  }\n"
"  exec mv .rasmolrc .rasmolrc_tmp\n"
"}\n";

char Tcl_Script_mv_rasmolrc_tmp[] =
"if {[file exists .rasmolrc_tmp]} {\n"
"  if {[file exists .rasmolrc]} {\n"
"     exec rm .rasmolrc\n"
"  }\n"
"  exec mv .rasmolrc_tmp .rasmolrc\n"
"}\n";

char Tcl_Script_load_rasmol_rc[] =
"proc create_rasmol_rc {} {\n"
"   global red green blue\n"
"   global wframe ambient sfill dots\n"
"   global axis box slab rlabel\n"
"   set read_rc [open .rasmol_rc w]\n"
"   for {set i 1} {$i < 6} {incr i} {\n"
"      set red($i) 20\n"
"      set blue($i) 140\n"
"      set green($i) 45\n"
"      set ambient($i) 0\n"
"      set wframe($i) 10\n"
"      set sfill($i) 50\n"
"      set dots($i) 0\n"
"      set slab($i) 100\n"
"      set axis($i) 0\n"
"      set box($i) 0\n"
"      set rlabel($i) 0\n"
"      puts $read_rc \"20 140 45 0 10 50 0 100 0 0 0\"\n"
"   }\n"
"   close $read_rc\n"
"}\n"
"proc rasmol_init {} {\n"
"   global red green blue\n"
"   global wframe ambient sfill dots\n"
"   global axis box slab rlabel\n"
"   set ok 1\n"
"   set count 1\n"
"   if {[file exists .rasmol_rc]} {\n"
"     set read_rc [open .rasmol_rc r]\n"
"     while {[gets $read_rc zeile($count)] >=0} {\n"
"       incr count\n"
"     }\n"
"     close $read_rc\n"
"   } else {\n"
"     set ok 0\n"
"   }\n"
"   if {$count == 6} {\n"
"      for {set i 1} {$i < 6} {incr i} {\n"
"        scan $zeile($i) \"%d %d %d %d %d %d %d %d %d %d %d\"  red($i) green($i)"
" blue($i) ambient($i) wframe($i) sfill($i) dots($i) slab($i) axis($i) box($i) rlabel($i)\n"
"     }\n"
"   } else {\n"
"     set ok 0\n"
"   }\n"
"   if {$ok == 0} {\n"
"     create_rasmol_rc\n"
"   }\n"
"}\n"
"\n"
"proc write_win {} {\n"
"   global red green blue\n"
"   global wframe ambient sfill dots\n"
"   global axis box slab rlabel\n"
"   global programm\n"
"   global trans_red trans_green trans_blue\n"
"   global trans_wframe trans_ambient trans_sfill trans_dots\n"
"   global trans_axis trans_box trans_slab trans_label\n"
"   set trans_red $red($programm)\n"
"   set trans_green $green($programm)\n"
"   set trans_blue $blue($programm)\n"
"   set trans_ambient $ambient($programm)\n"
"   set trans_wframe $wframe($programm)\n"
"   set trans_sfill $sfill($programm)\n"
"   set trans_dots $dots($programm)\n"
"   set trans_axis $axis($programm)\n"
"   set trans_box $box($programm)\n"
"   set trans_slab $slab($programm)\n"
"   set trans_label $rlabel($programm)\n"
"}\n"
"proc read_rasmol_rc {} {\n"
"   global red green blue\n"
"   global wframe ambient sfill dots\n"
"   global axis box slab rlabel\n"
"   global program\n"
"   set count 1\n"
"   if {[file exists .rasmol_rc]} {\n"
"     set read_rc [open .rasmol_rc r]\n"
"     while {[gets $read_rc zeile($count)] >=0} {\n"
"       incr count\n"
"     }\n"
"   }\n"
"   if {$count == 6} {\n"
"     for {set i 1} {$i < 6} {incr i} {\n"
"       scan $zeile($i) \"%d %d %d %d %d %d %d %d %d %d %d\"  red($i) green($i)"
" blue($i) ambient($i) wframe($i) sfill($i) dots($i) slab($i) axis($i) box($i) rlabel($i)\n"
"     }\n"
"   } else {\n"
"     create_rasmol_rc\n"
"   }\n"
"   write_win\n"
"}\n"
"\n"
"proc save_rasmol_defaults {} {\n"
"  global red blue green\n"
"  global wframe ambient sfill dots\n"
"  global axis box slab rlabel\n"
"  global programm\n"
"  set file_zap [open .rasmolrc w]\n"
"  set file_rc [open .rasmol_rc w]\n"
"  set color_nachricht \"set background \\[$red($programm),$green($programm),$blue($programm)\\]\"\n"
"  set ambient_nachricht \"set ambient $ambient($programm)\"\n"
"  set wframe_nachricht \"wireframe $wframe($programm)\"\n"
"  set sfill_nachricht \"spacefill $sfill($programm)\"\n"
"  if {$dots($programm) == 0} {\n"
"     set dots_nachricht \"dots false\"\n"
"  } else {\n"
"     set dots_nachricht \"dots $dots($programm)\"\n"
"  }\n"
"  if {$axis($programm) == 1} {\n"
"     set axis_nachricht \"set axis true\"\n"
"  } else {\n"
"     set axis_nachricht \"set axis false\"\n"
"  }\n"
"  if {$box($programm) == 1} {\n"
"     set box_nachricht \"set boundbox true\"\n"
"  } else {\n"
"     set box_nachricht \"set boundbox false\"\n"
"  }\n"
"  if {$slab($programm) == 100} {\n"
"     set slab_nachricht \"slab false\"\n"
"  } else {\n"
"     set slab_nachricht \"slab $slab($programm)\"\n"
"  }\n"
"  if {$rlabel($programm) == 1} {\n"
"     set label_nachricht \"label true\"\n"
"  } else {\n"
"     set label_nachricht \"label false\"\n"
"  }\n"
"  puts $file_zap $color_nachricht\n"
"  puts $file_zap $ambient_nachricht\n"
"  puts $file_zap $wframe_nachricht\n"
"  puts $file_zap $sfill_nachricht\n"
"  puts $file_zap $dots_nachricht\n"
"  puts $file_zap $axis_nachricht\n"
"  puts $file_zap $box_nachricht\n"
"  puts $file_zap $slab_nachricht\n"
"  puts $file_zap $label_nachricht\n"
"  close $file_zap\n"
"  for {set i 1} {$i < 6} {incr i} {\n"
"     puts $file_rc \"$red($i) $green($i) $blue($i) $ambient($i)"
" $wframe($i) $sfill($i) $dots($i) $slab($i) $axis($i) $box($i) $rlabel($i)\"\n"
"  }\n"
"  close $file_rc\n"
"}\n"
"\n"
"read_rasmol_rc\n"
"save_rasmol_defaults\n";

char Tcl_Skript_Rasmol_defaults[] =
"toplevel .rasmoldefaults\n"
"wm withdraw .rasmoldefaults\n"
"wm title .rasmoldefaults \"RasMol Defaults\"\n"
"frame .rasmoldefaults.color\n"
"frame .rasmoldefaults.com\n"
"frame .rasmoldefaults.ambient\n"
"frame .rasmoldefaults.wframe\n"
"frame .rasmoldefaults.sfill\n"
"frame .rasmoldefaults.dots\n"
"frame .rasmoldefaults.lines\n"
"frame .rasmoldefaults.slab\n"
"scale .rasmoldefaults.color.red -label \"Red\" -from 0 -to 255"
" -length 10c -orient horizontal -variable trans_red\n"
"scale .rasmoldefaults.color.green -label \"Green\" -from 0 -to 255"
" -length 10c -orient horizontal -variable trans_green\n"
"scale .rasmoldefaults.color.blue -label \"Blue\" -from 0 -to 255"
" -length 10c -orient horizontal -variable trans_blue\n"
"pack .rasmoldefaults.color.red"
" .rasmoldefaults.color.green .rasmoldefaults.color.blue"
" -side top -in .rasmoldefaults.color\n"
"pack .rasmoldefaults.color\n"
"scale .rasmoldefaults.ambient.ambient -label \"Ambient\" -from 0 -to 100"
" -length 10c -orient horizontal -variable trans_ambient\n"
"pack .rasmoldefaults.ambient.ambient"
" -side left -in .rasmoldefaults.ambient\n"
"pack .rasmoldefaults.ambient\n"
"scale .rasmoldefaults.wframe.wframe -label \"Wireframe\" -from 0 -to 100"
" -length 10c -orient horizontal -variable trans_wframe\n"
"pack .rasmoldefaults.wframe.wframe"
" -side left -in .rasmoldefaults.wframe\n"
"pack .rasmoldefaults.wframe\n"
"scale .rasmoldefaults.sfill.sfill -label \"Spacefill\" -from 0 -to 499"
" -length 10c -orient horizontal -variable trans_sfill\n"
"pack .rasmoldefaults.sfill.sfill"
" -side left -in .rasmoldefaults.sfill\n"
"pack .rasmoldefaults.sfill\n"
"scale .rasmoldefaults.dots.dots -label \"Dots\" -from 0 -to 1000"
" -length 10c -orient horizontal -variable trans_dots\n"
"pack .rasmoldefaults.dots.dots"
" -side left -in .rasmoldefaults.dots\n"
"pack .rasmoldefaults.dots\n"
"scale .rasmoldefaults.slab.slab -label \"Slab\" -from 0 -to 100"
" -length 10c -orient horizontal -variable trans_slab\n"
"pack .rasmoldefaults.slab.slab"
" -in .rasmoldefaults.slab\n"
"pack .rasmoldefaults.slab\n"
"checkbutton .rasmoldefaults.lines.axis -text \"Axis\" -variable trans_axis\n"
"checkbutton .rasmoldefaults.lines.box -text \"Box\" -variable trans_box\n"
"checkbutton .rasmoldefaults.label -text \"Numbers\" -variable trans_label\n"
"pack .rasmoldefaults.lines.axis .rasmoldefaults.lines.box"
" .rasmoldefaults.label -side left -in .rasmoldefaults.lines\n"
"pack .rasmoldefaults.lines\n"
"button .rasmoldefaults.com.reset -text \"Reset\" -command {read_rasmol_rc ; show}\n"
"button .rasmoldefaults.com.show -text \"Show\" -command {read_win ; show}\n"
"button .rasmoldefaults.com.cancel -text \"Cancel\""
" -command {read_rasmol_rc ; wm withdraw .rasmoldefaults}\n"
"button .rasmoldefaults.com.save -text \"Save\""
" -command {read_win ; save_rasmol_defaults}\n"
"pack .rasmoldefaults.com.reset .rasmoldefaults.com.show .rasmoldefaults.com.cancel"
" .rasmoldefaults.com.save -side left -in .rasmoldefaults.com\n"
"pack .rasmoldefaults.com\n";


char Tcl_Skript_StatusGraphengenerierung[] =	/* Statusmeldung */
".hauptframe.statusframe.bitmap configure -bitmap info\n"
".hauptframe.statusframe.msg configure -text "
"\"Status: Generating graphs\"\n"
"if {$file2d($programm)!=1 && $r_or_g($programm)>1 &&\\\n"
"    ($file2d($programm)==0 || $r_or_g($programm)==3)} {\n"
"  frame .hauptframe.graphframe\n"
"  label .hauptframe.graphframe.label -text \"Graph No.\"\n"
"  label .hauptframe.graphframe.number -width 6 -textvariable graphnr\\\n"
"    -anchor e -justify right\n"
"  pack .hauptframe.graphframe -side top -pady 1m\n"
"  pack .hauptframe.graphframe.label .hauptframe.graphframe.number\\\n"
"    -side left -padx 2m\n"
"}\n";

char Tcl_Skript_Zeige_Bedienfenster[] = "wm deiconify .aufruf\n";
char Tcl_Skript_Loesche_Bedienfenster[] = "wm withdraw .aufruf\n"
"wm withdraw .rasmoldefaults\n";
char Tcl_Skript_Entferne_Bedienfenster[] = "destroy .aufruf\n";
char Tcl_Skript_NachrichtAnRasmol[] =	/* Graph aus "CaGe3.tmp" laden */
"send $rasmol_name {zap}\n"
"send $rasmol_name {load pdb CaGe3.tmp}\n"
"if {[file exists .rasmolrc]} {send $rasmol_name {source .rasmolrc}}\n";
char Tcl_Skript_Loesche_Statusmeldung[] =
".hauptframe.statusframe.bitmap configure -bitmap {}\n"
".hauptframe.statusframe.msg configure -text \"\"\n"
"if {[winfo exists .hauptframe.graphframe]}\\\n"
"   {destroy .hauptframe.graphframe}\n";

char Tcl_Skript_Zeige_Fullgenlogfile[] =
"set logfilename \"Full_gen_$n_end(1)\"\n"
"if {$n_end(1)!=$n_anf(1)} {set logfilename "
"\"${logfilename}_$n_anf(1)\"}\n"
"if {$ipr(1)} {set logfilename ${logfilename}_ipr}\n"
"if {$cases(1)!=0} {set logfilename ${logfilename}_c$cases(1)}\n"
"set logfilename ${logfilename}.log\n"
"toplevel .log\n"
"wm title .log \"Fullerenes - Logfile contents\"\n";


char Tcl_Skript_Zeige_HCgenlogfile[] =
"if {$ipr(3)} {set iprstring \"_ipr\"} else {set iprstring {}}\n"
"if {$peric(3)} {set pcstring \"_pc\"} else {set pcstring {}}\n"
"set logfilename \"\" \n"
"append logfilename \"c\" $c(3) \"h\" $h(3) \"_\" $p3 \"pent\" $iprstring $pcstring \".log\" \n"
"toplevel .log\n"
"wm title .log \"Hydrocarbons - Logfile contents\"\n";


char Tcl_Skript_Zeige_3reggenlogfile[] =
"set logfilename [bilde_3reggen_dateiname 0]\n"
"toplevel .log\n"
"wm title .log \"3-regular planar maps - Logfile contents\"\n";


char Tcl_Skript_Zeige_Tubetypelogfile[] =
"set logfilename [bilde_tubetype_dateiname 0]\n"
"toplevel .log\n"
"wm title .log \"Tube-type fullerenes - Logfile contents\"\n";


char Tcl_Skript_Zeige_Fileinhalt[] =
"toplevel .log\n"
"wm title .log \"Input file - contents\"\n"
"label .log.label -text \"Read $absolutehighnr objects from input file.\"\n"
"button .log.cont -text Continue -command {set cancel 1}\n"
"pack .log.label .log.cont -side top -pady 5m -padx 5m -in .log\n";


char Tcl_Skript_Zeige_Logfile[] =
"label .log.frame2\n"
"pack .log.frame2 -pady 1m\n"
"button .log.cont -text Continue -command {set cancel 1}\n"
"if {[file exists $logfilename]==1} {\n"
"  label .log.label -text \"Output written to log file (${logfilename}):\"\n"
"  frame .log.frame\n"
"  text .log.text -relief ridge -bd 4 -yscrollcommand \".log.scroll set\" "
"-padx 2m -pady 2m\n"
"  scrollbar .log.scroll -command \".log.text yview\"\n"
"  set logfile [open $logfilename]\n"
"  while {![eof $logfile]} {.log.text insert end [read $logfile 1000]}\n"
"  close $logfile\n"
"  pack .log.scroll -in .log.frame -side right -fill y\n"
"  pack .log.text -in .log.frame -side left\n"
"  pack .log.label .log.frame .log.cont -in .log.frame2 -side top -padx 2m"
" -pady 1m\n"
"} \\\n"
"else {\n"
"  label .log.label1 -text \"Log file\"\n"
"  label .log.label2 -fg red -text $logfilename\n"
"  label .log.label3 -text \"does not exist:\"\n"
"  label .log.label4 -text "
"\"Maybe the generation program has detected illegal parameters.\"\n"
"  label .log.label5 -text \"(see output on stderr)\"\n"
"  pack .log.label1 .log.label2 .log.label3 .log.label4 .log.label5"
" -in .log.frame2 -side top -padx 2m\n"
"  pack .log.cont -in .log.frame2 -side top -padx 2m -pady 2m\n"
"}\n";


/****************/
/*  functions:  */
/****************/
/*************************TK_MAIN*******************************************/
/*        This procedure initializes the Tk world                          */

void Tk_Main (int argc, char **argv, Tcl_AppInitProc * appInitProc)
{
  /* appInitProc:  Application-specific initialization procedure to call 
     after most initialization but before starting to execute commands. */
  char *msg, *class;

  interp = Tcl_CreateInterp ();
#ifdef TCL_MEM_DEBUG
  Tcl_InitMemory (interp);
#endif

  /*
   * If a display was specified, put it into the DISPLAY
   * environment variable so that it will be available for
   * any sub-processes created by us.
   */

  if (display != NULL) {
    Tcl_SetVar2 (interp, "env", "DISPLAY", display, TCL_GLOBAL_ONLY);
  }
  /*
   * Initialize the Tk application. For the application's
   * class, capitalize the first letter of the name.
   */
  class = (char *) ckalloc ((unsigned) (strlen (name) + 1));
  strcpy (class, name);
  class[0] = toupper ((unsigned char) class[0]);
  /*   mainWindow = Tk_CreateMainWindow(interp, display, name, class); */
  ckfree (class);
  /*  if (mainWindow == NULL) {
     fprintf(stderr, "%s\n", interp->result);    
     exit(1);
     }
     if (synchronize) {
     XSynchronize(Tk_Display(mainWindow), True);
     }
   */
  /*
   * Invoke application-specific initialization.
   */

  if ((*appInitProc) (interp) != TCL_OK) {
    fprintf (stderr, "application-specific initialization failed: %s\n",
	     interp->result);
  }
  fflush (stdout);		/* obsolete */
  Tcl_DStringInit (&command);	/* obsolete */
  Tcl_ResetResult (interp);
  return;

error:				/* obsolete? */
  msg = Tcl_GetVar (interp, "errorInfo", TCL_GLOBAL_ONLY);
  if (msg == NULL) {
    msg = interp->result;
  }
  fprintf (stderr, "%s\n", msg);
  Tcl_Eval (interp, errorExitCmd);
}

/* 
 * tkAppInit.c --
 *
 *      Provides a default version of the Tcl_AppInit procedure for
 *      use in wish and similar Tk-based applications.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkAppInit.c 1.21 96/03/26 16:47:07
 */

/*#include "tk.h"
 */
/*
 * The following variable is a special hack that is needed in order for
 * Sun shared libraries to be used for Tcl.
 */
/*
   extern int matherr();
   int *tclDummyMathPtr = (int *) matherr; */

#ifdef TK_TEST
EXTERN int Tktest_Init _ANSI_ARGS_ ((Tcl_Interp * interp));
#endif
/* TK_TEST */
/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *      This is the main program for the application.
 *
 * Results:
 *      None: Tk_Main never returns here, so this procedure never
 *      returns either.
 *
 * Side effects:
 *      Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *      This procedure performs application-specific initialization.
 *      Most applications, especially those that incorporate additional
 *      packages, will have their own version of this procedure.
 *
 * Results:
 *      Returns a standard Tcl completion code, and leaves an error
 *      message in interp->result if an error occurs.
 *
 * Side effects:
 *      Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int Tcl_AppInit (interp)
     Tcl_Interp *interp;
{
  if (Tcl_Init (interp) == TCL_ERROR) {
    return TCL_ERROR;
  }
  if (Tk_Init (interp) == TCL_ERROR) {
    return TCL_ERROR;
  }
  Tcl_StaticPackage (interp, "Tk", Tk_Init, (Tcl_PackageInitProc *) NULL);
#ifdef TK_TEST
  if (Tktest_Init (interp) == TCL_ERROR) {
    return TCL_ERROR;
  }
  Tcl_StaticPackage (interp, "Tktest", Tktest_Init,
		     (Tcl_PackageInitProc *) NULL);
#endif /* TK_TEST */


  /*
   * Call the init procedures for included packages.  Each call should
   * look like this:
   *
   * if (Mod_Init(interp) == TCL_ERROR) {
   *     return TCL_ERROR;
   * }
   *
   * where "Mod" is the name of the module.
   */

  /*
   * Call Tcl_CreateCommand for application-specific commands, if
   * they weren't already created by the init procedures called above.
   */

  /*
   * Specify a user-specific startup file to invoke if the application
   * is run interactively.  Typically the startup file is "~/.apprc"
   * where "app" is the name of the application.  If this line is deleted
   * then no user-specific startup file will be run under any conditions.
   */

  Tcl_SetVar (interp, "tcl_rcFileName", "~/.wishrc", TCL_GLOBAL_ONLY);
  return TCL_OK;
}

/***********************READ_HEADERFORMAT*********************************/
/*  Determines the format of file f by reading the headername            */
/*  0 = planar_code,  2 = writegraph_2d,  3 = writegraph_3d,  -1 = error */

int read_headerformat (FILE * f)
{
  unsigned char c[128];
  int i = -1;
  if (fread (&c[0], sizeof (unsigned char), 2, f) < 2) {
    return (-2);
  }
  if (strncmp ((char *) &c[0], ">>", 2) != 0) {
    return (-4);
  }
  do {
    i++;
    if (i > 127) {
      return (-3);
    }
    if (fread (&c[i], sizeof (unsigned char), 1, f) == 0) {
      return (-2);
    }
  } while (c[i] != ' ' && c[i] != '<');
  if (c[i] == '<') {
    if (ungetc (c[i], f) == EOF) {
      return (-2);
    }
  }
  c[i] = 0;
/*
   if (strcmp(c,(char *)"planar_code")==0) {return(0);}
   else if (strcmp(c,(char *)"writegraph2d")==0) {return(2);}
   else if (strcmp(c,(char *)"writegraph3d")==0) {return(3);}
     return(-1); *//* falsches Format */
  if (strcmp ((char *) &c[0], (char *) "planar_code") == 0) {
    return (0);
  } else if (strcmp ((char *) &c[0], (char *) "writegraph2d") == 0) {
    return (2);
  } else if (strcmp ((char *) &c[0], (char *) "writegraph3d") == 0) {
    return (3);
  }
  return (-1);			/* falsches Format */
}

/****************************READ_TO_END_OF_HEADER************************/
/*  reads file input until "<<" appears                                  */

char read_to_end_of_header (FILE * f, unsigned char *c)
{
  while (strncmp ((char *) c, "<<", 2) != 0) {
    c[0] = c[1];
    if (fread (&c[1], sizeof (unsigned char), 1, f) == 0) {
      return (2);
    }
  }
  return (1);
}

/************************READ_ENDIAN**************************************/
/*  Reads in the endian type (le/be), if existing, and skips to the end  */
/*  of the header (the endian information is a part of the header        */

char read_endian (FILE * f, int *endian)
{
  unsigned char c[2];
  do {
    if (fread (&c[0], sizeof (unsigned char), 1, f) == 0) {
      return (2);
    }
  } while (isspace (c[0]));
  if (fread (&c[1], sizeof (unsigned char), 1, f) == 0) {
    return (2);
  }
  if (strncmp ((char *) &c[0], "le", 2) == 0) {
    *endian = LITTLE_ENDIAN;
  } else if (strncmp ((char *) &c[0], "be", 2) == 0) {
    *endian = BIG_ENDIAN;
  } else {
    *endian = ENDIAN_IN;
  }
  if (read_to_end_of_header (f, &c[0]) == 2) {
    return (2);
  }
  return (1);
}

/**********************WRITE_2BYTE_NUMBER***************************/
/*  This procedure takes care of the endian                        */

char write_2byte_number (FILE * f, unsigned short n, int endian)
{
  if (endian == BIG_ENDIAN) {
    fprintf (f, "%c%c", n / 256, n % 256);
  } else {
    fprintf (f, "%c%c", n % 256, n / 256);
  }
  return (ferror (f) ? 2 : 1);
}

/***********************READ_2BYTE_NUMBER***************************/
/*  This procedure takes care of the endian                        */

char read_2byte_number (FILE * f, unsigned short *n, int endian)
{
  unsigned char c[2];
  if (fread (&c[0], sizeof (unsigned char), 2, f) < 2) {
    return (2);
  }
  if (endian == BIG_ENDIAN) {
    *n = c[0] * 256 + c[1];
  } else {
    *n = c[1] * 256 + c[0];
  }
  return (1);
}

/**********************READ_OLD_OR_NEW***********************************/
/*  This function decides for a short-code  if a number is to be read   */
/*  from the last graph or from the file                                */

char read_old_or_new (FILE * f, unsigned short *lastinput, unsigned short s,
	       unsigned short *z, size_t maxentries, BOOL bignum, int endian,
		      unsigned short *num)
{
  unsigned char k;
  if (*z >= s) {		/* new number to be read from the file */
    if (bignum) {
      if (read_2byte_number (f, num, endian) == 2) {
	return (2);
      }
    } else {
      if (fread (&k, sizeof (unsigned char), 1, f) == 0) {
	return (2);
      }
      *num = (unsigned short) k;
    }
    if (lastinput && *z < maxentries) {
      lastinput[*z] = *num;
      (*z)++;
    }
  }
  /* if */
  else {
    *num = lastinput[*z];
    (*z)++;
  }
  return (1);
}

/**************************WRITE_PLANAR_CODE_S *******************************/
/*     This function covers the code PLANAR_CODE      (then old_g==NULL)     */
/*     and  _OLD codes  (then header==FALSE)                                 */

char write_planar_code_s (FILE * f, unsigned short *output_g, unsigned short
		 *old_g, unsigned long count, BOOL header, size_t maxentries)
{
  unsigned long s = 0, z = 0;
  int n, i = 1;
  if (header) {
    fprintf (f, ">>planar_code%s %s<<", (old_g == NULL) ? "" : "_s",
	     (ENDIAN_OUT == LITTLE_ENDIAN ? "le" : "be"));
  }
  n = (int) (output_g[0] == 0 ? output_g[1] : output_g[0]);
  if (old_g != NULL) {		/* write number of common entries */
    if (count > 0) {
      while (output_g[s] == old_g[s] && (unsigned short) (s + 1) > 0) {
	s++;
      }
    }
    if (output_g[0] && n > UCHAR_MAX && (unsigned short) (s + 1) > 0) {
      s++;
      output_g--;
    }
    /* a leading zero will be added to the current graph 
       and has been added to the old graph */
    else if (output_g[0] == 0 && n <= UCHAR_MAX) {
      s--;
      output_g++;
    }
    /* the leading zero will be skipped in the current graph
       and has been skipped in the old graph */
    if (write_2byte_number (f, (unsigned short) s, ENDIAN_OUT) == 2) {
      return (2);
    }
  }
  /* n>UCHAR_MAX <=> use unsigned short for output */
  if (n <= UCHAR_MAX) {
    if (z >= s) {
      fprintf (f, "%c", (unsigned char) n);
    }
    z++;
  } else {			/* big graph */
    if (z >= s) {
      fprintf (f, "%c", 0);
    }
    z++;
    if (z >= s) {
      if (write_2byte_number (f, (unsigned short) n, ENDIAN_OUT) == 2) {
	return (2);
      }
    }
    z++;
  }
  while (i <= n) {
    if (z > (unsigned long) maxentries) {
      return (3);
    }
    /* graph too big for output_g */
    if (output_g[z] == 0) {
      i++;
    }				/* next vertex */
    if (z >= s) {
      if (n <= UCHAR_MAX) {
	fprintf (f, "%c", (unsigned char) output_g[z++]);
      } else {
	if (write_2byte_number (f, output_g[z++], ENDIAN_OUT) == 2) {
	  return (2);
	}
      }
    }
  }
  return (ferror (f) ? 2 : 1);
}

/**********************READ_PLANAR_CODE_S_OLD*****************************/
/*  This function covers the code PLANAR_CODE_OLD (then sh==FALSE)       */
/*  ne = number of (directed) edges which were read                      */
/*  nv = number of vertices which were read                              */

char read_planar_code_s_old (FILE * f, int maxn, int endian,
     unsigned short *lastinput, size_t maxentries, BOOL sh, int *nv, int *ne)
{
  int i, n;
  unsigned short s = 0, z = 0, signum, num;	/* z = read numbers */

  if (sh && read_2byte_number (f, &s, endian) == 2) {
    return (feof (f) ? 0 : 2);
  }
  if ((size_t) s > maxentries) {
    return (3);
  }
  if (read_old_or_new (f, lastinput, s, &z, maxentries, FALSE, endian, &signum) == 2) {
    return (feof (f) ? 0 : 2);
  }
  if (signum == 0) {
    if (read_old_or_new (f, lastinput, s, &z, maxentries, TRUE, endian, &num) == 2) {
      return (2);
    }
  } else {
    num = signum;
  }
  if ((n = (int) num) > maxn) {
    return (3);
  }
  i = 1;
  *ne = 0;
  while (i <= n) {
    do {
      (*ne)++;
      if (read_old_or_new (f, lastinput, s, &z, maxentries, signum == 0, endian, &num)
	  == 2) {
	return (2);
      }
    } while (num != 0);
    i++;
    (*ne)--;
  }				/* while */
  *nv = n;
  return (1);
}

/*****************************READ_PLANAR_CODE_S******************************/
/*  This function covers the code PLANAR_CODE (then sh==FALSE)               */

char read_planar_code_s (FILE * f, int maxn, int *endian, unsigned long num,
     unsigned short *lastinput, size_t maxentries, BOOL sh, int *nv, int *ne)
{
  int dummy;
  if (num == 0) {
    if (read_headerformat (f) < 0) {
      return (5);
    }
    if (read_endian (f, endian) == 2) {
      return (2);
    }
  }
  return (read_planar_code_s_old (f, maxn, *endian, lastinput,
				  maxentries, sh, nv, ne));
}

/************************WRITE_WRITEGRAPH2D**********************************/
/*  covers code WRITEGRAPH3D (then dim=3, else dim=2) and _OLD
   (then old==TRUE)                                                        */
/*  If "planar==TRUE", then "inputgraph" must contain the planar embedding
   in "planar_code" style.                                                 */

char write_writegraph2d (FILE * f, double *coord, int n, int dim,
	      BOOL old, BOOL header, BOOL planar, unsigned short *inputgraph)
{
  int i, j, pos;
  pos = inputgraph[0] ? 0 : 1;	/* skip the leading zero */
  if (header) {
    fprintf (f, ">>writegraph%dd%s<<%s%s", dim, planar ?
	     (char *) " planar" : "", NL, NL);
  }
  for (i = 1; i <= n; i++) {
    fprintf (f, "%d", i);
    for (j = 1; j <= dim; j++) {
      fprintf (f, " %g", coord ? coord[(i - 1) * dim + (j - 1)] : 0);
    }
    if (planar) {
      while (inputgraph[++pos] != 0) {
	fprintf (f, " %d", inputgraph[pos]);
      }
      /* the first entry is the vertex number, so it must be skipped */
    }
    fprintf (f, NL);
  }
  if (old == FALSE) {
    fprintf (f, "0%s%s", NL, NL);
  }
  return (ferror (f) ? 2 : 1);
}

/************************READ_WRITEGRAPH2D_OLD********************************/
/*  covers code WRITEGRAPH3D_OLD (then dim=3, else dim=2) and new formats
   (then graph ends with 0 instead of a vertex-id)                     */
/*  "planar_out==TRUE", so "inputgraph" must provide memory to receive 
   the planar embedding in "planar_code" style. The input is already an
   embedding.                                                               */
/*  The input vertices must be ordered 1,2,...,n.                            */
/*  If dim==2, then "coord" is expected to be an array for two-dimensional
   coordinates (unlike in "gconv.c").                                       */

char read_writegraph2d_old (FILE * f, double *coord, int maxn, int dim,
	     unsigned short *inputgraph, size_t maxentries, int *nv, int *ne)
{
  char c[LINELEN];
  char *pos;
  long int i, j;
  int n;
  int inputpos = 1;		/* next position to fill in array "inputgraph" */
  double d;
  BOOL ende = FALSE;

  *ne = 0;
  n = 0;
  do {
    if (fgets (&c[0], LINELEN, f) == NULL) {
      /* in case of regular return */
      /* vs */
      if (!feof (f)) {
	inputgraph[0] = *nv = n;
	return 2;
      } else {
	if (n == 0)
	  return 0;		/* vs damit der letzte Graph nicht
				   ueberschrieben wird */
	else {
	  inputgraph[0] = *nv = n;
	  return 1;
	}
      }
      /*   return(feof(f) ? (n==0 ? 0 : 1) : 2); */
    }
    pos = &c[0];
    if ((i = strtol (pos, &pos, 10)) != 0L) {	/* regular line */
      /*<DEBUG>* fprintf(stderr, "%3d", i); /*</DEBUG>*/
      if (i != n + 1) {
	return (5);
      }				/* vertices must be ordered */
      if ((int) i > n) {
	n = (int) i;
      }
      if (n > maxn) {
	return (3);
      }
      for (j = 1; j <= dim; j++) {
	d = strtod (pos, &pos);
	if (coord) {
	  coord[(i - 1) * dim + (j - 1)] = d;
	}
      }
      while ((j = strtol (pos, &pos, 10)) != 0L) {
	inputgraph[inputpos++] = j;
	(*ne)++;
      }
      inputgraph[inputpos++] = 0;

    }
    /* if */
    else {
      if (*(pos - 1) == '0') {
	ende = TRUE;
      }
    }
  } while (!feof (f) && !ende);

  inputgraph[0] = *nv = n;
  /*printf("write2d ne %d\n",(*ne));*/
  return (1);
}

/************************READ_WRITEGRAPH2D************************************/
/*  covers code WRITEGRAPH3D                                                 */

char read_writegraph2d (FILE * f, double *coord, int maxn, int dim, BOOL header,
	     unsigned short *inputgraph, size_t maxentries, int *nv, int *ne)
{
  unsigned char c[2] =
  {' ', ' '};
  if (header && read_to_end_of_header (f, &c[0]) == 2) {
    return (2);
  }
  return (read_writegraph2d_old (f, coord, maxn, dim, inputgraph, maxentries, nv, ne));
}

/************************WRITE_BROOKHAVEN_PDB********************************/
/*  "coord" MUST point on a coordinate array                                */

char write_brookhaven_pdb (FILE * f, unsigned short *inputgraph, double *coord,
			   int n)
{
  int i, j, adj, pos;
  pos = inputgraph[0] ? 1 : 2;	/* skip leading zero and vertex number */

  for (i = 1; i <= n; i++) {
    adj = 0;			/* adjacency of vertex i */
    while (inputgraph[pos++]) {
      adj++;
    }
    fprintf (f, "ATOM  %5d  %c                %8.3f%8.3f%8.3f"
	     "                          %s",
	     i, adj == 1 ? 'H' : 'C', coord[(i - 1) * 3],
	     coord[(i - 1) * 3 + 1], coord[(i - 1) * 3 + 2], NL);
  }
  pos = inputgraph[0] ? 1 : 2;
  for (i = 1; i <= n; i++) {
    j = 0;
    while (inputgraph[pos]) {
      if ((j%6)==0) {
	if (j > 0)
	  fprintf(f, "\n");
	fprintf (f, "CONECT%5d", i);
      }
      fprintf (f, "%5d", inputgraph[pos++]);
      j++;
    }
    fprintf (f, "%s", NL);
    pos++;
  }
  fprintf (f, "END   %s", NL);
  return (ferror (f) ? 2 : 1);
}

/********************WRITE_OOGL********************************************/
/*  Diese Funktion gibt einen Graphen im oogl-Format aus.                 */
/*  nv,ne:  number of vertices, edges                                     */

char write_oogl (FILE * f, unsigned short *g, double *coords, int nv, int ne)
{
  int index;
  int i, j, k;
  index = g[0] ? 1 : 2;
  fprintf (f, "(geometry CaGe {LIST {\n");
  fprintf (f, "{VECT %d %d %d\n", ne, ne * 2, ne);
  for (i = 0; i < ne; i++) {
    fprintf (f, "2\n");
  }
  for (i = 0; i < ne; i++) {
    fprintf (f, "1\n");
  }
  i = 1;
  j = index;
  while (i <= nv) {
    while (k = g[j]) {
      if (i <= k) {		/* Kanten nicht doppelt ausgeben */
	fprintf (f, "%f %f %f    %f %f %f\n", coords[i * 3 - 3], coords[i * 3 - 2],
		 coords[i * 3 - 1], coords[k * 3 - 3], coords[k * 3 - 2], coords[k * 3 - 1]);
      }
      j++;
    }
    i++;
    j++;
  }
  for (i = 0; i < ne; i++) {
    fprintf (f, "%.6f %.6f %.6f  %.6f\n",
	     (double) 0.1, (double) 0.1, (double) 0.1, (double) 1);
  }
  fprintf (f, "}\n}}\n\n)");
  fflush (f);
  return (feof (f) ? 2 : 1);
}

/****************************GO_TO_BACKGROUND2****************************/
/* Diese Funktion setzt die Informationen ueber einen einzubettenden
   Graphen zurueck.                                                      */

void go_to_background2 (int outnr)
{
  int i;
  for (i = 0; i < ANZ_GRAPH; i++) {
    if (graphnr[i] == outnr) {
      graphstate[i] &= ~(SPRING_IN_ARBEIT | SCHLEGEL_IN_ARBEIT);
    }
  }
  status |= (SKIPMODUS | STOP_SPRING | STOP_SCHLEGEL);
  status &= ~REVIEWMODUS;
}


/**************************GO_TO_BACKGROUND******************************/
/* Diese Funktion fuehrt die notwendigen Schritte aus, um das Programm im
   Hintergrund weiterlaufen zu lassen                                   */

void go_to_background (void)
{
  char c[LINELEN] = "send rasmol exit";
  if (geompipe) {
 /* vs */fprintf (geompipe,"(exit)");
    pclose (geompipe);
  }
  if (rasmol_id != 0) {
    if (Tcl_Eval (interp, c) == TCL_ERROR) {
      fprintf (stderr, "\n RasMol exit\n ");
      exit (0);
    }
    rasmol_id = 0;
  }
  if (spring_id != 0) {
    kill (spring_id, SIGKILL);
    spring_id = 0;
  }
  if (schlegel_id != 0) {
    kill (schlegel_id, SIGKILL);
    schlegel_id = 0;
  }
  rasmol = nil;
  tkview = False;
  if (spring) {
    Tcl_DeleteFileHandler ((int) (fileno (spring)));
    pclose (spring);
    spring = nil;
  }
  if (schlegel) {
    Tcl_DeleteFileHandler ((int) (fileno (schlegel)));
    pclose (schlegel);
    schlegel = nil;
  }
  while (first_g) {		/* doppelt verkettete Liste loeschen */
    curr_g = first_g->next;
    free (first_g->planarcode);
    free (first_g);
    first_g = curr_g;
  }
  first_g = curr_g = last_g = nil;
  if (file_s2d) {
    fclose (file_s2d);
    file_s2d = nil;
  }
  if (file_s3d) {
    fclose (file_s3d);
    file_s3d = nil;
  }
  if (file_spdb) {
    fclose (file_spdb);
    file_spdb = nil;
  }
  if (global_background == 1) {
    global_background = 2;
  }
  /* Funktion wurde bearbeitet */
}

/*******************************ALLES_SCHLIESSEN**************************/
/*  Loescht alle nebenher laufenden Programme                            */
/*  und gibt belegten Speicher wieder frei                               */

void alles_schliessen (void)
{
  go_to_background ();
  if (inputfile_id != 0) {
    kill (inputfile_id, SIGKILL);
    inputfile_id = 0;
  }
  if (inputfile) {
    if (filehandler) {
      Tcl_DeleteFileHandler ((int) (fileno (inputfile)));
    }
    pclose (inputfile);
    inputfile = nil;
  }
  if (raw) {
    fclose (raw);
    raw = nil;
  }
  if (outpipe) {
    pclose (outpipe);
    outpipe = nil;
  }
  if (file_2d) {
    fclose (file_2d);
    file_2d = nil;
  }
  if (file_3d) {
    fclose (file_3d);
    file_3d = nil;
  }
}

/*********************SCHLUSS****************************************/
/*  Diese Funktion wird bei Beedingung des Programms aufgerufen.    */
/*  Wenn der Fehler nicht die Folge eines Tcl-Befehls ist, so hat
   interp->result keine Bedeutung.                                 */

void schluss (int exitcode)
{
  char c[6];

  if (exitcode != 0)		/* Fehler */
    fprintf (stderr, "Exit code %d:  %s\n", exitcode, interp->result);
  alles_schliessen ();
  /* mv .rasmolrc_tmp .rasmolrc */
  if (Tcl_Eval (interp, Tcl_Script_mv_rasmolrc_tmp) == TCL_ERROR)
    schluss (2031);

  sprintf (c, "%d", exitcode);
  Tcl_VarEval (interp, (char *) "exit ", c, (char *) NULL);

  Tcl_DeleteInterp (interp);
  exit (exitcode);
}

/***********************HOLE_SPEICHER****************************************/
/*  Stellt len Bytes dynamischen Speicher zur Verfuegung                    */

void *hole_speicher (size_t len)
{
  static void *ptr;
  if ((ptr = (void *) malloc (len)) == nil) {
    fprintf (stderr, "\n malloc(%d)", len);
    schluss (50);
  }
  return (ptr);
}


/*******************FUELLE_TCL_GRAPHVARIABLEN*******************************/
/* Diese Funktion fuellt die Tcl-Arrays "adj", "xpos" und "ypos" mit 
   den Werten aus "g" und "gcoords", so dass die Daten nachher in
   derselben Form wie beim Programm "tkviewgraph" zur Verfuegung 
   stehen. "gcoords" zeigt auf ein Array mit 2-dimensionalen Koordinaten.  */

void fuelle_tcl_graphvariablen (unsigned short *g, double *gcoords)
{
  char c[LINELEN], d[LINELEN];	/* reicht hoffentlich */
  char Tcl_Skript_LoescheXY[] =
  "if {[info exists xpos]==1} {unset xpos ypos}\n";
  unsigned short n, i = 1;
  int index, j;
  index = g[0] ? 1 : 2;		/* fuehrende Null wird uebersprungen */
  n = g[0] ? g[0] : g[1];
  j = index;			/* augenblicklich eingelesene Zahl */
  if (Tcl_Eval (interp, Tcl_Skript_LoescheXY) == TCL_ERROR) {
    schluss (99);
  }
  while (i <= n) {
    sprintf (c, "%d", i);
    sprintf (d, "%.3f", gcoords[(i - 1) * 2]);
    if (Tcl_SetVar2 (interp, (char *) "xpos", c, d, TCL_LEAVE_ERR_MSG) == NULL) {
      schluss (78);
    }
    sprintf (d, "%.3f", gcoords[(i - 1) * 2 + 1]);
    if (Tcl_SetVar2 (interp, (char *) "ypos", c, d, TCL_LEAVE_ERR_MSG) == NULL) {
      schluss (79);
    }
    if (Tcl_SetVar2 (interp, (char *) "adj", c, (char *) "", TCL_LEAVE_ERR_MSG)
	== NULL) {
      schluss (81);
    }
    while (g[j] != 0) {		/* es folgen Knotennummern */
      sprintf (d, "%d", g[j]);
      if (Tcl_SetVar2 (interp, (char *) "adj", c, d, TCL_LIST_ELEMENT |
		       TCL_APPEND_VALUE | TCL_LEAVE_ERR_MSG) == NULL) {
	schluss (80);
      }
      j++;
    }
    j++;
    i++;
  }
}


/***************KANTE_DES_GEWAEHLTEN_ZYKLUS******************************/
/* Diese Funktion bestimmt den vom Benutzer im Schlegel-Diagramm
   angewaehlten Randkreis und setzt i_out und j_out auf die
   Knotennummern des Anfangs- und Endpunktes einer gerichteten Kante,
   auf deren rechter Seite dieser Zyklus liegt. "gcoords" zeigt auf
   ein Array mit 2-dimensionalen Koordinaten. (x,y) ist der vom
   Benutzer angeklickte Punkt im Schlegel-Diagramm. "g" ist der Graph
   im Planarcode.                                                       */

BOOL kante_des_gewaehlten_zyklus (unsigned short *g, double *gcoords,
				  int *i_out, int *j_out)
{
  double x, y, x1, y1, x2, y2, yp, ypmin;
  BOOL set = False;		/* True => ypmin enthaelt sinnvollen Wert */
  unsigned short i, ii, imin;
  char *var;
  int index, j, jmin, k, i2;
  index = g[0] ? 1 : 2;		/* fuehrende Null wird uebersprungen */
  n = g[0] ? g[0] : g[1];
  j = index;			/* augenblicklich eingelesene Zahl */
  if ((var = Tcl_GetVar (interp, "schlegelx", TCL_LEAVE_ERR_MSG)) == NULL) {
    schluss (125);
  }
  x = atof (var);
  if ((var = Tcl_GetVar (interp, "schlegely", TCL_LEAVE_ERR_MSG)) == NULL) {
    schluss (126);
  }
  y = atof (var);

  i = 1;			/* Kanten durchlaufen */
  while (i <= n) {
    x1 = gcoords[(i - 1) * 2];
    y1 = gcoords[(i - 1) * 2 + 1];
    while (g[j]) {
      x2 = gcoords[(g[j] - 1) * 2];
      y2 = gcoords[(g[j] - 1) * 2 + 1];
      if (x1 != x2 && ((x1 <= x && x2 >= x) || (x1 >= x && x2 <= x))) {
	yp = y1 + (x - x1) * (y2 - y1) / (x2 - x1);	/* (x,yp) ist Geradenpunkt */
	if (yp > y) {		/* Gerade oberhalb des angeklickten Punkts */
	  if (!set || yp < ypmin) {
	    set = True;
	    ypmin = yp;
	    imin = i;
	    jmin = j;		/* Kantendaten festhalten */
	  }
	}
      }
      j++;
    }
    j++;
    i++;
  }

  if (set) {	/* sonst Aussenflaeche angeklickt => nichts passiert */
    i = imin;
    j = jmin;
    if (gcoords[(imin - 1) * 2] > gcoords[(g[jmin] - 1) * 2]) {
      j = imin;
      i = g[jmin];
    } else {
      i = imin;
      j = g[jmin];
    }
    *i_out = i;
    *j_out = j;
  }
  return (set);
}



/* --- obsolete section --- sl Feb 2001 ---

/**************************SCHREIBE_ZYKLUS2*****************************|
/* Schreibt die Knoten eines beliebigen Randkreises aus g nach f       *|
/* Funktioniert fuer beliebige planare Graphen                         *|
/* size: gewuenschte Mindestgroesse des Kreises                        *|

void schreibe_zyklus2 (FILE * f, unsigned short *g, int size)
{
  static char c[LINELEN], d[LINELEN];	/* reicht hoffentlich *|
  static int first[MAXN + 1], entries[MAXN + 1];
  /* g[first[i]],...,g[first[i]+entries[i]-1] sind die Nachbarn von i *|
  int i, ii, j, k = 0;
  int index = g[0] ? 1 : 2;	/* Verschiebung durch fuehrende Null *|
  int ki = 0;			/* Index fuer ersten Nachfolger *|
  int i2;			/* Index des Nachfolgerknotens *|
  int knoten;			/* Knoten, an dem der Zyklus beginnt *|

  /* Arrays fuellen: *|
  i = index;
  knoten = 1;
  /* fprintf(stderr,"Index %d %d %d Anzahl Knoten: %d\n",g[0],g[1],index,
     g[index-1]); *|
  while (knoten <= g[index - 1]) {
    first[knoten] = i;
    entries[knoten] = 0;
    while (g[i]) {		/*fprintf(stderr,"%d ",g[i]); *|
      entries[knoten]++;
      i++;
    }
    i++;
    knoten++;			/*fprintf(stderr,"\n"); *|
  }

  /* Zyklus ermitteln: *|
  knoten = 1;
  while (k < size && knoten <= g[index - 1]) {
    /* noch nichts gefunden, es stehen aber noch Knoten zur Verfuegung *|
    i = knoten;
    j = g[first[i] + ki];
    sprintf (c, "%d %d ", i, j);
    k = 2;			/* Groesse des Kreises *|
    while (j != knoten) {	/* j ist der aktuelle Knoten, i der Vorgaenger *|
      ii = 0;
      i2 = -1;
      while (ii < entries[j] && i2 == -1) {
	if (g[first[j] + ii] == i) {
	  i2 = (ii + entries[j] - 1) % entries[j];
	}
	ii++;
      }
      if (i2 == -1) {
	schluss (88);
      }
      i = j;
      j = g[first[j] + i2];
      if (j != knoten) {
	k++;
	sprintf (d, "%d ", j);
	strcat (c, d);
      }
    }
    ki++;
    if (ki > 2) {
      ki = 0;
      knoten++;
    }
  }
  fprintf (f, "%d %s\n", k, c);
  if (ferror (f)) {
    schluss (75);
  }
}


/**************BETTE_KNOTEN_EIN_REK******************************************|
/*  Bettet die zum Knoten i adjazenten Knoten ein. Einer von diesen Knoten
   und i selbst sind bereits eingebettet, wodurch der Rest eindeutig wird. *|
/*  Diese Funktion ist fuer Sechseckmuster mit und ohne Wasserstoffatome.   *|

void bette_knoten_ein_rek (unsigned short i, unsigned short *g, double *coords,
			   BOOL * set, int *first, int *entries)
{
  unsigned short k1, k2, k3, k4, k5, k6;	/* k1 = Nummer des Knotens rechts oben,
						   k3 = links oben, k2 = unten, k4 = oben, k5 = rechts unten, 
						   k6 = links unten *|
  unsigned short j, offset;
  double diffx;			/* Differenz zwischen i und 2.Knoten in x-Koordinate *|
  double diffy;

  if (entries[i] < 3) {
    return;
  }				/* keine nicht eingebetteten Nachbarn *|
  if (set[g[first[i]]] && set[g[first[i] + 1]] && set[g[first[i] + 2]]) {
    return;
  }
  /* alle benachbarten Knoten bereits eingebettet *|

  offset = 0;
  while (!set[g[first[i] + offset]]) {
    offset++;
  }				/* offset\in{0,1,2} *|

  /* k1 - k6 belegen: *|
  if ((diffx = coords[(i - 1) * 3] - coords[(g[first[i] + offset] - 1) * 3]) < -0.5) {
    j = 0;
  }
  /* 2.Knoten ist rechts von i *|
  else if (diffx > 0.5) {
    j = 1;
  }
  /* 2.Knoten ist links von i *|
  else {
    j = 2;
  }				/* 2.Knoten ist auf gleicher Hoehe wie i *|
  if ((diffy = coords[(i - 1) * 3 + 1] - coords[(g[first[i] + offset] - 1) * 3 + 1])
      < -0.3) {			/* 2.Knoten ist oberhalb von i *|
    switch (j) {
      case 0:{
	  k1 = g[first[i] + offset];
	  k2 = g[first[i] + (offset + 1) % 3];
	  k3 = g[first[i] + (offset + 2) % 3];
	  k4 = k5 = k6 = 0;
	  break;
	}
      case 1:{
	  k1 = g[first[i] + (offset + 1) % 3];
	  k2 = g[first[i] + (offset + 2) % 3];
	  k3 = g[first[i] + offset];
	  k4 = k5 = k6 = 0;
	  break;
	}
      case 2:{
	  k4 = g[first[i] + offset];
	  k5 = g[first[i] + (offset + 1) % 3];
	  k6 = g[first[i] + (offset + 2) % 3];
	  k1 = k2 = k3 = 0;
	  break;
	}
    }
  } else {			/* 2.Knoten ist unterhalb von i *|
    switch (j) {
      case 0:{
	  k5 = g[first[i] + offset];
	  k6 = g[first[i] + (offset + 1) % 3];
	  k4 = g[first[i] + (offset + 2) % 3];
	  k1 = k2 = k3 = 0;
	  break;
	}
      case 1:{
	  k4 = g[first[i] + (offset + 1) % 3];
	  k5 = g[first[i] + (offset + 2) % 3];
	  k6 = g[first[i] + offset];
	  k1 = k2 = k3 = 0;
	  break;
	}
      case 2:{
	  k2 = g[first[i] + offset];
	  k3 = g[first[i] + (offset + 1) % 3];
	  k1 = g[first[i] + (offset + 2) % 3];
	  k4 = k5 = k6 = 0;
	  break;
	}
    }
  }

  /* k1 - k6  numerieren: *|
  if (k1) {
    coords[(k1 - 1) * 3] = coords[(i - 1) * 3] + cospi6;
    coords[(k1 - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] + 0.5;
    coords[(k1 - 1) * 3 + 2] = 0;
    set[k1] = True;
  }
  if (k2) {
    coords[(k2 - 1) * 3] = coords[(i - 1) * 3];
    coords[(k2 - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] - 1;
    coords[(k2 - 1) * 3 + 2] = 0;
    set[k2] = True;
  }
  if (k3) {
    coords[(k3 - 1) * 3] = coords[(i - 1) * 3] - cospi6;
    coords[(k3 - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] + 0.5;
    coords[(k3 - 1) * 3 + 2] = 0;
    set[k3] = True;
  }
  if (k4) {
    coords[(k4 - 1) * 3] = coords[(i - 1) * 3];
    coords[(k4 - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] + 1;
    coords[(k4 - 1) * 3 + 2] = 0;
    set[k4] = True;
  }
  if (k5) {
    coords[(k5 - 1) * 3] = coords[(i - 1) * 3] + cospi6;
    coords[(k5 - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] - 0.5;
    coords[(k5 - 1) * 3 + 2] = 0;
    set[k5] = True;
  }
  if (k6) {
    coords[(k6 - 1) * 3] = coords[(i - 1) * 3] - cospi6;
    coords[(k6 - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] - 0.5;
    coords[(k6 - 1) * 3 + 2] = 0;
    set[k6] = True;
  }
  /* naechster Rekursionsschritt: *|
  if (k1) {
    bette_knoten_ein_rek (k1, g, coords, set, first, entries);
  }
  if (k2) {
    bette_knoten_ein_rek (k2, g, coords, set, first, entries);
  }
  if (k3) {
    bette_knoten_ein_rek (k3, g, coords, set, first, entries);
  }
  if (k4) {
    bette_knoten_ein_rek (k4, g, coords, set, first, entries);
  }
  if (k5) {
    bette_knoten_ein_rek (k5, g, coords, set, first, entries);
  }
  if (k6) {
    bette_knoten_ein_rek (k6, g, coords, set, first, entries);
  }
}


/********************BETTE_SECHSECKMUSTER_EIN******************************|
/* Bettet ein Sechseckmuster, das im Planarcode vorliegt, planar ein.
   h==True => Wasserstoffe werden auch eingebettet => anderer Algorithmus *|

void bette_sechseckmuster_ein (unsigned short *g, double *coords, BOOL h)
{
  static char c[LINELEN], d[LINELEN];	/* reicht hoffentlich *|
  static int first[MAXN + 1], entries[MAXN + 1];
  /* g[first[i]],...,g[first[i]+entries[i]-1] sind die Nachbarn von i *|
  static BOOL set[MAXN + 1];	/* set[i]==True <=> Koordinaten von i bestimmt *|
  int a, b, i, ii, j, k;
  int index = g[0] ? 1 : 2;	/* Verschiebung durch fuehrende Null *|
  int ki;			/* Index fuer ersten Nachfolger *|
  int i2;			/* Index des Nachfolgerknotens *|
  int knoten;			/* Knoten, an dem der Zyklus beginnt *|
  char richtung;		/* fuer Einbettung ohne Wasserstoffe:
				   0 = oben, 1 = oben rechts, 2 = unten rechts,
				   3 = unten, 4 = unten links, 5 = oben links *|

  /* Arrays fuellen: *|
  i = index;
  knoten = 1;
  while (knoten <= g[index - 1]) {
    first[knoten] = i;
    entries[knoten] = 0;
    while (g[i]) {
      entries[knoten]++;
      i++;
    }
    i++;
    knoten++;
  }
  for (i = 1; i <= g[index - 1]; i++) {
    set[i] = False;
  }				/* loeschen *|

  /* Algorithmus mit Wasserstoffen:  
     von 3-valentem Knoten aus rekursiv einbetten (aehnlich wie beim Numerieren
     eines 3-regulaeren Graphen) *|
  if (h) {
    i = 1;
    while (entries[i] != 3) {
      i++;
    }
    /* die ersten vier Knoten einbetten: *|
    coords[(i - 1) * 3] = coords[(i - 1) * 3 + 1] = coords[(i - 1) * 3 + 2] = 0;
    set[i] = True;
    j = g[first[i]];
    coords[(j - 1) * 3] = cospi6;
    coords[(j - 1) * 3 + 1] = 0.5;
    coords[(j - 1) * 3 + 2] = 0;
    set[j] = True;
    j = g[first[i] + 2];
    coords[(j - 1) * 3] = -cospi6;
    coords[(j - 1) * 3 + 1] = 0.5;
    coords[(j - 1) * 3 + 2] = 0;
    set[j] = True;
    j = g[first[i] + 1];
    coords[(j - 1) * 3] = 0;
    coords[(j - 1) * 3 + 1] = -1;
    coords[(j - 1) * 3 + 2] = 0;
    set[j] = True;
    bette_knoten_ein_rek (g[first[i]], g, coords, set, first, entries);
    bette_knoten_ein_rek (g[first[i] + 1], g, coords, set, first, entries);
    bette_knoten_ein_rek (g[first[i] + 2], g, coords, set, first, entries);
  }
  /* Algorithmus ohne Wasserstoffe:
     Erst die Umrandung einbetten, dann das Innere *|
  else {
    /* Umrandung ermitteln (einzige Flaeche mit mehr als 6 Ecken, Flaeche mit
       2-valenten Randknoten):    (siehe "schreibe_zyklus2) *|
    knoten = 1;
    k = 0;
    ki = 0;
    while (k < 7 && knoten <= g[index - 1]) {
      /* noch nichts gefunden, es stehen aber noch Knoten zur Verfuegung *|
      if (entries[knoten] == 2) {
	a = i = knoten;
	b = j = g[first[i] + ki];
	/* (a,b) ist Anfangskante, wobei rechts die betrachtete Flaeche ist *|
	k = 2;			/* Groesse des Kreises *|
	while (j != knoten) {	/* j ist der aktuelle Knoten, i der Vorgaenger *|
	  ii = 0;
	  i2 = -1;
	  while (ii < entries[j] && i2 == -1) {
	    if (g[first[j] + ii] == i) {
	      i2 = (ii + entries[j] - 1) % entries[j];
	    }
	    ii++;
	  }
	  if (i2 == -1) {
	    schluss (140);
	  }
	  i = j;
	  j = g[first[j] + i2];
	  if (j != knoten) {
	    k++;
	  }
	}
	ki++;
	if (ki > 2) {
	  ki = 0;
	  knoten++;
	}
      } else {
	knoten++;
      }
    }

    /* Nun ist die Aussenflaeche mit Anfangskante (a,b) gefunden. *|
    /* Umrandung einbetten: *|
    i = a;
    j = b;
    coords[(a - 1) * 3] = coords[(a - 1) * 3 + 1] = coords[(a - 1) * 3 + 2] = 0;
    coords[(b - 1) * 3] = coords[(b - 1) * 3 + 2] = 0;
    coords[(b - 1) * 3 + 1] = -1;
    richtung = 3;		/* Richtung der Einbettung *|
    set[a] = set[b] = True;
    while (j != a) {		/* j ist der aktuelle Knoten, i der Vorgaenger *|
      ii = 0;
      i2 = -1;
      while (ii < entries[j] && i2 == -1) {
	if (g[first[j] + ii] == i) {
	  i2 = (ii + entries[j] - 1) % entries[j];
	}
	ii++;
      }
      if (i2 == -1) {
	schluss (141);
      }
      i = j;
      j = g[first[j] + i2];
      if (j != a) {
	if (entries[i] == 2) {
	  richtung = (richtung + 5) % 6;
	} else {
	  richtung = (richtung + 1) % 6;
	}
	switch (richtung) {
	  case 0:{
	      coords[(j - 1) * 3] = coords[(i - 1) * 3];
	      coords[(j - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] + 1;
	      break;
	    }
	  case 1:{
	      coords[(j - 1) * 3] = coords[(i - 1) * 3] + cospi6;
	      coords[(j - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] + 0.5;
	      break;
	    }
	  case 2:{
	      coords[(j - 1) * 3] = coords[(i - 1) * 3] + cospi6;
	      coords[(j - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] - 0.5;
	      break;
	    }
	  case 3:{
	      coords[(j - 1) * 3] = coords[(i - 1) * 3];
	      coords[(j - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] - 1;
	      break;
	    }
	  case 4:{
	      coords[(j - 1) * 3] = coords[(i - 1) * 3] - cospi6;
	      coords[(j - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] - 0.5;
	      break;
	    }
	  case 5:{
	      coords[(j - 1) * 3] = coords[(i - 1) * 3] - cospi6;
	      coords[(j - 1) * 3 + 1] = coords[(i - 1) * 3 + 1] + 0.5;
	      break;
	    }
	}
	coords[(j - 1) * 3 + 2] = 0;
	set[j] = True;
      }
    }

    /* Nun ist die Umrandung eingebettet. Von JEDEM 3--valenten Randknoten aus
       muss nun eine Rekursion gestartet werden (da evtl. ein Randknoten zu
       drei Randknoten adjazent ist). Also Rand nochmals durchlaufen: *|
    i = a;
    j = b;
    while (j != a) {		/* j ist der aktuelle Knoten, i der Vorgaenger *|
      if (entries[j] == 3) {
	bette_knoten_ein_rek (j, g, coords, set, first, entries);
      }
      ii = 0;
      i2 = -1;
      while (ii < entries[j] && i2 == -1) {
	if (g[first[j] + ii] == i) {
	  i2 = (ii + entries[j] - 1) % entries[j];
	}
	ii++;
      }
      if (i2 == -1) {
	schluss (142);
      }
      i = j;
      j = g[first[j] + i2];
    }
  }
}

   --- end of obsolete section --- */


/******************TESTE_EINBETTUNG******************************************/
/*  Diese Funktion testet, ob eine Einbettung Koordinaten ungleich 0 aufweist
   (also ob es eine brauchbare Einbettung ist). "dim" gibt an, ob die
   Einbettung zwei- oder dreidimensional ist.                              */

BOOL teste_einbettung (int dim, int n, double *coords)
{
  int i;
  for (i = 0; i < n * dim; i++) {
    if (coords[i] != 0) {
      return (True);
    }
  }
  return (False);
}


/********************************INIT****************************************/
/*  Initialisiert globale Arrays                                            */

void init (void)
{
  int i;
  for (i = 0; i < MAXN * 3; i++) {
    newcoords[i] = 0.0;
  }
}


/********************DATEN_SIND_DA***********************************/
/*  Diese Funktion wird vom FileHandler aufgerufen, wenn an der
   Input-Pipe Daten anliegen. Die Funktion wird benutzt, um das
   Programm beim Warten auf Daten nicht zu blockieren.             */

void daten_sind_da (ClientData fd, int mask)
{
  /*tk */
  Tcl_CreateFileHandler ((int) fd, 0, nil, 0);
  filehandler = False;
  status |= GRAPH_IST_DA;
  if (absolutehighnr > 0) {
    event_loop ();
  }
  /* kein event-loop beim Warten auf den ersten Graphen */
}


/********************DATEN_SIND_DA2**********************************/
/*  Diese Funktion wird vom FileHandler aufgerufen, wenn an der
   Spring-Pipe Daten anliegen. Die Funktion wird benutzt, um das
   Programm beim Warten auf Daten nicht zu blockieren.             */

void daten_sind_da2 (ClientData fd, int mask)
{
  Tcl_DeleteFileHandler ((int) fd);	/* FileHandler nicht mehr noetig, 
					   denn ab jetzt gibt es keine Wartezeiten mehr */
  status |= SPRING_IST_DA;
  event_loop ();
}


/********************DATEN_SIND_DA3**********************************/
/*  Diese Funktion wird vom FileHandler aufgerufen, wenn an der
   Schlegel-Pipe Daten anliegen. Die Funktion wird benutzt, um das
   Programm beim Warten auf Daten nicht zu blockieren.             */

void daten_sind_da3 (ClientData fd, int mask)
{
  Tcl_DeleteFileHandler ((int) fd);	/* FileHandler nicht mehr noetig, 
					   denn ab jetzt gibt es keine Wartezeiten mehr */
  status |= SCHLEGEL_IST_DA;
  event_loop ();
}


/********************GET_CHILD_PID**********************************/
/* Die Funktion sucht nach Prozessen, die von CaGe ueber Pipe-Option
   gestartet wurden, und gibt deren pid zurueck.
   Zunaechst werden ps Befehle aufgerufen um einen geeigenten zu finden,
   der PID und PPID enthaelt. (Diese sind auf den unterschiedlichen UNIX
   Systemen nicht einheitlich.) Dabei werden die Positionen von
   PID und PPID (als Feldnummer) im eingelesenen String ermittel (pidno,pidno).
   Es wird nur nach dem Programm, nicht nach den Args. gesucht :
   .   CPF stdout pid n 16 s 4 f 4 f 9 f 5 con 3
   =>  CPF 

   Bsp.   ps -j -w # -U 1903 auf Linux : 
   PPID   PID  PGID   SID  TT TPGID STAT   UID   TIME COMMAND
   20948 20954 20954 20954  p4 22001 S     1903   0:00 rlogin dali 
   1 20966 20965 20954  p4 22001 S     1903   0:00  -bash 
   =>ppidno = 0, pidno = 1, commandno >= 10 .

   Achtung es kommt zu Problemen, wenn die Felder nicht eingehalten werden :
   .                                            !!!! 
   USER       PID %CPU %MEM   VSZ  RSS TTY      S    STARTED         TIME 
   kuenzer  31728 56.7  2.7 28.6M 6.8M ??       R N  17:52:11     3:49.10 .. 
   kuenzer  32493 37.9  6.0 21.7M  15M ttyp2    R  + 17:48:47     1:13.83 ..
   .                                            ~~~~ 
 */

int get_child_pid (char *child_call)
{
  FILE *file;
  char command[LINELEN];
  char c[LINELEN];
  char dummy[LINELEN];
  char child_search[LINELEN];

  pid_t father, child;
  uid_t user_id;

  int all_killed, last_child, ind_ex;

  int p_ppid, p_pid;
  char p_name[LINELEN];

  int begin, end, i, clength, try;
  int pidno, ppidno, commandno, no, wordno = 0;

  for (i = 0; child_call[i] != ' ' && child_call[i] != '\0'; i++);
  strncpy (child_search, child_call, ++i);

  child_search[--i] = '\0';

  user_id = getuid ();
  father = getpid ();
  /* Testen der ps Befehle */

  for (try = 0; try < 3; try++) {
    switch (try) {
      case 0:{
	  sprintf (command, "ps -j -w # -U %d", user_id);
	  break;		/* Linux */
	}
      case 1:{
	  sprintf (command, "ps -u %d -f", user_id);
	  break;		/* OSF1 */
	}
      case 2:{
	  sprintf (command, "ps -f -u %d", user_id);
	  break;		/* IRIX */
	}
    }
    /*    printf ("*PPID* %s\n", command); */
    pidno = ppidno = commandno = -1;
    file = popen (command, "r");
    if (fgets (&c[0], LINELEN, file) != NULL) {
      i = 0;
      while (c[i] != '\0' && c[i] != '\n') {
	for (; c[i] == ' ' && c[i] != '\0' && c[i] != '\n'; i++);
	begin = i;
	wordno++;
	for (; c[i] != ' ' && c[i] != '\0' && c[i] != '\n'; i++);
	end = i;

	if (!strncmp (&c[begin], "PPID", end - begin)) {
	  ppidno = wordno;
	}
	if (!strncmp (&c[begin], "PID", end - begin)) {
	  pidno = wordno;
	}
      }
      commandno = wordno;
    }
    if (pidno != -1 && ppidno != -1) {
      break;
    } else {
      pclose (file);
    }
  }				/* Test ende */
  if (try < 3) {
    while (!feof (file)) {
      if (fgets (&c[0], LINELEN, file) != NULL) {
	/*printf("*PPID* %s\n",c); */
	if (strstr (c, child_search) != NULL) {
	  i = no = 0;
	  clength = strlen (c);

	  while (c[i] != '\0' && c[i] != '\n') {
	    for (; c[i] == ' ' && c[i] != '\0' && c[i] != '\n'; i++);
	    begin = i;
	    no++;
	    for (; c[i] != ' ' && c[i] != '\0' && c[i] != '\n'; i++);
	    end = i;

	    if (no == ppidno) {
	      strncpy (dummy, &c[begin], end - begin);
	      dummy[end - begin] = '\0';
	      p_ppid = atoi (dummy);
	    }
	    if (no == pidno) {
	      strncpy (dummy, &c[begin], end - begin);
	      dummy[end - begin] = '\0';
	      p_pid = atoi (dummy);
	    }
	    if (no == commandno) {
	      strncpy (&p_name[0], &c[begin], clength - begin - 1);
	      p_name[clength - begin - 1] = '\0';
	    }
	  }
	  return (p_ppid);
	  /*printf("*PPID* PID %d PPID %d CALL %s\n",p_pid, p_ppid,p_name); */
	}
      }
    }
    pclose (file);
  }
  return (0);
}



/***************BEENDET_PROGRAMME_DER_INPUTPIPE_BEI_CANCEL********************/
/* Die Funktion sucht nach Prozessen, die von CaGe ueber Pipe-Option
   gestartet wurden, und verschickt kill Befehle.
   Zunaechst werden ps Befehle aufgerufen um einen geeigenten zu finden,
   der PID und PPID enthaelt. (Diese sind auf den unterschiedlichen UNIX
   Systemen nicht einheitlich.) Dabei werden die Positionen von
   PID und PPID (als Feldnummer) im eingelesenen String ermittel (pidno,pidno).

   Bsp.   ps -j -w # -U 1903 auf Linux : 
   PPID   PID  PGID   SID  TT TPGID STAT   UID   TIME COMMAND
   20948 20954 20954 20954  p4 22001 S     1903   0:00 rlogin dali 
   1 20966 20965 20954  p4 22001 S     1903   0:00  -bash 
   =>ppidno = 0, pidno = 1, commandno >= 10 .

   Achtung es kommt zu Problemen, wenn die Felder nicht eingehalten werden :
   .                                            !!!! 
   USER       PID %CPU %MEM   VSZ  RSS TTY      S    STARTED         TIME 
   kuenzer  31728 56.7  2.7 28.6M 6.8M ??       R N  17:52:11     3:49.10 .. 
   kuenzer  32493 37.9  6.0 21.7M  15M ttyp2    R  + 17:48:47     1:13.83 ..
   .                                            ~~~~ 
 */

int kill_child (ClientData clientdata, Tcl_Interp * interp, int argc, char *argv[])
{
  FILE *file;
  char command[LINELEN];
  char c[LINELEN];
  char dummy[LINELEN];
  char child_to_kill[LINELEN];

  pid_t father, child;
  uid_t user_id;

  int all_killed, last_child, ind_ex;

  int begin, end, i, clength, try;
  int pidno, ppidno, commandno, no, wordno = 0;

  int count = 0;
  int **process_id;
  char **process_name;

  if (argc == 0) {
    interp->result = "wrong # args...";
    return TCL_ERROR;
  }
  strcpy (child_to_kill, argv[1]);

  user_id = getuid ();
  /*  father = getpid(); */
  father = 25905;
  /* Testen der ps Befehle */

  for (try = 0; try < 3; try++) {
    switch (try) {
      case 0:{
	  sprintf (command, "ps -j -w # -U %d", user_id);
	  break;		/* Linux */
	}
      case 1:{
	  sprintf (command, "ps -u %d -f", user_id);
	  break;		/* OSF1 */
	}
      case 2:{
	  sprintf (command, "ps -f -u %d", user_id);
	  break;		/* IRIX */
	}
    }
    printf ("%s\n", command);
    pidno = ppidno = commandno = -1;
    file = popen (command, "r");
    if (fgets (&c[0], LINELEN, file) != NULL) {
      i = 0;
      while (c[i] != '\0' && c[i] != '\n') {
	for (; c[i] == ' ' && c[i] != '\0' && c[i] != '\n'; i++);
	begin = i;
	wordno++;
	for (; c[i] != ' ' && c[i] != '\0' && c[i] != '\n'; i++);
	end = i;

	if (!strncmp (&c[begin], "PPID", end - begin)) {
	  ppidno = wordno;
	}
	if (!strncmp (&c[begin], "PID", end - begin)) {
	  pidno = wordno;
	}
      }
      commandno = wordno;
    }
    if (pidno != -1 && ppidno != -1) {
      break;
    } else {
      pclose (file);
    }
  }				/* Test ende */
  if (try < 3) {
    while (!feof (file)) {
      if (fgets (&c[0], LINELEN, file) != NULL) {
	i = no = 0;
	clength = strlen (c);

	process_id = (int **) realloc (process_id, \
				       (count + 1) * sizeof (int *));
	process_id[count] = (int *) malloc (2 * sizeof (int));
	process_name = (char **) realloc (process_name, \
					  (count + 1) * sizeof (char *));
	process_name[count] = (char *) malloc (LINELEN * sizeof (char));

	while (c[i] != '\0' && c[i] != '\n') {
	  for (; c[i] == ' ' && c[i] != '\0' && c[i] != '\n'; i++);
	  begin = i;
	  no++;
	  for (; c[i] != ' ' && c[i] != '\0' && c[i] != '\n'; i++);
	  end = i;

	  if (no == ppidno) {
	    strncpy (dummy, &c[begin], end - begin);
	    dummy[end - begin] = '\0';
	    process_id[count][0] = atoi (dummy);
	  }
	  if (no == pidno) {
	    strncpy (dummy, &c[begin], end - begin);
	    dummy[end - begin] = '\0';
	    process_id[count][1] = atoi (dummy);
	  }
	  if (no == commandno) {
	    strncpy (&process_name[count][0], &c[begin], \
		     clength - begin - 1);
	    process_name[count][clength - begin - 1] = '\0';
	  }
	}
	count++;
      }
    }
    pclose (file);
    child = 0;
    for (i = 0; i < count; i++) {
      if (process_id[i][0] == father && \
	  (strstr (&process_name[i][0], "sh") != NULL)) {
	if (strstr (&process_name[i][0], child_to_kill) != NULL) {
	  father = process_id[i][1];
	  child = father;
	}
      }
    }
    /* es existiert eine shell */
    if (child) {
      all_killed = 0;
      last_child = 0;
      while (!all_killed) {
	while (!last_child) {
	  last_child = 1;
	  for (i = 0; i < count; i++) {
	    if (process_id[i][0] == child) {
	      child = process_id[i][1];
	      last_child = 0;
	      ind_ex = i;
	    }
	  }
	}
	if (child == father) {
	  all_killed = 1;
	} else {
	  kill (child, SIGKILL);
	  printf ("killed :%s", &process_name[ind_ex][0]);
	  process_id[ind_ex][0] = process_id[ind_ex][1] = 0;
	  child = father;
	  last_child = 0;
	}
      }
      kill (father, SIGKILL);
    }
    /* es existiert keine shell */
    if (!child) {
      for (i = 0; i < count; i++) {
	if (process_id[i][0] == father && \
	    (strstr (&process_name[i][0], child_to_kill) != NULL)) {
	  kill (process_id[i][1], SIGKILL);
	  printf ("killed :%s\n", &process_name[i][0]);
	  child = father;
	}
      }
    }
    if (!child) {
      printf ("nothing to kill\n");
    }
    for (i = 0; i < count; i++) {
      free (process_name[i]);
      free (process_id[i]);
    }
  } else {
    printf ("No ps-command found\n");
  }
  return TCL_OK;
}

/***************MINIMUM_H********************/
/* Die Funktion entspricht min_rand */

int minimum_h (ClientData clientdata, Tcl_Interp * interp, int argc, char *argv[])
{
  int level, faces, cur_c;
  int rand_level, h;
  int inner_faces, spiral_rand;

  /*spiral_rand = Randlaenge eines mit dem Spiral-Alogo. erzeugeten Graphen
     rand_level = Randlaenge des letzten vollen Levels
     inner_faces = Anzahl der Flaechen, aller vollen Level

     /* ursprungskonfiguration der 5-Ecke, bzw eines 6-Eckes: level 0 */
  int c, pentagons;		/* Argumente */

  if (argc != 3) {
    interp->result = "wrong # args...";
    return TCL_ERROR;
  }
  errno = 0;
  if (!(c = atoi (argv[1])) && errno) {
    interp->result = "wrong # arg 1...";
    return TCL_ERROR;
  }
  errno = 0;
  if (!(pentagons = atoi (argv[2])) && errno) {
    interp->result = "wrong # arg 2...";
    return TCL_ERROR;
  }
  switch (pentagons) {
    case 0:{
	cur_c = 6;
	/* level ist das erste nicht volle level */
	for (level = faces = 1; cur_c + 6 + 12 * (level) <= c; level++) {
	  faces += (6 * level);
	  cur_c += 6 + 12 * (level);
	}
	if (cur_c < c) {
	  rand_level = 6 + 12 * (level - 1);
	  inner_faces = faces;

	  for (; rand_level + 2 * ((faces - inner_faces) / level) + 2 <
	       (2 * (c - 2 * faces + 2) - 6); faces++) {
	  }
	  if (rand_level + 2 * ((faces - inner_faces) / level) + 2 >
	      (2 * (c - 2 * faces + 2) - 6)) {
	    faces--;
	  }
	}
	h = c - 2 * faces + 2;
	if (!(c >= 6 && h >= 6)) {
	  h = 0;
	}
	sprintf (interp->result, "%d", h);
	return TCL_OK;
      }
    case 1:{
	cur_c = 5;
	/* level ist das erste nicht volle level */
	for (level = faces = 1; cur_c + 5 + 10 * (level) <= c; level++) {
	  faces += (5 * level);
	  cur_c += 5 + 10 * (level);
	}
	if (cur_c < c) {
	  rand_level = 5 + 10 * (level - 1);
	  inner_faces = faces;
	  for (; rand_level + 2 * ((faces - inner_faces) / level) + 2 <
	       (2 * (c - 2 * faces + 2) - 6 + 1); faces++) {
	  }
	  if (rand_level + 2 * ((faces - inner_faces) / level) + 2 >
	      (2 * (c - 2 * faces + 2) - 6 + 1)) {
	    faces--;
	  }
	}
	h = c - 2 * faces + 2;
	if (!(c >= 5 && h >= 5)) {
	  h = 0;
	}
	sprintf (interp->result, "%d", h);
	return TCL_OK;
      }
    case 2:{
	cur_c = 8;
	/* level ist das erste nicht volle level */
	for (level = 1, faces = 2; cur_c + 8 + 8 * (level) <= c; level++) {
	  faces += (4 * level + 2);
	  cur_c += 8 + 8 * (level);
	}
	if (cur_c < c) {
	  spiral_rand = 8 + 8 * (level - 1);
	  if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level) + 2) - 6 + 3) {
	    spiral_rand += 2;
	    faces += level;
	    if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level) + 2) - 6 + 3) {
	      spiral_rand += 2;
	      faces += level;
	      if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level + 1) + 2) - 6 + 3) {
		spiral_rand += 2;
		faces += level + 1;
		if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level) + 2) - 6 + 3) {
		  spiral_rand += 2;
		  faces += level;
		}
	      }
	    }
	  }
	  for (; spiral_rand + 2 <= 2 * (c - 2 * (faces + 1) + 2) - 6 + 3; faces++) {
	  }
	}
	h = c - 2 * faces + 2;
	if (!(c >= 8 && h >= 6)) {
	  h = 0;
	}
	sprintf (interp->result, "%d", h);
	return TCL_OK;
      }
    case 3:{
	cur_c = 10;
	/* level ist das erste nicht volle level */
	for (level = 1, faces = 3; cur_c + 9 + 6 * (level) <= c; level++) {
	  faces += (3 * level + 3);
	  cur_c += 9 + 6 * (level);
	}
	if (cur_c < c) {
	  spiral_rand = 9 + 6 * (level - 1);
	  if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level) + 2) - 6 + 3) {
	    spiral_rand += 2;
	    faces += level;
	    if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level + 1) + 2) - 6 + 3) {
	      spiral_rand += 2;
	      faces += level + 1;
	      if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level + 1) + 2) - 6 + 3) {
		spiral_rand += 2;
		faces += level + 1;
	      }
	    }
	  }
	  for (; spiral_rand + 2 <= 2 * (c - 2 * (faces + 1) + 2) - 6 + 3; faces++) {
	  }
	}
	h = c - 2 * faces + 2;
	if (!(c >= 10 && h >= 6)) {
	  h = 0;
	}
	sprintf (interp->result, "%d", h);
	return TCL_OK;
      }
    case 4:{
	cur_c = 12;
	/* level ist das erste nicht volle level */
	for (level = 1, faces = 4; cur_c + 10 + 4 * (level) <= c; level++) {
	  faces += (2 * level + 4);
	  cur_c += 10 + 4 * (level);
	}

	if (cur_c < c) {
	  spiral_rand = 10 + 4 * (level - 1);
	  if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level + 1) + 2) - 6 + 4) {
	    spiral_rand += 2;
	    faces += level + 1;
	    if (spiral_rand + 2 <= 2 * (c - 2 * (faces + level + 2) + 2) - 6 + 4) {
	      spiral_rand += 2;
	      faces += level + 2;
	    }
	  }
	  for (; spiral_rand + 2 <= 2 * (c - 2 * (faces + 1) + 2) - 6 + 4; faces++) {
	  }
	}
	h = c - 2 * faces + 2;
	if (!(c >= 12 && h >= 6)) {
	  h = 0;
	}
	sprintf (interp->result, "%d", h);
	return TCL_OK;
      }
    case 5:{			/* Hier wird ein 6-Eck mit in die Anfangskonfiguration hineingenommen */
	if (c == 14) {
	  h = 6;
	  sprintf (interp->result, "%d", h);
	  return TCL_OK;
	}
	if (c == 15) {
	  h = 7;
	  sprintf (interp->result, "%d", h);
	  return TCL_OK;
	}
	cur_c = 16;
	/* level ist das erste nicht volle level */
	for (level = 1, faces = 6; cur_c + 11 + 2 * (level) <= c; level++) {
	  faces += (level + 5);
	  cur_c += 11 + 2 * (level);
	}

	if (cur_c < c) {
	  spiral_rand = 11 + 2 * (level - 1);
	  for (; spiral_rand + 2 <= 2 * (c - 2 * (faces + 1) + 2) - 6 + 6; faces++) {
	  }
	}
	h = c - 2 * faces + 2;
	if (!(c >= 14 && h >= 6)) {
	  h = 0;
	}
	sprintf (interp->result, "%d", h);
	return TCL_OK;
      }
    default:{
	fprintf (stderr, "This program is designed for only up to 5 pentagons\n");
	interp->result = "-1";
	return TCL_OK;
      }
  }
}

/***************MAXIMUM_C********************/
/* Die Funktion entspricht min_rand */

int maximum_c (ClientData clientdata, Tcl_Interp * interp, int argc, char *argv[])
{
  int level, faces, rand, cur_rand, c;	/*rand=randlaenge */
  int inner_faces;		/*faces der vollen level */
  /* ursprungskonfiguration der 5-Ecke, bzw eines 6-Eckes: level 0 */
  int h, pentagons;		/* Argumente */

  if (argc != 3) {
    interp->result = "wrong # args...";
    return TCL_ERROR;
  }
  errno = 0;
  if (!(h = atoi (argv[1])) && errno) {
    interp->result = "wrong # arg 1...";
    return TCL_ERROR;
  }
  errno = 0;
  if (!(pentagons = atoi (argv[2])) && errno) {
    interp->result = "wrong # arg 2...";
    return TCL_ERROR;
  }
  rand = 2 * h - 6 + pentagons;
  cur_rand = 6;

  switch (pentagons) {
    case 0:{			/* level ist das erste nicht volle level */
	for (level = faces = 1; 6 + 12 * (level) <= rand; level++) {
	  faces += (6 * level);
	  cur_rand = 6 + 12 * (level);
	}
	inner_faces = faces;
	if (cur_rand < rand) {
	  for (; cur_rand + 2 * ((faces - inner_faces) / level) + 2 <= rand; faces++) {
	  }
	  faces--;		/*wg for test */
	}
	c = 2 * (faces) + h - 2;
	if (!(c >= 6 && h >= 6)) {
	  c = 0;
	}
	sprintf (interp->result, "%d", c);
	return TCL_OK;
      }
    case 1:{			/* level ist das erste nicht volle level */
	for (level = faces = 1; 5 + 10 * (level) <= rand; level++) {
	  faces += (5 * level);
	  cur_rand = 5 + 10 * (level);
	}
	inner_faces = faces;

	if (cur_rand < rand) {
	  for (; cur_rand + 2 * ((faces - inner_faces) / level) + 2 <= rand; faces++) {
	  }
	  if (level > 1) {
	    faces--;		/*wg for test */
	  }
	}
	c = 2 * (faces) + h - 2;
	if (!(c >= 5 && h >= 5)) {
	  c = 0;
	}
	sprintf (interp->result, "%d", c);
	return TCL_OK;
      }
    case 2:{			/* level ist das erste nicht volle level */
	for (level = 1, faces = 2; 8 + 8 * (level) <= rand; level++) {
	  faces += (4 * level + 2);
	  cur_rand = 8 + 8 * (level);
	}
	if (level == 1) {	/*das erste level ist scheisse */
	  faces--;
	  if (rand == 12) {
	    faces--;
	  }
	}
	if (cur_rand + 2 <= rand) {
	  cur_rand += 2;
	  faces += level;	/* der erste verlaengert den rand um 2, die (level-1) danach
				   verlaengern ihn nicht */
	  if (cur_rand + 2 <= rand) {
	    cur_rand += 2;
	    faces += level;	/* der erste verlaengert den rand um 2, die (level-1) danach
				   verlaengern ihn nicht */
	    if (cur_rand + 2 <= rand) {
	      cur_rand += 2;
	      faces += level + 1;	/* der erste verlaengert den rand um 2, die level (!!) 
					   danach verlaengern ihn nicht */
	      if (cur_rand + 2 <= rand) {
		cur_rand += 2;
		faces += level;
	      }
	    }
	  }
	}
	c = 2 * (faces) + h - 2;
	if (!(c >= 8 && h >= 6)) {
	  c = 0;
	}
	sprintf (interp->result, "%d", c);
	return TCL_OK;
      }
    case 3:{			/* level ist das erste nicht volle level */
	for (level = 1, faces = 3; 9 + 6 * (level) <= rand; level++) {
	  faces += (3 * level + 3);
	  cur_rand = 9 + 6 * (level);
	}
	if (level == 1) {	/*das erste level ist scheisse */
	  faces--;
	  if (rand == 11) {
	    faces--;
	  }
	}
	if (cur_rand + 2 <= rand) {
	  cur_rand += 2;
	  faces += level;	/* der erste verlaengert den rand um 2, die (level-1) danach
				   verlaengern ihn nicht */
	  if (cur_rand + 2 <= rand) {
	    cur_rand += 2;
	    faces += level + 1;	/* der erste verlaengert den rand um 2, die (level) danach
				   verlaengern ihn nicht */
	    if (cur_rand + 2 <= rand) {
	      cur_rand += 2;
	      faces += level;	/* der erste verlaengert den rand um 2, die (level-1) 
				   danach verlaengern ihn nicht */
	    }
	  }
	}
	c = 2 * (faces) + h - 2;
	if (!(c >= 10 && h >= 6)) {
	  c = 0;
	}
	sprintf (interp->result, "%d", c);
	return TCL_OK;
      }
    case 4:{			/* level ist das erste nicht volle level */
	for (level = 1, faces = 4; 10 + 4 * (level) <= rand; level++) {
	  faces += (2 * level + 4);
	  cur_rand = 10 + 4 * (level);
	}
	if (level == 1) {	/*das erste level ist scheisse */
	  faces--;
	  if (rand == 10) {
	    faces -= 2;
	  }
	}
	if (cur_rand + 2 <= rand) {
	  cur_rand += 2;
	  faces += level + 1;	/* der erste verlaengert den rand um 2, die (level) danach
				   verlaengern ihn nicht */
	  if (cur_rand + 2 <= rand) {
	    cur_rand += 2;
	    faces += level;	/* der erste verlaengert den rand um 2, die (level-1) danach
				   verlaengern ihn nicht */
	  }
	}
	c = 2 * (faces) + h - 2;
	if (!(c >= 12 && h >= 6)) {
	  c = 0;
	}
	sprintf (interp->result, "%d", c);
	return TCL_OK;
      }
    case 5:{			/* Hier wird ein 6-Eck mit in die Anfangskonfiguration hineingenommen */
	if (h == 7) {
	  c = 29;
	  sprintf (interp->result, "%d", c);
	  return TCL_OK;
	}
	if (h == 6) {
	  c = 16;
	  sprintf (interp->result, "%d", c);
	  return TCL_OK;
	}
	/* level ist das erste nicht volle level */
	for (level = 1, faces = 6; 11 + 2 * (level) <= rand; level++) {
	  faces += (level + 5);
	  cur_rand = 11 + 2 * (level);
	}
	if (level == 1) {	/*das erste level ist scheisse */
	  faces--;
	}
	if (cur_rand + 2 <= rand) {
	  cur_rand += 2;
	  faces += level;	/* der erste verlaengert den rand um 2, die (level) danach
				   verlaengern ihn nicht */
	}
	c = 2 * (faces) + h - 2;
	if (!(c >= 14 && h >= 6)) {
	  c = 0;
	}
	sprintf (interp->result, "%d", c);
	return TCL_OK;
      }
    default:{
	fprintf (stderr, "This program is designed for only up to 5 pentagons\n");
	interp->result = "-1";
	return TCL_OK;
      }
  }
}

/***************FUER_HCGEN_SUCHT_MINIMALEN_RAND********************/
/* Die Funktion ist aus HCgen kopiert und wird als Tcl Befehl implementiert */

/* int min_rand(int hexagons, int pentagons) */
/* berechnet den minimal moeglichen Rand bei vorgegebener 5-Eck und 6-Eck-Zahl
   unter der Voraussetzung, dass der Spiralalgorithmus, startend mit den 
   5-Eckeneinen Patch mit minimaler Randlaenge erzeugt */

/* VORSICHT NOCH NICHT BEWIESEN !!!!!! */  


int min_rand (ClientData clientdata, Tcl_Interp * interp, int argc, char *argv[])
{
  int level, faces, gesamtfaces, randlaenge;
  /* ursprungskonfiguration der 5-Ecke, bzw eines 6-Eckes: level 0 */
  int hexagons, pentagons;	/* Argumente */

  if (argc != 3) {
    interp->result = "wrong # args...";
    return TCL_ERROR;
  }
  errno = 0;
  if (!(hexagons = atoi (argv[1])) && errno) {
    interp->result = "wrong # arg 1...";
    return TCL_ERROR;
  }
  errno = 0;
  if (!(pentagons = atoi (argv[2])) && errno) {
    interp->result = "wrong # arg 2...";
    return TCL_ERROR;
  }
  gesamtfaces = hexagons + pentagons;

  switch (pentagons) {
    case 0:{			/* level ist das erste nicht volle level */
	for (level = faces = 1; faces + (6 * level) <= gesamtfaces; level++)
	  faces += (6 * level);
	randlaenge = 6 + 12 * (level - 1);
	faces = gesamtfaces - faces;	/* der rest */
	if (faces)
	  randlaenge += 2 * (faces / level) + 2;
	sprintf (interp->result, "%d", randlaenge);
	return TCL_OK;
      }
    case 1:{			/* level ist das erste nicht volle level */
	for (level = faces = 1; faces + (5 * level) <= gesamtfaces; level++)
	  faces += (5 * level);
	randlaenge = 5 + 10 * (level - 1);
	faces = gesamtfaces - faces;	/* der rest */
	if (faces)
	  randlaenge += 2 * (faces / level) + 2;
	sprintf (interp->result, "%d", randlaenge);
	return TCL_OK;
      }
    case 2:{			/* level ist das erste nicht volle level */
	for (level = 1, faces = 2; faces + 2 + (4 * level) <= gesamtfaces; level++)
	  faces += (4 * level + 2);
	randlaenge = 8 + 8 * (level - 1);
	faces = gesamtfaces - faces;	/* der rest */
	if (faces) {
	  randlaenge += 2;
	  faces -= level;	/* der erste verlaengert den rand um 2, die 
				   (level-1) danach verlaengern ihn nicht */
	  if (faces > 0) {
	    randlaenge += 2;
	    faces -= level;	/* der erste verlaengert den rand um 2, 
				   die (level-1) danach verlaengern ihn nicht */
	    if (faces > 0) {
	      randlaenge += 2;
	      faces -= (level + 1);	/* der erste verlaengert den 
					   rand um 2, die level (!!) danach verlaengern ihn nicht */
	      if (faces > 0)
		randlaenge += 2;
	    }
	  }
	}
	sprintf (interp->result, "%d", randlaenge);
	return TCL_OK;
      }
    case 3:{			/* level ist das erste nicht volle level */
	for (level = 1, faces = 3; faces + 3 + (3 * level) <= gesamtfaces; level++)
	  faces += (3 * level + 3);
	randlaenge = 9 + 6 * (level - 1);
	faces = gesamtfaces - faces;	/* der rest */
	if (faces) {
	  randlaenge += 2;
	  faces -= level;	/* der erste verlaengert den rand um 2, die
				   (level-1) danach verlaengern ihn nicht */
	  if (faces > 0) {
	    randlaenge += 2;
	    faces -= (level + 1);	/* der erste verlaengert den rand 
					   um 2, die level (!!) danach verlaengern ihn nicht */
	    if (faces > 0)
	      randlaenge += 2;
	  }
	}
	sprintf (interp->result, "%d", randlaenge);
	return TCL_OK;
      }
    case 4:{			/* level ist das erste nicht volle level */
	for (level = 1, faces = 4; faces + 4 + (2 * level) <= gesamtfaces; level++)
	  faces += (2 * level + 4);
	randlaenge = 10 + 4 * (level - 1);
	faces = gesamtfaces - faces;	/* der rest */
	if (faces) {
	  randlaenge += 2;
	  faces -= (level + 1);	/* der erste verlaengert den rand um 
				   2, die level (!!) danach verlaengern ihn nicht */
	  if (faces > 0)
	    randlaenge += 2;
	}
	sprintf (interp->result, "%d", randlaenge);
	return TCL_OK;
      }
    case 5:{			/* Hier wird ein 6-Eck mit in die Anfangskonfiguration 
				   hineingenommen */
	if (hexagons == 0) {
	  interp->result = "11";
	  return TCL_OK;
	}
	/* level ist das erste nicht volle level */
	for (level = 1, faces = 6; faces + 5 + level <= gesamtfaces; level++)
	  faces += (level + 5);
	randlaenge = 11 + 2 * (level - 1);
	faces = gesamtfaces - faces;	/* der rest */
	if (faces)
	  randlaenge += 2;
	sprintf (interp->result, "%d", randlaenge);
	return TCL_OK;
      }
    default:{
	fprintf (stderr, "This program is designed for only up to 5 pentagons\n");
	interp->result = "-1";
	return TCL_OK;
      }
      /* Wobei der returnwert 12 fuer 6 natuerlich leicht ist ... */
  }
}

/********************EVENT_LOOP**********************************/
/*  Diese Funktion wird immer dann aufgerufen, wenn ein Event
   verarbeitet werden soll.                                    */

void event_loop (void)
{
  static char Tcl_Skript_DrawGraph[] = "drawGraph\n";
  static char c[LINELEN], d[LINELEN];	/* Input oder sonstige Strings */
  static signed int i, j;
  static char erg;
  static FILE *f;
  static char *var;
  static BOOL set;		/* True => innere Flaeche bei Schlegeldiagramm gewaehlt */
  static BOOL action;		/* True => ein Event wurde verarbeitet => noch ein
				   Schleifendurchlauf wegen moeglicher Statusaenderungen */
  static int dummy;
  /* vs */

  static int last_graph;
  static char Tcl_Command_last_graph[] = ".aufruf.ffwd configure -text \" \" \n"
  ".aufruf.ffwd configure -command { }\n"
  ".aufruf.fwd configure -text \"End\"";
  static char Tcl_Command_last_ten[] = ".aufruf.ffwd configure -text \" \" \n"
  ".aufruf.ffwd configure -command { }";
  static char Tcl_Command_last_end[] = ".aufruf.fwd configure -text \"End\"";

  last_graph = 0;		/* wird immer wieder mit null ueberschrieben */


  /* vs ende */
  do {
    while (Tcl_DoOneEvent (TK_DONT_WAIT) != 0) {
    }				/* alle Events verarbeiten */
    if (global_end == 1) {
      schluss (0);
    }
    if (global_background == 1) {
      go_to_background ();
      go_to_background2 (outnr);
    }
    action = False;

    /* 1. Wurde Skip-Wert veraendert? */
    if (!ende && skip_old != skip) {
      /* fprintf(stderr,"1 "); */
      action = True;
      skip_old = skip;
      if (skip) {
	status |= SKIPMODUS | STOP_SCHLEGEL | STOP_SPRING;
      } else {
	if (tkview || rasmol) {
	  spr_ebnr = sl_ebnr = outnr = highnr + 1;
	  status |= SCHREIBE_OUTNR;
	}
	status &= ~SKIPMODUS;
      }
    }
    /* 15. Wurde save_s2d-Wert veraendert? */
    if (!ende && save_s2d_old != save_s2d) {
      /* fprintf(stderr,"15 "); */
      action = True;
      save_s2d_old = save_s2d;
      if (save_s2d && curr_g && curr_g->schlegel_coords) {
	status |= SAVE_2D;
      }
    }
    /* 16. Wurde save_s3d-Wert veraendert? */
    if (!ende && save_s3d_old != save_s3d) {
      /* fprintf(stderr,"16 "); */
      action = True;
      save_s3d_old = save_s3d;
      if (save_s3d && curr_g && curr_g->rasmol_coords) {
	status |= SAVE_3D;
      }
    }
    /* 16.b Wurde save_spdb-Wert veraendert? */
    if (!ende && save_spdb_old != save_spdb) {
      /* fprintf(stderr,"16b "); */
      action = True;
      save_spdb_old = save_spdb;
      if (save_spdb && curr_g && curr_g->rasmol_coords) {
	status |= SAVE_PDB;
      }
    }
    /* Test : */
    /* {int i;
       for (i=0; i<=ANZ_GRAPH; i++) {
       if (graphstate[i]!=LEER && graph[i][0]==0) 
       {fprintf(stderr,"Alarm 6 %d\n",i);}
       }
       } */

    /* 2. Wurde outnr-Wert veraendert? */
    if (!ende && outnr_old != outnr && !(file_2d || file_3d)) {
      /* neuer outnr-Wert */
      /* fprintf(stderr,"2 (%d %d) ",outnr_old,outnr); */
      action = True;
      if (outnr_old > outnr) {
	outnr = outnr_old;	/* zurueck geht nicht */
	status |= SCHREIBE_OUTNR;
      } else {
	outnr_old = outnr;
	status |= SCHREIBE_OUTNR;
	status &= ~REVIEWMODUS;
	if (outnr <= highnr) {	/* Graph ist bereits geladen */
	  i = 0;
	  while (graphnr[i] != outnr) {
	    i++;
	  }
	  if (rasmol && !(status & SKIPMODUS)) {
	    if (!(graphstate[i] & SPRING_EINGEBETTET)) {
	      if (spr_ebindex != i) {
		status |= (STOP_SPRING | STOP_SCHLEGEL | BETTE_SPRING_EIN);
		spr_ebnr = outnr;
	      }
	    } else {
	      status |= SPRING_IN_LISTE | GIB_SPRING_AUS;

/* Fuer den letzten Graphen mit graphnr[spr_in_liste_idx] */
	      if (outnr == wp_last_graph && i == 0) {
		spr_in_liste_idx = 10;
	      } else {
		spr_in_liste_idx = i;
	      }
	      if (spr_ebindex != -1 && graphnr[spr_ebindex] < outnr) {
		status |= STOP_SPRING;
	      }
	    }
	  }
	  if (tkview && !(status & SKIPMODUS)) {
	    if (!(graphstate[i] & SCHLEGEL_EINGEBETTET)) {
	      if (sl_ebindex != i) {
		status |= (STOP_SPRING | STOP_SCHLEGEL | BETTE_SCHLEGEL_EIN);
		sl_ebnr = outnr;
	      }
	    } else {
	      status |= SCHLEGEL_IN_LISTE | GIB_SCHLEGEL_AUS;
	      sl_in_liste_idx = i;
	      if (sl_ebindex != -1 && graphnr[sl_ebindex] < outnr) {
		status |= STOP_SCHLEGEL;
	      }
	    }
	  }
	} else {
	  status |= STOP_SPRING | STOP_SCHLEGEL;
	  spr_ebnr = sl_ebnr = outnr;
	  if (status & SOFTENDE) {
	    ende = True;
	  }			/*vs muss weg */
	}
      }
    }
    /* {int i;
       for (i=0; i<=ANZ_GRAPH; i++) {
       if (graphstate[i]!=LEER && graph[i][0]==0) 
       {fprintf(stderr,"Alarm 5 %d\n",i);}
       }
       } */

    /* 3. Wurde Schlegel-Diagramm angeklickt? */
    if (!ende && newschlegel) {
      /* fprintf(stderr,"3 "); */
      action = True;
      status |= (NEWSCHLEGEL | STOP_SPRING | STOP_SCHLEGEL);
      newschlegel = False;
    }
    /* 4. Soll Einbettung abgebrochen werden? */
    if (status & STOP_SPRING) {
      action = True;
      /* fprintf(stderr,"4a "); */
      if (status & SPRING_IST_AN) {
	Tcl_DeleteFileHandler ((int) (fileno (spring)));
	kill (spring_id, SIGKILL);
	spring_id = 0;
	pclose (spring);
	spring = nil;
	status &= ~(SPRING_IST_AN | SPRING_IST_DA);
	graphstate[spr_ebindex] &= ~SPRING_IN_ARBEIT;
      }
      status &= ~STOP_SPRING;
      spr_ebindex = -1;
    }
    if (status & STOP_SCHLEGEL) {
      action = True;
      /* fprintf(stderr,"4b "); */
      if (status & SCHLEGEL_IST_AN) {
	Tcl_DeleteFileHandler ((int) (fileno (schlegel)));
	kill (schlegel_id, SIGKILL);
	schlegel_id = 0;
	pclose (schlegel);
	schlegel = nil;
	status &= ~(SCHLEGEL_IST_AN | SCHLEGEL_IST_DA);
	graphstate[sl_ebindex] &= ~SCHLEGEL_IN_ARBEIT;
      }
      status &= ~STOP_SCHLEGEL;
      sl_ebindex = -1;
    }
    /* 5. Wurde Review-Modus angewaehlt? */
    if (reviewstep != 0) {	/* review-Buttons gedrueckt */
      action = True;
      /* fprintf(stderr,"5 (%d) ",reviewstep); */
      if (curr_g) {		/* sonst keine Bedeutung */
	if (!(status & REVIEWMODUS)) {
	  status |= REVIEWMODUS;
	}
	while (reviewstep < 0) {	/* zurueck */
	  if (curr_g->prev) {
	    curr_g = curr_g->prev;
	    if (!(status & SKIPMODUS)) {
	      status |= GIB_SPRING_AUS | GIB_SCHLEGEL_AUS;
	    }
	  }
	  reviewstep++;
	}
	while (reviewstep > 0) {	/* weiter */
	  if (curr_g->next) {
	    curr_g = curr_g->next;
	    if (!(status & SKIPMODUS)) {
	      status |= GIB_SPRING_AUS | GIB_SCHLEGEL_AUS;
	    }
	  }
	  reviewstep--;
	}
      } else {
	reviewstep = 0;
      }
    }
    /* 6. Soll neuer Graph eingelesen werden? */
    if ((outnr > highnr || spr_ebnr > highnr || sl_ebnr > highnr ||
	 (status & SKIPMODUS)) && !(status & SOFTENDE)) {
      status |= LIES_GRAPH;
    }
    /*   {int i;
       for (i=0; i<=ANZ_GRAPH; i++) {
       if (graphstate[i]!=LEER && graph[i][0]==0) 
       {fprintf(stderr,"Alarm 4 %d\n",i);}
       }
       } */


    /* 7. Kann neuer Graph geladen werden?  */
    if (!ende && !(status & SOFTENDE) && (status & LIES_GRAPH) &&
	(status & GRAPH_IST_DA)) {
      /* fprintf(stderr,"7 (%d)",highnr); */
      /* Arrayplatz j fuer neuen Graphen zuweisen */
      j = 0;
      if (!(file_2d || file_3d || status & SKIPMODUS)) {
	while (graphnr[j] >= outnr || (graphstate[j] & SCHLEGEL_IN_ARBEIT)) {
	  j++;
	}
	/* 2.Teil der Bedingung wegen NEWSCHLEGEL-Option */
      }
      if (j < ANZ_GRAPH) {	/* Platz fuer neuen Graphen ist frei */
	action = True;
	graphnr[j] = highnr + 1;
	graphstate[j] = LEER;

	/* Graph j einlesen */
	if (inputcode < 2) {	/* planar_code oder planar_code_old */
	  erg = read_planar_code_s (inputfile, MAXN, &endian_in,
				  programm == DREGGEN ? highnr : 1, graph[j],
				MAXENTRIES, FALSE, &graphnv[j], &graphne[j]);
	} else {
	  erg = read_writegraph2d (inputfile, inputcode == 2 ? schlegel_coords[j] :
	      rasmol_coords[j], MAXN, inputcode, False, graph[j], MAXENTRIES,
				   &graphnv[j], &graphne[j]);
	  if (old_embed && !new_embed) {	/* Einbettung uebernehmen */
	    if (teste_einbettung ((int) inputcode, graphnv[j], inputcode == 2 ?
				  schlegel_coords[j] : rasmol_coords[j])) {
	      /* alte Einbettung vorhanden */
	      if (inputcode == 2) {
		graphstate[j] |= SCHLEGEL_EINGEBETTET;
	      } else {
		graphstate[j] |= SPRING_EINGEBETTET;
	      }
	    }
	  }
	}
	switch (erg) {
	  case 0:{
	      status |= SOFTENDE;
	      inputfile_id = 0;
	      inputfile = nil;
/* vs */
	      graphnr[j]--;	/* graphnr[j] == eof,
				   graphnr[j]-1 letzter Graph */
	      last_graph = graphnr[j];
	      if (status & SKIPMODUS) {
		status &= ~(SKIPMODUS);
		/*status|=GIB_SPRING_AUS; */
		/*status|=SPRING_IN_LISTE; */
		status |= BETTE_SPRING_EIN;
		spr_ebnr = last_graph;
		spr_in_liste_idx = j;
		last_skip = 1;
		outnr = last_graph;
		wp_last_graph = outnr;
	      }
	      if (outnr > last_graph) {
		if (Tcl_Eval (interp, Tcl_Command_last_graph) == TCL_ERROR) {
		  schluss (1200);
		}
		outnr = last_graph;
		outnr_old = outnr;
		sprintf (c, "%d", last_graph);
		if (Tcl_SetVar (interp, "graphnr", c,
				TCL_LEAVE_ERR_MSG) == NULL) {
		  schluss (1000);
		}
		if (inputcode == 2) {
		  status |= SPRING_IN_LISTE;
		  spr_in_liste_idx = j;
		} else {
		  status |= SCHLEGEL_IN_LISTE;
		  sl_in_liste_idx = j;
		}
	      } else {
		if (outnr == last_graph) {
		  if (Tcl_Eval (interp, Tcl_Command_last_graph) == TCL_ERROR) {
		    schluss (1200);
		  }
		  Tcl_last_graph = last_graph;
		  wp_last_graph = last_graph;
		} else {
		  if (Tcl_Eval (interp, Tcl_Command_last_ten) == TCL_ERROR) {
		    schluss (1201);
		  }
		  Tcl_last_graph = last_graph;	/* last_graph wird immer wieder mit Null
						   ueberschrieben */
		  wp_last_graph = last_graph;
		}
	      }
	      /* weniger als 10 Atome darum : */
	      sprintf (c, "set dummy 0;.aufruf.fffwd configure -variable dummy -command {set outnr %d}", last_graph);
	      if (Tcl_Eval (interp, c) == TCL_ERROR) {
		schluss (1205);
	      }
/*vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv */
	      if (no_output == 1) {
		no_output = 0;
		ende = TRUE;
	      }
/* end vs */
	      break;
	    }
	  case 1:{
	      highnr++;
	      if (highnr > absolutehighnr) {
		if (raw) {
		  if (ascfile) {
		    if (write_writegraph2d (raw, nil, graph[j][0] ? graph[j][0]
			  : graph[j][1], 2, False, absolutehighnr == 0, True,
					    graph[j]) != 1) {
		      schluss (90);
		    }
		  } else {
		    if (write_planar_code_s (raw, graph[j], nil, (unsigned long)
		     absolutehighnr, absolutehighnr == 0, MAXENTRIES) != 1) {
		      schluss (28);
		    }
		  }
		}
		if (outpipe) {
		  if (ascpipe) {
		    if (write_writegraph2d (outpipe, nil, graph[j][0] ?
		    graph[j][0] : graph[j][1], 2, False, absolutehighnr == 0,
					    True, graph[j]) != 1) {
		      schluss (91);
		    }
		  } else {
		    if (write_planar_code_s (outpipe, graph[j], nil, (unsigned
								      long) absolutehighnr, absolutehighnr == 0, MAXENTRIES) != 1) {
		      schluss (72);
		    }
		  }
		}
		if (file_2d) {
		  sl_ebnr = highnr;
		}
		if (file_3d) {
		  spr_ebnr = highnr;
		}
		absolutehighnr++;
	      }
	      graphstate[j] |= ROH;
	      break;
	    }
	  default:
	    schluss (29);
	}
	if (((status & SKIPMODUS) && (tkview || rasmol) && highnr % 200 == 0) ||
	    ((file_2d || file_3d) && !(tkview || rasmol))) {
	  sprintf (c, "%d", highnr);
	  if (Tcl_SetVar (interp, "graphnr", c,
			  TCL_LEAVE_ERR_MSG) == NULL) {
	    schluss (67);
	  }
	}
	status &= ~GRAPH_IST_DA;
	if (status & SOFTENDE) {	/* neue Einbettung festlegen */
	  if (spr_ebnr > highnr) {
	    spr_ebindex = -1;
	  }
	  if (sl_ebnr > highnr) {
	    sl_ebindex = -1;
	  }
	  if (outnr > highnr || file_2d || file_3d || (status & SKIPMODUS)) {
	    ende = True;
	  }
	} else if (!(status & SKIPMODUS)) {
	  if (highnr == spr_ebnr && (rasmol || file_3d)) {
	    status |= BETTE_SPRING_EIN;
	  }
	  if (highnr == sl_ebnr && (tkview || file_2d)) {
	    status |= BETTE_SCHLEGEL_EIN;
	  }
	}
      }
      status &= ~LIES_GRAPH;
    }				/* 7. */
    /*  {int i;
       for (i=0; i<=ANZ_GRAPH; i++) {
       if (graphstate[i]!=LEER && graph[i][0]==0) 
       {fprintf(stderr,"Alarm 3 %d\n",i);}
       }
       } */
    /* 10. Muss der einzubettende Graph selbst gewaehlt werden?
       -  wenn ebindex-Graph bereits eingebettet ist
       -  wenn ebindex == -1 
       -  UND wenn BETTE_EIN nicht gesetzt ist                         */
    /* Wenn BETTE_EIN-Flag gesetzt ist, dann steht "ebnr" fest         */
    if (!ende && rasmol && !(status & SKIPMODUS) && !(status & BETTE_SPRING_EIN)
	&& (!(file_2d || file_3d))
	&& (spr_ebindex == -1 || (graphstate[spr_ebindex] & SPRING_EINGEBETTET))) {
      spr_ebnr = 0;
      do {
	if (status & SOFTENDE) {
	  if (spr_ebnr < outnr) {
	    spr_ebnr = outnr;
	  } else {
	    spr_ebnr++;
	  }
	} else {
	  if (spr_ebnr < outnr) {
	    spr_ebnr = outnr;
	  } else if (spr_ebnr == outnr) {
	    spr_ebnr = outnr + 1;
	  } else if (spr_ebnr == outnr + 1) {
	    spr_ebnr = outnr + 10;
	  } else if (spr_ebnr == outnr + 10) {
	    spr_ebnr = outnr + 2;
	  } else if (spr_ebnr < outnr + 9) {
	    spr_ebnr++;
	  } else {
	    spr_ebnr = highnr + 1;
	  }			/* nicht einbetten */
	  /* Reihenfolge der Einbettungen */
	}
	if (spr_ebnr <= highnr) {
	  i = 0;
	  while (i < ANZ_GRAPH && graphnr[i] != spr_ebnr) {
	    i++;
	  }
	} else {
	  i = ANZ_GRAPH;
	  if (status & SOFTENDE) {
	    spr_ebnr = outnr;
	  }
	}
      } while (i < ANZ_GRAPH && graphstate[i] & SPRING_EINGEBETTET);
      if (i < ANZ_GRAPH) {	/* einzubettender Graph ist geladen */
	/* fprintf(stderr,"10 %d ",spr_ebnr); */
	status |= BETTE_SPRING_EIN;
	action = True;
      } else {
	spr_ebindex = -1;
      }
    }
    if (!ende && tkview && !(status & SKIPMODUS) && !(status & BETTE_SCHLEGEL_EIN)
	&& (!(file_2d || file_3d)) && !(status & NEWSCHLEGEL) &&
     (sl_ebindex == -1 || (graphstate[sl_ebindex] & SCHLEGEL_EINGEBETTET))) {
      sl_ebnr = 0;
      do {
	if (status & SOFTENDE) {
	  if (sl_ebnr < outnr) {
	    sl_ebnr = outnr;
	  } else {
	    sl_ebnr++;
	  }
	} else {
	  if (sl_ebnr < outnr) {
	    sl_ebnr = outnr;
	  } else if (sl_ebnr == outnr) {
	    sl_ebnr = outnr + 1;
	  } else if (sl_ebnr == outnr + 1) {
	    sl_ebnr = outnr + 10;
	  } else if (sl_ebnr == outnr + 10) {
	    sl_ebnr = outnr + 2;
	  } else if (sl_ebnr < outnr + 9) {
	    sl_ebnr++;
	  } else {
	    sl_ebnr = highnr + 1;
	  }			/* nicht einbetten */
	  /* Reihenfolge der Einbettungen */
	}
	if (sl_ebnr <= highnr) {
	  i = 0;
	  while (i < ANZ_GRAPH && graphnr[i] != sl_ebnr) {
	    i++;
	  }
	} else {
	  i = ANZ_GRAPH;
	  if (status & SOFTENDE) {
	    sl_ebnr = outnr;
	  }
	}
      } while (i < ANZ_GRAPH && graphstate[i] & SCHLEGEL_EINGEBETTET);
      if (i < ANZ_GRAPH) {	/* einzubettender Graph ist geladen */
	/* fprintf(stderr,"10b %d ",sl_ebnr); */
	status |= BETTE_SCHLEGEL_EIN;
	action = True;
      } else {
	sl_ebindex = -1;
      }
    }
    /*  {int i;
       for (i=0; i<=ANZ_GRAPH; i++) {
       if (graphstate[i]!=LEER && graph[i][0]==0) 
       {fprintf(stderr,"Alarm 2 %d\n",i);}
       }
       } */

    /*  9. Ist der einzubettende Graph bereits geladen?  */
    /*  BETTE_EIN-Flag gesetzt => Graph geladen und "ebnr" steht fest */
    if (!ende && (status & BETTE_SPRING_EIN)) {
      /* ebindex festlegen */
      action = True;
      /* fprintf(stderr,"9a (%d)  ",spr_ebnr); */
      i = 0;
      while (i < ANZ_GRAPH && graphnr[i] != spr_ebnr) {
	i++;
      }
      if (i < ANZ_GRAPH) {
	spr_ebindex = i;
	if (!(graphstate[spr_ebindex] & SPRING_EINGEBETTET) ||
	    (programm == INPUTFILE && new_embed)) {
	  n = graphnv[spr_ebindex];
	  if (True || programm != HCGEN || p_3 != 0) {
	    if ((f = fopen ((char *) "CaGe1.tmp", "w")) == nil) {
	      schluss (76);
	    }
	    if (write_writegraph2d (f, (programm != INPUTFILE || !old_embed ||
			   !(graphstate[spr_ebindex] & SPRING_EINGEBETTET)) ?
		newcoords : rasmol_coords[spr_ebindex], graphnv[spr_ebindex],
			    3, FALSE, TRUE, TRUE, graph[spr_ebindex]) != 1) {
	      schluss (30);
	    }
	    fclose (f);
	  }
	  switch (programm) {
	    case FULLGEN:
	      spring = popen("embed -d3 -z < CaGe1.tmp", "r");
	      break;
	    case DREGGEN:
	      spring = popen("embed -d3 -it -z < CaGe1.tmp", "r");
	      break;
	    case HCGEN:
	      spring = popen("embed -d3 -f1,1,4 -z < CaGe1.tmp", "r");
	      break;
	    case TUBETYPE:
	      spring = popen("embed -d3 -it -z < CaGe1.tmp", "r");
	      break;
	    case INPUTFILE:
	      if (old_embed && (graphstate[spr_ebindex] & SPRING_EINGEBETTET))
		sprintf(c, "embed -d3 -ik -f%lf,%lf,%lf -z < CaGe1.tmp",
			emb_a * 0.01, emb_b * 0.01, emb_c * 0.01);
	      else
		sprintf(c, "embed -d3 -f%lf,%lf,%lf -z < CaGe1.tmp",
			emb_a * 0.01, emb_b * 0.01, emb_c * 0.01);
	      spring = popen (c, "r");
	      break;
	  }
	  fgets (c, LINELEN, spring);
	  spring_id = atoi (c);
	  status |= SPRING_IST_AN;

	  Tcl_CreateFileHandler ((int) (fileno (spring)), TK_READABLE, daten_sind_da2,
				 (ClientData) fileno (spring));
	  graphstate[spr_ebindex] |= SPRING_IN_ARBEIT;
	  /* SPRING_IST_AN und !SPRING_IST_DA => Einbettung in Arbeit */
	} else {
	  if (spr_ebnr <= outnr && !file_3d && wp_last_graph != outnr) {
	    status |= SPRING_IN_LISTE | GIB_SPRING_AUS;
	    spr_in_liste_idx = spr_ebindex;
	  }
	  if (file_3d && !pdb_file) {	/* Einbettung speichern */
	    if (write_writegraph2d (file_3d, rasmol_coords[spr_ebindex],
			      graph[spr_ebindex][0] ? graph[spr_ebindex][0] :
	    graph[spr_ebindex][1], 3, False, graphnr[spr_ebindex] == 1, True,
				    graph[spr_ebindex]) != 1) {
	      schluss (180);
	    }
	  }
	  if (file_3d && pdb_file) {
	    if (write_brookhaven_pdb (file_3d, graph[spr_ebindex],
				      rasmol_coords[spr_ebindex], graph[spr_ebindex][0] ? graph[spr_ebindex][0] :
				      graph[spr_ebindex][1]) != 1) {
	      schluss (42);
	    }
	  }
	}
      }
      status &= ~BETTE_SPRING_EIN;
    }
    if (!ende && (status & BETTE_SCHLEGEL_EIN)) {
      /* ebindex festlegen */
      /* fprintf(stderr,"9b (%d) ",sl_ebnr); */
      action = True;
      i = 0;
      while (i < ANZ_GRAPH && graphnr[i] != sl_ebnr) {
	i++;
      }
      if (i < ANZ_GRAPH) {
	sl_ebindex = i;
	if (!(graphstate[i] & SCHLEGEL_EINGEBETTET) ||
	    (programm == INPUTFILE && new_embed)) {
	  if ((f = fopen ((char *) "CaGe2.tmp", "w")) == nil) {
	    schluss (77);
	  }
	  if (write_writegraph2d (f, (programm != INPUTFILE || !old_embed ||
	      !(graphstate[sl_ebindex] & SCHLEGEL_EINGEBETTET)) ? newcoords :
		  schlegel_coords[sl_ebindex], graphnv[sl_ebindex], 2, FALSE,
				  FALSE, TRUE, graph[sl_ebindex]) != 1) {
	    schluss (31);
	  }
	  switch (programm) {
/* --- obsoleted by new embedder - "embed" finds initial cycle itself
	    case FULLGEN:{
		schreibe_zyklus2 (f, graph[sl_ebindex], 6);
		break;
	      }
	    case DREGGEN:{
		schreibe_zyklus2 (f, graph[sl_ebindex], bigface_2);
		break;
	      }
	    case HCGEN:{
		schreibe_zyklus2 (f, graph[sl_ebindex], 7);	/* 7=> Rand aussen *|
		break;
	      }
	    case TUBETYPE:{
		schreibe_zyklus2 (f, graph[sl_ebindex], 7);	/* 7 => Rand aussen *|
		break;
	      }
   --- who knows if this is to be reactived some time */
	    case INPUTFILE:{

		/*  {int i;
		   for (i=0; i<=ANZ_GRAPH; i++) {
		   if (graphstate[i]!=LEER && graph[i][0]==0) 
		   {fprintf(stderr,"Alarm 1 %d\n",i);}
		   }
		   } */
/* --- obsoleted by new embedder - "embed" finds initial cycle itself
		schreibe_zyklus2 (f, graph[sl_ebindex], 7);
		/* 7 => irgendwas Grosses => wahrscheinlich Rand *|
   --- who knows if this is to be reactived some time */
		break;
	      }
	  }
	  fclose (f);
	  schlegel = popen("embed -z < CaGe2.tmp", "r");
	  fgets (c, LINELEN, schlegel);
	  schlegel_id = atoi (c);
	  status |= SCHLEGEL_IST_AN;
	  Tcl_CreateFileHandler ((int) (fileno (schlegel)), TK_READABLE, daten_sind_da3,
				 (ClientData) fileno (schlegel));
	  graphstate[sl_ebindex] |= SCHLEGEL_IN_ARBEIT;
	  /* SCHLEGEL_IST_AN und !SCHLEGEL_IST_DA => Einbettung in Arbeit */
	} else {
	  if (sl_ebnr <= outnr && !file_2d) {
	    status |= SCHLEGEL_IN_LISTE | GIB_SCHLEGEL_AUS;
	    sl_in_liste_idx = sl_ebindex;
	  }
	  /* diese Bedingung ist insbesondere bei NEWSCHLEGEL erfuellt */
	  if (file_2d) {	/* Einbettung speichern */
	    if (write_writegraph2d (file_2d, schlegel_coords[sl_ebindex],
				graph[sl_ebindex][0] ? graph[sl_ebindex][0] :
	      graph[sl_ebindex][1], 2, False, graphnr[sl_ebindex] == 1, True,
				    graph[sl_ebindex]) != 1) {
	      schluss (179);
	    }
	  }
	}
      }
      status &= ~(BETTE_SCHLEGEL_EIN | NEWSCHLEGEL);	/* gegen Seiteneffekte */
    }
    if (!ende && (status & NEWSCHLEGEL)) {
      /* ebindex festlegen */
      /* fprintf(stderr,"9c (%d %d) ",curr_g,curr_g->graphstate); */
      action = True;
      if (curr_g && (curr_g->graphstate & SCHLEGEL_EINGEBETTET)) {
	/* Einbettung ist vorhanden */
	sl_ebnr = curr_g->nr;
	i = 0;
	while (i < ANZ_GRAPH && graphnr[i] != sl_ebnr) {
	  i++;
	}
	if (i < ANZ_GRAPH) {
	  sl_ebindex = i;
	} else {
	  sl_ebindex = ANZ_GRAPH;	/* Review-Element */
	  graphnr[sl_ebindex] = sl_ebnr;
	  n = graphnv[sl_ebindex] = curr_g->nv;
	  graphne[sl_ebindex] = curr_g->ne;
	  memcpy (graph[sl_ebindex], curr_g->planarcode, sizeof (unsigned short) *
		    (size_t) (n + curr_g->ne + 2));
	  memcpy (schlegel_coords[sl_ebindex], curr_g->schlegel_coords,
		  sizeof (double) * (size_t) n * 2);
	  if (curr_g->rasmol_coords) {
	    memcpy (rasmol_coords[sl_ebindex], curr_g->rasmol_coords,
		    sizeof (double) * (size_t) n * 3);
	  }
	}
	graphstate[sl_ebindex] = curr_g->graphstate & ~SCHLEGEL_EINGEBETTET;
	/* SCHLEGEL_EINGEBETTET nicht bei "curr_g" loeschen, damit bei 
	   Abbruch alte Einbettung wieder genommen wird */
	if ((f = fopen ((char *) "CaGe2.tmp", "w")) == nil) {
	  schluss (130);
	}
	if (write_writegraph2d (f, newcoords, graphnv[sl_ebindex], 2, FALSE, FALSE,
				TRUE, graph[sl_ebindex]) != 1) {
	  schluss (129);
	}
	fclose (f);

	{ int i, j; char t[LINELEN];
	set = kante_des_gewaehlten_zyklus (graph[sl_ebindex],
					   schlegel_coords[sl_ebindex],
					   &i, &j);
	if (set) {
	  sprintf(t, "embed -b%d,%d -z < CaGe2.tmp", i, j);
	  schlegel = popen(t, "r");
	  fgets (c, LINELEN, schlegel);
	  schlegel_id = atoi (c);
	  status |= SCHLEGEL_IST_AN;
	  Tcl_CreateFileHandler ((int) (fileno (schlegel)), TK_READABLE, daten_sind_da3,
				 (ClientData) fileno (schlegel));
	  graphstate[sl_ebindex] |= SCHLEGEL_IN_ARBEIT;
	}
	}
	/* SCHLEGEL_IST_AN und !SCHLEGEL_IST_DA => Einbettung in Arbeit */
      }
      status &= ~(NEWSCHLEGEL | BETTE_SCHLEGEL_EIN);
    }
    /*  8. Ist eine Einbettung (von "ebindex" und "ebno") fertig?  */
    if (!ende && (status & SPRING_IST_DA)) {
      action = True;
      /* fprintf(stderr,"8a "); */
      if (graphstate[spr_ebindex] & SPRING_EINGEBETTET) {
	/* Timing-Problem:  Einbettung gehoert nicht zum aktuellen
	   Graphen => Graph lesen und vergessen */
	fprintf (stderr, "Timing-Problem\n");
	if (read_writegraph2d (spring, NULL, MAXN, 3, TRUE, newgraph,
			       MAXENTRIES, &dummy, &dummy) > 1) {
	  schluss (32);
	}
      } else {			/* Einbettung einlesen und ausgeben */
	/* fprintf(stderr,"ebindex %d\n",ebindex); */
	if (read_writegraph2d_old (spring, rasmol_coords[spr_ebindex], MAXN, 3,
				newgraph, MAXENTRIES, &dummy, &dummy) != 1) {
	  schluss (33);
	}
	/* in "newgraph" einlesen, da Einbettungsinformation 
	   verfaelscht ist */
	graphstate[spr_ebindex] |= SPRING_EINGEBETTET;
	graphstate[spr_ebindex] &= ~(SPRING_IN_ARBEIT);
	pclose (spring);
	spring = nil;
	status &= ~(SPRING_IST_AN | SPRING_IST_DA);
/* vs wp_last_graph */
	if (last_skip && rasmol != nil) {
	  status |= SPRING_IN_LISTE | GIB_SPRING_AUS;
	  spr_in_liste_idx = spr_ebindex;
	}
	if (spr_ebnr <= outnr && !file_3d && outnr != wp_last_graph) {
	  status |= SPRING_IN_LISTE | GIB_SPRING_AUS;
	  spr_in_liste_idx = spr_ebindex;
	}
	if (file_3d && !pdb_file) {	/* Einbettung speichern */
	  /*fprintf(stderr,"speichern %d %d ",file_3d,graph[spr_ebindex][0]); */
	  if (write_writegraph2d (file_3d, rasmol_coords[spr_ebindex],
			      graph[spr_ebindex][0] ? graph[spr_ebindex][0] :
	    graph[spr_ebindex][1], 3, False, graphnr[spr_ebindex] == 1, True,
				  graph[spr_ebindex]) != 1) {
	    schluss (180);
	  }
	}
	if (file_3d && pdb_file) {
	  if (write_brookhaven_pdb (file_3d, graph[spr_ebindex],
				    rasmol_coords[spr_ebindex], graph[spr_ebindex][0] ? graph[spr_ebindex][0] :
				    graph[spr_ebindex][1]) != 1) {
	    schluss (42);
	  }
	}
      }
    }
    if (!ende && (status & SCHLEGEL_IST_DA)) {
      action = True;
      /* fprintf(stderr,"8b "); */
      if (graphstate[sl_ebindex] & SCHLEGEL_EINGEBETTET) {
	/* Timing-Problem:  Einbettung gehoert nicht zum aktuellen
	   Graphen => Graph lesen und vergessen */
	fprintf (stderr, "Timing-Problem\n");
	if (read_writegraph2d (schlegel, NULL, MAXN, 2, TRUE, newgraph,
			       MAXENTRIES, &dummy, &dummy) > 1) {
	  schluss (36);
	}
      } else {			/* Einbettung einlesen und ausgeben */
	if (read_writegraph2d (schlegel, schlegel_coords[sl_ebindex],
		MAXN, 2, FALSE, newgraph, MAXENTRIES, &dummy, &dummy) != 1) {
	  schluss (37);
	}
	/* in "newgraph" einlesen, da Einbettungsinformation 
	   verfaelscht ist */
	graphstate[sl_ebindex] |= SCHLEGEL_EINGEBETTET;
	graphstate[sl_ebindex] &= ~(SCHLEGEL_IN_ARBEIT);
	pclose (schlegel);
	schlegel = nil;
	status &= ~(SCHLEGEL_IST_AN | SCHLEGEL_IST_DA);
	if (sl_ebnr <= outnr && !file_2d) {
	  status |= SCHLEGEL_IN_LISTE | GIB_SCHLEGEL_AUS;
	  sl_in_liste_idx = sl_ebindex;
	}
	/* diese Bedingung ist insbesondere bei NEWSCHLEGEL erfuellt */
	if (file_2d) {		/* Einbettung speichern */
	  if (write_writegraph2d (file_2d, schlegel_coords[sl_ebindex],
				graph[sl_ebindex][0] ? graph[sl_ebindex][0] :
	      graph[sl_ebindex][1], 2, False, graphnr[sl_ebindex] == 1, True,
				  graph[sl_ebindex]) != 1) {
	    schluss (161);
	  }
	}
      }
    }
    /* 11. Soll neuer Graph in Liste? (in_liste_index) */
    if (!ende && (status & SPRING_IN_LISTE)) {
      /* schauen, ob bereits Eintrag in Liste */
      action = True;
      /* fprintf(stderr,"11a (%d %d) ",spr_in_liste_idx,
         graphnr[spr_in_liste_idx]); */
      curr_g = last_g;
      i = graphnr[spr_in_liste_idx];	/* Nummer des neuen Graphen */
      while (curr_g && curr_g->nr > i) {
	curr_g = curr_g->prev;
      }
      while (curr_g && curr_g->next && curr_g->next->nr <= i) {
	curr_g = curr_g->next;
      }
      if (!(curr_g && curr_g->nr == i)) {	/* neues Listenelement */
	n = graphnv[spr_in_liste_idx];
	dummy_g = (LISTE *) hole_speicher (sizeof (LISTE));
	dummy_g->nr = i;
	dummy_g->nv = n;
	dummy_g->ne = graphne[spr_in_liste_idx];
	dummy_g->rasmol_coords = dummy_g->schlegel_coords = nil;
	dummy_g->planarcode = (unsigned short *)
	  hole_speicher (sizeof (unsigned short) * (size_t) (n + dummy_g->ne + 2));
	memcpy (dummy_g->planarcode, graph[spr_in_liste_idx],
		sizeof (unsigned short) * (size_t) (n + dummy_g->ne + 2));
	dummy_g->graphstate = graphstate[spr_in_liste_idx];
	/* einbinden: */
	if (curr_g) {		/* =>  curr_g->nr < ebnr */
	  dummy_g->next = curr_g->next;
	  dummy_g->prev = curr_g;
	  curr_g->next = dummy_g;
	  if (dummy_g->next) {
	    dummy_g->next->prev = dummy_g;
	  } else {
	    last_g = dummy_g;
	  }
	  curr_g = dummy_g;
	} else {
	  if (first_g) {	/* => ebnr < first_g->nr */
	    dummy_g->next = first_g;
	    dummy_g->prev = nil;
	    first_g->prev = dummy_g;
	    first_g = curr_g = dummy_g;
	  } else {		/* neuer Graph ist erster Graph */
	    first_g = last_g = curr_g = dummy_g;
	    dummy_g->next = dummy_g->prev = nil;
	  }
	}
      }
      if (!(curr_g->rasmol_coords)) {
	curr_g->rasmol_coords =
	  (double *) hole_speicher (sizeof (double) * (size_t) n * 3);
      }
      memcpy (curr_g->rasmol_coords, rasmol_coords[spr_in_liste_idx],
	      sizeof (double) * (size_t) n * 3);
      curr_g->graphstate |= SPRING_EINGEBETTET;
      curr_g->graphstate &= ~SPRING_IN_ARBEIT;
      status &= ~SPRING_IN_LISTE;
    }
    if (!ende && (status & SCHLEGEL_IN_LISTE)) {
      /* schauen, ob bereits Eintrag in Liste */
      action = True;
      /* fprintf(stderr,"11b (%d %d) ",sl_in_liste_idx,
         graphnr[sl_in_liste_idx]); */
      curr_g = last_g;
      i = graphnr[sl_in_liste_idx];	/* Nummer des neuen Graphen */
      while (curr_g && curr_g->nr > i) {
	curr_g = curr_g->prev;
      }
      while (curr_g && curr_g->next && curr_g->next->nr <= i) {
	curr_g = curr_g->next;
      }
      if (!(curr_g && curr_g->nr == i)) {	/* neues Listenelement */
	n = graphnv[sl_in_liste_idx];
	dummy_g = (LISTE *) hole_speicher (sizeof (LISTE));
	dummy_g->graphstate = graphstate[sl_in_liste_idx];
	dummy_g->nr = i;
	dummy_g->nv = n;
	dummy_g->ne = graphne[sl_in_liste_idx];
	dummy_g->rasmol_coords = dummy_g->schlegel_coords = nil;
	dummy_g->planarcode = (unsigned short *)
	  hole_speicher (sizeof (unsigned short) * (size_t) (n + dummy_g->ne + 2));
	memcpy (dummy_g->planarcode, graph[sl_in_liste_idx],
		sizeof (unsigned short) * (size_t) (n + dummy_g->ne + 2));
	/* einbinden: */
	if (curr_g) {		/* =>  curr_g->nr < ebnr */
	  dummy_g->next = curr_g->next;
	  dummy_g->prev = curr_g;
	  curr_g->next = dummy_g;
	  if (dummy_g->next) {
	    dummy_g->next->prev = dummy_g;
	  } else {
	    last_g = dummy_g;
	  }
	  curr_g = dummy_g;
	} else {
	  if (first_g) {	/* => ebnr < first_g->nr */
	    dummy_g->next = first_g;
	    dummy_g->prev = nil;
	    first_g->prev = dummy_g;
	    first_g = curr_g = dummy_g;
	  } else {		/* neuer Graph ist erster Graph */
	    first_g = last_g = curr_g = dummy_g;
	    dummy_g->next = dummy_g->prev = nil;
	  }
	}
      }
      if (!(curr_g->schlegel_coords)) {
	curr_g->schlegel_coords =
	  (double *) hole_speicher (sizeof (double) * (size_t) n * 2);
      }
      memcpy (curr_g->schlegel_coords, schlegel_coords[sl_in_liste_idx],
	      sizeof (double) * (size_t) n * 2);
      curr_g->graphstate |= SCHLEGEL_EINGEBETTET;
      curr_g->graphstate &= ~SCHLEGEL_IN_ARBEIT;
      status &= ~SCHLEGEL_IN_LISTE;
    }
    /* 12. Soll eingebetteter Graph ausgegeben werden? */
    if (!ende && (status & GIB_SPRING_AUS) && curr_g->rasmol_coords) {
      /* der Graph befindet sich an der Position "curr_g" in der Liste */
      action = True;
      /* fprintf(stderr,"12a "); */
      if ((Tcl_last_graph) && (curr_g->nr == Tcl_last_graph)) {
	if (Tcl_Eval (interp, Tcl_Command_last_end) == TCL_ERROR) {
	  schluss (1204);
	}
	Tcl_last_graph = 0;
      }
      you_see_nr1 = curr_g->nr;
      sprintf (c, "%d", you_see_nr1);
      current_vertex_number = curr_g->nv;
      if (Tcl_SetVar (interp, "graphnr3", c,
		      TCL_LEAVE_ERR_MSG) == NULL) {
	schluss (63);
      }
      current_vertex_number = curr_g->nv;	/* vs */
      sprintf (c, "%d", current_vertex_number);
      if (Tcl_SetVar (interp, "current_vertex_number", c,
		      TCL_LEAVE_ERR_MSG) == NULL) {
	schluss (1000);
      }
      if (d3torasmol) {
	if ((f = fopen (rasmol, "w")) == nil) {
	  schluss (41);
	}
	if (write_brookhaven_pdb (f, curr_g->planarcode,
				  curr_g->rasmol_coords, curr_g->nv) != 1) {
	  schluss (42);
	}
	fclose (f);
	if (Tcl_Eval (interp, Tcl_Skript_NachrichtAnRasmol) == TCL_ERROR) {
	  schluss (43);
	}
      } else {
	if (write_oogl (geompipe, curr_g->planarcode, curr_g->rasmol_coords,
			curr_g->nv, curr_g->ne / 2) != 1) {
	  schluss (98);
	}
      }
      status &= ~GIB_SPRING_AUS;
      if (save_s3d) {
	status |= SAVE_3D;
      }
      if (save_spdb) {
	status |= SAVE_PDB;
      }
    }
    if (!ende && (status & GIB_SCHLEGEL_AUS) && curr_g->schlegel_coords) {
      action = True;
      /* fprintf(stderr,"12b (%d) ",curr_g->nr); */
      if ((Tcl_last_graph) && (curr_g->nr == Tcl_last_graph)) {
	if (Tcl_Eval (interp, Tcl_Command_last_end) == TCL_ERROR) {
	  schluss (1204);
	}
	Tcl_last_graph = 0;
      }
      you_see_nr2 = curr_g->nr;
      sprintf (c, "%d", you_see_nr2);
      if (Tcl_SetVar (interp, "graphnr2", c,
		      TCL_LEAVE_ERR_MSG) == NULL) {
	schluss (124);
      }
      current_vertex_number = curr_g->nv;	/* vs */
      sprintf (c, "%d", current_vertex_number);
      if (Tcl_SetVar (interp, "current_vertex_number", c,
		      TCL_LEAVE_ERR_MSG) == NULL) {
	schluss (1001);
      }
      fuelle_tcl_graphvariablen (curr_g->planarcode,
				 curr_g->schlegel_coords);
      if (Tcl_Eval (interp, Tcl_Skript_DrawGraph) == TCL_ERROR) {
	schluss (44);
      }
      status &= ~(GIB_SCHLEGEL_AUS | NEWSCHLEGEL);	/* keine Fruehstarts */
      if (save_s2d) {
	status |= SAVE_2D;
      }
    }
    /* 13. Soll outnr ausgegeben werden? */
    if ((status & SCHREIBE_OUTNR) && !(status & SKIPMODUS)) {
      /* gesuchte Nummer ausgeben */
      /* fprintf(stderr,"13 "); */
      if (rasmol || tkview) {
	sprintf (c, "%d", outnr);
	if (Tcl_SetVar (interp, "graphnr", c,
			TCL_LEAVE_ERR_MSG) == NULL) {
	  schluss (65);
	}
      }
      status &= ~SCHREIBE_OUTNR;
    }
    /* 17. Soll 2D-Darstellung gespeichert werden? */
    if (!ende && (status & SAVE_2D)) {
      if (curr_g && curr_g->schlegel_coords) {
	/* fprintf(stderr,"17 "); */
	if (!file_s2d) {	/* File muss noch geoeffnet werden */
	  sprintf (c, "dateiname_s2d");
	  sprintf (d, "%d", programm);
	  if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	    schluss (154);
	  }
	  file_s2d = fopen (var, "w");
	  if (file_s2d == nil) {
	    schluss (155);
	  }
	  i = 1;
	} else {
	  i = 0;
	}
	if (write_writegraph2d (file_s2d, curr_g->schlegel_coords, curr_g->nv, 2,
			     False, i == 1, True, curr_g->planarcode) != 1) {
	  schluss (156);
	}
      }
      status &= ~SAVE_2D;
    }
    /* 18. Soll 3D-Darstellung gespeichert werden? */
    if (!ende && (status & SAVE_3D)) {
      if (curr_g && curr_g->rasmol_coords &&
	  !(curr_g->graphstate & SPRING_SAVED)) {
	/* Jede Einbettung hoechstens einmal speichern (nicht bei SCHLEGEL,
	   da mehrere Schlegel-Einbettungen moeglich) */
	if (!file_s3d) {	/* File muss noch geoeffnet werden */
	  sprintf (c, "dateiname_s3d");
	  sprintf (d, "%d", programm);
	  if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	    schluss (157);
	  }
	  file_s3d = fopen (var, "w");
	  if (file_s3d == nil) {
	    schluss (158);
	  }
	  i = 1;
	} else {
	  i = 0;
	}
	if (write_writegraph2d (file_s3d, curr_g->rasmol_coords, curr_g->nv, 3,
			     False, i == 1, True, curr_g->planarcode) != 1) {
	  schluss (159);
	}
	curr_g->graphstate |= SPRING_SAVED;
      }
      status &= ~SAVE_3D;
    }
    /* 18.b Soll PDB-Darstellung gespeichert werden? */
    if (!ende && (status & SAVE_PDB)) {
      if (curr_g && curr_g->rasmol_coords &&
	  !(curr_g->graphstate & SPRING_SAVED_PDB)) {
	/* Jede Einbettung hoechstens einmal speichern (nicht bei SCHLEGEL,
	   da mehrere Schlegel-Einbettungen moeglich) */
	if (!file_spdb) {	/* File muss noch geoeffnet werden */
	  sprintf (c, "dateiname_spdb");
	  sprintf (d, "%d", programm);
	  if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	    schluss (157);
	  }
	  file_spdb = fopen (var, "w");
	  if (file_spdb == nil) {
	    schluss (158);
	  }
	  i = 1;
	} else {
	  i = 0;
	}
	/*if (write_writegraph2d(file_spdb,curr_g->rasmol_coords,curr_g->nv,3,
	   False,i==1,True,curr_g->planarcode)!=1) {schluss(159);} */
	if (write_brookhaven_pdb (file_spdb, curr_g->planarcode,
				  curr_g->rasmol_coords, curr_g->nv) != 1) {
	  schluss (159);
	}
	curr_g->graphstate |= SPRING_SAVED_PDB;
      }
      status &= ~SAVE_PDB;
    }
    /* 19. Soll bei file_2d/file_3d naechster Graph geladen werden? */
    if (!ende && (file_2d || file_3d)) {
      if ((!file_2d || sl_ebindex == -1 ||
	   (graphstate[sl_ebindex] & SCHLEGEL_EINGEBETTET)) &&
	  (!file_3d || spr_ebindex == -1 ||
	   (graphstate[spr_ebindex] & SPRING_EINGEBETTET))) {
	status |= LIES_GRAPH;
      }
    }
    /* 14. Ist der Durchlauf beendet? */
    if ((status & SOFTENDE) && outnr > absolutehighnr) {
      ende = True;
    }				/*vs muss weg */
  } while (!ende && action);

  /* Wenn alle Loops verarbeitet sind, kann der Filehandler fuer die
     Inputpipe wieder aktiviert
     werden (vorher nicht, da sonst einige Loop-Events keine Gelegenheit 
     haben, bearbeitet zu werden). Desweiteren soll der FileHandler nur
     dann aktiviert werden, wenn auch tatsaechlich gelesen werden soll, 
     denn sonst rufen sich "daten_sind_da" und "event_loop" staendig
     gegenseitig auf. */
  if (!ende && !(status & SOFTENDE) && (status & LIES_GRAPH) &&
      filehandler == False) {
    Tcl_CreateFileHandler ((int) (fileno (inputfile)), TK_READABLE, daten_sind_da,
			   (ClientData) fileno (inputfile));
    filehandler = True;
  }
}


/******************FULLGEN_HANDLING************************************/
/* Diese Funktion stellt das Auswahlfenster fuer Programme, die wie
   "fullgen" arbeiten, zur Verfuegung und steuert den Programmablauf. */
/* "programm" ist die Nummer des aufzurufenden Programms.             */

void fullgen_handling (int programm)
{
  char c[LINELEN], d[LINELEN];	/* Input oder sonstige Strings */
  signed int i, j;
  char *aufrufkommando = nil;	/* fuer Inputpipe bzw. -file */
  char *var;			/* fuer Tcl-Variablen */
  static int start, cancel;	/* "static" ist wichtig, weil
				   die Adressen konstant bleiben muessen */
  char *file_or_pipe;		/* fuer input */

  char Tcl_Skript_ZeigeAuswahlfenster1[] = "wm deiconify .fulltop\n";
  char Tcl_Skript_LoescheAuswahlfenster1[] =
  "wm withdraw .fulltop; wm withdraw .fullcases; wm withdraw .fullsymm\n";
  char Tcl_Skript_EntferneAuswahlfenster1[] =
  "destroy .fulltop; destroy .fullcases; destroy .fullsymm\n";
  char Tcl_Skript_ZeigeAuswahlfenster2[] =
  "wm deiconify .r3top;  wm deiconify .r3faces\n";
  char Tcl_Skript_LoescheAuswahlfenster2[] =
  "wm withdraw .r3top; wm withdraw .r3cases; wm withdraw .r3faces\n";
  char Tcl_Skript_EntferneAuswahlfenster2[] =
  "destroy .r3top; destroy .r3cases; destroy .r3faces; destroy\n";
  char Tcl_Skript_ZeigeAuswahlfenster3[] = "wm deiconify .hctop\n";
  char Tcl_Skript_LoescheAuswahlfenster3[] = "wm withdraw .hctop;\n";
  char Tcl_Skript_EntferneAuswahlfenster3[] = "destroy .hctop;\n";
  char Tcl_Skript_ZeigeAuswahlfenster4[] = "wm deiconify .tubetop\n";
  char Tcl_Skript_LoescheAuswahlfenster4[] = "wm withdraw .tubetop\n";
  char Tcl_Skript_EntferneAuswahlfenster4[] = "destroy .tubetop\n";
  char Tcl_Skript_ZeigeAuswahlfenster5[] = "wm deiconify .filetop\n";
  char Tcl_Skript_LoescheAuswahlfenster5[] = "wm withdraw .filetop\n";
  char Tcl_Skript_EntferneAuswahlfenster5[] = "destroy .filetop\n";
  char Tcl_Skript_ZeigeSchlegelFenster[] = "wm deiconify .schlegel\n";
  char Tcl_Skript_LoescheSchlegelFenster[] =
  "wm withdraw .schlegel;  .schlegel.c delete all\n";
  char Tcl_Skript_EntferneLogfilefenster[] = "destroy .log\n";
  /*vorher bigface(2) siehe unten -> z.B transfer_woh(3) */
  char Tcl_Skript_Hole3reggenBigface[] = "set bigface2"
  " [lindex [lsort -integer -decreasing [array names face2]] 0]";

  if (prg_used[programm - 1] == 0) {	/* initialisieren */
    /* Es koennten auch zu Programmbeginn alle fuenf Initialisierungen auf 
       einmal vorgenommen werden, was aber ein wenig mehr Speicher erfordert */
    switch (programm) {
      case FULLGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_Fullgen_Init) == TCL_ERROR) {
	    schluss (6);
	  }
	  break;
	}
      case DREGGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_3reggen_Init) == TCL_ERROR) {
	    schluss (109);
	  }
	  break;
	}
      case HCGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_HCgen_Init) == TCL_ERROR) {
	    schluss (100);
	  }
	  break;
	}
      case TUBETYPE:{
	  if (Tcl_Eval (interp, Tcl_Skript_Tubetype_Init) == TCL_ERROR) {
	    schluss (131);
	  }
	  break;
	}
      case INPUTFILE:{
	  if (Tcl_Eval (interp, Tcl_Skript_Inputfile_Init) == TCL_ERROR) {
	    schluss (162);
	  }
	  break;
	}
    }
  }
  if (Tcl_Eval (interp, Tcl_Skript_Rasmol_defaults_init) == TCL_ERROR) {
    schluss (1621);
  }
  Tcl_LinkVar (interp, "start", (char *) &start, TCL_LINK_INT);
  Tcl_LinkVar (interp, "cancel", (char *) &cancel, TCL_LINK_INT);

  /* Fenster zeichnen (im Hintergrund, noch nicht auf dem Bildschirm): */
  switch (programm) {
    case FULLGEN:{
	if (Tcl_Eval (interp, Tcl_Skript_Fullgen_Caseauswahl) == TCL_ERROR) {
	  schluss (7);
	}
	if (Tcl_Eval (interp, Tcl_Skript_Fullgen_Symmetrieauswahl) == TCL_ERROR) {
	  schluss (8);
	}
	if (Tcl_Eval (interp, Tcl_Skript_Fullgenfenster) == TCL_ERROR) {
	  schluss (9);
	}
	break;
      }
    case DREGGEN:{
	if (Tcl_Eval (interp, Tcl_Skript_3reggenfenster) == TCL_ERROR) {
	  schluss (112);
	}
	if (Tcl_Eval (interp, Tcl_Skript_3reggen_Caseauswahl) == TCL_ERROR) {
	  schluss (110);
	}
	if (Tcl_Eval (interp, Tcl_Skript_3reggen_Faceauswahl) == TCL_ERROR) {
	  schluss (111);
	}
	break;
      }
    case HCGEN:{
	if (Tcl_Eval (interp, Tcl_Skript_HCgenfenster) == TCL_ERROR) {
	  schluss (11);
	}
	break;
      }
    case TUBETYPE:{
	if (Tcl_Eval (interp, Tcl_Skript_Tubetypefenster) == TCL_ERROR) {
	  schluss (132);
	}
	break;
      }
    case INPUTFILE:{
	if (Tcl_Eval (interp, Tcl_Skript_Inputfilefenster) == TCL_ERROR) {
	  schluss (163);
	}
	break;
      }
  }


  /* Schleife ueber mehrere Durchlaeufe ein und desselben Generierungsprgs.: */
  do {

    /* Auswahlfenster auf dem Bildschirm anzeigen: */
    switch (programm) {
      case FULLGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_ZeigeAuswahlfenster1) == TCL_ERROR) {
	    schluss (47);
	  }
	  break;
	}
      case DREGGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_ZeigeAuswahlfenster2) == TCL_ERROR) {
	    schluss (115);
	  }
	  break;
	}
      case HCGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_ZeigeAuswahlfenster3) == TCL_ERROR) {
	    schluss (102);
	  }
	  break;
	}
      case TUBETYPE:{
	  if (Tcl_Eval (interp, Tcl_Skript_ZeigeAuswahlfenster4) == TCL_ERROR) {
	    schluss (134);
	  }
	  break;
	}
      case INPUTFILE:{
	  if (Tcl_Eval (interp, Tcl_Skript_ZeigeAuswahlfenster5) == TCL_ERROR) {
	    schluss (164);
	  }
	  break;
	}
    }

    /* Aktion abwarten: */
    start = cancel = 0;
    while (!start && !cancel) {
      Tcl_DoOneEvent (0);
      if (global_end == 1 || global_background == 1) {
	schluss (0);
      }
    }

    /* Auswahlfenster vom Bildschirm loeschen (es ist aber noch vorhanden): */
    switch (programm) {
      case FULLGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_LoescheAuswahlfenster1) == TCL_ERROR) {
	    schluss (48);
	  }
	  break;
	}
      case DREGGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_LoescheAuswahlfenster2) == TCL_ERROR) {
	    schluss (116);
	  }
	  break;
	}
      case HCGEN:{
	  if (Tcl_Eval (interp, Tcl_Skript_LoescheAuswahlfenster3) == TCL_ERROR) {
	    schluss (103);
	  }
	  break;
	}
      case TUBETYPE:{
	  if (Tcl_Eval (interp, Tcl_Skript_LoescheAuswahlfenster4) == TCL_ERROR) {
	    schluss (135);
	  }
	  break;
	}
      case INPUTFILE:{
	  if (Tcl_Eval (interp, Tcl_Skript_LoescheAuswahlfenster5) == TCL_ERROR) {
	    schluss (165);
	  }
	  break;
	}
    }


    /* Auswerten: */
    if (start) {		/* Generierungsprogramm starten */

      /* Anfangswerte: */
      outnr = outnr_old = 1;
      spr_ebnr = sl_ebnr = 1;
      spr_ebindex = sl_ebindex = -1;
      you_see_nr1 = you_see_nr2 = 0;
      reviewstep = 0;
      ende = False;
      skip = skip_old = 0;
      old_embed = new_embed = False;
      highnr = 0;
      absolutehighnr = 0;
      newschlegel = False;
      inputfile = raw = outpipe = spring = schlegel = nil;
      rasmol = nil;
      tkview = False;
      geompipe = nil;
      inputfile_id = spring_id = schlegel_id = rasmol_id = 0;
      file_2d = file_3d = file_s2d = file_s3d = nil;
      save_s2d = save_s3d = save_s2d_old = save_s3d_old = False;
      save_spdb = save_spdb_old = False;
      for (i = 0; i < ANZ_GRAPH; i++) {
	graphnr[i] = 0;
	graphstate[i] = LEER;
      }
      status = NONE;

      wp_last_graph = 0;
      Tcl_last_graph = 0;
      last_skip = 0;

      /* Outputbedienfenster zeichnen: */
      if (Tcl_Eval (interp, Tcl_Skript_Outputbedienfenster) == TCL_ERROR) {
	schluss (13);
      }
      /* Programm starten: */
      switch (programm) {
	case FULLGEN:{
	    if (Tcl_Eval (interp, Tcl_Skript_Fullgen_Programmaufruf) == TCL_ERROR) {
	      schluss (10);
	    }
	    if ((aufrufkommando = Tcl_GetVar (interp, "fullcall",
					      TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (11);
	    }
	    inputcode = 1;
	    break;
	  }
	case DREGGEN:{
	    if (Tcl_Eval (interp, Tcl_Skript_3reggen_Programmaufruf) == TCL_ERROR) {
	      schluss (117);
	    }
	    if ((aufrufkommando = Tcl_GetVar (interp, "r3call",
					      TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (118);
	    }
	    if (Tcl_Eval (interp, Tcl_Skript_Hole3reggenBigface) == TCL_ERROR) {
	      schluss (122);
	    }
	    if ((var = Tcl_GetVar (interp, "bigface2", TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (123);
	    }
	    bigface_2 = atoi (var);
	    inputcode = 0;
	    break;
	  }
	case HCGEN:{
	    if (Tcl_Eval (interp, Tcl_Skript_HCgen_Programmaufruf) == TCL_ERROR) {
	      schluss (104);
	    }
	    if ((aufrufkommando = Tcl_GetVar (interp, "hccall",
					      TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (105);
	    }
	    if ((var = Tcl_GetVar (interp, "p3", TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (121);
	    }
	    p_3 = atoi (var);
	    if ((var = Tcl_GetVar (interp, "woh3", TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (143);
	    }
	    woh_3 = atoi (var);
	    inputcode = 1;

	    break;
	  }
	case TUBETYPE:{
	    if (Tcl_Eval (interp, Tcl_Skript_Tubetype_Programmaufruf) == TCL_ERROR) {
	      schluss (136);
	    }
	    if ((aufrufkommando = Tcl_GetVar (interp, "ttcall",
					      TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (137);
	    }
	    inputcode = 1;
	    break;
	  }
	case INPUTFILE:{
	    if (Tcl_Eval (interp, Tcl_Skript_File_Programmaufruf) == TCL_ERROR) {
	      schluss (166);
	    }
	    if ((file_or_pipe = Tcl_GetVar (interp, "inputradio",
					    TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (1167);
	    }
	    if (*file_or_pipe == '1') {
	      if ((aufrufkommando = Tcl_GetVar (interp, "inputfilename5",
						TCL_LEAVE_ERR_MSG)) == NULL) {
		schluss (167);
	      }
	    } else {
	      if ((aufrufkommando = Tcl_GetVar (interp, "inputpipename5",
						TCL_LEAVE_ERR_MSG)) == NULL) {
		schluss (167);
	      }
	    }
	    if ((var = Tcl_GetVar (interp, "old_embed5", TCL_LEAVE_ERR_MSG))
		== NULL) {
	      schluss (168);
	    }
	    old_embed = atoi (var);
	    if ((var = Tcl_GetVar (interp, "new_embed5", TCL_LEAVE_ERR_MSG))
		== NULL) {
	      schluss (169);
	    }
	    new_embed = atoi (var);
	    /* emb-Werte auf jeden Fall einlesen, falls alte Einbettung nicht
	       existiert */
	    if ((var = Tcl_GetVar (interp, "emb_a5", TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (170);
	    }
	    emb_a = atoi (var);
	    if ((var = Tcl_GetVar (interp, "emb_b5", TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (171);
	    }
	    emb_b = atoi (var);
	    if ((var = Tcl_GetVar (interp, "emb_c5", TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (172);
	    }
	    emb_c = atoi (var);
	    if ((var = Tcl_GetVar (interp, "emb_d5", TCL_LEAVE_ERR_MSG)) == NULL) {
	      schluss (173);
	    }
	    emb_d = atoi (var);
	    break;
	  }
      }
      printf ("CaGe calls : %s\n", aufrufkommando);
      if (programm == INPUTFILE) {
	if (*file_or_pipe == '1') {
	  if ((inputfile = fopen (aufrufkommando, "r")) == nil) {
	    schluss (174);
	  }
	  inputfile_id = 0;
	  if ((inputcode = read_headerformat (inputfile)) < 0) {
	    ende = True;	/*schluss(181); */
	  }
	  if (inputcode == 0 && old_embed) {
	    old_embed = False;
	    new_embed = True;
	    emb_a = 10;
	    emb_b = 200;
	    emb_c = 20;
	    emb_d = 0;
	  }
	  if (read_endian (inputfile, &endian_in) == 2) {
	    ende = True;
	    /*schluss(181); */
	  }
	} else {
	  if ((inputfile = popen (aufrufkommando, "r")) == nil) {
	    schluss (174);
	  }
	  inputfile_id = 0;
	  if ((inputcode = read_headerformat (inputfile)) < 0) {
	    ende = True;	/*schluss(181); */
	  }
	  if (inputcode == 0 && old_embed) {
	    old_embed = False;
	    new_embed = True;
	    emb_a = 10;
	    emb_b = 200;
	    emb_c = 20;
	    emb_d = 0;
	  }
	  if (read_endian (inputfile, &endian_in) == 2) {
	    ende = True;
	    /*schluss(181); */
	  }
	}
      } else {
	if ((inputfile = popen (aufrufkommando, "r")) == nil) {
	  schluss (12);
	}
	if (strstr(aufrufkommando,"pid") != NULL) {
	  fgets (c, LINELEN, inputfile);
	  inputfile_id = atoi (c);
	} else {
	  inputfile_id = get_child_pid (aufrufkommando);
	}
      }
      /* Auf ersten Graphen warten : */
      Tcl_CreateFileHandler ((int) (fileno (inputfile)), TK_READABLE, daten_sind_da,
			     (ClientData) fileno (inputfile));
      filehandler = True;
      while (!(status & GRAPH_IST_DA)) {
	Tcl_DoOneEvent (0);
	if (global_end == 1) {
	  schluss (0);
	}
	if (global_background == 1) {
	  go_to_background ();
	  go_to_background2 (outnr);
	}
      }

      /* Ausgabekanaele oeffnen und Ausgabemodus festlegen: */
      if (global_background == 0) {
	sprintf (c, "r_or_g");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (15);
	}
	if (atoi (var) < 2) {
	  d3torasmol = (atoi (var) == 0);
	  rasmol = (char *) "CaGe3.tmp";	/* auch wichtig, falls Ausgabe
						   nach GeomView, denn "rasmol!=nil" <=> es erfolgt 3D-Ausgabe */
	  if (!d3torasmol) {	/* GeomView als Ausgabeprogramm */
	    geompipe = popen ((char *) "togeomview", "w");
	    if (geompipe == nil) {
	      schluss (95);
	    }
	  }
	}
	sprintf (c, "file2d");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (16);
	}
	if (atoi (var) == 1) {
	  tkview = True;
	  if (Tcl_Eval (interp, Tcl_Skript_ZeigeSchlegelFenster) == TCL_ERROR) {
	    schluss (17);
	  }
	}
      }
      sprintf (c, "tofile");
      sprintf (d, "%d", programm);
      if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	schluss (18);
      }
      if (atoi (var) == 1) {
	sprintf (c, "dateiname_f");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (19);
	}
	raw = fopen (var, "wb");
	if (raw == nil) {
	  schluss (20);
	}
	sprintf (c, "filecode");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (86);
	}
	ascfile = (atoi (var) == 0);
      }
      sprintf (c, "topipe");
      sprintf (d, "%d", programm);
      if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	schluss (68);
      }
      if (atoi (var) == 1) {
	sprintf (c, "pipename");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (69);
	}
	outpipe = popen (var, "w");
	if (outpipe == nil) {
	  schluss (70);
	}
	sprintf (c, "filecode");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (87);
	}
	ascpipe = (atoi (var) == 0);
      }
      sprintf (c, "file2d");
      sprintf (d, "%d", programm);
      if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	schluss (148);
      }
      if (atoi (var) == 0) {
	sprintf (c, "dateiname_2d");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (149);
	}
	file_2d = fopen (var, "w");
	if (file_2d == nil) {
	  schluss (150);
	}
      }
      sprintf (c, "r_or_g");
      sprintf (d, "%d", programm);
      if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	schluss (151);
      }
      if (atoi (var) == 3) {
	sprintf (c, "dateiname_3d");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (152);
	}
	file_3d = fopen (var, "w");
	if (file_3d == nil) {
	  schluss (153);
	}
      }
      sprintf (c, "pdb_file");
      sprintf (d, "%d", programm);
      if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	schluss (151);
      }
      pdb_file = atoi (var);
      if (pdb_file == 1) {
	sprintf (c, "dateiname_3d");
	sprintf (d, "%d", programm);
	if ((var = Tcl_GetVar2 (interp, c, d, TCL_LEAVE_ERR_MSG)) == NULL) {
	  schluss (152);
	}
	file_3d = fopen (var, "w");
	if (file_3d == nil) {
	  schluss (153);
	}
      }
      if (global_end == 1) {
	schluss (0);
      }
      if (global_background == 1) {
	go_to_background ();
	go_to_background2 (outnr);
      }
      /* Bei Bedarf Bedienfenster (Output panel) aufrufen: */
      if (rasmol || tkview) {
	Tcl_LinkVar (interp, "outnr", (char *) &outnr, TCL_LINK_INT);
	Tcl_LinkVar (interp, "reviewstep", (char *) &reviewstep, TCL_LINK_INT);
	Tcl_LinkVar (interp, "skip", (char *) &skip, TCL_LINK_INT);
	Tcl_LinkVar (interp, "outcancel", (char *) &ende, TCL_LINK_BOOLEAN);
	Tcl_LinkVar (interp, "save2d", (char *) &save_s2d, TCL_LINK_BOOLEAN);
	Tcl_LinkVar (interp, "save3d", (char *) &save_s3d, TCL_LINK_BOOLEAN);
	Tcl_LinkVar (interp, "savepdb", (char *) &save_spdb, TCL_LINK_BOOLEAN);
	if (tkview) {
	  Tcl_LinkVar (interp, "newschlegel", (char *) &newschlegel,
		       TCL_LINK_BOOLEAN);
	}
	if (Tcl_Eval (interp, Tcl_Skript_Zeige_Bedienfenster) == TCL_ERROR) {
	  schluss (52);
	}
      }
      if (global_background == 0 &&
       Tcl_Eval (interp, Tcl_Skript_StatusGraphengenerierung) == TCL_ERROR) {
	schluss (53);
      }
      /* Falls keine Ausgabe von Einbettungen:  SKIPMODUS einschalten */
      if (rasmol || tkview) {
	spr_ebindex = sl_ebindex = 0;
	spr_ebnr = sl_ebnr = 1;
	sprintf (c, "0");
	if (rasmol) {
	  if (Tcl_SetVar (interp, "graphnr3", c,
			  TCL_LEAVE_ERR_MSG) == NULL) {
	    schluss (63);
	  }
	}
	if (tkview) {
	  if (Tcl_SetVar (interp, "graphnr2", c,
			  TCL_LEAVE_ERR_MSG) == NULL) {
	    schluss (147);
	  }
	}
	sprintf (c, "%d", outnr);
	if (Tcl_SetVar (interp, "graphnr", c,
			TCL_LEAVE_ERR_MSG) == NULL) {
	  schluss (64);
	}
      } else if (!(file_2d || file_3d)) {
	status |= SKIPMODUS;
	no_output = 1;
      }
      /* Graphen einlesen und steuern: */
      while (!ende) {
	/* Event abwarten: */
	Tcl_DoOneEvent (0);
	event_loop ();
      }				/* while (!ende) */

      /* Programmdurchlauf beenden: */
      if (global_background == 0) {
	if (Tcl_Eval (interp, Tcl_Skript_Loesche_Bedienfenster) == TCL_ERROR) {
	  schluss (26);
	}
	if (Tcl_Eval (interp, Tcl_Skript_Loesche_Statusmeldung) == TCL_ERROR) {
	  schluss (46);
	}
	if (Tcl_Eval (interp, Tcl_Skript_LoescheSchlegelFenster) == TCL_ERROR) {
	  schluss (83);
	}
      }
      if (tkview || rasmol) {
	Tcl_UnlinkVar (interp, "outnr");
	Tcl_UnlinkVar (interp, "reviewstep");
	Tcl_UnlinkVar (interp, "skip");
	Tcl_UnlinkVar (interp, "outcancel");
	Tcl_UnlinkVar (interp, "save2d");
	Tcl_UnlinkVar (interp, "save3d");
	if (tkview) {
	  Tcl_UnlinkVar (interp, "newschlegel");
	}
      }
      if (global_background > 0) {
	schluss (0);
      }
      alles_schliessen ();

      /* Logfile anzeigen: */
      switch (programm) {
	case FULLGEN:{
	    if (Tcl_Eval (interp, Tcl_Skript_Zeige_Fullgenlogfile) == TCL_ERROR) {
	      schluss (84);
	    }
	    break;
	  }
	case DREGGEN:{
	    if (Tcl_Eval (interp, Tcl_Skript_Zeige_3reggenlogfile) == TCL_ERROR) {
	      schluss (119);
	    }
	    break;
	  }
	case HCGEN:{
	    if (Tcl_Eval (interp, Tcl_Skript_Zeige_HCgenlogfile) == TCL_ERROR) {
	      schluss (106);
	    }
	    break;
	  }
	case TUBETYPE:{
	    if (Tcl_Eval (interp, Tcl_Skript_Zeige_Tubetypelogfile) == TCL_ERROR) {
	      schluss (138);
	    }
	    break;
	  }
	case INPUTFILE:{
	    sprintf (c, "%d", absolutehighnr);
	    if (Tcl_SetVar (interp, "absolutehighnr", c,
			    TCL_LEAVE_ERR_MSG) == NULL) {
	      schluss (175);
	    }
	    if (Tcl_Eval (interp, Tcl_Skript_Zeige_Fileinhalt) == TCL_ERROR) {
	      schluss (176);
	    }
	    break;
	  }
      }
      if (programm != INPUTFILE &&
	  Tcl_Eval (interp, Tcl_Skript_Zeige_Logfile) == TCL_ERROR) {
	schluss (108);
      }
      cancel = 0;
      while (!cancel) {
	Tcl_DoOneEvent (0);
	if (global_end == 1 || global_background > 0) {
	  schluss (0);
	}
      }
      cancel = 0;		/* fuer aeussere Schleife */
      if (Tcl_Eval (interp, Tcl_Skript_EntferneLogfilefenster) == TCL_ERROR) {
	schluss (85);
      }
      if (Tcl_Eval (interp, Tcl_Skript_Entferne_Bedienfenster) == TCL_ERROR) {
	schluss (51);
      }
    }				/* if (start) */
  } while (cancel == 0);

  Tcl_UnlinkVar (interp, "start");
  Tcl_UnlinkVar (interp, "cancel");

  /* Auswahlfenster endgueltig loeschen: */
  switch (programm) {
    case FULLGEN:{
	if (Tcl_Eval (interp, Tcl_Skript_EntferneAuswahlfenster1) == TCL_ERROR) {
	  schluss (49);
	}
	break;
      }
    case DREGGEN:{
	if (Tcl_Eval (interp, Tcl_Skript_EntferneAuswahlfenster2) == TCL_ERROR) {
	  schluss (120);
	}
	break;
      }
    case HCGEN:{
	if (Tcl_Eval (interp, Tcl_Skript_EntferneAuswahlfenster3) == TCL_ERROR) {
	  schluss (107);
	}
	break;
      }
    case TUBETYPE:{
	if (Tcl_Eval (interp, Tcl_Skript_EntferneAuswahlfenster4) == TCL_ERROR) {
	  schluss (139);
	}
	break;
      }
    case INPUTFILE:{
	if (Tcl_Eval (interp, Tcl_Skript_EntferneAuswahlfenster5) == TCL_ERROR) {
	  schluss (177);
	}
	break;
      }
  }
}

/****************************MAIN************************************/

int main (int argc, char **argv)
{
  int code;
  char Tcl_Skript_Auswahlfenster_anzeigen[] = "wm deiconify .haupt\n";
  char Tcl_Skript_Auswahlfenster_verbergen[] = "wm withdraw .haupt\n";

  /* Initialisierung: */
  cospi6 = cos (pi / 6);
  fprintf (stderr, "Welcome to CaGe V0.3\n");
  fprintf (stderr, "Please wait. In a few seconds the frame of the\n"
	   "main window will appear.\n");

  name = argv[0];		/* use program name as application name */
  Tk_Main (0, argv, Tcl_AppInit);	/* Tk_Main(0,argv,Tcl_AppInit) */
  init ();
  if (Tcl_Eval (interp, Tcl_Skript_Initialisierung) == TCL_ERROR) {
    schluss (1);
  }
  if (Tcl_Eval (interp, Tcl_Skript_Titelfenster) == TCL_ERROR) {
    schluss (2);
  }
  if (Tcl_Eval (interp, Tcl_Skript_Schlegelfenster) == TCL_ERROR) {
    schluss (82);
  }
  /* Links fuer PIDs und Programmende schaffen: */
  /* Tcl_LinkVar(interp,"inputfile_id",(char *)&inputfile_id,TCL_LINK_INT);
     Tcl_LinkVar(interp,"schlegel_id",(char *)&schlegel_id,TCL_LINK_INT);
     Tcl_LinkVar(interp,"spring_id",(char *)&spring_id,TCL_LINK_INT); */
  Tcl_LinkVar (interp, "rasmol_id", (char *) &rasmol_id, TCL_LINK_INT);
  Tcl_LinkVar (interp, "global_end", (char *) &global_end, TCL_LINK_INT);
  Tcl_LinkVar (interp, "global_background", (char *) &global_background,
	       TCL_LINK_INT);
  if (Tcl_Eval (interp, Tcl_Skript_Rasmol_defaults) == TCL_ERROR) {
    schluss (1620);
  }
  if (Tcl_Eval (interp, Tcl_Skript_File_Fenster) == TCL_ERROR) {
    schluss (1622);
  }
  /* Auswahlfenster einrichten: */
  if (Tcl_Eval (interp, Tcl_Skript_Hauptfenster) == TCL_ERROR) {
    schluss (3);
  }
  Tcl_LinkVar (interp, "programm", (char *) &programm, TCL_LINK_INT);


  /* mv .rasmolrc .rasmol_tmp */
  if (Tcl_Eval (interp, Tcl_Script_mv_rasmolrc) == TCL_ERROR) {
    schluss (2030);
  }
  /* neue Tcl Befehle */
  Tcl_CreateCommand (interp, "kill_child", kill_child, (ClientData) NULL,
		     (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand (interp, "min_rand", min_rand, (ClientData) NULL,
		     (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand (interp, "maximum_c", maximum_c, (ClientData) NULL,
		     (Tcl_CmdDeleteProc *) NULL);
  Tcl_CreateCommand (interp, "minimum_h", minimum_h, (ClientData) NULL,
		     (Tcl_CmdDeleteProc *) NULL);

  /* Schleife ueber den gesamten Programmdurchlauf: */
  while (Tk_GetNumMainWindows () > 0) {		/* Programm laeuft noch */

    /* Auswahlfenster anzeigen: */
    if (Tcl_Eval (interp, Tcl_Skript_Auswahlfenster_anzeigen) == TCL_ERROR) {
      schluss (4);
    }
    /* Auf Auswahl warten: */
    programm = 0;
    while (programm == 0) {
      Tcl_DoOneEvent (0);
      if (global_end == 1 || global_background > 0) {
	schluss (0);
      }
    }

    /* Auswahlfenster verbergen: */
    if (Tcl_Eval (interp, Tcl_Skript_Auswahlfenster_verbergen) == TCL_ERROR) {
      schluss (5);
    }
    /* Rasmoldefaults zu dem ausgewaehlten Programm laden */
    if (Tcl_Eval (interp, Tcl_Script_load_rasmol_rc) == TCL_ERROR) {
      schluss (2003);
    }
    /* Gewaehltes Programm aufrufen: */
    fullgen_handling (programm);
    if (programm) {
      prg_used[programm - 1]++;
    }
  }

  /* der folgende Teil wird nie erreicht: */

  /*
   * Don't exit directly, but rather invoke the Tcl "exit" command.
   * This gives the application the opportunity to redefine "exit"
   * to do additional cleanup.
   */

  Tcl_Eval (interp, "exit");
  Tcl_DeleteInterp (interp);
  schluss (0);
}
