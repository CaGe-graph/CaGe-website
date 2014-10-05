/*
 *	vega-schlegel.c		Calculation of Schlegel diagrams for
 *				3-connected planar graphs/maps.
 *
 *	Based on a Schlegel program by Bor Plestenjak, Ljubljana 1995
 *
 *	Re-writing in C and addition of Tcl/Tk output option by
 *	ODF (Olaf Delgado Friedrichs) 1998/03/09
 */


#ifndef USE_TCL
#define USE_TCL 1		/* if 0, graphics output is disabled	*/
#endif

#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<sys/types.h>
#include<unistd.h>

#if USE_TCL
#include<tcl.h>
#include<tk.h>
#endif /*USE_TCL*/

#define MAX_LINE_LENGTH 3000
#define MAX_ITERATIONS 500

struct coor { float x,y; };  /* 2D coordinates */

struct vertex {              /* vertex */
  struct vertex *next;
  int dist;                  /* peripheral distance (0 fixed) */
  struct coor v,v1,v2,rez;
  };

struct edge {                /* edge */
   struct edge *next;
   struct vertex *u, *v;
  };

struct vertex *v=NULL;       /* vertices */
struct edge *e=NULL;         /* edges */
int nvertices=0, cycle_length, max_dist=0;
int maxx,maxy;                     /* screen dimensions */

double radius = 1000.0;

int maxstep = 0;
int sleep_ms = 0;

void normalize_coordinates(void);  /* normalizes and centralizes vertices */
void calculate_distances(void);    /* calculates edge distances */
void main_loop(int, int);          /* main_loop with the algorithm */
int koor(float x, int max);        /* translation to screen coordinates */
int read_graph(void);              /* reads graph */
int write_graph(void);       	   /* writes graph */
float dol(struct coor c);          /* length */

#if USE_TCL
int draw_graph(int);		/* draws graph				*/
#endif /*USE_TCL*/

/* ------------------------------------------------------------------------ */

#if USE_TCL

Tcl_Interp *interp;		/* Interpreter for this application. */


int
init_tk()
{
  char val[20];
  int code;

  interp = Tcl_CreateInterp();

  code = Tcl_Init(interp);
  if (code != TCL_OK) {
    fprintf(stderr, "in Tcl_Init: %s\n", interp->result);
    return code;
  }

  code = Tk_Init(interp);
  if (code != TCL_OK) {
    fprintf(stderr, "in Tk_Init: %s\n", interp->result);
    return code;
  }

  Tcl_StaticPackage(interp, "Tk", Tk_Init, (Tcl_PackageInitProc *) NULL);

  sprintf(val, "%d", maxstep);
  if (Tcl_SetVar(interp, "maxstep", val, TCL_LEAVE_ERR_MSG) == NULL) {
    fprintf(stderr, "in Tcl_SetVar: %s\n", interp->result);
    return code;
  }

  code = Tcl_Eval(interp, "source $env(EMBED_LIB)/tkembed.tcl");
  if (code != TCL_OK) {
    fprintf(stderr, "in Tcl_Eval: %s\n", interp->result);
    return code;
  }

  return TCL_OK;
}


int
exit_tk()
{
  if (Tk_GetNumMainWindows() > 0) {
    int code = Tcl_Eval(interp, "done_calculation");
    if (code != TCL_OK) {
      fprintf(stderr, "in Tcl_Eval: %s\n", interp->result);
      return code;
    }
  }

  while (Tk_GetNumMainWindows() > 0) {
    int code = Tcl_Eval(interp, "tkwait variable window_changed");
    if (code != TCL_OK) {
      fprintf(stderr, "in Tcl_Eval: %s\n", interp->result);
      return code;
    }
    draw_graph(maxstep);
  }

  Tcl_Eval(interp, "exit");
  Tcl_DeleteInterp(interp);
}

#endif /*USE_TCL*/

/* ------------------------------------------------------------------------ */

