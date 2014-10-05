#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<limits.h>
#include<time.h>
#include<sys/times.h>
#include<sys/stat.h>

#define S        700           /* Maximale Anzahl der 6-Ecke */
#define N        ((4*S)+20)    /* Maximal moegliche Anzahl der Knoten in einem patch
				  -- wird auch als Beschraenkung fuer die Anzahl der
				  Knoten im tube benutzt */
#define MAXN (2*N)             /* Maximales Label fuer vorkommende Knoten */

#define infty    LONG_MAX
#define int_infty INT_MAX
#define FL_MAX   USHRT_MAX
#define KN_MAX   USHRT_MAX
#define unbelegt FL_MAX
#define leer     (KN_MAX-1)
#define aussen   (KN_MAX-2) /* so werden kanten nach aussen markiert */
#define f_leer   (FL_MAX-1)
#define False    0
#define True     1
#define nil      0
#define reg      3
#define filenamenlaenge 30 /* sollte vorerst reichen */

#ifdef __linux /* Linux */
#define time_factor CLK_TCK
#endif


#ifdef __osf__ /* OSF auf den alphas */
#define time_factor CLK_TCK
#endif

#ifdef __sgi /* Silicon Graphics */
#define time_factor CLK_TCK
#endif
#ifdef sun /* Sun */
#define time_factor CLK_TCK
#endif
#ifdef __hpux /* Hewlet Packard */
#define time_factor CLK_TCK
#endif
#ifdef __ultrix /* DEC */
#define time_factor 60
#endif

/* Typ-Deklarationen: */

typedef  char BOOL; /* von 0 verschieden entspricht True */

typedef unsigned short KNOTENTYP;   
typedef unsigned short FLAECHENTYP; /* wurde von unsigned char hochgesetzt -- checken !! */

typedef KNOTENTYP GRAPH[N+1][3]; /* fuer schreibegraph und Isomorphietest */

/* Element der Adjazenztabelle: */


typedef struct iL {
		  struct iL *next_item;
		  FLAECHENTYP code[3]; /* konnte auf 3 gekuerzt werden, da die 2er und 1er ja nicht
					mehr gespeichert werden */
		} ITEMLISTE; /* die Items -- d.h. Codes */


typedef struct sL {
                  struct sL **next_level;
		  int number_next;
		  ITEMLISTE *items;
		  ITEMLISTE *last_item;
		} SEQUENZLISTE; /* die verzweigung der liste nach der Sequenz */


typedef struct SL {
		  int total_maps; 
                  SEQUENZLISTE *sechser[S+1];
 		  } S_LISTE; /* die erste stufe der liste -- verzweigung nach Anzahl der 6-Ecke */


typedef struct K {
                   KNOTENTYP ursprung; /* bei welchem knoten startet die kante */
                   KNOTENTYP name;  /* Identifikation des Knotens, mit
                                       dem Verbindung besteht */
		   long dummy;   /* fuer alle moeglichen zwecke */
                   struct K *prev;  /* vorige Kante im Uhrzeigersinn */
                   struct K *next;  /* naechste Kante im Uhrzeigersinn */
		   struct K *invers; /* die inverse Kante (in der Liste von "name") */
		   struct K *merkekante; 
		   KNOTENTYP merkename;
                  } KANTE;

typedef struct  {
                   int laenge;
                   int sequenz[7];  /* die laenge der luecke. Konvention: Beendet durch "leer" */
		   KANTE *kanten[7];/* die letzten aussenkanten vor der sequenz */
		   char k_marks[7]; /* 1 wenn anfang einer kanonischen Sequenz, 0 sonst */
		 } SEQUENZ;


typedef struct le { FLAECHENTYP code[12];
		    struct le *smaller;
		    struct le *larger; } LISTENTRY;

typedef struct bble { FLAECHENTYP code[8*1000];
		      /* der erste von 8 Eintraegen ist 1 bei Spiegelsymmetrie, 0 sonst,
			 der zweite ist die Anzahl der Orbits von Flaechen auf dem Rand,
			 danach kommt der Spiralcode */
		      struct bble *next;
		      int anzahl; } BBLISTENTRY;


/* "Ueberschrift" der Adjazenztabelle (Array of Pointers): */
typedef KANTE PLANMAP[N+1][3];
                  /* Jeweils 3 KANTEn */
                  /* ACHTUNG: 1. Zeile der Adjazenztabelle hat Index 0 -
                     wird fast nicht benutzt, N Zeilen auch ausreichend
		     In [0][0].name wird aber die knotenzahl gespeichert */



static int markvalue_v = 30000;
static int marks__v[MAXN+1];
#define RESETMARKS_V {int mki; if ((++markvalue_v) > 30000) \
       { markvalue_v = 1; for (mki=0;mki<MAXN;++mki) marks__v[mki]=0;}}
#define UNMARK_V(x) (marks__v[x] = 0)
#define ISMARKED_V(x) (marks__v[x] == markvalue_v)
#define MARK_V(x) (marks__v[x] = markvalue_v)
#define MARK_ENDS(x) {MARK_V(x->ursprung); MARK_V(x->name);}
#define ISMARKED_ENDS(x) ((ISMARKED_V(x->ursprung)) || (ISMARKED_V(x->name)))



/*********************GLOBALE*VARIABLEN********************************/

/* Die folgenden Variablen werden von fast allen Programmteilen benutzt
   oder werden aus anderen Gruenden global verwaltet. Trotzdem werden
   sie in einigen Faellen auch an Funktionen uebergeben, um die
   Parameterabhaengigkeit der Funktionen zu betonen */
/* Variablen-Deklarationen: */


void konstruiere();
int check_rand();
void *memcpy();

PLANMAP tube; /* der 6-eck-Koerper des tubes */
PLANMAP testtube; /* der -- eventuell laengere -- Koerper zum Testen auf Isomorphie */

char longfilename[30], shortfilename[30]; /* in longfile werden alle caps mit mindestens einem
					     5-Eck am Rand gespeichert, in shortfile die mit
					     mindestens einem an jedem Randsegment */
FILE *longfile, *shortfile;
BBLISTENTRY *last_bbliste, *first_bbliste;
KANTE *F_eck_kanten[30]; /* rechts von diesen Kanten liegen 5-Ecke */
int anzahl_5ek;
int max_segment; /* das maximum von first_part und second_part */
int max_entry;
int output; /* soll auf stdout geschrieben werden */
int fullerenes=0; /* sollen tubetype fullerene erzeugt werden ? */
int tubelaenge=0; /* welche laenge soll der angeklebte Koerper haben ? 
		     default: randlaenge */
int noout=0, vegaout=0, mit_spiegelsymmetrie=0;
int dangling_tube=0; /* soll der output mit einem teil des tubekoerpers gemacht werden ? */
int graphenzaehler=0, storepatchzaehler[6]={0,0,0,0,0,0};
int all_caps=0, caps_mit_pent=0, caps_mit_2pent=0, good_caps=0; 

/* die folgenden drei werden gebraucht, wenn ein tube drangeklebt wird */
KANTE *first_edge[3*N+1];
KNOTENTYP nummer[3*N+1];
int knotenzahl_incl_tube;

int knotenzahl;  /* Knotenzahl des Graphen;
		   ergibt sich aus Eingabe; wird im Verlauf
		   der Konstruktion nicht geaendert */

int docase[9]= {1,1,1,1,1,1,1,1,1};
int first_part=-1, second_part=-1; /* die laenge des ersten bzw zweiten Teils */
int bbpatch=0; /* soll ein bbpatch generiert werden ? */

int non_iso_graphenzahl[N+1];
int graphenzahl[N+1];

S_LISTE mapliste;

int max_sechsecke,min_sechsecke,max_knoten,max_kanten,randlaenge;

FLAECHENTYP **lastcode[N+1]; /* wird bei codeart 3 gebraucht */
FLAECHENTYP last_code[12]; /* wird bei codeart 2 gebraucht */

int debugzaehler=0, debugsignal=0;
int minbbl=N, maxbbl=0;
int minbrillenglas=N, maxbrillenglas=0;
int min_2_3_4=N;
int no_penta_spiral=0, no_hexa_spiral=0;

BOOL bblmark[N+1], brillenglasmark[N+1], zwei_3_4_mark[N+1];
BOOL do_bauchbinde,do_brille,do_sandwich;

LISTENTRY codeliste[N+1];

FILE *fil[N+1];
FILE *logfile;
char logfilename[30], no_penta_spiral_filename[30], no_spiral_filename[30];
char no_hexa_spiral_filename[30];

BOOL IPR=0, is_ipr=1;
BOOL to_stdout=0, splitter=0;
BOOL hexspi=0, spistat=0;
BOOL spiralcheck=0;
int codenumber, listenlaenge;
int spiralnumbers[12*S+120+1];

KNOTENTYP globaladr[10];

int automorphisms, mirror_symmetric;
int auts_statistic[13]={0}, auts_statistic_mirror[13]={0};


/* Prototypen: */

void codiereplanar();
void common_history();
void schreibemap();
void schreibehistory();
BOOL check_ipr();


/****************SCHREIBEITEM******************************/

void schreibeitem(ITEMLISTE *it)
{
int i;
if (it==nil) return;
fprintf(stderr,"\nITEM: %d\n",it);
for (i=0; i<8; i++) fprintf(stderr," %d ",it->code[i]);
fprintf(stderr,"\n next_item: %d\n",it->next_item);
if (it->next_item != nil) schreibeitem(it->next_item);
}

/*****************SCHREIBEADRESSE***************************/

void schreibeadresse(KNOTENTYP *a)
{ int i, ende;
if (a[0]==6) ende=3; else ende=8-a[0];
fprintf(stderr,"Adresse:  %d %d    ",a[0],a[1]);
for (i=2; i<ende; i++) fprintf(stderr," %d ",a[i]);
fprintf(stderr," \n");
}


/******************SCHREIBE_SELISTE***************************/

void  schreibe_seliste(SEQUENZLISTE *li, KNOTENTYP *se, int tiefe)
{
int i;
if (li==nil) return;
fprintf(stderr,"Ort: "); for (i=0; i<tiefe; i++) fprintf(stderr," %d ",se[i]);
fprintf(stderr,"\n number_next: %d items: %d last_item: %d\n",li->number_next, li->items, li->last_item);
if (li->items != nil) { fprintf(stderr,"Die Items:"); schreibeitem(li->items); }
for (i=0; i<li->number_next; i++) 
{ fprintf(stderr,"next_level[%d]: %d\n",i,li->next_level[i]); se[tiefe]=i;
  schreibe_seliste(li->next_level[i],se,tiefe+1); }
}


/*******************SCHREIBELISTE******************************/

void schreibeliste()
/* schreibt immer mapliste */
{
int j;
KNOTENTYP sequenz[8];


fprintf(stderr," total_maps: %d\n",mapliste.total_maps);

fprintf(stderr,"\n level 6-Ecke: \n ");

  for (j=0; j<=S; j++) 
    { fprintf(stderr,"sechser[%d]: %d",j,mapliste.sechser[j]);
      fprintf(stderr,"\n\n Die nachfolgenden Sequenzlisten: \n");
      sequenz[0]=0; sequenz[1]=j;
      schreibe_seliste(mapliste.sechser[j], sequenz, 2);
    }
}

/************************SCHREIBEITEM2************************************/

void schreibeitem2(ITEMLISTE *it, KNOTENTYP wo[], BOOL bb)
{
int i, grenze;
if (bb) grenze=8; else grenze=wo[0];
if (it==nil) return;
for (i=0; ((int)i<(int)3) || ((int)i< 8-(int)wo[0]); i++) fprintf(stderr,"%d ",wo[i]); fprintf(stderr,"   |   ");
for (i=0; i<grenze; i++) if (it->code[i]!=f_leer) fprintf(stderr," %d ",it->code[i]);
fprintf(stderr,"\n");
if (it->next_item != nil) schreibeitem2(it->next_item,wo,bb);
}




/******************SCHREIBE_SELISTE2***************************/

void  schreibe_seliste2(SEQUENZLISTE *li, KNOTENTYP *se, int tiefe, BOOL bb)
{
int i;
if (li==nil) return;
if (li->items != nil) { if (!bb) se[0]=8-tiefe; schreibeitem2(li->items,se,bb); }
for (i=0; i<li->number_next; i++) 
  { se[tiefe]=i;
    schreibe_seliste2(li->next_level[i],se,tiefe+1,bb); }
}


/*******************SCHREIBELISTITEMS******************************/

void schreibelistitems()
/* schreibt immer mapliste */
{
int j;
KNOTENTYP sequenz[13];

fprintf(stderr,"\n  total_maps: %d\n", mapliste.total_maps);

  for (j=0; j<=S; j++) 
    { sequenz[1]=j;
      schreibe_seliste2(mapliste.sechser[j], sequenz, 2,0);
    }

fprintf(stderr,"\n total_maps: %d\n", mapliste.total_maps);

  sequenz[0]=6;
  for (j=0; j<=S; j++) 
    { sequenz[1]=j;
      /*schreibe_seliste2(bbliste.sechser[j], sequenz, 2,1);*/
    }


}






/********************SCHREIBESEQUENZ****************************/

void schreibesequenz(SEQUENZ s)
{
int *sequenz;
KANTE **sk;
char *ch;
int i;

sequenz=s.sequenz;
sk=s.kanten;
ch=s.k_marks;

fprintf(stderr,"Sequenzlaenge: %d\n",s.laenge);
if (s.laenge > 0)
{
fprintf(stderr,"Die Sequenz:     ");
for(i=0; i < s.laenge; i++) fprintf(stderr," %2d ", sequenz[i]);
fprintf(stderr,"\nDie Startpunkte: ");
for(i=0; i < s.laenge; i++) fprintf(stderr," %2d ", (sk[i])/*->ursprung*/);
fprintf(stderr,"\nDie K-Marken:    ");
for(i=0; i < s.laenge; i++) fprintf(stderr," %2d ", ch[i]);
}

else fprintf(stderr,"sequenz[0]: %d  Startpunkt[0]: %d\n",sequenz[0],(sk[0])->ursprung);

fprintf(stderr,"\n\n");

}


/****************************SCHREIBECODE************************/

void schreibecode(FLAECHENTYP *code, int laenge)
{
int i;
if (laenge==8) { fprintf(stderr,"%d %d    ",code[0],code[1]);
		 i=2; }
else i=0;

while (i<laenge) { fprintf(stderr,"%d ",code[i]); i++; }
fprintf(stderr,"\n");
}


/********************SCHREIBEZYKLISCHELISTE**********************/

/* gibt die adjazenzliste incl pointer ... aus */

void schreibezyklischeliste(KNOTENTYP which, PLANMAP map)
{
KANTE *lauf, *li;

li=map[which];

fprintf(stderr,"Anfang Adjazenzliste von Knoten: %d\n",which);
fprintf(stderr,"name: %d adresse: %d prev: %d next: %d invers: %d ursprung:%d \n",li->name, li, li->prev, li->next, li->invers, li->ursprung);
for (lauf=li->next; lauf !=li; lauf=lauf->next)
fprintf(stderr,"name: %d adresse: %d prev: %d next: %d invers: %d ursprung: %d \n",lauf->name, lauf, lauf->prev, lauf->next, lauf->invers, lauf->ursprung);
fprintf(stderr,"Ende Adjazenzliste\n");
}

/*********************SCHREIBELISTEN*****************************/

void schreibelisten(PLANMAP map) /*alle*/

{
KNOTENTYP i;
for(i=1; i<=map[0][0].name; i++) schreibezyklischeliste(i,map);
}



/*********************SCHREIBEKANTE***************************/

void schreibeKANTE(KANTE * li)
{
fprintf(stderr,"Adresse: %d, name: %d, next: %d, prev: %d, invers: %d\n",
                li,li->name,li->next,li->prev,li->invers);
}



/**********************SCHREIBEGRAPH**********************************/

void schreibegraph(g)
GRAPH g;
{
 int x,y, unten,oben, maxvalence;
fprintf(stderr,"\n\n ");

/*maxvalence=0;
for (x=1; x<=g[0][0]; x++)
 { for (y=1; g[x][y] != leer; y++);
   if (y>maxvalence) maxvalence=y; }*/

maxvalence=reg;


fprintf(stderr,"|%3d",g[0][0]);

for(x=1; (x <= g[0][0])&&(x<=24); x++)  fprintf(stderr,"|%3d",x); fprintf(stderr,"|\n");

fprintf(stderr," ");

for(x=0; (x <= g[0][0])&&(x<=24); x++) fprintf(stderr,"|===");    fprintf(stderr,"|\n");

for(x=0; x < maxvalence; x++)
  {
   fprintf(stderr," |   ");
   for(y=1; (y<=g[0][0])&&(y<=24); y++)
       if (g[y][x] ==leer) fprintf(stderr,"|  ");
/*       else if (g[y][x] ==MARKE) fprintf(stderr,"| M");*/
       else if (g[y][x] ==aussen) fprintf(stderr,"|  a");
       else fprintf(stderr,"|%3d",g[y][x]);
       fprintf(stderr,"|\n");
  }

unten=25; oben=48;

while(g[0][0]>=unten)
{
fprintf(stderr,"\n");

fprintf(stderr,"     ");

for(x=unten; (x <= g[0][0])&&(x<=oben); x++)  fprintf(stderr,"|%3d",x); fprintf(stderr,"|\n");

fprintf(stderr,"     ");

for(x=unten; (x <= g[0][0])&&(x<=oben); x++) fprintf(stderr,"|===");    fprintf(stderr,"|\n");

for(y=0; y < maxvalence; y++)
  {
   fprintf(stderr,"     ");
   for(x=unten; (x <= g[0][0])&&(x<=oben); x++)
       if (g[x][y]==leer) fprintf(stderr,"|  "); 
/*       else if (g[x][y] ==MARKE) fprintf(stderr,"| M");*/
       else if (g[x][y] ==aussen) fprintf(stderr,"|  a");
       else fprintf(stderr,"|%3d",g[x][y]);
       fprintf(stderr,"|\n");
  }
unten += 24; oben += 24;
}

}


/**************************MAP_TO_GRAPH******************************/

void map_to_graph(PLANMAP map, GRAPH g)

{
KANTE *run, *merk;
int i,val,j;

g[0][0]=map[0][0].name;

for (i=1; i<=map[0][0].name; i++)
    {
      merk=map[i]; g[i][0]=merk->name; run=merk->next; val=1;
      while (run!=merk) { g[i][val]=run->name; val++; run=run->next; 
      }
      for(j=val; j<=reg; j++) g[i][j]=leer;
    }
}

/**************************SCHREIBEMAP***********************************/

void schreibemap(PLANMAP map)
{
GRAPH g;
map_to_graph(map,g);
schreibegraph(g);
}

/**********************CHECKSIZE_MARK_RETURN**************************************/

/* bestimmt die groesse der flaeche links von edge -- ist da keine gibt's Probleme 
   ausserdem setzt er fuer alle kanten, so dass diese flaeche links davon ist, dummy
   auf mark. In nextedge wird die im Gegen-Uhrzeigersinn letzte unmarkierte Kante
   (heisst: die letzte kante mit markierung < mark) zurueckgegeben -- und nil, wenn
   sie nicht existiert. */

int checksize_mark_return(KANTE* edge, int mark, KANTE **nextedge)

{
KANTE *run; 
int zaehler=1;

*nextedge=nil;
if (edge->dummy < mark) { *nextedge=edge->invers; edge->dummy=edge->invers->dummy=mark; }

for (run=edge->invers->next; run != edge; run=run->invers->next) 
        { if (run->dummy < mark) { *nextedge=run->invers; run->dummy=run->invers->dummy=mark; }
	  zaehler++; }
return(zaehler);
}



/**********************CHECKSIZE_MARK_RETURN_RIGHT**************************************/

/* bestimmt die groesse der flaeche rechts von edge -- sonst wie oben */

int checksize_mark_return_right(KANTE* edge, int mark, KANTE **nextedge)

{
KANTE *run; 
int zaehler=1;

*nextedge=nil;
if (edge->dummy < mark) { *nextedge=edge->invers; edge->dummy=edge->invers->dummy=mark; }

for (run=edge->invers->prev; run != edge; run=run->invers->prev) 
      {if (run->dummy < mark) { *nextedge=run->invers; run->dummy=run->invers->dummy=mark; }
       zaehler++;}
return(zaehler);
}




/*******************INIT_MAP************************/

void init_map(PLANMAP map)
{
int i,j;

map[0][0].name=0;

for (i=1; i<=N; i++)
{
map[i][0].next= map[i]+1; map[i][0].prev= map[i]+2;
map[i][1].next= map[i]+2; map[i][1].prev= map[i];
map[i][2].next= map[i]; map[i][2].prev= map[i]+1;

for (j=0; j<3; j++) 
          { map[i][j].ursprung=i;
	    map[i][j].name=leer;
            map[i][j].invers=nil; }
}
}

/*******************INIT_DANGLING_TUBE************************/

void init_dangling_tube(PLANMAP map)
{
int i,j;

map[0][0].name=0;

for (i=1; i<=N; i++)
{
map[i][0].next= map[i]+1; map[i][0].prev= map[i]+2;
map[i][1].next= map[i]+2; map[i][1].prev= map[i];
map[i][2].next= map[i]; map[i][2].prev= map[i]+1;

for (j=0; j<3; j++) 
          { map[i][j].ursprung=i+max_knoten; /* um keinen Konflikt mit den Knotennamen 
						im Cap zu haben */
	    map[i][j].name=leer;
            map[i][j].invers=nil; }
}
}

/*******************INIT_FULL_TUBE************************/

void init_full_tube(PLANMAP map)
{
int i,j;

map[0][0].name=0;

for (i=1; i<=N; i++)
{
map[i][0].next= map[i]+1; map[i][0].prev= map[i]+2;
map[i][1].next= map[i]+2; map[i][1].prev= map[i];
map[i][2].next= map[i]; map[i][2].prev= map[i]+1;

for (j=0; j<3; j++) 
          { map[i][j].ursprung=i;
	    map[i][j].name=leer;
            map[i][j].invers=nil; }
}
}


/********************BAUE_POLYGON*******************/
/* Baut ein einzelnes leeres Polygon mit n Ecken (n>=3) 
   und initialisiert map */

void baue_polygon(int n, PLANMAP map, KANTE **marke )
{
int j;

if (n<3) { fprintf(stderr,"Error, no 2-gons allowed !\n"); return; }

/* sicherheitshalber erstmal loeschen und initialisieren */

init_map(map);

/* Immer: erster Eintrag zurueck, 2. nach aussen, dritter vor */

map[1][0].name=n;   map[1][1].name=aussen;   map[1][2].name=2;
map[1][0].invers=map[n]+2; map[1][1].invers=nil; map[1][2].invers=map[2];

(*marke)=map[1]+1;

for (j=2; j<n; j++)
{
map[j][0].name=j-1;   map[j][1].name=aussen;   map[j][2].name=j+1;
map[j][0].invers=map[j-1]+2; map[j][1].invers=nil; map[j][2].invers=map[j+1];
}

map[n][0].name=n-1;   map[n][1].name=aussen;   map[n][2].name=1;
map[n][0].invers=map[n-1]+2; map[n][1].invers=nil; map[n][2].invers=map[1];

map[0][0].name=n;

}