main( int argc, char *argv[] ) {

  int c;
  int draw = 0;

  while ((c = getopt(argc, argv, "d:n:ow:")) != EOF) {
    switch (c) {
    case 'd':
      draw = atoi(optarg);
      break;
    case 'n':
      maxstep = atoi(optarg);
      break;
    case 'o':
      fprintf(stdout,"%d\n",getpid());
      fflush(stdout);
      break;
    case 'w':
      sleep_ms = atoi(optarg);
      break;
    default:
      break;
    }
  }

  if (read_graph()!=0) { return 1; };

  if (maxstep == 0) {
    if (nvertices > 1000)
      maxstep = nvertices / 2;
    else
      maxstep = MAX_ITERATIONS;
  }

  calculate_distances();

#if USE_TCL
  if (draw > 0) {
    init_tk();
    draw_graph(0);
    c = Tcl_Eval(interp, "stop_go");
    if (c != TCL_OK) {
      fprintf(stderr, "in Tcl_Eval: %s\n", interp->result);
      return c;
    }
  }
#endif /*USE_TCL*/

  main_loop(maxstep, draw);
  write_graph();

#if USE_TCL
  if (draw > 0)
    exit_tk();
#endif /*USE_TCL*/

  return 0;
}

/* ------------------------------------------------------------------------ */

int read_graph(void)  {

  char line[MAX_LINE_LENGTH];  /* one line from file                   */
  char *remain_line;           /* remaining line                       */
  int  read_char;              /* number of read characters            */
  int  vert;                   /* read vertex index                    */
  int  tnei;                   /* read neighbour index (vertex index)  */
  int  i, k;                   /* counters                             */
  float tmpx, tmpy;            /* read coordinates                     */
  struct vertex *tmpv;         /* temporary vertex (new)               */
  struct vertex *last_v;       /* last vertex in a line                */
  struct vertex *indv;         /* moving pointer in loops              */
  struct edge *tmpe;           /* temporary edge (new)                 */
  struct edge *last_e;         /* last edge in a line                  */

  last_v=v;
  last_e=e;
  /* First vertices and edges are read from std input */

  while(fgets(line,MAX_LINE_LENGTH,stdin)==line) {
    nvertices++;
    if(sscanf(line,"%d %n",&vert,&read_char)!=1) {
      fprintf(stderr,"Data format error in line %d",nvertices);
      return 1;
      }

    if(vert==0) { break; }   /* End of graph data */

    if(vert!=nvertices) {
      fprintf(stderr,"Data format error in line %d",nvertices);
      return 2;
      }
    remain_line=&line[read_char];

    if(sscanf(remain_line,"%f %f %n",&tmpx,&tmpy,&read_char)!=2) {
      fprintf(stderr,"Coordinates format error in vertex %d",nvertices);
      return 3;
      }
    remain_line+=read_char;

    if ((tmpv=malloc(sizeof(struct vertex)))==NULL) {
      fprintf(stderr,"Not enough memory in vertex %d",nvertices);
      return 4;
      }

    if (last_v==NULL)
      v=last_v=tmpv;
    else
      last_v=last_v->next=tmpv;

     tmpv->next=NULL;
     tmpv->v.x=tmpx;
     tmpv->v.y=tmpy;
     tmpv->dist=-1;
     tmpv->v1.x=tmpv->v2.x=tmpv->v1.y=tmpv->v2.y=0;

     while(sscanf(remain_line,"%d%n",&tnei,&read_char)==1) {
       if (tnei<vert) {
	 if ((tmpe=malloc(sizeof(struct edge)))==NULL) {
	   fprintf(stderr,"Not enough memory in vertex %d",nvertices);
	   return 4;
	   };
	 for(indv=v,i=1; i!=tnei; i++,indv=indv->next);

	 if (last_e==NULL)
	   e=last_e=tmpe;
	 else
	   last_e=last_e->next=tmpe;

	 tmpe->next=NULL;
	 tmpe->u=tmpv;
	 tmpe->v=indv;
	 }
       remain_line+=read_char;
       }
    }
   nvertices--;

  /* Preberemo fiksne tocke */
  normalize_coordinates();

  if (fscanf(stdin,"%d \n",&cycle_length)!=1) {
    fprintf(stderr,"Outer cycle format error");
    return 5;
    }
  if (fgets(line,MAX_LINE_LENGTH,stdin)!=line) {
    fprintf(stderr,"Outer cycle format error 1");
    return 7;
    }
  remain_line=line;
  for(i=1; i<=cycle_length; i++) {
    if(sscanf(remain_line,"%d %n",&vert,&read_char)!=1) {
      fprintf(stderr,"Outer cycle format error 2 in %d step",i);
      return 7;
      }
    for(k=1, indv=v; k!=vert; k++, indv=indv->next);
    indv->dist=0;
    indv->v.x=radius*cos(2*i*M_PI/cycle_length);
    indv->v.y=radius*sin(2*i*M_PI/cycle_length);
    remain_line+=read_char;
    }
  return 0;
  }

/* ------------------------------------------------------------------------ */

int
write_graph(void)
{
  int i, j;
  struct vertex *uu, *vv;
  struct edge *ee;

  for (i = 1, vv = v; vv != NULL; ++i, vv = vv->next) {
    (void)printf("%3d %8.3lf %8.3lf", i, vv->v.x, vv->v.y);
    for (ee = e; ee != NULL; ee = ee->next) {
      if (ee->u == vv) {
	for (j = 1, uu = v; uu != NULL; ++j, uu = uu->next)
	  if (uu == ee->v)
	    break;
	(void)printf(" %3d", j);
      }
      if (ee->v == vv) {
	for (j = 1, uu = v; uu != NULL; ++j, uu = uu->next)
	  if (uu == ee->u)
	    break;
	(void)printf(" %3d", j);
      }
    }
    (void)printf("\n");
  }
  return 0;
}


/* ------------------------------------------------------------------------ */

#if USE_TCL

int
draw_graph(int step)
{
  int code;
  int i, j, k;
  double width, height;
  double xmid, ymid;
  double xscale, yscale, scale;
  struct edge *tmpe;
  double xa, ya, xe, ye;
  static char buf[256];

  if (Tk_GetNumMainWindows() <= 0)
    return TCL_RETURN;

  sprintf(buf, "update; set step %d", step);

  code = Tcl_Eval(interp, buf);
  if (code != TCL_OK) {
    fprintf(stderr, "in Tcl_Eval: %s\n", interp->result);
    return code;
  }

  if (Tk_GetNumMainWindows() <= 0)
    return TCL_RETURN;

  code = Tcl_Eval(interp, "get_canvas_size");
  if (code != TCL_OK) {
    fprintf(stderr, "in Tcl_Eval: %s\n", interp->result);
    return code;
  }
  sscanf(interp->result, "%lf %lf", &width, &height);

  xmid = width / 2.0;
  ymid = height / 2.0;
  xscale = (xmid - 10.0) / radius;
  yscale = (ymid - 10.0) / radius;

  if (xscale <= yscale)
    scale = xscale;
  else
    scale = yscale;

  code = Tcl_Eval(interp, "clear_canvas");
  if (code != TCL_OK) {
    fprintf(stderr, "in Tcl_Eval: %s\n", interp->result);
    return code;
  }

  for (tmpe = e; tmpe != NULL; tmpe = tmpe->next) {
    xa = tmpe->u->v.x * scale + xmid;
    ya = tmpe->u->v.y * scale + ymid;
    xe = tmpe->v->v.x * scale + xmid;
    ye = tmpe->v->v.y * scale + ymid;

    sprintf(buf, "add_line %lf %lf %lf %lf #000", xa, ya, xe, ye);
    code = Tcl_Eval(interp, buf);
    if (code != TCL_OK) {
      fprintf(stderr, "in Tcl_VarEval: %s\n", interp->result);
      return code;
    }
  }

  code = Tcl_Eval(interp, "update idletasks");
  if (code != TCL_OK) {
    fprintf(stderr, "in Tcl_Eval: %s\n", interp->result);
    return code;
  }

  Tcl_Sleep(sleep_ms);

  return TCL_OK;
}

#endif /*USE_TCL*/

/* ------------------------------------------------------------------------ */

void normalize_coordinates(void) {

  struct vertex *indv;
  float max=0, sumx=0 ,sumy=0, r;

  for(indv=v; indv!=NULL; indv=indv->next) {
    sumx+=indv->v.x;
    sumy+=indv->v.y;
    }
  sumx/=nvertices;
  sumy/=nvertices;
  for(indv=v; indv!=NULL; indv=indv->next) {
    indv->v.x-=sumx;
    indv->v.y-=sumx;
    }
  for(indv=v; indv!=NULL; indv=indv->next) {
    r=sqrt(indv->v.x*indv->v.x+indv->v.y*indv->v.y);
    if (r>max) max=r;
    }
  max/=radius;
  if (max>0)
    for(indv=v; indv!=NULL; indv=indv->next) {
      indv->v.x/=max;
      indv->v.y/=max;
      }
  }