/**********************CHECKSIZE_RIGHT**************************************/

/* bestimmt die groesse der flaeche rechts von edge -- ist da keine gibt's Probleme */

int checksize_right( KANTE* edge)
{
KANTE *run; 
int zaehler=1;

for (run=edge->invers->prev; run != edge; run=run->invers->prev) zaehler++;
return(zaehler);
}




/*********************ADD_POLYGON***********************************/

void add_polygon(int n, PLANMAP map, KANTE *start, KANTE **lastout)
/* fuegt ein weiteres polygon einer Reihe an. Dabei ist n die groesse des polygons. 
   Angefuegt wird immer an start. Die Marke wird nicht versetzt. Ueber lastout wird
   die letzte Aussenkante des Polygons zurueckgegeben. */


{
int new_tempknz, tempknz;
KANTE *ende;
int common_vertices;


if (IPR && (n==5))
  {
    if (checksize_right(start->next)==5) is_ipr=0;
    for (ende=start->next->invers->next, common_vertices=2; ende->name != aussen; 
	  ende=ende->invers->next) { if (checksize_right(ende)==5) is_ipr=0;
                                     common_vertices++;
				   }
  }
else for (ende=start->next->invers->next, common_vertices=2; ende->name != aussen; 
	  ende=ende->invers->next) common_vertices++;


if (n<common_vertices) 
   { fprintf(stderr,"polygon to insert too small !\n"); 
     exit(0); }

/* es muessen also n-common_vertices knoten hinzugefuegt werden */

tempknz=map[0][0].name;
new_tempknz=tempknz+n-common_vertices;

if (n-common_vertices==0) /* dann kommt kein knoten dazu */
  { start->name=ende->ursprung; start->invers=ende;
    ende->name=start->ursprung; ende->invers=start;
    *lastout=nil;
    return;
  }

if (n-common_vertices==1) /* dann kommt nur ein knoten dazu */
 {
 tempknz++;
 start->name=tempknz; start->invers=map[tempknz];
 map[tempknz][0].name=start->ursprung; map[tempknz][0].invers=start;
 map[tempknz][1].name=aussen; map[tempknz][1].invers=nil;
 map[tempknz][2].name=ende->ursprung; map[tempknz][2].invers=ende;
 ende->name=tempknz; ende->invers=map[tempknz]+2;
 *lastout=map[tempknz]+1;
 map[0][0].name=tempknz;
 return;
 }


/* es bleibt: mindestens zwei neue knoten */

tempknz++;
start->name=tempknz; start->invers=map[tempknz];
map[tempknz][0].name=start->ursprung; map[tempknz][0].invers=start;
map[tempknz][1].name=aussen; map[tempknz][1].invers=nil;
map[tempknz][2].name=tempknz+1; map[tempknz][2].invers=map[tempknz+1];

for (tempknz++; tempknz<new_tempknz; tempknz++)
    { map[tempknz][0].name=tempknz-1; map[tempknz][0].invers=map[tempknz-1]+2;
      map[tempknz][1].name=aussen; map[tempknz][1].invers=nil;
      map[tempknz][2].name=tempknz+1; map[tempknz][2].invers=map[tempknz+1]; }

/* und nun noch den letzten knoten */
map[tempknz][0].name=tempknz-1; map[tempknz][0].invers=map[tempknz-1]+2;
map[tempknz][1].name=aussen; map[tempknz][1].invers=nil;
map[tempknz][2].name=ende->ursprung; map[tempknz][2].invers= ende;
ende->name=tempknz; ende->invers=map[tempknz]+2;
*lastout=map[tempknz]+1;
map[0][0].name=tempknz;
}


/**********************SEQUENZ_KANONISCH***********************************/

int sequenz_kanonisch( int sequenz[] )
/* checkt, ob eine sequenz kanonisch ist, gibt 1 zurueck wenn ja, 0 sonst */

{ int i,j, laenge, max;
  int longseq[14];


  /*for (i=0; sequenz[i] != leer; i++) fprintf(stderr," %d ",sequenz[i]); fprintf(stderr,"\n");*/

  max=sequenz[0]; j=1; /* j hier nur als merker */
  for (i=0; sequenz[i] != leer; i++) { longseq[i]=sequenz[i];
				       if (longseq[i]==max) j=0;
				         else if (longseq[i]>max) { return(0); }
				     }
  if (j) return(1); /* der erste ist eindeutig, also die ganze sequenz */

  laenge=i;
  for (j=0; j<laenge; i++, j++) longseq[i]=sequenz[j];

for (j=1; j<laenge; i++, j++)
    if (longseq[j]==max)
	{ for (i=1; (i<laenge) && (longseq[j+i]==sequenz[i]) ; i++);
	  if (longseq[j+i]>sequenz[i]) 
                        { return(0); }
        }
return(1);
}



/***********************BERECHNE_SEQUENZ********************************/

void berechne_sequenz(SEQUENZ *sq, SEQUENZ altsq, int start,int f_ecke)
/* berechnet die neue sequenz startend bei der Kante start */
/* geht fest davon aus, dass 2 mal nach innen hintereinander nicht vorkommt */
/* zwei der Kanten-Eintraege koennen noch nicht belegt werden (die ersten beiden) */
/* start ist der laufindex */
{
int i, j, k, laenge,alt_laenge;
int *sequenz; 
int puffer[13];
char *kan;
KANTE **sqkanten;
KANTE *kpuffer[13];

/* if (sq->laenge==0) return; duerfte nicht vorkommen */
/*fprintf(stderr,"anfang berechne_sequenz\n");
schreibesequenz(altsq); fprintf(stderr,"5-Ecke: %d start: %d\n",f_ecke,start);*/

sequenz=sq->sequenz;
kan=sq->k_marks;
sqkanten=sq->kanten;

alt_laenge=altsq.laenge;
sq->laenge= alt_laenge-f_ecke;
laenge=sq->laenge;

for (i=0; i<alt_laenge; i++) { puffer[i]=puffer[i+alt_laenge]=altsq.sequenz[i];
			       kpuffer[i]=kpuffer[i+alt_laenge]=altsq.kanten[i]; }

if (puffer[start]==0) { fprintf(stderr,"Berechne_sequenz should not be called for 0-gaps ! \n");
                        exit(0); }

if (f_ecke==0)
  { if (laenge==1) sequenz[0]=puffer[0]+1;
    else 
    if (laenge==2)
         { if (puffer[start]>1) { sequenz[0]=puffer[start]-1; sequenz[1]=puffer[start+1]+2; }
	      else { sequenz[1]=0; sequenz[0]=puffer[start+1]+2; }
	 }
    else /* d.h. laenge > 2 */
      { if (puffer[start]>1)
	  {
	    sequenz[0]= puffer[start]-1;
	    sequenz[1]= puffer[start+1]+1;
	    sequenz[laenge-1]=puffer[start+laenge-1]+1;
	    sqkanten[laenge-1]=kpuffer[start+laenge-1];
	    for (j=2; j<laenge-1; j++) { sequenz[j]=puffer[start+j];
					 sqkanten[j]=kpuffer[start+j]; }
	  }
	else /* d.h. die luecke hat laenge 1 */
	  {
	    sequenz[laenge-1]= 0;
	    sequenz[0]= puffer[start+1]+1;
	    sequenz[laenge-2]=puffer[start+laenge-1]+1;
	    sqkanten[laenge-2]=kpuffer[start+laenge-1];
	    for (j=1; j<laenge-2; j++) { sequenz[j]=puffer[start+j+1];
					 sqkanten[j]=kpuffer[start+j+1]; }
	  }
      } /* ende laenge > 2 */
  } /* ende 0 Fuenfecke */

else
if (f_ecke==1)
  { if (laenge==0) sequenz[0]=puffer[0]+1;
    else
    if (laenge==1) sequenz[0]=puffer[0]+puffer[1]+1;
    else 
    if (laenge==2)
          { sequenz[0]=puffer[start] + puffer[start+1]; sequenz[1]=puffer[start+2]+1;
	    sqkanten[1]=kpuffer[start+2];}
    else /* d.h. laenge > 2 */
      { sequenz[0]= puffer[start]+puffer[start+1];
	sequenz[laenge-1]=puffer[start+laenge]+1;
	sqkanten[laenge-1]=kpuffer[start+laenge];
	for (j=1; j<laenge-1; j++) { sequenz[j]=puffer[start+j+1];
				     sqkanten[j]=kpuffer[start+j+1]; }
      }
  } /* ende 1 Fuenfecke */

else /* d.h. f_ecke==2 */
  { if (laenge==0) sequenz[0]=puffer[0]+puffer[1]+1;
    else { fprintf(stderr,"ERROR: Two 5-gons not leading to 0-Sequence !\n");
	   exit(0); }
  } /* ende 2 Fuenfecke */

if (sequenz[0]==0) /* nur im Fall eines einzelnen 6-Ecks moeglich */
  { puffer[0]=sequenz[0];
    for (i=0; i<laenge-1; i++) sequenz[i]=sequenz[i+1]; sequenz[laenge-1]=puffer[0];
    for (i=1; i<laenge-1; i++) sqkanten[i]=sqkanten[i+1]; }



if (laenge==0) { sequenz[1]=leer; kan[0]=1; return; }

sequenz[laenge]=leer;

kan[0]=sequenz_kanonisch(sequenz);

for (i=1; i < laenge; i++)
  { for (j=i, k=0; j<laenge; j++, k++) puffer[k]=sequenz[j];
    for (j=0; j<i; j++, k++) puffer[k]=sequenz[j];
    puffer[k]=leer;
    kan[i]=sequenz_kanonisch(puffer);
  }

}






/***********************BELEGE_SEQUENZ********************************/

void belege_sequenz( KANTE *start, SEQUENZ *sq)
/* belegt die sequenz startend bei der Kante start */
/* geht fest davon aus, dass 2 mal nach innen hintereinander nicht vorkommt */

{
int i, j, k, zaehler, position;
KANTE *run;
int *sequenz; 
KANTE **seqkanten;
int puffer[7];
char *kan;


sequenz=sq->sequenz;
seqkanten=sq->kanten;
kan=sq->k_marks;


if (start->next->invers->next->name == aussen) 
    { fprintf(stderr,"Achtung -- naechste Kante nicht nach innen -- FEHLER !\n");
      exit(0); }

for (i=0; i<7; i++) { sequenz[i]=leer; seqkanten[i]=nil; kan[i]=0;}

if (start->prev->invers->prev->name != aussen) /* d.h. vorige Kante nicht nach aussen, also nur
						  Randlaenge zu bestimmen */
    { sq->laenge=0;
      for (run=start->next->invers->next->invers->next, zaehler=1; run != start;
           run=run->next->invers->next->invers->next) zaehler++;
      sequenz[0]=zaehler;
      seqkanten[0]=start;
      return;
    }

sq->laenge=0;
zaehler=1;
position=0;
seqkanten[0]=start;


for (run=start->next->invers->next->invers->next; run->next->invers->next->name < aussen;
     run=run->next->invers->next->invers->next) zaehler++;
sequenz[0]=zaehler; position=1; 
if (run->next->invers->next != start) seqkanten[position]=run->next->invers->next;
for (run=run->next->invers->next; run->next->invers->next->name >= aussen;
     run=run->next->invers->next) 
{ sequenz[position]=0; position++;
if (run->next->invers->next != start) seqkanten[position]=run->next->invers->next; }
/* naechste Kante vor nicht-0-sequenz suchen */


while (run != start)
{
for (zaehler=0; run->next->invers->next->name < aussen; 
                run=run->next->invers->next->invers->next) zaehler++;
sequenz[position]=zaehler;  position++;
if (run->next->invers->next != start) seqkanten[position]=run->next->invers->next; 
for (run=run->next->invers->next; run->next->invers->next->name >= aussen;
     run=run->next->invers->next) { sequenz[position]=0;  position++;
if (run->next->invers->next != start) seqkanten[position]=run->next->invers->next; }

}

sequenz[position]=leer; seqkanten[position]=nil;
sq->laenge=position;

kan[0]=sequenz_kanonisch(sequenz);

for (i=1; sequenz[i] != leer; i++)
  { for (j=i, k=0; sequenz[j]!=leer; j++, k++) puffer[k]=sequenz[j];
    for (j=0; j<i; j++, k++) puffer[k]=sequenz[j];
    puffer[k]=leer;
    kan[i]=sequenz_kanonisch(puffer);
  }



}


/**********************CHECKSIZE_LEFT**************************************/

/* bestimmt die groesse der flaeche links von edge -- ist da keine gibt's Probleme */

int checksize_left(KANTE* edge)
{
KANTE *run; 
int zaehler=1;

for (run=edge->invers->next; run != edge; run=run->invers->next) zaehler++;
return(zaehler);
}


/*********************CODECMP*****************************************/

int codecmp(FLAECHENTYP *p1, FLAECHENTYP *p2, int max)
{
max--;
while ((*p1==*p2) && max) { p1++; p2++; max--; }
return( (int)(*p1)-(int)(*p2) );
}



/*************************CODIERE***************************************/
/* speziell fuer diese Flaechenstuecke. Sie werden ab der Marke von aussen
   in eine "Spirale" entwickelt. Die Eindeutigkeit ergibt sich nur zusammen
   mit der Sequenz. der "code" sind die stellen, an denen 5-Ecke vorkommen 

   Ein mieser sonderfall sind die 0-sequenzen. da kann nicht einfach nur abgewickelt
   werden. Die Codierung ist dort: erst die Anzahl der 6-Eck Schichten, dann die
   Anzahl der Spitzen, die man gegen den Uhrzeigersinn zurueckgehen muss, um ein 
   5-Eck zu finden und dann erst kann normal fortgefahren werden. Die Schichten werden 
   spiralfoermig abgebaut.

   laenge !=0 gilt nur fuer diese Situation. Dann ist laenge die anzahl der
   aussenkanten auf dem rand.

   Im Falle von 6 Pentagonen wird 1 zurueckgegeben, wenn der entwickelte Code
   kleinstmoeglich ist und 0 sonst.

   Fuer den miesen sonderfall wird der code aber (in der aufrufroutine) zum wegspeichern 
   so geaendert, dass die zweite stelle immer die anzahl N der verschiedenen markierten 
   Pflasterungen ist. die koennen dann erzeugt werden, indem 0 bis N Schritte zum ersten 
   5-Eck zurueckgegangen wird bei der Rekonstruktion.

   Die Anzahl der 6-Eck-Ringe ist immer 0 beim Aufruf.

*/


int codiere(PLANMAP map, FLAECHENTYP *code, KANTE *start, int codesize, int laenge)
{
int i, j, stelle, zaehler, knotenzahl, flaechennumber, merkeknoten, verschiebung;
/* zaehler zaehlt die Flaechengroesse, knotenzahl die zahl der restlichen knoten */
int tempknz, laufzaehler, autozaehler, minitest;
KANTE *run, *merke, *run2;
FLAECHENTYP testcode[9];

/*fprintf(stderr,"anfang codiere. codesize: %d start: %d \n", codesize, start->ursprung);
schreibemap(map);*/

if (start->name != aussen) { fprintf(stderr,"Codiere must start at external edge !\n"); 
			     exit(0); }

tempknz=map[0][0].name;

if (tempknz==5) { code[0]=1; return(1); }

for (i=1; i<=tempknz; i++) for (j=0; j<3; j++)
  if (map[i][j].name == aussen) map[i][j].dummy= infty; else map[i][j].dummy=0;

run=start; stelle=0; knotenzahl=tempknz;


if (laenge) /* d.h. 6-Fuenfecke-patch */
  { verschiebung=0;
    code[2]=unbelegt; 
    code[0]=0; /*code[1]=verschiebung;*/ stelle=2; 
    merkeknoten=knotenzahl;
    laufzaehler=1;
    /* Jetzt den minimalen Code ausrechnen: */
    for ( autozaehler=0, run2=start->prev; 
	 (verschiebung<laenge) && !autozaehler; verschiebung++, run2=run2->prev->invers->next->invers)
      if (checksize_left(run2)==5)
	{
	  laufzaehler++;
	  run=run2->next;
	  stelle=2; knotenzahl=merkeknoten;
	  flaechennumber=0;
	  while (stelle<codesize)
	    { flaechennumber++;
	      zaehler=2;
	      while (run->prev->invers->prev->dummy>=laufzaehler) 
		                                     run=run->prev->invers->prev; /* sicherstellen, dass davor
										     keine aussenkanten sind */
	      run->prev->invers->dummy=laufzaehler; run->next->invers->dummy=laufzaehler;
	      run=run->next->invers->next; knotenzahl--;
	      while (run->dummy>=laufzaehler)
		{ zaehler++; knotenzahl--; run->prev->invers->dummy=laufzaehler; 
		  run->next->invers->dummy=laufzaehler;
		  run=run->next->invers->next; }
	      merke=run->next;
	      run=run->prev;
	      while (merke->dummy<laufzaehler)
		{ zaehler++; merke=merke->invers->prev; }
	      if (zaehler==5) { testcode[stelle]=flaechennumber; stelle++; }
	      else if (zaehler!=6) { fprintf(stderr,"ERROR in CODIERE: No 5- or 6-Gon !\n"); exit(9); }
	      if (knotenzahl==5) { testcode[stelle]=flaechennumber+1; stelle++; }
	    } /* ende while */
	  if ((minitest=codecmp(code+2,testcode+2,6))>0) 
	    { if (verschiebung) return(0);
	      else { for (stelle=2; stelle<8; stelle++) code[stelle]=testcode[stelle]; }
	    }
	  if (minitest==0) autozaehler=1; /* verschiebung>=1 ist automatisch */
	} /* ende checksize==5 */
    if (autozaehler) { code[1]=verschiebung-1; automorphisms=(laenge/code[1]);
		       if (laenge % code[1]) { fprintf(stderr,"error in automorphism computation \n");
					       exit(0); }
		     }
    else { code[1]=laenge; automorphisms=1; }
    return(1);
  } /* ende if */


 flaechennumber=0;



while (stelle<codesize)
  { flaechennumber++;
    zaehler=2;
    while (run->prev->invers->prev->dummy) run=run->prev->invers->prev; /* sicherstellen, dass davor
									   keine aussenkanten sind */
    run->prev->invers->dummy=1; run->next->invers->dummy=1;
    run=run->next->invers->next; knotenzahl--;
    while (run->dummy)
      { zaehler++; knotenzahl--; run->prev->invers->dummy=1; run->next->invers->dummy=1;
	run=run->next->invers->next; }
    merke=run->next;
    run=run->prev;
    while (!(merke->dummy))
      { zaehler++; merke=merke->invers->prev; }
    if (zaehler==5) { code[stelle]=flaechennumber; stelle++; }
    else if (zaehler!=6) { fprintf(stderr,"ERROR in CODIERE(2): No 5- or 6-Gon !\n"); exit(0); }
    if (knotenzahl==5) { code[stelle]=flaechennumber+1; stelle++; }
  } /* ende while */

return(1);

}



/***********************ITEMALLOC********************************************/

ITEMLISTE *itemalloc()
/* gibt immer die Adresse eines neuen items zurueck */
{
static ITEMLISTE *back=nil; /* back enthaelt immer den letzten, der zurueckgegeben 
			       wurde -- erst hochsetzen -- wichtig */
static ITEMLISTE *last=nil;

if (back==last) { back=(ITEMLISTE *)malloc(sizeof(ITEMLISTE)*1001);
		  if (back==NULL) { fprintf(stderr,"Can not get more memory for items"); exit(0); }
		  last=back+1000;
		  return(back); }

/* else */
back++;
return(back);
}


/*******************PUT_IN_LISTE***********************************************/

void put_in_liste(int sechsecke, SEQUENZ sq, FLAECHENTYP *code, int codesize )
{

ITEMLISTE *item;
SEQUENZLISTE *anfang;
SEQUENZLISTE **puffer;
int i,j, s_eintrag;


/*schreibesequenz(sq);*/
/*schreibelistitems();*/


anfang=mapliste.sechser[0]; /* schreibt immer in den ersten -- umgemodelt */
mapliste.total_maps++;
for (i=0; i<sq.laenge; i++)
  { 
    s_eintrag=sq.sequenz[i]; 
    if (anfang->number_next <= s_eintrag)
      { puffer=anfang->next_level;
	anfang->next_level=(SEQUENZLISTE **)malloc((s_eintrag+1)*sizeof(SEQUENZLISTE *));
	if (anfang->next_level==NULL) { fprintf(stderr,"Can not get more memory"); exit(0); }
	for (j=0; j< anfang->number_next; j++) anfang->next_level[j]=puffer[j];
	for (   ; j <= s_eintrag; j++) anfang->next_level[j]=nil;
	anfang->number_next=s_eintrag+1;
	free(puffer);
      }
    if (anfang->next_level[s_eintrag]==nil)
      {	anfang->next_level[s_eintrag]=(SEQUENZLISTE *)malloc(sizeof(SEQUENZLISTE));
	if (anfang->next_level[s_eintrag]==NULL) { fprintf(stderr,"Can not get more memory"); exit(0); }
	anfang=anfang->next_level[s_eintrag];
	anfang->next_level=nil; anfang->number_next=0;
	anfang->items=anfang->last_item=nil;
      }
    else anfang=anfang->next_level[s_eintrag];
  } /* ende for */

/* jetzt muesste anfang passend stehen zum Eintragen des Codes */

if (anfang->items==nil) item=anfang->items=anfang->last_item=itemalloc(); 
   else { item=anfang->last_item->next_item=itemalloc(); 
	  anfang->last_item=item; }
item->next_item=nil;
for (j=0; j<codesize; j++) item->code[j]=code[j];
}



/************************FUENFTEST***************************************/

/* Ueberprueft, ob ein vierer eventuell nuetzlich sein kann -- d.h. einige einfach
   herzuleitende Eigenschaften hat */

int fuenftest(int *sq0)
{

int sq[10], i;

for (i=0; i<5; i++) sq[i]=sq[i+5]=sq0[i];

for (i=0; i<5; i++)
  { if (sq[i]>max_entry) return(0);
    if ((sq[i]==0) && (sq[i+2]==second_part) && (sq[i+3]==first_part+1)) return(1); }
return(0);
}




/************************VIERTEST***************************************/

/* Ueberprueft, ob ein vierer eventuell nuetzlich sein kann */

int viertest(int *sq0)
{

int sq[8], i;

for (i=0; i<4; i++) sq[i]=sq[i+4]=sq0[i];

for (i=0; i<4; i++)
  { if (sq[i]>max_entry) return(0);
    if (sq[i]==0) { if ((sq[i+2]==second_part) || (sq[i+2]==first_part+1)) return(1); }
    /* alle mit 0 werden jetzt also erkannt. Es fehlen noch die mit first_part+1 und second_part */
    else if ((sq[i]==second_part) && (sq[i+1]==first_part+1)) return(1);
  }
return(0);
}


/************************SCHREIBE_AUF************************************/

void schreibe_auf(PLANMAP map, SEQUENZ sq, int rest_sechsecke, int durchlauf)
/* codiert und schreibt eine markierte Pflasterung -- wird nur fuer
   bis zu 5 Fuenfecken aufgerufen */
{
int fuenfecke, sechsecke, i, codesize, pfadlaenge, treffer;
FLAECHENTYP code[9];
int merke_ipr;

if (bbpatch) return;
if (IPR && (!is_ipr)) return;

if (durchlauf==2) 
    { if (sq.laenge>3) return;
      if (sq.laenge==1) { if ((first_part + second_part + sq.sequenz[0] + 2)> rest_sechsecke) 
			    return;  }
      /* im anderen Teil eines Einschlusses liegen immer mindestens i+j+sq[0]+3 Flaechen
	 -- das ergibt sich aus der Randkodierung (wenig Nullen, deshalb alle Flaechen am Rand
	 verschieden), also i+j+sq[0]+2 sechsecke */
      else
      if (sq.laenge==2) { if ((sq.sequenz[1]>first_part-1) && (sq.sequenz[1]>second_part-2))
			    return; }
      else /* d.h. (sq.laenge==3) */
                        /* dann hat man Kriterien, die einige Patches sofort als unnuetz 
			   erkennen : first_part+1 oder second_part muessen
			   bei sql=3 im zweiten Durchlauf vorkommen */
	{ 
	  for (i=0; i<sq.laenge; i++) 
	    if ((sq.sequenz[i]==(first_part+1)) || (sq.sequenz[i]==second_part)) treffer=1; 
	  if (!treffer) return;
	}

      fuenfecke=6-sq.laenge;

      pfadlaenge=sq.laenge;
      for (i=0; i< sq.laenge; i++) pfadlaenge += (2*sq.sequenz[i]);

      sechsecke=(map[0][0].name)-10+sq.laenge-sq.sequenz[0];
      for (i=1; i<sq.laenge; i++) sechsecke -= sq.sequenz[i];
      sechsecke=sechsecke/2;        /* alles leicht aus Euler Formel */
      if (sechsecke+rest_sechsecke != max_sechsecke) 
	{ fprintf(stderr,"Error in 6-gon calculation (schreibe_auf) !\n");
						 exit(0); }

      codesize=fuenfecke; 
      codiere(map, code, sq.kanten[0], fuenfecke,0);
      merke_ipr=is_ipr;
      konstruiere(sechsecke, sq, code, codesize );
      is_ipr=merke_ipr;
      return;
    } /* ende durchlauf==2 */

/* ab hier ist durchlauf==1 */
if (sq.laenge<3) return;

if (sq.laenge==3) /* Vereinbarung: Im ersten Durchlauf muss der patch erzeugt werden, der
		     eine Null und einen Eintrag <= first_part oder second_part-1 enthaelt 
		     eine Null steht auf jeden Fall an Position 1 oder 2: */
                 { if (sq.sequenz[0]>max_entry) return;
		   if (sq.sequenz[2]!=0)
		     { if ((sq.sequenz[1]!=0) || ((sq.sequenz[2]>first_part) 
						  && (sq.sequenz[2]>second_part-1))) return; }
		   else if ((sq.sequenz[1]>first_part) && (sq.sequenz[1]>second_part-1)) return; 
		 }
else if(sq.laenge==4) { if (!viertest(sq.sequenz)) return; }
else if(sq.laenge==5) if (!fuenftest(sq.sequenz)) return;

(storepatchzaehler[sq.laenge])++;

fuenfecke=6-sq.laenge;

/*for (i=non_nuller=0; i< sq.laenge; i++) if (sq.sequenz[i] != 0) non_nuller++;
if (non_nuller >2) return;*/ /* Bei jedem Patch der GEBRAUCHT wird, koennen nur
			      an 2 Stellen Probleme auftreten */


pfadlaenge=sq.laenge;
for (i=0; i< sq.laenge; i++) pfadlaenge += (2*sq.sequenz[i]);

sechsecke=(map[0][0].name)-10+sq.laenge-sq.sequenz[0];
for (i=1; i<sq.laenge; i++) sechsecke -= sq.sequenz[i];
sechsecke=sechsecke/2;        /* alles leicht aus Euler Formel */


if (sechsecke+rest_sechsecke != max_sechsecke) { fprintf(stderr,"Error in 6-gon calculation (schreibe_auf) !\n");
						 exit(0); }

codesize=fuenfecke; 
codiere(map, code, sq.kanten[0], fuenfecke,0);

/*for (i=0; i<fuenfecke; i++) fprintf(stderr," %d ",code[i]);
fprintf(stderr,"\n");*/

put_in_liste(sechsecke, sq, code, codesize );

return;
	   }



/******************************TESTCANON_MIRROR*****************************************/

static int 
testcanon_mirror(KNOTENTYP nv, KANTE *givenedge, int representation[])

/* Tests whether starting from a given edge and constructing the code in
   "->prev" direction, an automorphism or even a better representation can 
   be found. Returns 0 for a larger representation (worse), 1 for an automorphism and 2 for 
   a smaller representation (better). Comments see testcanon in "plantri.c" -- it is 
   exactly the same except for the orientation and obviously necessary changes */
{
	KANTE *temp, *run;  
	static KANTE *startedge[MAXN+2]; /* startedge[i] is the starting edge for 
			       exploring the vertex with the number i+1 */
	static int number[MAXN+2], i; /* The new numbers of the vertices, starting 
				at 1 in order to have "0" as a possibility to
		                mark ends of lists and not yet given numbers */
	int last_number, actual_number, vertex, col;


	for (i = 1; i <= nv+1; i++) { number[i] = 0; startedge[i]=nil; }

	number[givenedge->ursprung] = 1; 
	number[givenedge->name] = 2;
	actual_number = 1;
	last_number = 2;
	temp = givenedge;
	startedge[1] = givenedge->invers;

/* A representation is a clockwise ordering of all neighbours ended with a 0.
   The neighbours are numbered in the order that they are reached by the BFS 
   procedure. Every number is preceded by the colour of the vertex.
   Since every representation starts with "2", we do not 
   have to note that. Every first entry in a new clockwise ordering (and its 
   colour) is determined by the entries before (the first time it occurs in 
   the list to be exact), so this is not given either. 
   The K4 could be denoted  3 4 0 4 3 0 2 4 0 3 2 0 */

	while (temp)
	{  
   	    for (run = temp->prev; run != temp; run = run->prev)
	    /* this loop marks all edges around temp->origin. */
	      { vertex = run->name;
		if (vertex != aussen)
		  { if (!number[vertex]) { startedge[last_number] = run->invers;
					   last_number++; number[vertex] = last_number; }
		    vertex = number[vertex];
		    if (vertex > (*representation)) return(0);
		    if (vertex < (*representation)) return(2);
		    representation++;
		  }
	      }
	      
   /* check whether representation[] is also at the end of a cyclic list */
   	    if ((*representation) != 0) return(2); 
   	    representation++;
   /* Next vertex to explore: */
   	    temp = startedge[actual_number];  actual_number++; 
	}
	if (int_infty > *representation) return(0);
	return(1);
}
/******************************TESTCANON*****************************************/

static int 
testcanon(KNOTENTYP nv, KANTE *givenedge, int representation[])

/* Tests whether starting from a given edge and constructing the code in
   "->next" direction, an automorphism or even a better representation can 
   be found. Returns 0 for a larger representation (worse), 1 for an automorphism and 2 for 
   a smaller representation (better). Comments see testcanon in "plantri.c" */
{
	KANTE *temp, *run;  
	static KANTE *startedge[MAXN+2]; /* startedge[i] is the starting edge for 
			       exploring the vertex with the number i+1 */
	static int number[MAXN+2], i; /* The new numbers of the vertices, starting 
				at 1 in order to have "0" as a possibility to
		                mark ends of lists and not yet given numbers */
	int last_number, actual_number, vertex, col;


	for (i = 1; i <= nv+1; i++) { number[i] = 0; startedge[i]=nil; }

	number[givenedge->ursprung] = 1; 
	number[givenedge->name] = 2;
	actual_number = 1;
	last_number = 2;
	temp = givenedge;
	startedge[1] = givenedge->invers;

/* A representation is a clockwise ordering of all neighbours ended with a 0.
   The neighbours are numbered in the order that they are reached by the BFS 
   procedure. Every number is preceded by the colour of the vertex.
   Since every representation starts with "2", we do not 
   have to note that. Every first entry in a new clockwise ordering (and its 
   colour) is determined by the entries before (the first time it occurs in 
   the list to be exact), so this is not given either. 
   The K4 could be denoted  3 4 0 4 3 0 2 4 0 3 2 0 */

	while (temp)
	{  
   	    for (run = temp->next; run != temp; run = run->next)
	    /* this loop marks all edges around temp->origin. */
	      { vertex = run->name;
		if (vertex != aussen)
		  { if (!number[vertex]) { startedge[last_number] = run->invers;
					   last_number++; number[vertex] = last_number; }
		    vertex = number[vertex];
		    if (vertex > (*representation)) return(0);
		    if (vertex < (*representation)) return(2);
		    representation++;
		  }
	      }
	      
   /* check whether representation[] is also at the end of a cyclic list */
   	    if ((*representation) != 0) return(2); 
   	    representation++;
   /* Next vertex to explore: */
   	    temp = startedge[actual_number];  actual_number++; 
	}
	if (int_infty > *representation) return(0);
	return(1);
}


/******************************TESTCANON_INIT**********************************************/

static int 
testcanon_init(KNOTENTYP nv, KANTE *givenedge, int representation[])

/* Tests whether starting from a given edge and constructing the code in
   "->next" direction, an automorphism or even a better representation can 
   be found. A better representation will be completely constructed and 
   returned in "representation".  It works pretty similar to testcanon except 
   for obviously necessary changes, so for extensive comments see testcanon */
{
	register KANTE *run;
	register int col, vertex;
	KANTE *temp;  
	static KANTE *startedge[MAXN+2]; 
	static int number[MAXN+2], i; 
	int better = 0; /* is the representation already better ? */
	int last_number, actual_number;

	for (i = 1; i <= nv+1; i++) { number[i] = 0; startedge[i]=nil; }

	number[givenedge->ursprung] = 1; 
	number[givenedge->name] = 2;
	actual_number = 1;
	last_number = 2;
	temp = givenedge;
	startedge[1] = givenedge->invers;

	while (last_number < nv)
	{  
   	    for (run = temp->next; run != temp; run = run->next)
	      { 
		vertex = run->name;
		if (vertex != aussen)
		  {
		    if (!number[vertex]) { 
		                           startedge[last_number] = run->invers;
					   last_number++; number[vertex] = last_number; }
		    if (better)
		      {	
			*representation = number[vertex]; representation++; }
		    else
		      { 
		        vertex = number[vertex];
		        if (vertex > (*representation)) return(0);
		        if (vertex < (*representation)) { better = 1;  
						   *representation = vertex; }
		        representation++;
		      }
		  } /* ende vertex != aussen */
	      }
   	    if ((*representation) != 0) { better = 1; *representation = 0; }
   	    representation++; 
   	    temp = startedge[actual_number];  actual_number++;
	  }

	while (actual_number <= nv) 
	  {
	    for (run = temp->next; run != temp; run = run->next)
	      { vertex = run->name; 
		if (vertex != aussen)
		  {
		    if (better)
		      { *representation = number[vertex]; representation++; }
		    else
		      {
			vertex = number[vertex];
			if (vertex > (*representation)) return(0);
			if (vertex < (*representation)) { better = 1;  
							  *representation = vertex; }
			representation++;
		      }
		  }
	      }
   	    if ((*representation) != 0) { better = 1; *representation = 0; }
   	    representation++;
   	    temp = startedge[actual_number];  actual_number++;
	}

	if (better) return(2);
	return(1);
}

/**************************TESTCANON_FIRST_INIT***************************************/
 
static void
testcanon_first_init(KNOTENTYP nv, KANTE *givenedge, int representation[])
 
/* Tests whether starting from a given edge and constructing the code in
   "->next" direction, an automorphism or even a better representation can 
   be found. A better representation will be completely constructed and 
   returned in "representation".  It works pretty similar to testcanon_mirror except 
   for obviously necessary changes, so for extensive comments see testcanon */
{
	register KANTE *run;
	register vertex;
	KANTE *temp;  
	static KANTE *startedge[MAXN+2]; 
	static int number[MAXN+2], i; 
	int last_number, actual_number;
 
	/* vorsicht -- nv ist nur eine obere Schranke fuer die Knotenzahl */
	for (i = 1; i <= nv+1; i++) { number[i] = 0; startedge[i]=nil; }
 
	number[givenedge->ursprung] = 1; 
	number[givenedge->name] = 2;
	actual_number = 1;
	last_number = 2;
	temp = givenedge;
	startedge[1] = givenedge->invers;
 
	while (temp)
	{  
	    for (run = temp->next; run != temp; run = run->next)
	      { vertex = run->name;
		if (vertex != aussen)
		  {
		    if (!number[vertex]) { startedge[last_number] = run->invers;
					   last_number++; number[vertex] = last_number; }
		    *representation = number[vertex]; representation++;
		  }
	      }
	    *representation = 0;
	    representation++;
	    temp = startedge[actual_number];  actual_number++;
	}
	*representation=int_infty; /* Zeichen, dass der Code zu Ende ist */
	return;
}


/***********************DFS*****************************/

void dfs( KANTE *start, KNOTENTYP *nummer, KANTE **first_edge, int *next_number)

/* belegt einfach durch dfs die nummern und die startkanten */

{ 

KANTE *run;

/* start->name hat immer schon eine Nummer -- daher kommt man */


for (run=start->next; run!=start; run=run->next)
  if ((run->name != aussen) && (!nummer[run->name]))
    { nummer[run->name]= *next_number; first_edge[*next_number]=run->invers;
      (*next_number)++;
      dfs(run->invers, nummer, first_edge, next_number);
    }
}



/***********************BELEGE_NUMMERN*****************************/

void belege_nummern( KANTE *start, KNOTENTYP *nummer, KANTE **first_edge, int *knotenzahl)

/* belegt einfach durch dfs die nummern und die startkanten */

{ 

int i, next_number;
KANTE *run;


for (i=1; i<= 3*N; i++) nummer[i]=0;

while (start->name == aussen) start=start->next;
nummer[start->ursprung]=1; first_edge[1]=start;
nummer[start->name]=2; first_edge[2]=start->invers;

next_number=3;
dfs(start->invers, nummer, first_edge, &next_number);
for (run=start->next; run!=start; run=run->next)
  if ((run->name != aussen) && (!nummer[run->name]))
    { nummer[run->name]=next_number; first_edge[next_number]=run->invers;
      next_number++;
      dfs(run->invers, nummer, first_edge, &next_number);
    }

*knotenzahl=next_number-1; 
}


/************************VEGACODE_TUBE*****************************/

/* codiert die gesamte Zusammenhangskomponente, die die Knoten um
   map[1] enthaelt, auch wenn urspruenglich nicht in dem Vektor
   map enthaltene Kanten und Knoten vorkommen */


int vegacode_tube( FILE *fil, PLANMAP map )

{

int i,j;
KANTE *run;


belege_nummern(map[1],nummer,first_edge,&knotenzahl_incl_tube);

for (i=1; i<=knotenzahl_incl_tube; i++)
  { fprintf(fil,"%d  0 0 0 ",i);
    run=first_edge[i];
    do
      { if (run->name != aussen) fprintf(fil," %d",nummer[run->name]);
        run=run->next; }
    while (run != first_edge[i]);
    fprintf(fil,"\n");
  }

fprintf(fil,"0\n");

}

/************************VEGACODE_TUBE_MIRROR*****************************/


/* muss immer NACH planarcode_tube benutzt werden, damit nummer und first_edge
   passend belegt sind */


int vegacode_tube_mirror( FILE *fil, PLANMAP map )

{
int i,j;
KANTE *run;


for (i=1; i<=knotenzahl_incl_tube; i++)
  { fprintf(fil,"%d  0 0 0 ",i);
    run=first_edge[i];
    do
      { if (run->name != aussen) fprintf(fil," %d",nummer[run->name]);
        run=run->prev; }
    while (run != first_edge[i]);
    fprintf(fil,"\n");
  }

fprintf(fil,"0\n");

}


/*************************PLANARCODE_TUBE*****************************/

/* siehe vegacode_tube */

int planarcode_tube( FILE *fil, PLANMAP map )
{
/* Codiert die Einbettung in codeK oder codeF und gibt die laenge des codes zurueck */
KANTE *run;
int i,zaehler;
KANTE *lauf;
unsigned char codeF[12*N+1];
unsigned short codeK[12*N+1];

belege_nummern(map[1],nummer,first_edge,&knotenzahl_incl_tube);

if (knotenzahl_incl_tube <= UCHAR_MAX)
{ 
zaehler=1;
codeF[0]=knotenzahl_incl_tube;
for(i=1;i<=knotenzahl_incl_tube;i++)
    { lauf=first_edge[i];
      do { if (lauf->name != aussen) { codeF[zaehler]=nummer[lauf->name]; zaehler++; }
	   lauf=lauf->next; }
      while (lauf != first_edge[i]);
      codeF[zaehler]=0; zaehler++; } 
fwrite(codeF,sizeof(unsigned char),zaehler,fil);
}
else /* zu viele knoten fuer unsigned char */
{
zaehler=1;
codeK[0]=knotenzahl_incl_tube;
for(i=1;i<=knotenzahl_incl_tube;i++)
    { lauf=first_edge[i];
      do { if (lauf->name != aussen) { codeK[zaehler]=nummer[lauf->name]; zaehler++; }
	   lauf=lauf->next; }
      while (lauf != first_edge[i]);
      codeK[zaehler]=0; zaehler++; }
fputc(0,fil);
fwrite(codeK,sizeof(unsigned short),zaehler,fil); 
}

return(zaehler);
}



/*************************PLANARCODE_TUBE_MIRROR*****************************/

/* siehe vegacode_tube */

/* muss immer NACH planarcode_tube benutzt werden, damit nummer und first_edge
   passend belegt sind */

int planarcode_tube_mirror( FILE *fil, PLANMAP map )
{
/* Codiert die Einbettung in codeK oder codeF und gibt die laenge des codes zurueck */
KANTE *run;
int i,zaehler;
KANTE *lauf;
unsigned char codeF[12*N+1];
unsigned short codeK[12*N+1];



if (knotenzahl_incl_tube <= UCHAR_MAX)
{ 
zaehler=1;
codeF[0]=knotenzahl_incl_tube;
for(i=1;i<=knotenzahl_incl_tube;i++)
    { lauf=first_edge[i];
      do { if (lauf->name != aussen) { codeF[zaehler]=nummer[lauf->name]; zaehler++; }
	   lauf=lauf->prev; }
      while (lauf != first_edge[i]);
      codeF[zaehler]=0; zaehler++; } 
fwrite(codeF,sizeof(unsigned char),zaehler,fil);
}
else /* zu viele knoten fuer unsigned char */
{
zaehler=1;
codeK[0]=knotenzahl_incl_tube;
for(i=1;i<=knotenzahl_incl_tube;i++)
    { lauf=first_edge[i];
      do { if (lauf->name != aussen) { codeK[zaehler]=nummer[lauf->name]; zaehler++; }
	   lauf=lauf->prev; }
      while (lauf != first_edge[i]);
      codeK[zaehler]=0; zaehler++; }
fputc(0,fil);
fwrite(codeK,sizeof(unsigned short),zaehler,fil); 
}

return(zaehler);
}






/************************VEGACODE*****************************/

int vegacode( FILE *fil, PLANMAP map )

{

int i,j;
KANTE *run;

for (i=1; i<=map[0][0].name; i++)
  { fprintf(fil,"%d  %d %d   %d %d ",i,rand()%200,rand()%200,map[i][0].name,map[i][1].name);
    if (map[i][2].name != aussen) fprintf(fil,"%d\n",map[i][2].name);
    else fprintf(fil,"\n");
  }
fprintf(fil,"0\n%d ",randlaenge);

if (bbpatch)
  { for (i=map[0][0].name, run=map[randlaenge]+2; run->name != aussen; i--)
      for (j=0; (j<3) && (run->name != aussen); j--) run=map[i]+j;
    /* jetzt ist eine aussenkante gefunden */
    for (i=1; i<=randlaenge; i+=2) { fprintf(fil,"%d ",run->ursprung);
				     run=run->next->invers; fprintf(fil,"%d ",run->ursprung);
				     run=run->next->invers->next; }
  }
else
  { for (i=1; i<=randlaenge; i++) fprintf(fil,"%d ",i); }
    
fprintf(fil,"\n"); 
}

/************************VEGACODE_MIRROR*****************************/

int vegacode_mirror( FILE *fil, PLANMAP map )

{

int i,j;
KANTE *run;

for (i=1; i<=map[0][0].name; i++)
  { fprintf(fil,"%d  %d %d   %d %d ",i,rand()%200,rand()%200,map[i][1].name,map[i][0].name);
    if (map[i][2].name != aussen) fprintf(fil,"%d\n",map[i][2].name);
    else fprintf(fil,"\n");
  }
fprintf(fil,"0\n%d ",randlaenge);

if (bbpatch)
  { for (i=map[0][0].name, run=map[randlaenge]+2; run->name != aussen; i--)
      for (j=0; (j<3) && (run->name != aussen); j--) run=map[i]+j;
    /* jetzt ist eine aussenkante gefunden */
    for (i=1; i<=randlaenge; i+=2) { fprintf(fil,"%d ",run->ursprung);
				     run=run->prev->invers; fprintf(fil,"%d ",run->ursprung);
				     run=run->prev->invers->prev; }
  }
else
  { for (i=1; i<=randlaenge; i++) fprintf(fil,"%d ",i);
    fprintf(fil,"\n"); }
}


/*************************PLANARCODE*****************************/

int planarcode( FILE *fil, PLANMAP map )
{
/* Codiert die Einbettung in codeK oder codeF und gibt die laenge des codes zurueck */
int i,zaehler;
KANTE *merke, *lauf;
unsigned char codeF[30+6*S+N];
unsigned short codeK[30+6*S+N];

if (noout) return(0);

if (map[0][0].name <= UCHAR_MAX)
{ 
zaehler=1;
codeF[0]=map[0][0].name;
for(i=1;i<=map[0][0].name;i++)
    { 
      merke=map[i]; if (merke->name != aussen) { codeF[zaehler]=merke->name; zaehler++; }
      for(lauf=merke->next; lauf!=merke; lauf=lauf->next) 
	           { if (lauf->name != aussen) { codeF[zaehler]=lauf->name; zaehler++; } }
      codeF[zaehler]=0; zaehler++; }
fwrite(codeF,sizeof(unsigned char),zaehler,fil);
}
else /* zu viele knoten fuer unsigned char */
{
zaehler=1;
codeK[0]=map[0][0].name;
for(i=1;i<=map[0][0].name;i++)
    { merke=map[i];  if (merke->name != aussen) { codeK[zaehler]=merke->name; zaehler++; }
      for(lauf=merke->next; lauf!=merke; lauf=lauf->next) 
	           { if (lauf->name != aussen) { codeK[zaehler]=lauf->name; zaehler++; } }
      codeK[zaehler]=0; zaehler++; }
fputc(0,fil);
fwrite(codeK,sizeof(unsigned short),zaehler,fil); 
}

return(zaehler);
}