/* ------------------------------------------------------------------------ */

void calculate_distances(void) {

  struct vertex *indv;
  struct edge *tmpe;
  int to_do;

  to_do=nvertices-cycle_length;
  while (to_do>0) {
    for (tmpe=e; tmpe!=NULL; tmpe=tmpe->next) {
      if ((tmpe->u->dist==max_dist) && (tmpe->v->dist==-1)) {
	tmpe->v->dist=max_dist+1;
	to_do--;
	}
      if ((tmpe->v->dist==max_dist) && (tmpe->u->dist==-1)) {
	tmpe->u->dist=max_dist+1;
	to_do--;
	}
      }
    max_dist++;
    }
  max_dist++;
  }

/* ------------------------------------------------------------------------ */

int koor(float x, int max) {
    return (int)((float)max*(1+x/radius)/2+0.5);
  }

/* ------------------------------------------------------------------------ */

float dol(struct coor c) {
  return sqrt(c.x*c.x+c.y*c.y+0.1);
  }

/* ------------------------------------------------------------------------ */

void main_loop(int maxstep, int draw) {

  int step=0,i,cont=0;
  float koef,d,d1,dd, factor=80, epsilon=0.15, fac2, razm, maxpr2;
  struct coor delta,dis,t;
  struct vertex *indv;
  struct edge *tmpe;

  t.x=t.y=factor;
  fac2=1.4+max_dist/24;
  while ((step<maxstep) && !cont ) {
    step++;
    for (indv=v; indv!=NULL; indv=indv->next)
      indv->rez.x=indv->rez.y=0;
    /* Calculation of attractive forces */
    for (tmpe=e; tmpe!=NULL; tmpe=tmpe->next) {
      delta.x=tmpe->u->v.x - tmpe->v->v.x;
      delta.y=tmpe->u->v.y - tmpe->v->v.y;
      d=dol(delta);
      razm=(2*max_dist - tmpe->u->dist - tmpe->v->dist)/max_dist;
      koef=d*exp(fac2*razm);
      dis.x=delta.x*koef;
      dis.y=delta.y*koef;
      tmpe->u->rez.x-=dis.x;
      tmpe->u->rez.y-=dis.y;
      tmpe->v->rez.x+=dis.x;
      tmpe->v->rez.y+=dis.y;
      }
    for (indv=v; indv!=NULL; indv=indv->next) {
      d1=dol(indv->rez);
      dd=0.5 * ((dol(t)<d1) ? dol(t)/d1 : 1);
      if (indv->dist>0) {
	indv->v.x+=indv->rez.x*dd;
	indv->v.y+=indv->rez.y*dd;
	}
      }

    if (step>2) {
      maxpr2=0;
      for (indv=v; indv!=NULL; indv=indv->next) {
	delta.x=indv->v.x - indv->v2.x;
	delta.y=indv->v.y - indv->v2.y;
	dd=sqrt(delta.x*delta.x+delta.y*delta.y);
	if (dd>maxpr2) maxpr2=dd;
	}
      if (maxpr2<epsilon) cont=1;
      }

    for(indv=v; indv!=NULL; indv=indv->next) {
      indv->v2=indv->v1;
      indv->v1=indv->v;
      }

    koef=exp(-4*(float)step/ ((step<250) ? 250 : (step+1)));
    t.x=t.y=factor*koef;

#if USE_TCL
    if (draw > 0 && step % draw == 0)
      if (draw_graph(step) != TCL_OK)
	break;
#endif /*USE_TCL*/
  }

#if USE_TCL
  if (draw > 0 && maxstep % draw != 0)
    draw_graph(maxstep);
#endif /*USE_TCL*/
}

/*
** Local Variables:
** compile-command: "make -k vega-schlegel"
** End:
*/

/* -- EOF vega-schlegel.c -- */