/*************************PLANARCODE_MIRROR*****************************/

int planarcode_mirror( FILE *fil, PLANMAP map )
{
/* Codiert die Einbettung in codeK oder codeF und gibt die laenge des codes zurueck */
int i,zaehler;
KANTE *merke, *lauf;
unsigned char codeF[30+6*S+N];
unsigned short codeK[30+6*S+N];

if (noout) return(0);

if (map[0][0].name <= UCHAR_MAX)
{ 
zaehler=1;
codeF[0]=map[0][0].name;
for(i=1;i<=map[0][0].name;i++)
    { 
      merke=map[i]; if (merke->name != aussen) { codeF[zaehler]=merke->name; zaehler++; }
      for(lauf=merke->prev; lauf!=merke; lauf=lauf->prev) 
	           { if (lauf->name != aussen) { codeF[zaehler]=lauf->name; zaehler++; } }
      codeF[zaehler]=0; zaehler++; }
fwrite(codeF,sizeof(unsigned char),zaehler,fil);
}
else /* zu viele knoten fuer unsigned char */
{
zaehler=1;
codeK[0]=map[0][0].name;
for(i=1;i<=map[0][0].name;i++)
    { merke=map[i];  if (merke->name != aussen) { codeK[zaehler]=merke->name; zaehler++; }
      for(lauf=merke->prev; lauf!=merke; lauf=lauf->prev) 
	           { if (lauf->name != aussen) { codeK[zaehler]=lauf->name; zaehler++; } }
      codeK[zaehler]=0; zaehler++; }
fwrite(codeK,sizeof(unsigned short),zaehler,fil); 
}

return(zaehler);
}



/*************************BB_KANONISCH*************************/

int bb_kanonisch(PLANMAP map)
/* ueberprueft, ob der Patch oder sein Spiegelbild kanonisch sind. Gibt 1 zurueck, 
   wenn der patch kanonisch ist und keine Spiegelsymmetrie hat, 2 wenn der patch 
   kanonisch ist und eine Spiegelsymmetrie hat, die eine Flaeche stabilisiert,
   3 wenn der patch kanonisch ist und eine Spiegelsymmetrie hat, die keine Flaeche 
   stabilisiert, und 0 sonst (d.h. wenn das Spiegelbild kanonisch ist). 

   check_mark_und_schreibe hat schon ueberprueft, ob er relativ zur Orientierung kanonisch
   ist, hier muss also nur noch gecheckt werden, welche der beiden Orientierungen gewaehlt
   wird.

*/

{
KANTE *run, *start;
int i, j, gefunden, merkewo;
int representation[4*N];

/* suche eine Aussenkante */

for (i=map[0][0].name, gefunden=0; (i>=1) && !gefunden; i--)
  for (j=0; (j<3) && !gefunden; j++) 
    if (map[i][j].name==aussen) { start=map[i]+j; gefunden=1; }

merkewo=0;
			    
testcanon_first_init(map[0][0].name, start->next, representation);
run=start->next->invers->next->invers->next;

/* gefunden wird jetzt fuer automorphismen verwendet (dann kann abgebrochen werden) */

for (i=1, gefunden=0; 
     (i<first_part) && (gefunden != 1); run=run->next->invers->next->invers->next, i++)
  { 
    gefunden=testcanon_init(map[0][0].name, run->next, representation);
    if (gefunden==2) /* better representation */ merkewo=i;}

/* merkewo ist jetzt 0, wenn die repraesentation schon bei start OK ist */

gefunden=0;
/* und jetzt schauen, ob man in die andere Richtung etwas besseres findet. Wird eine
   Spiegelsymmetrie gefunden, kann abgebrochen werden */
for (i=0, run=start; (i<first_part) ; run=run->next->invers->next->invers->next, i++)
  { gefunden=testcanon_mirror(map[0][0].name, run->prev, representation);
    if (gefunden==2) return(0);
    if (gefunden==1) { mit_spiegelsymmetrie++; 
		       mirror_symmetric=1; automorphisms *= 2;
		       if ((merkewo+i)%2) return(3); else return(2); } 
  }

return(1);
}


/**************************KANONISCH**************************/

int kanonisch(PLANMAP map)

/* Wird aufgerufen fuer first_part=second_part patches. Gibt 1 zurueck,
   wenn der patch kanonisch ist und 0, wenn das Spiegelbild kanonisch ist */

{

int representation[4*N];
int test;

fprintf(stderr,"should not be here............\n");

testcanon_first_init(map[0][0].name, map[1]+2, representation);

/* und jetzt schauen, ob man in die andere Richtung etwas besseres findet */

test=testcanon_mirror(map[0][0].name, map[randlaenge]+2, representation);
if (test==1) mit_spiegelsymmetrie++;
if (test==2) return(0);

return(1);
}

/*************************GLUE*****************************************************/

/* Verklebt cap und tube */

void glue (PLANMAP map, PLANMAP tube, KANTE **merkestart)

/* merkestart merkt sich im BBfall, wo die Verklebung beim Patch gestartet ist */

{

int i,j;
KANTE *merket, *merkem, *dummy;

/* schreibemap(map); schreibemap(tube); */


if (bbpatch)
  { /* suche aussenkante */
    for (i=map[0][0].name, merkem=nil; merkem==nil; i++) 
      for (j=0; j<3; j++) if (map[i][j].name==aussen) *merkestart=merkem=map[i]+j;
    merket=tube[2]+2;
    for (i=1; i<= first_part; i++)
      { 
	merkem->name=merket->name;
	merkem->invers=merket->invers;
	dummy=merket->invers;
	dummy->invers=merkem;
	dummy->name=merkem->ursprung;
	merkem=merkem->next->invers->next->invers->next;
	merket=merket->next->invers->next->invers->next;
      }
    return;
  }


for (i=2; i<= 2*(second_part+1); i+=2)
  { merket=tube[i]+2; merkem=map[i]+2;
    merkem->name=merket->name;
    merkem->invers=merket->invers;
    merket=merket->invers;
    merket->invers=merkem;
    merket->name=merkem->ursprung;
  }

i--;
merket=tube[i]+2; merkem=map[i]+2;
merkem->name=merket->name;
merkem->invers=merket->invers;
merket=merket->invers;
merket->invers=merkem;
merket->name=merkem->ursprung;

for (  ; i< randlaenge; i+=2)
  { merket=tube[i]+2; merkem=map[i]+2;
    merkem->name=merket->name;
    merkem->invers=merket->invers;
    merket=merket->invers;
    merket->invers=merkem;
    merket->name=merkem->ursprung;
  }

/*fprintf(stderr,"nach glue: \n"); schreibemap(map); schreibemap(tube);*/

map[0][0].name += (tube[0][0].name-randlaenge);



}


/*************************UNGLUE*****************************************************/

/* Trennt cap und tube */
/* die Werte bei tube werden nicht wieder restauriert -- sie werden spaeter bei
   einer neuen Verklebung einfach ueberschrieben */


void unglue (PLANMAP map, PLANMAP tube, KANTE *start)

{

int i;
KANTE *run;


if (bbpatch)
  { 
    for (i=1; i<= first_part; i++)
      { 
	start->name=aussen;
	start->invers=nil;
	start=start->next->invers->next->invers->next;
      }
    return;
  }




for (i=2; i<= 2*(second_part+1); i+=2)
  { map[i][2].name=aussen;
    map[i][2].invers=nil; }

i--;
map[i][2].name=aussen;
map[i][2].invers=nil;

for (  ; i< randlaenge; i+=2)
  { map[i][2].name=aussen;
    map[i][2].invers=nil; }

map[0][0].name -= (tube[0][0].name-randlaenge);
}


/*************************AUSGABE*****************************/

void ausgabe( FILE *fil, PLANMAP map )

{
KANTE *merkestart;

all_caps++;


if (bbpatch) { if (!bb_kanonisch(map)) return; }
else if (!check_rand(map)) return;

if (mirror_symmetric) (auts_statistic_mirror[automorphisms])++;
  else (auts_statistic[automorphisms])++;
graphenzaehler++;
if (noout) return;
if (vegaout)
    {if (dangling_tube) 
      {  glue(map,tube,&merkestart);
	 vegacode_tube(fil,map);
	 unglue(map,tube,merkestart);
	 return; }
    vegacode(fil,map);
    return; } /* ende vegaout */
/* else -- d.h. planarcode */
if (dangling_tube) 
  {  glue(map,tube,&merkestart);  
     planarcode_tube(fil,map);
     unglue(map,tube,merkestart);
     return; }
planarcode(fil,map);
return;
}

/************************CHECK_RAND_WEAK**************************************************/

int check_rand_weak(PLANMAP map)         
/*                                             _                                         */
/* Checkt, ob auf einem oder beiden Wegen von / \ zu \_/ ein 5-Eck liegt. 
   Gibt 0 zurueck, wenn auf keinem Weg, 1, wenn auf einem Weg und 2 wenn auf beiden
   Wegen ein 5-Eck liegt. */

{
int i, treffer=0, abbruch;
KANTE *anfang, *merkestart, *codestart;
int code[4*N+2];
int test;

fprintf(stderr,"Not repaired !!!!!\n");
fprintf(stderr,"Check self-intersecting paths !\n");
exit(0);

anfang=map[2*second_part+1]+1;
for (i=treffer=0; (i<=second_part) && (!treffer); i++) 
  { if (checksize_right(anfang)==5) treffer=1;
    else anfang=anfang->prev->invers->next->invers; }

anfang=map[2*(second_part+1)];

for (i=abbruch=0; (i<=first_part) && !abbruch; i++) 
  { if (checksize_right(anfang)==5) { treffer++; abbruch=1; }
    else anfang=anfang->invers->prev->invers->next; }

if (treffer==0) return(0);

/*
geaendert -- zweiter Codeteil wird nicht mehr betrachtet 
if ((first_part==second_part) && (code[1]<code[0])) return(0);  Spiegelbild besser */

/* jetzt checken, ob der Pfad kanonisch liegt: */ 
glue(map,testtube,&merkestart);
test = canonical_path(map,code,codestart);
unglue(map,testtube,merkestart);
if (!test) return(0);

return(1);
}


/*************************PRE_AUSGABE*****************************/

void pre_ausgabe( PLANMAP map, KNOTENTYP code[])

/* wird nur fuer nicht-bb-patches bei Fullerenkonstruktion aufgerufen.
   Schreibt die codes in longfile und shortfile zwecks spaeterer 
   Verwendung */

{

int test;

test=check_rand_weak(map);




return;
}

/**************************ADDCODE_BBLISTE*******************************/

void addcode_bbliste(FLAECHENTYP *code)
/* haengt code an last_bbliste an */
{

fprintf(stderr,"Code Nr %d  ",last_bbliste->anzahl); schreibecode(code,8);

if (last_bbliste->anzahl<1000) { memcpy(last_bbliste->code+(8*last_bbliste->anzahl),code,8*sizeof(FLAECHENTYP));
				 (last_bbliste->anzahl)++; }
else
{ 
  last_bbliste=last_bbliste->next=malloc(sizeof(BBLISTENTRY));
  if (last_bbliste==nil) { fprintf(stderr,"Can not get more memory for BBLISTENTRIES (2)\n");
			   exit(1); }
  memcpy(last_bbliste->code,code,8*sizeof(FLAECHENTYP));
  (last_bbliste->anzahl)=1;
  last_bbliste->next=nil; 
}
}


/*************************CHECK_MARK_UND_SCHREIBE***************************/

/* Ueberprueft, ob eine Einbettung mit 6 5-Ecken, also ohne 
Doppelte Aussenkanten, neu ist. Wenn ja, codiert und speichert sie die Einbettung. */


void check_mark_und_schreibe(PLANMAP map, KANTE *first, int laenge, int rest_sechsecke)
{


SEQUENZ localsq;
int sechsecke, test;
FLAECHENTYP code[8];

automorphisms=mirror_symmetric=0;

if (!bbpatch) fprintf(stderr,"ERRRRRRRRRRRRRRRRRRRRRRROR\n");
if (laenge != first_part) return;
if (IPR && (!is_ipr))  return; 

sechsecke=((map[0][0].name)-10-laenge)/2;
               /* leicht aus Euler Formel */

if (sechsecke+rest_sechsecke != max_sechsecke) 
  { fprintf(stderr,"Error in 6-gon calculation (check_mark_und_schreibe) !\n");
    fprintf(stderr,"%d %d %d \n",sechsecke, rest_sechsecke , max_sechsecke);
						 exit(0); }


if (!codiere(map, code, first, 8, laenge)) return; /* nicht kanonisch */


if (code[0] !=0) { fprintf(stderr," there was a hexagon ring.......\n"); exit(0); }
/*schreibemap(map);*/
if (fullerenes) /* in eine Liste von Caps schreiben -- bei der auch Spiegelbilder
		   unterschieden werden muessen. Es wird aber vermerkt, ob es sich
		   um die "kanonische Orientierung" handelt. */
  { all_caps++;
    if (test=bb_kanonisch(map)) 
      { good_caps++; 
	code[0]=test-1;/* code[0] > 0 <=> hat Spiegelsymmetrie -- 1 fuer flaechenstabilisierend, 2 sonst */
	addcode_bbliste(code); }
  }
else ausgabe(stdout,map);

return;

}


/***************************BAUE_AUF**************************************/

/* die eigentliche konstruktionsroutine -- legt auf alle moeglichen arten eine
neue reihe an */

void baue_auf(PLANMAP map, SEQUENZ sq, int sechsecke, int durchlauf)
{

SEQUENZ localseq;
int j, i, laenge;
KANTE *naechste, *nextmark, *run;
int anzahl, naechste_stelle, sql;


/*fprintf(stderr,"anfang bau: %d\n",debugzaehler);
schreibemap(map);
schreibesequenz(sq); */

sql=sq.laenge;

if (durchlauf==1 && !bbpatch)
  {
    if (sql<=2) return; 
    /* Es werden ja keine kuerzeren Raender als 3 im ersten Durchlauf gebraucht */
    /* Die 3er muessen eine 0 haben und einen Eintrag, der maximal first_part oder
       second_part-1 ist. Das kann nur dann erreicht werden, wenn die Summe der
       beiden kleinsten Eintraege <= first_part oder second_part-1 ist */
    if (sql==3) if ((sq.sequenz[1]+sq.sequenz[2] > first_part) && 
		    (sq.sequenz[1]+sq.sequenz[2] > second_part-1)) return;
  }

if (bbpatch) /* hier wird nachher nichts identifiziert, die Groesse der Patches
		waechst also monoton. Es kann also abgebrochen werden , sobald die
		gewuenschte Groesse ueberschritten wird */
   /* fuer die Reduzierung der sequenz um 1 wird der Rand immer um mindestens eins 
      laenger (ausser am Schluss, wo eventuell 2 Fuenfecke auf einmal verbaut werden,
      um es zu einer 0-Sequenz zu machen.) */
  { if (sql >= 2) laenge=sq.sequenz[0] + sql-1;
      else laenge=sq.sequenz[0] +1;
    for (i=1; i<sql; i++) laenge += sq.sequenz[i];
    if (laenge>first_part) return; /* die minimal erreichbare laenge ist zu gross */
  }


if (IPR && (!is_ipr))  return;


if (sql >=2)
{
for (i=0; i<sql && ((i==0) || !((sq.k_marks)[i])); i++) 
                                            /* Schleife ueber alle moeglichen Startpunkte */
  {
    laenge=sq.sequenz[i]; 
    if (i==sql-1) naechste_stelle=0; else naechste_stelle=i+1;
    if (laenge==0)
    {
    /* da nie eine 0 am anfang einer kanonischen Sequenz stehen kann, kann die erste einer
       doppel-0 nie am ende stehen, also: */
      if ( (i<sql-1) && (sq.sequenz[i+1]==0) )
	{
	  if (sechsecke >= 1)
	    { 
	      naechste=sq.kanten[i];
	      { add_polygon(6,map,naechste,&naechste); nextmark=naechste; }
	      belege_sequenz(nextmark,&localseq); 
	      if (localseq.k_marks[0])
		{ schreibe_auf(map,localseq,sechsecke-1,durchlauf);
		  baue_auf(map, localseq, sechsecke-1, durchlauf); }
	      /* aufraeumen: */
	      (map[0][0].name) = (map[0][0].name) - 4;
	      run=sq.kanten[i]; run->name=aussen; run->invers=nil; 
	      run=run->next->invers->next; run->name=aussen; run->invers=nil;
	    }   /* ende if ...*/
	  naechste=sq.kanten[i];
	  { add_polygon(5,map,naechste,&naechste); nextmark=naechste; }
	  belege_sequenz(nextmark,&localseq); 
	  if (localseq.k_marks[0])
	    { schreibe_auf(map,localseq,sechsecke, durchlauf);
	      baue_auf(map, localseq, sechsecke, durchlauf); }
	  /* aufraeumen: */
	  is_ipr=1;
	  (map[0][0].name) = (map[0][0].name) - 3;
	  run=sq.kanten[i]; run->name=aussen; run->invers=nil; 
	  run=run->next->invers->next; run->name=aussen; run->invers=nil;

	}

    if (sq.laenge==2) /* nur dann oder im vorigen Fall muss auch an 0er angelegt werden */
    {
    /* In diesem Teil wird die Sequenz nicht a priori berechnet, sondern erst im nachhinein --
       das kostet zwar Zeit, ist aber einfacher und braucht nicht so wahnsinnig viele
       Fallunterscheidungen */

    if (i != 1) { fprintf(stderr,"ERROR: i should be 1 !\n"); exit(0); }
    anzahl= 2 + sq.sequenz[0]; /* Bei laenge 0 muss der naechste mit aufgefuellt werden,
					       um keine Doppel-Innenkanten zu erhalten */

    /* erst nur 6-Ecke */
    if (sechsecke >= anzahl)
      { 
      naechste=sq.kanten[i];
      if (anzahl==1) { add_polygon(6,map,naechste,&naechste); nextmark=naechste; }
        else {
               add_polygon(6,map,naechste,&naechste); nextmark=naechste->prev->invers->prev;
               for (j=1; j<anzahl; j++)  add_polygon(6,map,naechste,&naechste); }
      belege_sequenz(nextmark,&localseq); 
      if (localseq.k_marks[0])
	   { schreibe_auf(map,localseq,sechsecke-anzahl, durchlauf);
	     baue_auf(map, localseq, sechsecke-anzahl, durchlauf); }
      /* aufraeumen: */
      (map[0][0].name) = (map[0][0].name) - 2*anzahl -2;
      run=sq.kanten[i]; run->name=aussen; run->invers=nil; 
      run=run->next->invers->next; run->name=aussen; run->invers=nil;
      for (j=2 ; j<anzahl; j++) { run=run->next->invers->next->invers->next;
	                         run->name=aussen; run->invers=nil; }
      }   /* ende if ...*/



    /* dann ein 5- und 6-Ecke */
    if (sechsecke >= anzahl-1)
      { 
      naechste=sq.kanten[i];
      if (anzahl==1) { add_polygon(5,map,naechste,&naechste); nextmark=naechste; }
        else {
               add_polygon(6,map,naechste,&naechste); nextmark=naechste->prev->invers->prev;
               for (j=1; j<anzahl-1; j++)  add_polygon(6,map,naechste,&naechste);  
               add_polygon(5,map,naechste,&naechste);  }
      belege_sequenz(nextmark,&localseq);
      if (localseq.k_marks[0])
	   { schreibe_auf(map,localseq,sechsecke-anzahl+1, durchlauf);
	     baue_auf(map, localseq, sechsecke-anzahl+1, durchlauf); }
      /* aufraeumen: */
      is_ipr=1;
      (map[0][0].name) = (map[0][0].name) - 2*anzahl-1;
      run=sq.kanten[i]; run->name=aussen; run->invers=nil; 
      run=run->next->invers->next; run->name=aussen; run->invers=nil;
      for (j=2 ; j<anzahl; j++) { run=run->next->invers->next->invers->next;
	                         run->name=aussen; run->invers=nil; }
      }   /* ende if ... */

    /* dann eventuell zwei 5- und der Rest 6-Ecke um es zu 0-Sequenz zu machen */
    if ( bbpatch && (sechsecke >= anzahl-2))
      {
      naechste=sq.kanten[i];
      add_polygon(5,map,naechste,&naechste); nextmark=naechste->prev->invers->prev;
      for (j=1; j<anzahl-1; j++)  add_polygon(6,map,naechste,&naechste);  
      add_polygon(5,map,naechste,&naechste); 
      belege_sequenz(nextmark,&localseq);
      check_mark_und_schreibe(map,nextmark,localseq.sequenz[0],sechsecke-anzahl+2);

      /* aufraeumen: */
      is_ipr=1;
      (map[0][0].name) = (map[0][0].name) - 2*anzahl;
      run=sq.kanten[i]; run->name=aussen; run->invers=nil; 
      run=run->next->invers->next; run->name=aussen; run->invers=nil;
      for (j=2; j<anzahl; j++) { run=run->next->invers->next->invers->next;
	                         run->name=aussen; run->invers=nil; }

      }   /* ende if */
     } /* ende if sq.laenge==2 */
    } /* ende laenge==0 */


    else /* d.h. laenge !=0 */
    {

    /* ein sonderfall, wenn der naechste ein 0er ist (der erste aber nicht)--
       dann musss da noch ein 6-Eck mehr angefuegt werden: */

      if ((sq.sequenz[naechste_stelle]==0) && (sql>2))
	  { 
	    if (sechsecke >= laenge+1)
	    { 
	      naechste=sq.kanten[i];
              add_polygon(6,map,naechste,&naechste); nextmark=naechste->prev->invers->prev;
	      /* insgesamt laenge+1: */
	      for (j=1; j<=laenge; j++)  add_polygon(6,map,naechste,&naechste);  
	      belege_sequenz(nextmark,&localseq); 
	      if (localseq.k_marks[0])
		{ schreibe_auf(map,localseq,sechsecke-laenge-1, durchlauf);
		  baue_auf(map, localseq, sechsecke-laenge-1, durchlauf); }
              /* aufraeumen: */
	      (map[0][0].name) = (map[0][0].name) - 2*laenge -4;
	      run=sq.kanten[i]; run->name=aussen; run->invers=nil;
	      for (j=0; j<laenge; j++) { run=run->next->invers->next->invers->next;
					 run->name=aussen; run->invers=nil; }
	      run=run->next->invers->next; run->name=aussen; run->invers=nil;

	    }   /* ende if ...*/
	    if (sechsecke >= laenge)
	    { 
	      naechste=sq.kanten[i];
              add_polygon(6,map,naechste,&naechste); nextmark=naechste->prev->invers->prev;
	      /* insgesamt laenge+1: */
	      for (j=1; j<laenge; j++)  add_polygon(6,map,naechste,&naechste);  
	      add_polygon(5,map,naechste,&naechste);  
	      belege_sequenz(nextmark,&localseq); 
	      if (localseq.k_marks[0])
		{ schreibe_auf(map,localseq,sechsecke-laenge, durchlauf);
		  baue_auf(map, localseq, sechsecke-laenge, durchlauf); }
              /* aufraeumen: */
	      is_ipr=1;
	      (map[0][0].name) = (map[0][0].name) - 2*laenge -3;
	      run=sq.kanten[i]; run->name=aussen; run->invers=nil;
	      for (j=0; j<laenge; j++) { run=run->next->invers->next->invers->next;
					 run->name=aussen; run->invers=nil; }
	      run=run->next->invers->next; run->name=aussen; run->invers=nil;

	    }   /* ende if ...*/
	  } /* ende sonderfall naechste_stelle==0-sequenz */



    /* erst nur 6-Ecke */
    if (sechsecke >= laenge)
      { 
      naechste=sq.kanten[i];
      berechne_sequenz(&localseq,sq,i,0);
      if (localseq.k_marks[0])
      {
      if (laenge==1) { add_polygon(6,map,naechste,&naechste); nextmark=naechste;
                       localseq.kanten[0]=nextmark; 
		       localseq.kanten[localseq.laenge-1]=naechste->prev->invers->prev; }
        else {
               add_polygon(6,map,naechste,&naechste); nextmark=naechste->prev->invers->prev;
               for (j=1; j<laenge; j++)  add_polygon(6,map,naechste,&naechste); 
               localseq.kanten[0]=nextmark; localseq.kanten[1]=naechste; }
      /*belege_sequenz(nextmark,&testseq); seq_vgl(localseq,testseq);*/
      if (localseq.k_marks[0])
	   { schreibe_auf(map,localseq,sechsecke-laenge, durchlauf);
	     baue_auf(map, localseq, sechsecke-laenge, durchlauf); }
      /* aufraeumen: */
      (map[0][0].name) = (map[0][0].name) - 2*laenge -1;

      run=sq.kanten[i]; run->name=aussen; run->invers=nil;
      for (j=0; j<laenge; j++) { run=run->next->invers->next->invers->next;
	                         run->name=aussen; run->invers=nil; }
    }/* ende if kanonisch */
    }   /* ende if ...*/



    /* dann ein 5- und 6-Ecke */
    if (sechsecke >= laenge-1)
      { 
      naechste=sq.kanten[i];
      berechne_sequenz(&localseq,sq,i,1); 
      if (localseq.k_marks[0])
      {
      if (laenge==1) { add_polygon(5,map,naechste,&naechste); nextmark=naechste; 
                       localseq.kanten[0]=nextmark; }
        else {
               add_polygon(6,map,naechste,&naechste); nextmark=naechste->prev->invers->prev;
               for (j=1; j<laenge-1; j++)  add_polygon(6,map,naechste,&naechste);  
               add_polygon(5,map,naechste,&naechste);  
               localseq.kanten[0]=nextmark; }
      /*belege_sequenz(nextmark,&testseq); seq_vgl(localseq,testseq);*/
      if (localseq.k_marks[0])
	   { schreibe_auf(map,localseq,sechsecke-laenge+1, durchlauf);
	     baue_auf(map, localseq, sechsecke-laenge+1, durchlauf); }
      /* aufraeumen: */
      is_ipr=1;
      (map[0][0].name) = (map[0][0].name) - 2*laenge;
      run=sq.kanten[i]; run->name=aussen; run->invers=nil;
      for (j=0; j<laenge; j++) { run=run->next->invers->next->invers->next;
	                         run->name=aussen; run->invers=nil; }
    }/* ende if kanonisch */
      }   /* ende if ... */

    /* dann eventuell zwei 5- und der Rest 6-Ecke um es zu 0-Sequenz zu machen */
    if (bbpatch && (laenge>=2) && (sechsecke >= laenge-2) && (sq.laenge==2))
      { 
      naechste=sq.kanten[i];
      berechne_sequenz(&localseq,sq,i,2);
      add_polygon(5,map,naechste,&naechste); nextmark=naechste->prev->invers->prev;
      for (j=1; j<laenge-1; j++)  add_polygon(6,map,naechste,&naechste);  
      add_polygon(5,map,naechste,&naechste); 
      localseq.kanten[0]=nextmark;
      /* belege_sequenz(nextmark,&testseq); seq_vgl(localseq,testseq); */
      check_mark_und_schreibe(map,nextmark,localseq.sequenz[0],sechsecke-laenge+2);

      /* aufraeumen: */
      is_ipr=1;
      (map[0][0].name) = (map[0][0].name) - 2*laenge + 1;
      run=sq.kanten[i]; run->name=aussen; run->invers=nil;
      for (j=0; j<laenge; j++) { run=run->next->invers->next->invers->next;
	                         run->name=aussen; run->invers=nil; }
      }   /* ende if */
    }/* ende laenge != 0 */
   } /* ende for */

} /* ende if sequenzlaenge >=2 */

else /* d.h. sequenzlaenge==1 */
{
laenge=sq.sequenz[0];

/* erst nur 6-Ecke */
if (laenge && (sechsecke >= (laenge+1)))
 { 
 naechste=sq.kanten[0];
 berechne_sequenz(&localseq,sq,0,0);
 for (j=0; j<=laenge; j++)  add_polygon(6,map,naechste,&naechste);  
 nextmark=naechste;
 localseq.kanten[0]=nextmark;
 /* belege_sequenz(nextmark,&testseq); seq_vgl(localseq,testseq); */
 schreibe_auf(map,localseq,sechsecke-laenge-1, durchlauf);
 baue_auf(map, localseq, sechsecke-laenge-1, durchlauf); 
 /* aufraeumen: */
 (map[0][0].name) = (map[0][0].name) - 2*laenge -3;

      run=sq.kanten[0]; run->name=aussen; run->invers=nil;
      for (j=0; j<laenge; j++) { run=run->next->invers->next->invers->next;
	                         run->name=aussen; run->invers=nil; }
      }   /* ende if */

/* dann ein 5- und 6-Ecke */
if (bbpatch && laenge && (sechsecke >= laenge))
  {
  naechste=sq.kanten[0];
  berechne_sequenz(&localseq,sq,0,1);
  for (j=0; j<laenge; j++)  add_polygon(6,map,naechste,&naechste);  
  add_polygon(5,map,naechste,&naechste); 
  nextmark=naechste;
  /* das ist zwar zwangslaeufig kanonisch, aber trotzdem: */
  localseq.kanten[0]=nextmark; 
  /* belege_sequenz(nextmark,&testseq); seq_vgl(localseq,testseq); */
  check_mark_und_schreibe(map,nextmark,localseq.sequenz[0],sechsecke-laenge);
  /* aufraeumen: */
  is_ipr=1;
  (map[0][0].name) = (map[0][0].name) - 2*laenge -2;
  run=sq.kanten[0]; run->name=aussen; run->invers=nil;
  for (j=0; j<laenge; j++) { run=run->next->invers->next->invers->next;
			     run->name=aussen; run->invers=nil; }
      }   /* ende if */


} /* ende else */

}

/***************************BAUE_PATCHES************************************/

void baue_patches(int sechsecke, int durchlauf)
{
PLANMAP map;
SEQUENZ sq;
KANTE *marke;
int i;


for (i=0; i<=N; i++) bblmark[i]=brillenglasmark[i]=zwei_3_4_mark[i]=0;


/* Patches, die nur aus einer Flaeche bestehen, brauchen nicht betrachtet zu
   werden */

if (sechsecke >=2)
/* zuerst Start mit 2 Sechsecken */
{
baue_polygon(6,map,&marke);
add_polygon(6,map,marke,&marke);
belege_sequenz(marke, &sq);
schreibe_auf(map,sq,sechsecke-2, durchlauf);
baue_auf( map, sq, sechsecke-2, durchlauf);
}

if (sechsecke >=1)
/* dann ein 5- und ein 6-Eck */
{
baue_polygon(5,map,&marke);
add_polygon(6,map,marke,&marke);
belege_sequenz( marke, &sq);
schreibe_auf(map,sq,sechsecke-1, durchlauf);
baue_auf( map, sq, sechsecke-1, durchlauf);
}

/* dann zwei 5-Ecke */
if (!IPR)
{
baue_polygon(5,map,&marke);
add_polygon(5,map,marke,&marke);
belege_sequenz( marke, &sq);
schreibe_auf(map,sq,sechsecke, durchlauf);
baue_auf( map, sq, sechsecke, durchlauf);
}

}

/***********************INITIALIZE_LIST**********************************/

void initialize_list()
{
int j;
SEQUENZLISTE *qq;

mapliste.total_maps=0;

for (j=0; j<=max_sechsecke; j++)
  {
    qq=(mapliste.sechser)[j]=(SEQUENZLISTE *)malloc(sizeof(SEQUENZLISTE));
    if (qq==NULL) { fprintf(stderr,"Can not get more memory"); exit(0); }
    qq->next_level=nil;
    qq->number_next=0;
    qq->items=qq->last_item=nil;
  }


}

/**********************CHECKSIZE_RIGHT_2**************************************/

/* bestimmt die groesse der flaeche rechts von edge -- ist da keine gibt's 
   hier aber keine Probleme, sondern 6 wird zurueckgegeben -- dient nur
   zur Ueberpruefung, ob da ein 5-Eck ist.

   Es wird allerdings angenommen, dass edge->name nicht aussen ist
*/


int checksize_right_2( KANTE* edge)
{
KANTE *run; 
int zaehler=1;

for (run=edge->invers->prev; run != edge; run=run->invers->prev) 
  { if (run->name==aussen) return(6); zaehler++; }
return(zaehler);
}




/**********************CHECKSIZE_LEFT_2**************************************/

/* bestimmt die groesse der flaeche links von edge -- ist da keine gibt's 
   hier aber keine Probleme, sondern 6 wird zurueckgegeben -- dient nur
   zur Ueberpruefung, ob da ein 5-Eck ist.

   Es wird allerdings angenommen, dass edge->name nicht aussen ist
*/


int checksize_left_2( KANTE* edge)
{
KANTE *run; 
int zaehler=1;

for (run=edge->invers->next; run != edge; run=run->invers->next) 
  { if (run->name==aussen) return(6); 
    zaehler++;  }
return(zaehler);
}


      
	      
/************************CHECK_IPR***************************************/

BOOL check_ipr(PLANMAP map)
{
int i,j;
KANTE *run;

/* nur voruebergehend als sicherer test gedacht -- sonst koennte man
besser direkt von den 5-eck-kanten ausgehen */

for (i=1; i<=map[0][0].name; i++) for (j=0;j<3; j++) map[i][j].dummy=0;

for (i=1; i<=map[0][0].name; i++) 
  for (j=0;j<3; j++) 
    { run=map[i]+j;
      if (!run->dummy)
	{
	  run->dummy=1;
	  if (run->invers != nil) run->invers->dummy=1;
	  if (run->name != aussen)
	    if ((checksize_left_2(run)==5) && (checksize_right_2(run)==5)) 
	      { fprintf(stderr,"%d-->%d\n",run->ursprung,run->name); schreibemap(map); exit(0); }
	}
    }
return(1);
}


/***********************SUCHESTART********************************/

KANTE *suchestart( KANTE *start)
/* belegt eine sequenz und sucht die kanonische Kante mit dem kleinsten Namen 
   arbeitet "invers", d.h. es wird als Innenrand gesehen, der gefuellt werden
   muss. Wird aufgerufen fuer Brille und Sandwich. Start muss eine Kante sein, die
   ins innere zeigt.*/

{
int i, j, k, zaehler, position;
KANTE *run;
int sequenz[7]; 
KANTE *seqkanten[7];
int puffer[7];
char kan[7];


/*schreibemap(map); fprintf(stderr,"anfang: %d -> %d \n",start->ursprung, start->name);*/


while (start->next->invers->next->invers->next->name == aussen) 
                      start=start->next->invers->next->invers->next;
/* Sucht 2 Kanten hintereinander nach aussen -- zu unterscheiden vom namen aussen, was 
   auch nach innen heissen kann. Duerfte nur fuer bauchbinden eine Endlosschleife sein */

for (i=0; i<7; i++) { sequenz[i]=leer; seqkanten[i]=nil; kan[i]=0; }

position=0;
seqkanten[0]=start;


for (zaehler=1, run=start;
     run->prev->invers->prev->invers->prev->name == aussen;
     run=run->prev->invers->prev->invers->prev) zaehler++;
sequenz[0]=zaehler; position=1; seqkanten[1]=nil;
for (run=run->prev->invers->prev->invers->prev->invers->prev; run->name != aussen;
     run=run->invers->prev) 
{ sequenz[position]=0; position++; seqkanten[position]=nil; }
/* naechste Kante vor nicht-0-sequenz suchen -- entsprechende innenkanten gibt es nicht
   und muessen sich dementsprechend auch nicht gemerkt werden */


while (run != start)
{
seqkanten[position]=run;
for (zaehler=1; 
     run->prev->invers->prev->invers->prev->name == aussen;
     run=run->prev->invers->prev->invers->prev) { zaehler++; }
sequenz[position]=zaehler; position++; seqkanten[position]=nil;
for (run=run->prev->invers->prev->invers->prev->invers->prev; run->name != aussen;
     run=run->invers->prev) 
{ sequenz[position]=0; position++; seqkanten[position]=nil; }
}


sequenz[position]=leer; seqkanten[position]=nil;

kan[0]=sequenz_kanonisch(sequenz);


for (i=0; sequenz[i] != leer; i++)
  { for (j=i, k=0; sequenz[j]!=leer; j++, k++) puffer[k]=sequenz[j];
    for (j=0; j<i; j++, k++) puffer[k]=sequenz[j];
    puffer[k]=leer;
    kan[i]=sequenz_kanonisch(puffer);
  }



for (i=0, run=nil; sequenz[i] != leer; i++)
  { if (kan[i])
      if ((run==nil) || (seqkanten[i]->ursprung < run->ursprung)) run=seqkanten[i];
  }

/* Jetzt die vorige Innenkante suchen, um rechts davon dann einfuegen zu koennen */
for (run=run->next->invers->next->invers->next; run->name != aussen; run=run->invers->next);

/*fprintf(stderr,"Ende: %d \n",run->ursprung);*/

return(run);


}



/***********************SUCHE_ITEM**************************************/

ITEMLISTE *suche_item(KNOTENTYP *adresse)
{
int i, laenge;
SEQUENZLISTE *sq;

laenge=8-adresse[0];

sq=mapliste.sechser[0]; /* umgemodelt vom Fullerenprogramm */

for (i=2; i<laenge; i++)
  { 
    if (sq->number_next <= adresse[i]) return(nil);
    if (sq->next_level[adresse[i]]==nil) return(nil);
    sq=sq->next_level[adresse[i]]; }
return(sq->items);
}



/*********************ADD_POLYGON_INVERS***********************************/

void add_polygon_invers(int n, PLANMAP map, KANTE *start, KANTE **lastout)
/* fuegt ein weiteres polygon einer Reihe an. Dabei ist n die groesse des polygons. 
   Angefuegt wird immer an start. Ueber lastout wird die letzte Aussenkante des 
   Polygons zurueckgegeben.
   Es arbeitet wie add_polygon nur im anderen Drehsinn. Das wird fuer die REkonstruktion
   der Patches gebraucht
   Im Falle, dass nur eine Kante eingefuegt werden muss, wird *lastout auf die naechste 
   zu benutzende Aussenkante gesetzt. Die ist nicht mit dem gebauten Polygon
   inzident. Im Falle, dass es sie nicht gibt, wird *lastout auf nil gesetzt.

   angefuegt wird in den Winkel in prev-Richtung, d.h. zwischen start und start->prev

 */


{
int i, new_tempknz, tempknz;
KANTE *ende, *run;
int common_vertices;

debugzaehler++;

if (start->name != aussen) { fprintf(stderr,"ERROR ADD_P_INV not starting at external edge\n");
			     exit(86); }



if (IPR && (n==5))
  { 
    if (checksize_left_2(start->prev)==5) is_ipr=0;
    for (ende=start->prev->invers->prev, common_vertices=2; ende->name != aussen; 
	  ende=ende->invers->prev) {if (checksize_left_2(ende)==5) is_ipr=0;
                                     common_vertices++;
				   }
  }
else for (ende=start->prev->invers->prev, common_vertices=2; ende->name != aussen; 
	  ende=ende->invers->prev) common_vertices++;

/* schreibemap(map); fprintf(stderr,"size %d common: %d anfang: %d\n",n,common_vertices,start->ursprung);*/

if (n<common_vertices) 
   { fprintf(stderr,"polygon to insert (inv) too small !\n"); 
     schreibemap(map);
     fprintf(stderr,"%d \n",debugzaehler);
     exit(87); }

/* es muessen also n-common_vertices knoten hinzugefuegt werden */


tempknz=map[0][0].name;
new_tempknz=tempknz+n-common_vertices;


if (n-common_vertices==0) /* dann kommt kein knoten dazu */
  { start->name=ende->ursprung; start->invers=ende;
    ende->name=start->ursprung; ende->invers=start;
    for (ende=start->next->invers->next, common_vertices=2; 
	 (ende->name != aussen) && (common_vertices<6); 
         ende=ende->invers->next) common_vertices++;
    if (common_vertices<6) *lastout=ende; /* ein common_vertices+1 -- Eck ist noch moeglich */
    else { *lastout=nil;
	   if (ende==start->next) /* dann ist nur noch eine 5-Eck Luecke */
	     { for (i=anzahl_5ek+5, run=start; anzahl_5ek<i; 
		  anzahl_5ek++, run=run->invers->prev) { F_eck_kanten[anzahl_5ek]=run;
							 if (IPR)
							   if (checksize_left_2(run)==5) is_ipr=0; }
	     }
	 }
  }
else
  {
    if (n-common_vertices==1) /* dann kommt nur ein knoten dazu */
      {
	tempknz++;
	start->name=tempknz; start->invers=map[tempknz];
	map[tempknz][0].name=start->ursprung; map[tempknz][0].invers=start;
	map[tempknz][1].name=ende->ursprung; map[tempknz][1].invers=ende;
	ende->name=tempknz; ende->invers=map[tempknz]+1;
	map[tempknz][2].name=aussen; map[tempknz][2].invers=nil;
	*lastout=map[tempknz]+2;
	map[0][0].name=tempknz;
      }
    else
      {
	
	/* es bleibt: mindestens zwei neue knoten */
	
	tempknz++;
	start->name=tempknz; start->invers=map[tempknz];
	map[tempknz][0].name=start->ursprung; map[tempknz][0].invers=start;
	map[tempknz][1].name=tempknz+1; map[tempknz][1].invers=map[tempknz+1];
	map[tempknz][2].name=aussen; map[tempknz][2].invers=nil;
	
	for (tempknz++; tempknz<new_tempknz; tempknz++)
	  { map[tempknz][0].name=tempknz-1; map[tempknz][0].invers=map[tempknz-1]+1;
	    map[tempknz][1].name=tempknz+1; map[tempknz][1].invers=map[tempknz+1]; 
	    map[tempknz][2].name=aussen; map[tempknz][2].invers=nil; }
	
	/* und nun noch den letzten knoten */
	map[tempknz][0].name=tempknz-1; map[tempknz][0].invers=map[tempknz-1]+1;
	map[tempknz][1].name=ende->ursprung; map[tempknz][1].invers= ende;
	ende->name=tempknz; ende->invers=map[tempknz]+1;
	map[tempknz][2].name=aussen; map[tempknz][2].invers=nil;
	*lastout=map[tempknz]+2;
	map[0][0].name=tempknz;
      } /* ende zweites else */
  } /* ende erstes else */


if (n==5)
  for (i=anzahl_5ek+5, run=start->invers; anzahl_5ek<i; 
       anzahl_5ek++, run=run->invers->prev) F_eck_kanten[anzahl_5ek]=run;


return;
}



/*********************INSERT_PATCH*********************************/

void insert_patch(PLANMAP map, KANTE *anfang, ITEMLISTE *item, KNOTENTYP fuenfecke)
{

FLAECHENTYP *code;
KANTE *run;
int zaehler, codestelle;

code=item->code;
    for (codestelle=0, zaehler=1, run=anfang; run != nil; zaehler++ )
      { 
	if ((codestelle<fuenfecke) && (code[codestelle]==zaehler))
          { add_polygon_invers(5, map, run, &run); codestelle++; }
        else add_polygon_invers(6, map, run, &run);
      }
}

/*********************INSERT_PATCH_CODE_INV*********************************/

void insert_patch_code_inv(PLANMAP map, KANTE *anfang, FLAECHENTYP code[] , KNOTENTYP fuenfecke)
{

KANTE *run;
int zaehler, codestelle;


/*schreibemap(map);*/
/* schreibecode(code,6); */

    for (codestelle=0, zaehler=1, run=anfang; run != nil; zaehler++ )
      { 
	if ((codestelle<fuenfecke) && (code[codestelle]==zaehler))
          { add_polygon(5, map, run, &run); codestelle++; }
        else add_polygon(6, map, run, &run);
      }
}



/*********************INSERT_PATCH_CODE*********************************/

void insert_patch_code(PLANMAP map, KANTE *anfang, FLAECHENTYP code[] , KNOTENTYP fuenfecke)
{

KANTE *run;
int zaehler, codestelle;


/*schreibemap(map);*/
/* schreibecode(code,6); */
    for (codestelle=0, zaehler=1, run=anfang; run != nil; zaehler++ )
      { 
	if ((codestelle<fuenfecke) && (code[codestelle]==zaehler))
          { add_polygon_invers(5, map, run, &run); codestelle++; }
        else add_polygon_invers(6, map, run, &run);
      }
}


/**********************DELETE_PATCH**************************************/

void delete_patch(PLANMAP map, KANTE *anfang, KNOTENTYP *adresse, int knotenzahl)
{
int sqlaenge, i, j;
KANTE *run;

sqlaenge=6-adresse[0];

map[0][0].name = knotenzahl;
for (i=1+sqlaenge; adresse[i]==0; i--);
for ( run=anfang ; i>1 ; i--)
  { if (adresse[i])
      { for (j=0; j < adresse[i]; j++)
	  { 
	    run->name=aussen; run->invers=nil; run = run->next->invers->next->invers->next; }
	run=run->invers->next;
      }
    else { run=run->invers->next; }
  }
}


/************************BAUE_PFAD**************************************************/

/* baut den Randpfad */

void baue_pfad(PLANMAP map, int firstpart,int secondpart)

{
int knotenzahl, i;

knotenzahl=map[0][0].name= 2*(firstpart+secondpart+2);

/* nummerierung so, dass [2] immer "aussen" ist --- DARAUF WIRD SICH SPAETER
   VERLASSEN !!!! */

map[1][0].name= knotenzahl; map[1][0].invers=map[knotenzahl]+1;
map[1][1].name=2; map[1][1].invers=map[2]+1;
map[1][2].name=aussen; map[1][2].invers=nil; 
  


/* Zuerst der secondpart zickzackweg beginnend am Startpunkt */

for (i=2; i<= 2*(secondpart); i++) 
  { map[i][0].name=i+1; map[i][0].invers=map[i+1];
    map[i][1].name=i-1; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil; 
    i++; 
    map[i][0].name=i-1; map[i][0].invers=map[i-1];
    map[i][1].name=i+1; map[i][1].invers=map[i+1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
  }

/* jetzt die konvexe Ecke  */

    map[i][0].name=i+1; map[i][0].invers=map[i+1]+1;
    map[i][1].name=i-1; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
    i++;

    map[i][0].name=i+1; map[i][0].invers=map[i+1];
    map[i][1].name=i-1; map[i][1].invers=map[i-1];
    map[i][2].name=aussen; map[i][2].invers=nil;
    i++;

/* und der firstpart weg */

for ( ; i< knotenzahl; i++) 
  { map[i][0].name=i-1; map[i][0].invers=map[i-1];
    map[i][1].name=i+1; map[i][1].invers=map[i+1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
    i++; 
    map[i][0].name=i+1; map[i][0].invers=map[i+1];
    map[i][1].name=i-1; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
  }

/* und jetzt noch den letzten: (jetzt ist i==knotenzahl) */ 

    map[i][0].name=i-1; map[i][0].invers=map[i-1];
    map[i][1].name=1; map[i][1].invers=map[1];
    map[i][2].name=aussen; map[i][2].invers=nil;
    
}

/************************BAUE_PFAD_DANGLING_TUBE**************************************************/

/* baut den Randpfad mit um max_knoten verschobenen Knotennahmen*/

void baue_pfad_dangling_tube(PLANMAP map, int firstpart,int secondpart)

{
int knotenzahl, i;

knotenzahl=map[0][0].name= 2*(firstpart+secondpart+2);

/* nummerierung so, dass [2] immer "aussen" ist --- DARAUF WIRD SICH SPAETER
   VERLASSEN !!!! */

map[1][0].name= knotenzahl+max_knoten; map[1][0].invers=map[knotenzahl]+1;
map[1][1].name=2+max_knoten; map[1][1].invers=map[2]+1;
map[1][2].name=aussen; map[1][2].invers=nil; 
  


/* Zuerst der secondpart zickzackweg beginnend am Startpunkt */

for (i=2; i<= 2*(secondpart); i++) 
  { map[i][0].name=i+1+max_knoten; map[i][0].invers=map[i+1];
    map[i][1].name=i-1+max_knoten; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil; 
    i++; 
    map[i][0].name=i-1+max_knoten; map[i][0].invers=map[i-1];
    map[i][1].name=i+1+max_knoten; map[i][1].invers=map[i+1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
  }

/* jetzt die konvexe Ecke  */

    map[i][0].name=i+1+max_knoten; map[i][0].invers=map[i+1]+1;
    map[i][1].name=i-1+max_knoten; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
    i++;

    map[i][0].name=i+1+max_knoten; map[i][0].invers=map[i+1];
    map[i][1].name=i-1+max_knoten; map[i][1].invers=map[i-1];
    map[i][2].name=aussen; map[i][2].invers=nil;
    i++;

/* und der firstpart weg */

for ( ; i< knotenzahl; i++) 
  { map[i][0].name=i-1+max_knoten; map[i][0].invers=map[i-1];
    map[i][1].name=i+1+max_knoten; map[i][1].invers=map[i+1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
    i++; 
    map[i][0].name=i+1+max_knoten; map[i][0].invers=map[i+1];
    map[i][1].name=i-1+max_knoten; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
  }

/* und jetzt noch den letzten: (jetzt ist i==knotenzahl) */ 

    map[i][0].name=i-1+max_knoten; map[i][0].invers=map[i-1];
    map[i][1].name=1+max_knoten; map[i][1].invers=map[1];
    map[i][2].name=aussen; map[i][2].invers=nil;
    
}


/************************BAUE_PFAD_BB**************************************************/

/* baut den Randpfad */

void baue_pfad_bb(PLANMAP map, int laenge)

{
int knotenzahl, i;

knotenzahl=map[0][0].name= 2*(laenge);

/* nummerierung so, dass [2] immer "aussen" ist --- DARAUF WIRD SICH SPAETER
   VERLASSEN !!!! */

map[1][0].name= knotenzahl; map[1][0].invers=map[knotenzahl];
map[1][1].name=2; map[1][1].invers=map[2]+1;
map[1][2].name=aussen; map[1][2].invers=nil; 
  


/* Zuerst der secondpart zickzackweg beginnend am Startpunkt */

for (i=2; i< knotenzahl; i++) 
  { map[i][0].name=i+1; map[i][0].invers=map[i+1];
    map[i][1].name=i-1; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil; 
    i++; 
    map[i][0].name=i-1; map[i][0].invers=map[i-1];
    map[i][1].name=i+1; map[i][1].invers=map[i+1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
  }

/* und jetzt noch den letzten: (jetzt ist i==knotenzahl) */ 

    map[i][0].name=1; map[i][0].invers=map[1];
    map[i][1].name=i-1; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
    
}

/************************BAUE_PFAD_BB_DANGLING_TUBE**************************************************/

/* baut den Randpfad mit um max_knoten verschobenen knotennamen*/

void baue_pfad_bb_dangling_tube(PLANMAP map, int laenge)

{
int knotenzahl, i;

knotenzahl=map[0][0].name= 2*(laenge);

/* nummerierung so, dass [2] immer "aussen" ist --- DARAUF WIRD SICH SPAETER
   VERLASSEN !!!! */

map[1][0].name= knotenzahl+max_knoten; map[1][0].invers=map[knotenzahl];
map[1][1].name=2+max_knoten; map[1][1].invers=map[2]+1;
map[1][2].name=aussen; map[1][2].invers=nil; 
  


/* Zuerst der secondpart zickzackweg beginnend am Startpunkt */

for (i=2; i< knotenzahl; i++) 
  { map[i][0].name=i+1+max_knoten; map[i][0].invers=map[i+1];
    map[i][1].name=i-1+max_knoten; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil; 
    i++; 
    map[i][0].name=i-1+max_knoten; map[i][0].invers=map[i-1];
    map[i][1].name=i+1+max_knoten; map[i][1].invers=map[i+1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
  }

/* und jetzt noch den letzten: (jetzt ist i==knotenzahl) */ 

    map[i][0].name=1+max_knoten; map[i][0].invers=map[1];
    map[i][1].name=i-1+max_knoten; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
    
}



/*********************ADD_HEXAGON_DANGLING_TUBE***********************************/

void add_hexagon_dangling_tube(PLANMAP map, KANTE *start, KANTE **lastout)
/* fuegt ein weiteres hexagon einer Reihe an. Dabei ist n die groesse des polygons. 
   Angefuegt wird immer an start. Die Marke wird nicht versetzt. Ueber lastout wird
   die letzte Aussenkante des Polygons zurueckgegeben. */
/* der Unterschied zu add_polygon ist haupsaechlich, dass die Namen immer um max_knoten
   verschoben sind */

{
int new_tempknz, tempknz;
KANTE *ende;
int common_vertices;

for (ende=start->next->invers->next, common_vertices=2; ende->name != aussen; 
	  ende=ende->invers->next) common_vertices++;


if (6<common_vertices) 
   { fprintf(stderr,"polygon to insert too small !\n"); 
     exit(0); }

/* es muessen also 6-common_vertices knoten hinzugefuegt werden */

tempknz=map[0][0].name;
new_tempknz=tempknz+6-common_vertices;
if (new_tempknz > N) { fprintf(stderr,"Definition of S does not allow that long tubes (%d -- please enlarge\n",tempknz);
		       exit(0); }


if (6-common_vertices==0) /* dann kommt kein knoten dazu */
  { start->name=ende->ursprung; start->invers=ende;
    ende->name=start->ursprung; ende->invers=start;
    *lastout=nil;
    return;
  }

if (common_vertices==5) /* dann kommt nur ein knoten dazu */
 {
 tempknz++;
 start->name=tempknz+max_knoten; start->invers=map[tempknz];
 map[tempknz][0].name=start->ursprung; map[tempknz][0].invers=start;
 map[tempknz][1].name=aussen; map[tempknz][1].invers=nil;
 map[tempknz][2].name=ende->ursprung; map[tempknz][2].invers=ende;
 ende->name=tempknz+max_knoten; ende->invers=map[tempknz]+2;
 *lastout=map[tempknz]+1;
 map[0][0].name=tempknz;
 return;
 }


/* es bleibt: mindestens zwei neue knoten */

tempknz++;
start->name=tempknz+max_knoten; start->invers=map[tempknz];
map[tempknz][0].name=start->ursprung; map[tempknz][0].invers=start;
map[tempknz][1].name=aussen; map[tempknz][1].invers=nil;
map[tempknz][2].name=tempknz+1+max_knoten; map[tempknz][2].invers=map[tempknz+1];

for (tempknz++; tempknz<new_tempknz; tempknz++)
    { map[tempknz][0].name=tempknz-1+max_knoten; map[tempknz][0].invers=map[tempknz-1]+2;
      map[tempknz][1].name=aussen; map[tempknz][1].invers=nil;
      map[tempknz][2].name=tempknz+1+max_knoten; map[tempknz][2].invers=map[tempknz+1]; }

/* und nun noch den letzten knoten */
map[tempknz][0].name=tempknz-1+max_knoten; map[tempknz][0].invers=map[tempknz-1]+2;
map[tempknz][1].name=aussen; map[tempknz][1].invers=nil;
map[tempknz][2].name=ende->ursprung; map[tempknz][2].invers= ende;
ende->name=tempknz+max_knoten; ende->invers=map[tempknz]+2;
*lastout=map[tempknz]+1;
map[0][0].name=tempknz;
}

/*************************BAUE_FULL_TUBE************************************************/

void baue_full_tube()
{
KANTE *run;
int i, grenze;

init_full_tube(tube);
if (bbpatch) baue_pfad_bb(tube,first_part);
else baue_pfad(tube,first_part,second_part);
/* ich brauche bis zu tubelaenge ringe. Jeder davon hat randlaenge/2 6-Ecke */
/* der erste ring dient dabei eigentlich nur als "merker" -- er wird jeweils durch
   den rand des anzuklebenden patches ersetzt */

if (bbpatch) run=tube[2]+2;
else run=tube[randlaenge-1]+2;
grenze=randlaenge*tubelaenge;
grenze /= 2;
for (i=0; i< grenze; i++) add_polygon(6,tube,run,&run);

}


/*************************BAUE_DANGLING_TUBE************************************************/

void baue_dangling_tube()
{
KANTE *run;
int i, grenze;

init_dangling_tube(tube);
if (bbpatch) baue_pfad_bb_dangling_tube(tube,first_part);
else baue_pfad_dangling_tube(tube,first_part,second_part);
/* ich brauche bis zu tubelaenge ringe. Jeder davon hat randlaenge/2 6-Ecke */
/* der erste ring dient dabei eigentlich nur als "merker" -- er wird jeweils durch
   den rand des anzuklebenden patches ersetzt */

if (bbpatch) run=tube[2]+2;
else run=tube[randlaenge-1]+2;
grenze=randlaenge*tubelaenge;
grenze /= 2;
for (i=0; i< grenze; i++) add_hexagon_dangling_tube(tube,run,&run);

if (!bbpatch)
  {
    init_dangling_tube(testtube);
    baue_pfad_dangling_tube(testtube,first_part,second_part);
    /* ich brauche bis zu randlaenge+1 ringe. Jeder davon hat randlaenge/2 6-Ecke */
    /* der erste ring dient dabei eigentlich nur als "merker" -- er wird jeweils durch
       den rand des anzuklebenden patches ersetzt */
    
    run=testtube[randlaenge-1]+2;
    grenze=randlaenge*(first_part+second_part+3); 
    grenze /= 2;
    for (i=0; i< grenze; i++) add_hexagon_dangling_tube(testtube,run,&run);
  }


}

/************************INSERT_PATH************************************************/

void insert_path(PLANMAP map, int p, KANTE *start, KANTE *end)

/* baut einen zickzackpfad (zunaechst nach rechts) mit p Kanten von start nach end 
   funktioniert sogar, wenn end schon vorausschauend gegeben wird, wie im Fall
   eines Einschlusses */

{
int i, knotenzahl;

if (p<3) { fprintf(stderr,"p<3 --- not prepared for that !!!!\n"); return; }

i=map[0][0].name+1;
knotenzahl=map[0][0].name=map[0][0].name+p-1;

start->invers=map[i]+1;
start->name=i;

map[i][0].name=i+1; map[i][0].invers=map[i+1];
map[i][1].name=start->ursprung; map[i][1].invers=start;
map[i][2].name=aussen; map[i][2].invers=nil;

if (p%2) /* ungerade */
  { for (i++ ; i<knotenzahl; i++ )
      { map[i][0].name=i-1; map[i][0].invers=map[i-1];
	map[i][1].name=i+1; map[i][1].invers=map[i+1]+1;
	map[i][2].name=aussen; map[i][2].invers=nil;
	i++; 
	map[i][0].name=i+1; map[i][0].invers=map[i+1];
	map[i][1].name=i-1; map[i][1].invers=map[i-1]+1;
	map[i][2].name=aussen; map[i][2].invers=nil;
      }

    /* und jetzt noch den letzten: (jetzt ist i==knotenzahl) */ 
    
    map[i][0].name=i-1; map[i][0].invers=map[i-1];
    map[i][1].name=end->ursprung; map[i][1].invers=end;
    map[i][2].name=aussen; map[i][2].invers=nil;
    end->name=i; end->invers=map[i]+1;
  }
else /* p gerade */
  { for (i++ ; i<knotenzahl-1; i++ )
      { map[i][0].name=i-1; map[i][0].invers=map[i-1];
	map[i][1].name=i+1; map[i][1].invers=map[i+1]+1;
	map[i][2].name=aussen; map[i][2].invers=nil;
	i++; 
	map[i][0].name=i+1; map[i][0].invers=map[i+1];
	map[i][1].name=i-1; map[i][1].invers=map[i-1]+1;
	map[i][2].name=aussen; map[i][2].invers=nil;
      }

    /* und jetzt noch die letzten beiden: */

    map[i][0].name=i-1; map[i][0].invers=map[i-1];
    map[i][1].name=i+1; map[i][1].invers=map[i+1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
    i++; 
	
    /* (jetzt ist i==knotenzahl) */ 
    
    map[i][0].name=end->ursprung; map[i][0].invers=end;
    map[i][1].name=i-1; map[i][1].invers=map[i-1]+1;
    map[i][2].name=aussen; map[i][2].invers=nil;
    end->name=i; end->invers=map[i];
  }
}

/************************DELETE_PATH*************************************************/

void delete_path(PLANMAP map, int p, KANTE *start, KANTE *end)


{
start->name=end->name=aussen;
start->invers=end->invers=nil;
map[0][0].name -= (p-1);
}


/************************MACHE_KANONISCH********************************************/

void mache_kanonisch( int l, KNOTENTYP *adr)

/* macht die Folge adr mit laenge l kanonisch */

{
int i,j;
KNOTENTYP puffer[20];

for (i=0, j=l; i<l; i++, j++) puffer[i]=puffer[j]=adr[i];

for (i=1; i<l; i++)
  { for (j=0; j<l; j++) { if (puffer[i+j]>adr[j]) 
			    { for (j=0; j<l; j++) adr[j]=puffer[i+j]; } 
			  else if (puffer[i+j]<adr[j]) j=l;
			}
  }
}


/***************************PATHOK************************************************/

/* ueberprueft, ob ein Pfad startend an kante und first_step Schritte laufend, bevor
   die pseudoconvexe Ecke kommt sich schliesst und ein 5-Eck am zweiten Teil
   hat. Der Pfad an sich hat natuerlich auch die first_part, second_part struktur */

int pathok(KANTE *kante,int first_step)

{

int gefunden=0; /* das zweite 5-Eck */
int i;
KANTE *run;

RESETMARKS_V;
MARK_ENDS(kante);
for (i=0, run=kante; i< first_step; i++) 
{ 
  run=run->invers->prev->invers->next; 
  if (ISMARKED_ENDS(run)) return(0); MARK_ENDS(run);
}

run=run->invers->prev; /* jetzt muesste run oben auf dem pseudoconvexen Buckel
			  stehen */
if (ISMARKED_V(run->name)) return 0; MARK_V(run->name);


gefunden= (checksize_right_2(run)==5);
for (i=0; i<first_part; i++) /* Achtung -- andere Bedeutung von first_part und second_part,
				 als in der (01)^i(10)^j Notation -- deshalb noch einer nach
				 der Schleife */
  { 
    run=run->invers->prev->invers->next;
    if (ISMARKED_ENDS(run)) return(0); MARK_ENDS(run);
    if (!gefunden) gefunden= (checksize_right_2(run)==5); }
if (!gefunden) return(0);

run=run->invers->prev->invers->next;
if (ISMARKED_V(run->ursprung)) return(0); MARK_V(run->ursprung);

run=run->invers->next; 
/* rechter Rand der konkaven Ecke */

for (i=0 ; i< second_part-first_step; i++)
     { if (ISMARKED_ENDS(run)) return(0); MARK_ENDS(run);
       run=run->invers->prev->invers->next; }


if (run==kante) return(1);
else return(0);
}


/***************************PATHOK_MIRROR************************************************/

/* ueberprueft, ob ein Pfad startend an kante und first_step Schritte laufend, bevor
   die pseudoconvexe Ecke kommt sich schliesst und ein 5-Eck am zweiten Teil
   hat. Der Pfad an sich hat natuerlich auch die first_part, second_part struktur */

int pathok_mirror(KANTE *kante,int first_step)

{

int gefunden=0; /* das zweite 5-Eck */
int i;
KANTE *run;

RESETMARKS_V;
MARK_ENDS(kante);

for (i=0, run=kante; i< first_step; i++) 
  { run=run->invers->next->invers->prev;
    if (ISMARKED_ENDS(run)) return(0); MARK_ENDS(run);
  }


run=run->invers->next; /* jetzt muesste run oben auf dem pseudoconvexen Buckel
			  stehen */
if (ISMARKED_V(run->name)) return 0; MARK_V(run->name);

gefunden= (checksize_left_2(run)==5);
for (i=0; i<first_part; i++) 
  { 
    run=run->invers->next->invers->prev;
    if (ISMARKED_ENDS(run)) return(0); MARK_ENDS(run);
    if (!gefunden) gefunden= (checksize_left_2(run)==5); }
if (!gefunden) return(0);

run=run->invers->next->invers->prev;
if (ISMARKED_V(run->ursprung)) return(0); MARK_V(run->ursprung);

run=run->invers->prev;
/* rechter Rand der konkaven Ecke */

for (i=0 ; i< second_part-first_step; i++) 
  { if (ISMARKED_ENDS(run)) return(0); MARK_ENDS(run);
    run=run->invers->next->invers->prev;
  }

if (run==kante) return(1);
else return(0);
}

/*************************TESTE_CODE********************************************/

int teste_code(KNOTENTYP nv, KANTE *start, int code[])
/* gibt 2 zurueck, wenn code[] besser ist, als der bei start 
   startende -- 0 wenn code[] schlechter ist und 1 fuer einen Automorphismus */
{
int i, testergebnis;
KANTE *run;

/* zuerst den Pfad markieren: */

run=start->invers->next;
run->merkename=run->name; run->merkekante=run->invers;
run->name=aussen; run->invers=nil;

for (i=0; i< code[0]; i++) 
  { run=run->next->invers->next->invers->next;
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil; }

run=run->next->invers->next; /* jetzt muesste run der zweite aeussere auf dem Buckel sein */
run->merkename=run->name; run->merkekante=run->invers;
run->name=aussen; run->invers=nil;

for (i=0; i<first_part; i++) 
  { 
    run=run->next->invers->next->invers->next; 
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil; }

/* Jetzt wurde bis zur Mulde markiert. */

run=run->next->invers->next->invers->next->invers->next; /* der erste aeussere von second_part */

for (i=0 ; i< second_part-code[0]; i++) 
  {  
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil;
    run=run->next->invers->next->invers->next; }

/* so -- jetzt ist der entsprechende cap abgeschnitten */

testergebnis=testcanon(nv, start, code+2);

/* jetzt wieder verkleben: */

run=start->invers->next;
run->name=run->merkename; run->invers=run->merkekante;


for (i=0; i< code[0]; i++) 
  { run=run->next->invers->next->invers->next;
    run->name=run->merkename; run->invers=run->merkekante;
     }
run=run->next->invers->next; /* jetzt muesste run der zweite aeussere auf dem Buckel sein */
run->name=run->merkename; run->invers=run->merkekante;

for (i=0; i<first_part; i++) 
  { 
    run=run->next->invers->next->invers->next;
    run->name=run->merkename; run->invers=run->merkekante;
     }

/* Jetzt wurde bis zur Mulde markiert. */

run=run->next->invers->next->invers->next->invers->next; /* der erste aeussere von second_part */

for (i=0 ; i< second_part-code[0]; i++) 
  {  
    run->name=run->merkename; run->invers=run->merkekante;
    run=run->next->invers->next->invers->next; }

return (2-testergebnis);


}


/*************************TESTE_CODE_MIRROR********************************************/

int teste_code_mirror(KNOTENTYP nv, KANTE *start, int code[])
/* s.o. */

{
int i, testergebnis;
KANTE *run;

/* zuerst den Pfad markieren: */

run=start->invers->prev;
run->merkename=run->name; run->merkekante=run->invers;
run->name=aussen; run->invers=nil;

for (i=0; i< code[0]; i++) 
  { run=run->prev->invers->prev->invers->prev;
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil; }

run=run->prev->invers->prev; /* jetzt muesste run der zweite aeussere auf dem Buckel sein */
run->merkename=run->name; run->merkekante=run->invers;
run->name=aussen; run->invers=nil;

for (i=0; i<first_part; i++) 
  { 
    run=run->prev->invers->prev->invers->prev; 
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil; }

/* Jetzt wurde bis zur Mulde markiert. */

run=run->prev->invers->prev->invers->prev->invers->prev; /* der erste aeussere von second_part */

for (i=0 ; i< second_part-code[0]; i++) 
  {  
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil;
    run=run->prev->invers->prev->invers->prev; }

/* so -- jetzt ist der entsprechende cap abgeschnitten */

testergebnis=testcanon_mirror(nv, start, code+2);

/* jetzt wieder verkleben: */

run=start->invers->prev;
run->name=run->merkename; run->invers=run->merkekante;


for (i=0; i< code[0]; i++) 
  { run=run->prev->invers->prev->invers->prev;
    run->name=run->merkename; run->invers=run->merkekante;
     }
run=run->prev->invers->prev; /* jetzt muesste run der zweite aeussere auf dem Buckel sein */
run->name=run->merkename; run->invers=run->merkekante;

for (i=0; i<first_part; i++) 
  { 
    run=run->prev->invers->prev->invers->prev;
    run->name=run->merkename; run->invers=run->merkekante;
     }

/* Jetzt wurde bis zur Mulde markiert. */

run=run->prev->invers->prev->invers->prev->invers->prev; /* der erste aeussere von second_part */

for (i=0 ; i< second_part-code[0]; i++) 
  {  
    run->name=run->merkename; run->invers=run->merkekante;
    run=run->prev->invers->prev->invers->prev; }

return(2-testergebnis);

}


/*************************BERECHNE_CODE******************************************/

void berechne_code(KNOTENTYP nv, KANTE *start, int code[])

{
int i;
KANTE *run;

/* zuerst den Pfad markieren: */

run=start->invers->next;
run->merkename=run->name; run->merkekante=run->invers;
run->name=aussen; run->invers=nil;

for (i=0; i< code[0]; i++) 
  { run=run->next->invers->next->invers->next;
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil; }

run=run->next->invers->next; /* jetzt muesste run der zweite aeussere auf dem Buckel sein */
run->merkename=run->name; run->merkekante=run->invers;
run->name=aussen; run->invers=nil;

for (i=0; i<first_part; i++) 
  { 
    run=run->next->invers->next->invers->next; 
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil; }

/* Jetzt wurde bis zur Mulde markiert. */

run=run->next->invers->next->invers->next->invers->next; /* der erste aeussere von second_part */

for (i=0 ; i< second_part-code[0]; i++) 
  {  
    run->merkename=run->name; run->merkekante=run->invers;
    run->name=aussen; run->invers=nil;
    run=run->next->invers->next->invers->next; }

/* so -- jetzt ist der entsprechende cap abgeschnitten */

testcanon_first_init(nv, start, code+2);

/* jetzt wieder verkleben: */

run=start->invers->next;
run->name=run->merkename; run->invers=run->merkekante;


for (i=0; i< code[0]; i++) 
  { run=run->next->invers->next->invers->next;
    run->name=run->merkename; run->invers=run->merkekante;
     }
run=run->next->invers->next; /* jetzt muesste run der zweite aeussere auf dem Buckel sein */
run->name=run->merkename; run->invers=run->merkekante;

for (i=0; i<first_part; i++) 
  { 
    run=run->next->invers->next->invers->next;
    run->name=run->merkename; run->invers=run->merkekante;
     }

/* Jetzt wurde bis zur Mulde markiert. */

run=run->next->invers->next->invers->next->invers->next; /* der erste aeussere von second_part */

for (i=0 ; i< second_part-code[0]; i++) 
  {  
    run->name=run->merkename; run->invers=run->merkekante;
    run=run->next->invers->next->invers->next; }

}

/***************************CANONICAL_PATH******************************************/

int canonical_path(PLANMAP map, int code[], KANTE *codestart)


{

KANTE *check_start[30]; /* hier sind alle 5-Eckkanten, die ebenfalls zu einem
			   geschlossenen Pfad fuehren und ein 5-Eck im zweiten Teil
			   an der gleichen Stelle haben */
KANTE *check_start_mirror[30];
int i, kante, test, listenlaenge=0, listenlaenge_mirror=0;
KANTE *run;
int newcode[4*N+2];


code[2]=0; /* alles ab hier muss erst noch berechnet werden */
automorphisms=mirror_symmetric=0;

/* geaendert -- der zweite Codeeintrag wird nicht mehr beruecksichtigt !! */


for (kante=0; kante < 30; kante++)
  { if ((run=F_eck_kanten[kante]) != codestart)
      { for (i=0; i< code[0]; i++) if (pathok(run,i)) return(0);
	/* jetzt die Pfade, bei denen der erste codeeintrag gleich ist: */
	if (pathok(run,code[0])) { check_start[listenlaenge]=run; listenlaenge++; }
      }
    /* und jetzt noch in Spiegelrichtung */
    if (first_part==second_part)
      {
	run=run->invers;
	for (i=0; i< code[0]; i++) if (pathok_mirror(run,i)) return(0);
	/* jetzt die Pfade, bei denen der erste codeeintrag gleich ist: */
	if (pathok_mirror(run,code[0])) { check_start_mirror[listenlaenge_mirror]=run; listenlaenge_mirror++; }
      }
  }

automorphisms=1;

/* die gleichguten Pfade muessen jetzt noch getestet werden auf den kleinsten Code */
if (listenlaenge+listenlaenge_mirror)
  { 
    berechne_code(map[0][0].name,codestart,code);
    automorphisms=1;
    for (i=0; i<listenlaenge; i++) 
      { test=teste_code(map[0][0].name,check_start[i],code);
	if (test==1) automorphisms++;
	if (test==0) return(0); }
    for (i=0; i<listenlaenge_mirror; i++)
      { test=teste_code_mirror(map[0][0].name,check_start_mirror[i],code);
	if (test==1) { automorphisms++; mirror_symmetric=1; }
	if (test==0) return(0); }
  }

return(1);

}

/************************CHECK_RAND**************************************************/

int check_rand(PLANMAP map)         
/*                                  _                                             */
/* Checkt, ob auf beiden Wegen von / \ zu \_/ ein 5-Eck liegt. Wenn nicht, gibt es
   einen echt in diesem Patch enthaltenen Patch, der die gleiche Randstruktur hat.
   Dabei reicht auch ein 5-Eck im rand, wenn es in der konvexen Ecke liegt, um den
   Patch nicht-reduzierbar zu machen.

   Checkt ebenfalls, ob ein anders liegender Pfad eventuell guenstiger -- dieser
   also nicht kanonisch ist.

   Code: { first_part, second_part, abstand1, abstand2, ... }

   abstand1 ist der Weg von der pseudoconvexen Ecke zum ersten fuenfeck auf dem
   second_part segment (also in prev-Richtung) und abstand2 auf dem first_part segment
   (also next-Richtung)

   first_part und second_part werden aber nicht explizit in code[] aufgenommen.

 */

{
int i, treffer=0, test;
KANTE *anfang, *merkestart, *codestart;
int code[4*N+2];


anfang=map[2*second_part+1]+1;
for (i=0, codestart=nil; (i<=second_part) && (codestart==nil); i++) 
  { if (checksize_right(anfang)==5) { codestart=anfang; code[0]=i; }
    else anfang=anfang->prev->invers->next->invers; }

if (codestart==nil) return(0);

treffer=0;

anfang=map[2*(second_part+1)];

for (i=0; (i<=first_part) && !treffer; i++) 
  { if (checksize_right(anfang)==5) { treffer=1; code[1]=i; }
    else anfang=anfang->invers->prev->invers->next; }

if (treffer==0) return(0);
if ((first_part==second_part) && (code[1]<code[0])) return(0); /* Spiegelbild besser */

/* jetzt checken, ob der Pfad kanonisch liegt: */ 
glue(map,testtube,&merkestart);
test = canonical_path(map,code,codestart);
unglue(map,testtube,merkestart);
if (!test) return(0);

return(1);
}

/**********************KONSTRUIERE**************************************/

void  konstruiere(int sechsecke, SEQUENZ sq, FLAECHENTYP code[], int codesize )
{

static PLANMAP map;
static int initialized=0;
static int maxp;
int p, p1, p2; /* laenge des cutpfades */
int x; /* treffpunkt auf dem Randpfad */
KNOTENTYP adresse1[8], adresse2[8];
ITEMLISTE *item1, *item2, *runitem;
KANTE *start1, *start2;
int merkeknoten1, merkeknoten2, sql;

if (!initialized) { init_map(map);
		    baue_pfad(map,first_part,second_part);
		        /* map[1][2] ist immer der Anfang des Pfades P */
		    maxp= max_knoten-randlaenge +1;
		    initialized=1; }

sql=sq.laenge;

if (sql==1)
  { /* dann kommen die Faelle 6 und 7 in Frage */
    if (docase[6]) /* P trifft sich selbst p>=7 ungerade, p1>2 gerade => p2>=5 ungerade */
      { 
	p2=(2*sq.sequenz[0])+1;
	adresse2[0]=5; /* 5 Fuenfecke in Teil 2 */
	adresse2[2]=sq.sequenz[0]; 
	for (p=p2+2; p<=maxp; p+=2)
	  { p1=p-p2; 
	    adresse1[0]=1; /* 1 Fuenfeck in Teil 1 */
	    adresse1[2]=(p1-2)/2; adresse1[3]=0; adresse1[4]=(p-1)/2; adresse1[5]=second_part; 
	    adresse1[6]=first_part+1;
	    mache_kanonisch(5, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      {
		insert_path(map,p,map[1]+2,map[randlaenge+p1]+2);
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+p1+2]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 5);
		merkeknoten2=map[0][0].name;
		for ( ; item1 != nil; item1=item1->next_item)
		  { anzahl_5ek=25;
		    is_ipr=1; /* einfach immer auf true setzen */
		    insert_patch(map, start1, item1, adresse1[0]);
		    if (is_ipr)
		       { ausgabe(stdout,map); }
		    delete_patch(map, start1, adresse1, merkeknoten2);
		  } 
		delete_patch(map, start2, adresse2, merkeknoten1);
		delete_path(map,p,map[1]+2,map[randlaenge+p1]+2);
	    } /* ende if item1 */
	} /* ende for p */
    } /* ende docase[6] */

    if (docase[7]) /*  P trifft sich selbst p>=8 gerade, p1>=3 ungerade => p2>=5 ungerade */
      {	p2=(2*sq.sequenz[0])+1;
	adresse2[0]=5; /* 5 Fuenfecke in Teil 2 */
	adresse2[2]=sq.sequenz[0]; 
	for (p=p2+1; p<=maxp; p+=2)
	  { p1=p-p2;
	    adresse1[0]=1; /* 1 Fuenfeck in Teil 1 */
	    adresse1[2]=(p-2)/2; adresse1[3]=0; adresse1[4]=(p1-1)/2; adresse1[5]=second_part; 
	    adresse1[6]=first_part+1;
	    mache_kanonisch(5, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      { 
		insert_path(map,p,map[1]+2,map[randlaenge+p1]+2);
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+p1+2]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 5);
		merkeknoten2=map[0][0].name;
		for ( ; item1 != nil; item1=item1->next_item)
		{ is_ipr=1; /* einfach immer auf true setzen */
		  anzahl_5ek=25;
		  insert_patch(map, start1, item1, adresse1[0]);
		  if (is_ipr)
		     { ausgabe(stdout,map); }
		  delete_patch(map, start1, adresse1, merkeknoten2);
		}
	    delete_patch(map, start2, adresse2, merkeknoten1);
	    delete_path(map,p,map[1]+2,map[randlaenge+p1]+2);
	    } /* ende if item1 */
	} /* ende for p */
    } /* ende docase[7] */
   } /* ende (sql==1) */

else
if (sql==2)
  { /* dann kommen die Faelle 2,3,5 und 8 in Frage */

    /* Fall 2: */
    if (docase[2]) /* Zweiter Fall: P trifft first_part p>=3 ungerade */
      { 
	adresse1[0]=4; /* 4 Fuenfecke in Teil 1 */
	adresse1[2]=sq.sequenz[0]; adresse1[3]=sq.sequenz[1]; 
 	if (sq.sequenz[0] <= first_part-1)
	  { x=sq.sequenz[0]; p=(2*sq.sequenz[1])+1;
	    adresse2[0]=2; /* 2 Fuenfecke in Teil 2 */
	    adresse2[2]=second_part; adresse2[3]=first_part-x; adresse2[4]=0; 
	    adresse2[5]=sq.sequenz[1]; /*=(p-1)/2*/
	    mache_kanonisch(4, adresse2+2);
	    item2=suche_item(adresse2);
	    if (item2)
	      { 
		insert_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
		start1=suchestart(map[randlaenge+2]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start1, code, 4);
		merkeknoten2=map[0][0].name;
		    for ( runitem=item2 ; runitem != nil; runitem=runitem->next_item)
		      { is_ipr=1; /* einfach immer auf true setzen */
			anzahl_5ek=20;
			insert_patch(map, start2, runitem, adresse2[0]);
			if (is_ipr) 
			  { ausgabe(stdout,map); }
			delete_patch(map, start2, adresse2, merkeknoten2); 
		      }
		delete_patch(map, start1, adresse1, merkeknoten1);
		delete_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
	      } /* ende if item2 */
	      } /* ende sq.sequenz[0] <= first_part-1 */
 	if ((sq.sequenz[1] <= first_part-1) && (sq.sequenz[1] != sq.sequenz[0]))
	  { x=sq.sequenz[1]; p=(2*sq.sequenz[0])+1; 
	    adresse2[0]=2; /* 2 Fuenfecke in Teil 2 */
	    adresse2[2]=second_part; adresse2[3]=first_part-x; adresse2[4]=0; 
	    adresse2[5]=sq.sequenz[0]; /*=(p-1)/2*/
	    mache_kanonisch(4, adresse2+2);
	    item2=suche_item(adresse2);
	    if (item2)
	      { 
		insert_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
		start1=suchestart(map[randlaenge+2]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start1, code, 4);
		    merkeknoten2=map[0][0].name;
		    for ( runitem=item2 ; runitem != nil; runitem=runitem->next_item)
		      { is_ipr=1; /* einfach immer auf true setzen */
			anzahl_5ek=20;
			insert_patch(map, start2, runitem, adresse2[0]);
			if (is_ipr) 
			  { ausgabe(stdout,map); }
			delete_patch(map, start2, adresse2, merkeknoten2); 
		      }
		    delete_patch(map, start1, adresse1, merkeknoten1);
		    delete_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
		  } /* ende if item2 */
	      } /* ende sq.sequenz[1] <= first_part-1 */
      } /* ende docase[2] */

    if (docase[3]) /* Dritter Fall: P trifft second_part p>=4 gerade */
      { 
	adresse2[0]=4; /* 4 Fuenfecke in Teil 2 */
	adresse2[2]=sq.sequenz[0]; adresse2[3]=sq.sequenz[1]; 
	if (sq.sequenz[0] <= second_part-2) /* dann kann sq.sequenz[0] als x dienen */
	  { 
	    x=sq.sequenz[0]; p=2*sq.sequenz[1];
	    adresse1[0]=2; /* 2 Fuenfecke in Teil 1 */
	    adresse1[2]=(p-2)/2; adresse1[3]=0; adresse1[4]=second_part-x-1; adresse1[5]=first_part+1;
	    mache_kanonisch(4, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      { 
		insert_path(map,p,map[1]+2,map[2*x+3]+2); 
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 4);
		merkeknoten2=map[0][0].name;
		for ( runitem=item1 ; runitem != nil; runitem=runitem->next_item)
		  { is_ipr=1; /* einfach immer auf true setzen */
		    anzahl_5ek=20;
		    insert_patch(map, start1, runitem, adresse1[0]);
		    if (is_ipr) 
		      { ausgabe(stdout,map); }
		    delete_patch(map, start1, adresse1, merkeknoten2);
		  }
		delete_patch(map, start2, adresse2, merkeknoten1);
		delete_path(map,p,map[1]+2,map[2*x+3]+2);
	      } /* ende if item2 */
	  } /* ende sq.sequenz[0] <= second_part-2 */
	if ((sq.sequenz[1] <= second_part-2) && (sq.sequenz[1] != sq.sequenz[0]))
	    /* dann kann sq.sequenz[1] auch als x dienen */
	  { 
	    x=sq.sequenz[1]; p=2*sq.sequenz[0];
	    adresse1[0]=2; /* 2 Fuenfecke in Teil 1 */
	    adresse1[2]=(p-2)/2; adresse1[3]=0; adresse1[4]=second_part-x-1; adresse1[5]=first_part+1;
	    mache_kanonisch(4, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      { 
		insert_path(map,p,map[1]+2,map[2*x+3]+2);
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 4);
		merkeknoten2=map[0][0].name;
		for ( runitem=item1 ; runitem != nil; runitem=runitem->next_item)
		  { is_ipr=1; /* einfach immer auf true setzen */
		    anzahl_5ek=20;
		    insert_patch(map, start1, runitem, adresse1[0]);
		    if (is_ipr) 
		      { ausgabe(stdout,map); }
		    delete_patch(map, start1, adresse1, merkeknoten2); 
		  }
		delete_patch(map, start2, adresse2, merkeknoten1);
		delete_path(map,p,map[1]+2,map[2*x+3]+2);
	      } /* ende if item2 */
	  } /* ende sq.sequenz[1] <= second_part-2 */
      } /* ende docase[3] */

 if (docase[5] && (sq.sequenz[1]==0)) 
    /* P trifft sich selbst p>=7 ungerade, p1 ungerade => p2 gerade. Der zweite Eintrag in sq muss immer
       0 sein, sonst geht es eh nicht */
    { 	adresse2[0]=4; /* 4 Fuenfecke in Teil 2 */
	adresse2[2]=sq.sequenz[0]; adresse2[3]=0; 
	p2=(2*sq.sequenz[0])+2;
	for (p=p2+1; p<=maxp; p+=2)
	  { p1=p-p2;
	    adresse1[0]=2; /* 2 Fuenfecke in Teil 1 */
	    adresse1[2]=(p-1)/2; adresse1[3]=(p1-1)/2; adresse1[4]=second_part; adresse1[5]=first_part+1;
	    mache_kanonisch(4, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      { 
		insert_path(map,p,map[1]+2,map[randlaenge+p1]+2);
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+p1+2]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 4);
		merkeknoten2=map[0][0].name;
		for ( ; item1 != nil; item1=item1->next_item)
		  { is_ipr=1; /* einfach immer auf true setzen */
		    anzahl_5ek=20;
		    insert_patch(map, start1, item1, adresse1[0]);
		    if (is_ipr)
			{ ausgabe(stdout,map); }
		    delete_patch(map, start1, adresse1, merkeknoten2);
		  }
		delete_patch(map, start2, adresse2, merkeknoten1); 
		delete_path(map,p,map[1]+2,map[randlaenge+p1]+2);
	    } /* ende if item1 */
	  } /* ende for p... */
    } /* ende docase[5] */
   
if (docase[8] && (sq.sequenz[1]==0)) 
               /* P trifft sich selbst p>=8 gerade, p1>=2 gerade => p2>=5 gerade.
		  Der zweite Eintrag in sq muss immer 0 sein, sonst geht es eh nicht */
    { 	adresse2[0]=4; /* 4 Fuenfecke in Teil 2 */
	adresse2[2]=sq.sequenz[0]; adresse2[3]=0; 
	p2=(2*sq.sequenz[0])+2; 
	for (p=p2+2; p<=maxp; p+=2)
	  { p1=p-p2;
	    adresse1[0]=2; /* 2 Fuenfecke in Teil 1 */
	    adresse1[2]=(p1-2)/2; adresse1[3]=p/2; adresse1[4]=second_part; adresse1[5]=first_part+1;
	    mache_kanonisch(4, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      { 
		insert_path(map,p,map[1]+2,map[randlaenge+p1]+2);
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+p1+2]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 4);
		merkeknoten2=map[0][0].name;
		for ( ; item1 != nil; item1=item1->next_item)
		  { is_ipr=1; /* einfach immer auf true setzen */
		    anzahl_5ek=20;
		    insert_patch(map, start1, item1, adresse1[0]);
		    if (is_ipr && (check_rand(map)))
		       { ausgabe(stdout,map); }
		    delete_patch(map, start1, adresse1, merkeknoten2);
		  }
		delete_patch(map, start2, adresse2, merkeknoten1); 
		delete_path(map,p,map[1]+2,map[randlaenge+p1]+2);
	    } /* ende if item1 */
	} /* ende for p */
    } /* ende docase[8] */
  } /* ende (sql==2) */

else
/* d.h. if (sql==3)  */
  { /* dann kommen die Faelle 1 und 4 in Frage */
    if (docase[1]) /* P trifft first_part p>=4 gerade */
      { adresse2[0]=3; /* 3 Fuenfecke in Teil 2 */
	adresse2[2]=sq.sequenz[0]; adresse2[3]=sq.sequenz[1]; adresse2[4]=sq.sequenz[2];
	if ((sq.sequenz[0]==second_part) && (sq.sequenz[1]<first_part) && sq.sequenz[2])
	  { x=first_part-sq.sequenz[1]; p=2*sq.sequenz[2];
	    adresse1[0]=3; /* 3 Fuenfecke in Teil 1 */
	    adresse1[2]=x; adresse1[3]=(p-2)/2; adresse1[4]=0; 
	    mache_kanonisch(3, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      { 
		insert_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 3);
		merkeknoten2=map[0][0].name;
		for ( ; item1 != nil; item1=item1->next_item)
		  { is_ipr=1; /* einfach immer auf true setzen */
		    anzahl_5ek=15;
		    insert_patch(map, start1, item1, adresse1[0] );
		    if (is_ipr) 
		      { ausgabe(stdout,map); }
		  delete_patch(map, start1, adresse1, merkeknoten2);
		  }
		delete_patch(map, start2, adresse2, merkeknoten1);
		delete_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
	    } /* ende if item1 */
	} /* ende if ((sq.se...) && (...)) */
	/* vielleicht kann es auch in anderer Reihenfolge eingeklebt werden (um eins verdreht): */
	if ((sq.sequenz[1]==second_part) && (sq.sequenz[2]<first_part) && sq.sequenz[0]
	    && ((sq.sequenz[0]!=second_part) || (sq.sequenz[2]!=second_part)))
	  { x=first_part-sq.sequenz[2]; p=2*sq.sequenz[0];
	    adresse1[0]=3; /* 3 Fuenfecke in Teil 1 */
	    adresse1[2]=x; adresse1[3]=(p-2)/2; adresse1[4]=0; 
	    mache_kanonisch(3, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      { 
		insert_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 3);
		merkeknoten2=map[0][0].name;
		for ( ; item1 != nil; item1=item1->next_item)
		  { is_ipr=1; /* einfach immer auf true setzen */
		    anzahl_5ek=15;
		    insert_patch(map, start1, item1, adresse1[0] );
		    if (is_ipr) 
		      { ausgabe(stdout,map); }
		  delete_patch(map, start1, adresse1, merkeknoten2);
		  }
		delete_patch(map, start2, adresse2, merkeknoten1);
		delete_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
	    } /* ende if item1 */
	} /* ende if ((sq.se...) && (...)) */

	/* oder um zwei verdreht: */
	if ((sq.sequenz[2]==second_part) && (sq.sequenz[0]<first_part) && sq.sequenz[1]
	    && ((sq.sequenz[0]!=second_part) || (sq.sequenz[1]!=second_part)))
	  { x=first_part-sq.sequenz[0]; p=2*sq.sequenz[1];
	    adresse1[0]=3; /* 3 Fuenfecke in Teil 1 */
	    adresse1[2]=x; adresse1[3]=(p-2)/2; adresse1[4]=0; 
	    mache_kanonisch(3, adresse1+2);
	    item1=suche_item(adresse1);
	    if (item1)
	      { 
		insert_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
		start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
		start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
		merkeknoten1=map[0][0].name;
		anzahl_5ek=0;
		insert_patch_code(map, start2, code, 3);
		merkeknoten2=map[0][0].name;
		for ( ; item1 != nil; item1=item1->next_item)
		  { is_ipr=1; /* einfach immer auf true setzen */
		    anzahl_5ek=15;
		    insert_patch(map, start1, item1, adresse1[0] );
		    if (is_ipr) 
		      { ausgabe(stdout,map); }
		  delete_patch(map, start1, adresse1, merkeknoten2);
		  }
		delete_patch(map, start2, adresse2, merkeknoten1);
		delete_path(map,p,map[1]+2,map[randlaenge-2*x]+2);
	    } /* ende if item1 */
	} /* ende if ((sq.se...) && (...)) */
    } /* ende docase[1] */

    if (docase[4]) /* P trifft second_part p>=3 ungerade */
    { 
      adresse1[0]=3; /* 3 Fuenfecke in Teil 1 */
      adresse1[2]=sq.sequenz[0]; adresse1[3]=sq.sequenz[1]; adresse1[4]=sq.sequenz[2];
      if ((sq.sequenz[0]==first_part+1) && sq.sequenz[1] && (sq.sequenz[2]<second_part-1))
	{ p=(2*sq.sequenz[1])+1; x=second_part-1-sq.sequenz[2];
	  adresse2[0]=3; /* 3 Fuenfecke in Teil 2 */
	  adresse2[2]=(p-1)/2; adresse2[3]=x; adresse2[4]=0;
	  mache_kanonisch(3, adresse2+2);
	  item2=suche_item(adresse2);
	  if (item2)
	    { 
	      insert_path(map,p,map[1]+2,map[2*x+3]+2);
	      start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
	      start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
	      merkeknoten1=map[0][0].name;
	      anzahl_5ek=0;
	      insert_patch_code(map, start1, code, 3);
	      merkeknoten2=map[0][0].name;
	      for ( ; item2 != nil; item2=item2->next_item)
		{ is_ipr=1; /* einfach immer auf true setzen */
		  anzahl_5ek=15;
		  insert_patch(map, start2, item2, adresse2[0]);
		  if (is_ipr) 
		    { ausgabe(stdout,map); }
		  delete_patch(map, start2, adresse2, merkeknoten2); 
		}
	      delete_patch(map, start1, adresse1, merkeknoten1);
	      delete_path(map,p,map[1]+2,map[2*x+3]+2);
	    } /* ende for item2 */
	} /* ende if ((sq.se...) && (...)) */
      /* vielleicht kann es auch in anderer Reihenfolge eingeklebt werden (um eins verdreht): */
      if ((sq.sequenz[1]==first_part+1) && sq.sequenz[2] && (sq.sequenz[0]<second_part-1) &&
	  ((sq.sequenz[0]!=first_part+1)|| (sq.sequenz[2]!=first_part+1)))
	{ p=(2*sq.sequenz[2])+1; x=second_part-1-sq.sequenz[0];
	  adresse2[0]=3; /* 3 Fuenfecke in Teil 2 */
	  adresse2[2]=(p-1)/2; adresse2[3]=x; adresse2[4]=0;
	  mache_kanonisch(3, adresse2+2);
	  item2=suche_item(adresse2);
	  if (item2)
	    { 
	      insert_path(map,p,map[1]+2,map[2*x+3]+2);
	      start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
	      start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
	      merkeknoten1=map[0][0].name;
	      anzahl_5ek=0;
	      insert_patch_code(map, start1, code, 3);
	      merkeknoten2=map[0][0].name;
	      for ( ; item2 != nil; item2=item2->next_item)
		{ is_ipr=1; /* einfach immer auf true setzen */
		  anzahl_5ek=15;
		  insert_patch(map, start2, item2, adresse2[0]);
		  if (is_ipr) 
		    { ausgabe(stdout,map); }
		  delete_patch(map, start2, adresse2, merkeknoten2); 
		}
	      delete_patch(map, start1, adresse1, merkeknoten1);
	      delete_path(map,p,map[1]+2,map[2*x+3]+2);
	    } /* ende for item2 */
	} /* ende if ((sq.se...) && (...)) */
      /* oder um zwei verdreht: */
      if ((sq.sequenz[2]==first_part+1) && sq.sequenz[0] && (sq.sequenz[1]<second_part-1) &&
	  ((sq.sequenz[1]!=first_part+1)|| (sq.sequenz[0]!=first_part+1)))
	{ p=(2*sq.sequenz[0])+1; x=second_part-1-sq.sequenz[1];
	  adresse2[0]=3; /* 3 Fuenfecke in Teil 2 */
	  adresse2[2]=(p-1)/2; adresse2[3]=x; adresse2[4]=0;
	  mache_kanonisch(3, adresse2+2);
	  item2=suche_item(adresse2);
	  if (item2)
	    { 
	      insert_path(map,p,map[1]+2,map[2*x+3]+2);
	      start1=suchestart(map[randlaenge]+2); /* Kante, die in den ersten Teil zeigt */
	      start2=suchestart(map[randlaenge+1]+2); /* Kante, die in den zweiten Teil zeigt */
	      merkeknoten1=map[0][0].name;
	      anzahl_5ek=0;
	      insert_patch_code(map, start1, code, 3);
	      merkeknoten2=map[0][0].name;
	      for ( ; item2 != nil; item2=item2->next_item)
		{ is_ipr=1; /* einfach immer auf true setzen */
		  anzahl_5ek=15;
		  insert_patch(map, start2, item2, adresse2[0]);
		  if (is_ipr) 
		    { ausgabe(stdout,map); }
		  delete_patch(map, start2, adresse2, merkeknoten2); 
		}
	      delete_patch(map, start1, adresse1, merkeknoten1);
	      delete_path(map,p,map[1]+2,map[2*x+3]+2);
	    } /* ende for item2 */
	} /* ende if ((sq.se...) && (...)) */
    } /* ende docase[4] */
  } /* ende sql==3 */
}




/*************************USAGE**********************************/

void usage()
{
fprintf(stderr,"Usage: gencaps i j [noout vega mirror] [case 1..8]\n");
exit(0);
}



/************************MAX_HEX*****************************************/

/* berechnet die maximal moegliche Anzahl von 6-Ecken, die in einen
   vorgegebenen Rand zusammen mit 5 Fuenfecken passen. Benutzt,
   dass der Spiralalgorithmus, startend mit den 5-Ecken
   einen Patch mit minimaler Randlaenge erzeugt */

int max_hex(int max_rand)
{
int ringgroesse, sechsecke, randlaenge;

/* Fuer IPR ist die kleinste Ursprungskonfiguration das eindeutige
   C40 H10 mit P=5, sonst die entsprechende Spirale, die mit den 5 Ecken
   beginnt und ein 6-Eck enthaelt. Dafuer wird die Maximale Anzahl von 6-Ecken
   berechnet, wobei der Rand um 1 laenger sein darf. Dann muss eines der 6-Ecke
   wieder in ein 5-Eck umgewandelt werden. */


if (IPR)
  { if (max_rand < 18) { fprintf(stderr,"No IPR structures with such a short boundary possible\n");
			 exit(0); }
    randlaenge=19; sechsecke=11; ringgroesse=10;
  }

else
  { if (max_rand < 10) { fprintf(stderr,"No structures with such a short boundary possible\n");
			 exit(0); }
    randlaenge=11; sechsecke=1; ringgroesse=6;
  }


while (randlaenge+1 <= max_rand) /* oder genauer: randlaenge+2 <= maxrand+1 */
  { randlaenge += 2;
    sechsecke += ringgroesse;
    ringgroesse ++;
  }

sechsecke--; /* ein Sechseck wird wieder ein Fuenfeck -- das haette natuerlich auch gleich durch
		einen geringeren Startwert gemacht werden koennen, aber so ist es wohl leichter
		verstaendlich */

return(sechsecke);
}

/**************************BELEGE_DIVISOR_OR**********************************/

void belege_divisor_or(int d[7][7])
/* Beim Verkleben wird in der Tat ein Nebenklassenargument benutzt. Da aber nur
   sehr wenige orientierungserhaltende Automorphismen auftreten koennen (1er, 2er,
   dreier, fuenfer und sechser-Drehungen), kann hier einfach ein "divisor" angegeben
   werden, so dass jeweils n/d[j][k] jeweils um 1 gegeneinander verdrehte Fullerene
   generiert werden muessen. n ist die Randlaenge und j, k die Ordnung der Drehsymmetrie
   der beiden Kappen. d[j][k] ist genauer kgV(j,k), wird hier aber in einem Feld
   festgelegt, um nicht immer neu berechnet werden zu muessen */
{}


/****************************KONSTRUIERE_BB_FULLERENE***********************/

void konstruiere_bb_fullerene()
{
KANTE *start1, *start2, *laufstart2, *run;
int laufcap1, laufcap2, merkeknoten1, merkeknoten2;
BBLISTENTRY *cap1_bble, *cap2_bble; /* cap1 ist das kanonische cap */
FLAECHENTYP *cap1, *cap2;
int fund_groesse1, fund_groesse2, fund_groesse1_sp, fund_groesse2_sp, i, j;
/* Anzahl der Flaechen im Fundamentalbereich -- einmal mit, einmal ohne Spiegelungen */
int spiegelsymm1, spiegelsymm;
int divisor_or[7][7];

belege_divisor_or(divisor_or);
baue_full_tube();
start1=tube[1]+2;
start2=tube[tube[0][0].name]+1;

fprintf(stderr,"Not yet ready for doing it...\n"); exit(0);
if (start2->name != aussen) { fprintf(stderr,"Error with starting edge of the second cap (127) !\n");
			     exit(0); }

for (cap1_bble=first_bbliste; cap1_bble!=nil; cap1_bble=cap1_bble->next)
  for (laufcap1=0; laufcap1<cap1_bble->anzahl; laufcap1++)
       { 
	 cap1=cap1_bble->code+(8*laufcap1);
	 spiegelsymm1= (int) *cap1;
	 cap1++;
	 fund_groesse1= (int)*cap1;
	 if (spiegelsymm1==0) fund_groesse1_sp=fund_groesse1; 
	   else if (spiegelsymm1==1) fund_groesse1_sp=fund_groesse1; fprintf(stderr,"hier weitermachen....\n");
	 cap1++;
	 merkeknoten1=tube[0][0].name;
	 anzahl_5ek=30;
	 insert_patch_code(tube, start1, cap1 , 6);
	 for (cap2_bble=first_bbliste; cap2_bble!=nil; cap2_bble=cap2_bble->next)
	   {
	   for (laufcap2=0; laufcap2<cap2_bble->anzahl; laufcap2++)
	     if (codecmp(cap1,cap2_bble->code+(8*laufcap2)+2,6)>=0) /* der erste Code muss groessergleich sein */
	       { 
		 cap2=cap2_bble->code+(8*laufcap2);
		 spiegelsymm = spiegelsymm1 || (*cap2); /* wenn eines der beiden caps eine Spiegelsymmetrie hat, braucht 
							   das tube nur mit einem der Spiegelbilder von cap2 verklebt  werden */
		 cap2++;
		 fund_groesse2= (int)*cap2;
		 cap2++;
		 for (i=0, laufstart2=start2; (i<fund_groesse1) && (i<fund_groesse2); 
		      i++, laufstart2=laufstart2->next->invers->next->invers->next)
		   { fprintf(stderr,"normal  \n"); schreibecode(cap1,6); schreibecode(cap2,6);
		     graphenzaehler++;

		     merkeknoten2=tube[0][0].name;
                     anzahl_5ek=30;
		     insert_patch_code(tube, laufstart2, cap2 , 6);
		     planarcode(stdout,tube); 
		     for (run=laufstart2, j=0; j<first_part; 
				j++, run=run->next->invers->next->invers->next) 
		       { run->name = aussen; run->invers=nil; }
		     tube[0][0].name = merkeknoten2;
		     if (!spiegelsymm)
		       { 
			 graphenzaehler++;
			 merkeknoten2=tube[0][0].name;
                         anzahl_5ek=30;
			 insert_patch_code_inv(tube, laufstart2, cap2 , 6);
			 planarcode(stdout,tube); 
			 for (run=laufstart2, j=0; j<first_part; 
			      j++, run=run->next->invers->next->invers->next) 
			   { run->name = aussen; run->invers=nil; }
			 tube[0][0].name = merkeknoten2;
		       }
		  
		   }
	       }
	 }
	 for (run=start1, j=0; j<first_part; 
	      j++, run=run->next->invers->next->invers->next) 
	   { run->name = aussen; run->invers=nil; }
	 tube[0][0].name = merkeknoten1; 
       }
}


/**************************MAIN*********************************************/

int main(argc,argv)

int argc; char *argv[];

{
  clock_t savetime=0, buffertime;
  struct tms TMS;
  int i, which_case, j;
  int perimeter_int;
  double perimeter = -1; /* Der Durchmesser in Sechseckeinheiten kann ganzzahlig oder
			    ganzzahlig+1/2 sein */
  int shift = INT_MAX;


if (argc<2) usage();


fprintf(stderr,"Command: ");
for (i=0; i<argc; i++) fprintf(stderr,"%s ",argv[i]); fprintf(stderr,"\n");

/* leider arbeitet das Programm mit i-1 und j-1, wenn man die (10)^i(01)^j Notation nimmt.
   es wurde urspruenglich noch mit first= Anzahl_Spitzen und so geschrieben..... */

if (argc<3) { fprintf(stderr,"caps i j [ipr noout case x \n"); exit(0); }

first_part = atoi(argv[1])-1; /* -1 korrektur um von der (10)^i(01)^j Notation zu der
				 Spitzennotation zu kommen */
second_part = atoi(argv[2])-1; /* == -1 bei bb-patch. Haesslich aber alles aendern auf 
				  (10)^i(01)^j Notation ist fehleranfaellig */

/* Konvention: second_part >= first_part, wenn nicht bb ----- das hilft, weniger patches zu 
   speichern und ist aus Symmetriegruenden Moeglich */

if ((second_part>=0) && (first_part>second_part)) { i=first_part; first_part=second_part;
						    second_part=i; }

if (first_part<0) { i=first_part; first_part=second_part; second_part=i; }


if (first_part > second_part) max_segment=first_part; else max_segment=second_part;

for (i=3; i<argc; i++)
  { 
    switch (argv[i][0])
      {
      case 'i': { if (strcmp(argv[i],"ipr")==0) IPR=1;
		    else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }

      case 'f': { if (strcmp(argv[i],"fullerenes")==0) fullerenes=1;
		    else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }

      case 't': { if (strcmp(argv[i],"tube")==0) { i++; tubelaenge = atoi(argv[i]); }
		    else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }

      case 'p': { if (strcmp(argv[i],"perim")==0) { i++; perimeter = atof(argv[i]); }
                  else if (strcmp(argv[i],"pid")==0)
                    {fprintf(stdout,"%d\n",getpid());  fflush(stdout);}
		  else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }

      case 's': { if (strcmp(argv[i],"shift")==0) { i++; shift = atoi(argv[i]); }
		       else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }

      case 'd': { if (strcmp(argv[i],"dt")==0) dangling_tube=1;
                     else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }


      case 'n': { if (strcmp(argv[i],"noout")==0) noout=1;
		    else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }

      case 'c': { if (strcmp(argv[i],"case")==0) { i++; which_case = atoi(argv[i]); 
						   docase[which_case]=2;
						   for (j=1; j<=8; j++) 
						     if (docase[j]==1) docase[j]=0; }
		    else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }

      case 'v': { if (strcmp(argv[i],"vega")==0) { vegaout=1; srand(time(0));  }
                  else { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
		  break; }


      default: { fprintf(stderr,"Nonidentified option: %s \n",argv[i]); exit(0); }
      }
  }

if (dangling_tube && noout) dangling_tube=0; /* keine Wirkung in diesem Fall */
if (tubelaenge==0) dangling_tube=0; else dangling_tube=1;
/* Option dt und tube getrennt sind mehr aus historischen Gruenden noch drin. Nur tube reicht. */

if (second_part < 0) { bbpatch=1; 
		       first_part++; /* korrigieren, da dann beide Notationen gleich sind */
		       randlaenge=2*first_part; fprintf(stderr,"bbpatch generation\n"); }
 else randlaenge=2*(first_part+second_part) + 4;

if ((max_segment+1)*randlaenge > N) 
  { fprintf(stderr,"The constant N is too small for the tube -- at least %d\n",(max_segment+1)*randlaenge);
    exit(0); }



/* Auskommentiert -- vorerst werden i und j gegeben 
if (perimeter<0) { fprintf(stderr,"The perimeter must be given !\n"); exit(0); }
if (shift==INT_MAX) { fprintf(stderr,"The shift must be given !\n"); exit(0); }

perimeter_int=(int)nint(10*perimeter);
if ((((perimeter_int % 10) == 0) && (shift % 2)) ||
    ((perimeter_int % 10) && ((shift % 2) == 0)))
  { fprintf(stderr,"The combination of perimeter and shift is not possible.\n");
    exit(0); }

if (perimeter_int % 5) { fprintf(stderr,"Impossible perimeter\n"); exit(0); }


if (shift==0) { bbpatch=1; randlaenge=2*perimeter; }
else { if (shift>0) 
	 { if (shift % 2)
	     { first_part= (perimeter_int-(10*shift)-5)/10;
	       second_part= shift-1; }
	   else
	     { first_part= (perimeter_int-(10*shift))/10;
	       second_part= shift-1; }
	 }
       else 
	 { if (shift % 2)
	     { second_part= (perimeter_int-(10*shift)-5)/10;
	       first_part= shift-1; }
	   else
	     { second_part= (perimeter_int-(10*shift))/10;
	       first_part= shift-1; }
	 }
       randlaenge=2*(first_part+second_part) + 4;
     }
*/



if (fullerenes)
  { 

    if (bbpatch) { last_bbliste=first_bbliste=malloc(sizeof(BBLISTENTRY));
		   if (last_bbliste==nil) { fprintf(stderr,"Can not get more memory for BBLISTENTRIES (1)\n");
					    exit(1); }
		   last_bbliste->anzahl=0; last_bbliste->next=nil; }
    else
      {
	srand(time(0));
	sprintf(longfilename,"longfile_tmp_%d",rand());
	while ((longfile=fopen(longfilename,"r")) != nil) /* das file gabs schon */
	  { fclose(longfile);
	    sprintf(longfilename,"longfile_tmp_%d",rand()); }
	fclose(longfile);
	longfile=fopen(longfilename,"w");
	sprintf(shortfilename,"shortfile_tmp_%d",rand());
	while ((shortfile=fopen(shortfilename,"r")) != nil) /* das file gabs schon */
	  { fclose(shortfile);
	    sprintf(shortfilename,"shortfile_tmp_%d",rand()); }
	fclose(shortfile);
	shortfile=fopen(shortfilename,"w");
      }
  }


max_sechsecke = max_hex(randlaenge);
if (S<max_sechsecke) { fprintf(stderr,"Constant S too small -- at least %d !\n",max_sechsecke); 
		       exit(0); }

max_kanten=(randlaenge+(6*max_sechsecke)+30)/2; 
max_knoten= (2*max_kanten + first_part + second_part + 2)/3;

if (fullerenes) { if (randlaenge*(tubelaenge-1) + (2*max_knoten) > N) 
		    { fprintf(stderr,"N too small for the Fullerenes -- at least %d \n" ,randlaenge*(tubelaenge-1) + (2*max_knoten));
		      exit(0); }
		}


if (3*N >= leer) { fprintf(stderr,"Definition of N too large for the datatypes used.\n");
		    exit(0); }
if (S+6 >= FL_MAX) { fprintf(stderr,"Definition of S too large for the datatypes used.\n");
		    exit(0); }

fprintf(stderr,"Will generate patches with at most %d hexagons %d edges and %d vertices.\n",max_sechsecke,max_kanten,max_knoten);

initialize_list();
if (!bbpatch || dangling_tube) baue_dangling_tube();

max_entry= (max_knoten-randlaenge +1)/2; /* maximaler Eintrag einer Sequenzliste -- erstmal maxp/2 */
if (second_part>max_entry) max_entry=second_part;
if ((first_part+1)>max_entry) max_entry=first_part+1;

baue_patches(max_sechsecke,1);  /* erster Durchlauf */

if (!bbpatch)  baue_patches(max_sechsecke,2);  /* zweiter Durchlauf */

times(&TMS);
savetime= TMS.tms_utime;
fprintf(stderr,"Time for generating the patches: %.1f seconds \n",(double)savetime/time_factor);

if (fullerenes)
  { if (bbpatch) konstruiere_bb_fullerene();
  }


fprintf(stderr,"Generated %d caps.\n",graphenzaehler);

if (!bbpatch)
  {
for (i=1; i<=5; i++)
fprintf(stderr,"(had to store %d patches with boundary length %d)\n",storepatchzaehler[i],i);
fprintf(stderr,"(had to store %d patches in all)\n",storepatchzaehler[1]+storepatchzaehler[2]+storepatchzaehler[3]+storepatchzaehler[4]+storepatchzaehler[5]);
}

if (auts_statistic[1]) fprintf(stderr,"With group C1: %d \n",auts_statistic[1]);
if (auts_statistic[2]) fprintf(stderr,"With group C2: %d \n",auts_statistic[2]);
if (auts_statistic[3]) fprintf(stderr,"With group C3: %d \n",auts_statistic[3]);
if (auts_statistic[5]) fprintf(stderr,"With group C5: %d \n",auts_statistic[5]);
if (auts_statistic[6]) fprintf(stderr,"With group C6: %d \n",auts_statistic[6]);

if (auts_statistic_mirror[2]) fprintf(stderr,"With group Cs: %d \n",auts_statistic_mirror[2]);
if (auts_statistic_mirror[4]) fprintf(stderr,"With group C2v: %d \n",auts_statistic_mirror[4]);
if (auts_statistic_mirror[6]) fprintf(stderr,"With group C3v: %d \n",auts_statistic_mirror[6]);
if (auts_statistic_mirror[10]) fprintf(stderr,"With group C5v: %d \n",auts_statistic_mirror[10]);
if (auts_statistic_mirror[12]) fprintf(stderr,"With group C6v: %d \n",auts_statistic_mirror[12]);


fprintf(stderr,"ende programm\n");

return(0);
}

