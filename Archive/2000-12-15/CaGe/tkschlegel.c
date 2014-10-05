/*
 *	tkschlegel.c		Calculation of Schlegel diagrams for
 *				2-connected, not necessarily 3-connected,
 *				planar maps.
 *
 *	Based on a Schlegel program by Bor Plestenjak, Ljubljana 1995
 *
 *	ODF (Olaf Delgado Friedrichs) 1998/03/09
 */


#ifndef USE_TCL
#define USE_TCL 1		/* if 0, graphics output is disabled	*/
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if USE_TCL
#include<tcl8.0.h>
#include<tk8.0.h>
#endif /*USE_TCL*/


#define MAX_LINE_LENGTH 3000


struct coor { double x,y; };	/* 2D coordinates			*/

struct vertex			/* data for a single vertex		*/
{
  int fixed;			/* in outer cycle?			*/
  int depth;			/* distance of farthest other vertex	*/
  struct coor v;		/* current coordinates			*/
  struct coor disp;		/* displacement (temporary)		*/
  int valency;			/* the number of neighbors		*/
  int orig_valency;		/* valency in original graph		*/
  int alloc_valency;		/* how much space was allocated for adj	*/
  int *adj;			/* list of indices of neighbors		*/
  int *rev;			/* indices of reverse edges in
				   their corresponding adjacency lists	*/
};

struct triangle { int a, b, c; };

struct vertex *v = NULL;	/* list of all vertices			*/
int nvertices = 0;
int n_orig_vertices;
int n_alloc_vertices;

struct triangle *tri = NULL;
int ntriangles = 0;

int *outer_cycle = NULL;
int cycle_length = 0;
double radius = 1000.0;

int maxstep = 0;
int verbose = 0;
int sleep_ms = 0;

void preprocess(void);		/* find and process faces		*/
void main_loop(int, int, int);	/* main loop of the algorithm		*/
int read_graph(void);		/* reads graph				*/
int write_graph(int);		/* writes graph				*/
double norm(struct coor c);	/* euclidean norm			*/

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


int
main( int argc, char *argv[] )
{
  int c;
  int draw = 0;
  int write_all_vertices = 0;
  int tutte_mode = 0;

  extern char* optarg;

  while ((c = getopt(argc, argv, "ad:n:otTvw:")) != EOF) {
    switch (c) {
    case 'a':
      write_all_vertices = !write_all_vertices;
      break;
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
    case 't':
      tutte_mode = 1;
      break;
    case 'T':
      tutte_mode = 2;
      break;
    case 'v':
      verbose = !verbose;
      break;
    case 'w':
      sleep_ms = atoi(optarg);
      break;
    default:
      break;
    }
  }

  if (read_graph() != 0)
    return 1;

  if (maxstep == 0) {
    if (nvertices > 200)
      maxstep = nvertices / 2;
    else
      maxstep = 100;
  }

  preprocess();

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

  main_loop(maxstep, draw, tutte_mode);
  write_graph(write_all_vertices);

#if USE_TCL
  if (draw > 0)
    exit_tk();
#endif /*USE_TCL*/

  return 0;
}

/* ------------------------------------------------------------------------ */


int
read_graph(void)
{
  char line[MAX_LINE_LENGTH];	/* one line from file			*/
  char *remain_line, *p;	/* remaining line			*/
  int  lineno;			/* number of current input line		*/
  int  read_char;		/* number of read characters		*/
  int  vert;			/* read vertex index			*/
  int  tnei;			/* read neighbour index (vertex index)	*/
  int  i;			/* counter				*/
  int  alloc;			/* number of vertices allocated		*/
  int  dim;			/* dimension of input coordinates	*/
  double tmpx, tmpy, tmpz;	/* read coordinates			*/
  int  tmp;			/* temporary */

  alloc = 10;
  v = (struct vertex *) malloc( alloc * sizeof(struct vertex) );
  if (v == NULL) {
    fprintf(stderr, "Not enough memory at start");
    return 4;
  }

  /* First vertices and edges are read from std input */

  dim = 0;
  lineno = 0;
  while(fgets(line, MAX_LINE_LENGTH, stdin) == line)
  {
    lineno++;
    if (lineno == 1) {
      sscanf(line, " %n", &read_char);
      if (line[read_char] == '>') {
	if (strncmp(line+read_char, ">>writegraph3d", 14) == 0)
	  dim = 3;
	else if (strncmp(line+read_char, ">>writegraph2d", 14) == 0)
	  dim = 2;
	else {
	  fprintf(stderr, "Unknown format %s.\n", line+read_char);
	  return 1;
	}
	continue;
      }
    }

    if(sscanf(line,"%d %n", &vert, &read_char) !=1 )
      continue;

    if(vert==0)
      break;			/* End of graph data */

    if(vert != ++nvertices) {
      fprintf(stderr,
	      "At line %d: vertex numbers must be sequential.\n", lineno);
      return 2;
    }
    remain_line = &line[read_char];

    if(sscanf(remain_line,"%lf %lf %n", &tmpx, &tmpy, &read_char) < 2) {
      fprintf(stderr,"Coordinates format error in vertex %d!\n",nvertices);
      return 3;
    }
    remain_line += read_char;

    if (dim == 0) {
      if(sscanf(remain_line,"%d %n", &tmp, &read_char) < 1)
	dim = 2;
      else if (tmp <= 0 || strchr(".eE", remain_line[read_char]))
	dim = 3;
      else
	dim = 2;
    }

    if (dim == 3) {
      if(sscanf(remain_line,"%lf %n", &tmpz, &read_char) < 1) {
	fprintf(stderr,"Missing z coordinate at vertex %d!\n",nvertices);
	return 3;
      }
      remain_line += read_char;
    }

    if (nvertices >= alloc) {
      alloc = alloc * 2;
      v = (struct vertex *) realloc( v, alloc * sizeof(struct vertex) );
      if (v == NULL) {
	fprintf(stderr, "Not enough memory at vertex %d", nvertices);
	return 4;
      }
    }
      
    v[nvertices].v.x = 0.0;
    v[nvertices].v.y = 0.0;
    v[nvertices].fixed = 0;

    i = 0;
    p = remain_line;
    while(sscanf(p, "%d%n", &tnei, &read_char) == 1) {
      i++;
      p += read_char;
    }

    v[nvertices].valency = i;
    v[nvertices].orig_valency = i;
    v[nvertices].alloc_valency = 2 * i;
    v[nvertices].adj =
      (int *) malloc( v[nvertices].alloc_valency * sizeof(int) );
    if (v[nvertices].adj == NULL) {
      fprintf(stderr, "Not enough memory at vertex %d", nvertices);
      return 4;
    }

    i = 0;
    p = remain_line;
    while(sscanf(p, "%d%n", &tnei, &read_char) == 1) {
      v[nvertices].adj[i] = tnei;
      i++;
      p += read_char;
    }
  }

  n_orig_vertices = nvertices;
  n_alloc_vertices = alloc;

  if (fscanf(stdin,"%d",&cycle_length) == 1) {
    lineno++;
    if (cycle_length < 3 || fgets(line,MAX_LINE_LENGTH,stdin) != line) {
      fprintf(stderr,"Outer cycle format error 1 at line %d!\n", lineno);
      return 7;
    }

    outer_cycle = (int *) malloc( sizeof(int) * cycle_length );
    if (outer_cycle == NULL) {
      fprintf(stderr, "Not enough memory");
      return;
    }

    remain_line=line;
    for (i = 0; i < cycle_length; i++) {
      if(sscanf(remain_line,"%d%n",&vert,&read_char) != 1) {
	fprintf(stderr,"Outer cycle format error 2 in %d step",i);
	return 7;
      }
      outer_cycle[i] = vert;
      remain_line += read_char;
    }
  }

  return 0;
}


/* ------------------------------------------------------------------------ */


int
write_graph(all)
{
  int i, j, w;

  if (all) {
    for (i = 1; i <= nvertices; i++) {
      printf("%3d %8.3lf %8.3lf", i, v[i].v.x, v[i].v.y);
      for (j = 0; j < v[i].valency; j++)
	printf( " %3d", v[i].adj[j] );
      printf("\n");
    }
  }
  else {
    for (i = 1; i <= n_orig_vertices; i++) {
      printf("%3d %8.3lf %8.3lf", i, v[i].v.x, v[i].v.y);
      for (j = 0; j < v[i].orig_valency; j++)
	printf( " %3d", v[i].adj[j] );
      printf("\n");
    }
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
  struct coor *from, *to;
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

  for (i = 1; i <= n_orig_vertices; i++) {
    xa = v[i].v.x * scale + xmid;
    ya = v[i].v.y * scale + ymid;

    for (j = 0; j < v[i].orig_valency; j++) {
      k = v[i].adj[j];
      if (k > i) {
	xe = v[k].v.x * scale + xmid;
	ye = v[k].v.y * scale + ymid;

	sprintf(buf, "add_line %lf %lf %lf %lf #000", xa, ya, xe, ye);
	code = Tcl_Eval(interp, buf);
	if (code != TCL_OK) {
	  fprintf(stderr, "in Tcl_VarEval: %s\n", interp->result);
	  return code;
	}
      }
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


double
norm(struct coor c)
{
  return sqrt(c.x*c.x+c.y*c.y);
}

/* ------------------------------------------------------------------------ */


void
make_reverses(void)
{
  int i, j, k;
  struct vertex *w;

  for (i = 1; i <= nvertices; i++) {
    v[i].rev = (int *)malloc(sizeof(int) * v[i].valency);
    if (v[i].rev == NULL) {
      fprintf(stderr, "Not enough memory");
      return;
    }
    
    for (j = 0; j < v[i].valency; j++) {
      w = &(v[v[i].adj[j]]);
      for (k = 0; k < w->valency; k++)
	if (w->adj[k] == i)
	  break;
      if (k >= w->valency) {
	fprintf(stderr,
		"Oops! Graph inconsistent at vertices %d and %d\n",
		i, v[i].adj[j]);
	abort();
      }
      v[i].rev[j] = k;
    }
  }
}


/* ------------------------------------------------------------------------ */


void
calculate_depths()
{
  int *depth, *queue;
  int head, tail;
  int i, j, k, n;

  depth = (int *) malloc( sizeof(int) * (nvertices+1) );
  queue = (int *) malloc( sizeof(int) * (nvertices+1) );
  if (depth == NULL || queue == NULL) {
    fprintf(stderr, "Not enough memory in depth_at_vertex()\n");
    exit(1);
  }

  for (n = 1; n <= nvertices; n++) {
    for (i = 1; i <= nvertices; i++)
      depth[i] = 0;

    depth[n] = 1;
    queue[1] = n;
    head = 1;
    tail = 0;

    while (tail < head) {
      i = queue[++tail];
      for (j = 0; j < v[i].orig_valency; j++) {
	k = v[i].adj[j];
	if (depth[k] == 0) {
	  depth[k] = depth[i] + 1;
	  queue[++head] = k;
	}
      }
    }

    if (head < nvertices) {
      fprintf(stderr, "Graph is not connected\n");
      exit(1);
    }
    else if (head > nvertices) {
      fprintf(stderr, "Bug in breadth-first search\n");
      exit(1);
    }

    v[n].depth = depth[queue[head]];
  }

  free(depth);
  free(queue);
}


/* ------------------------------------------------------------------------ */

void
make_outer_cycle(int i, int j)
{
  int k, n;
  int fdeg;
  int i1, j1;
  int tmp;

  if (outer_cycle == NULL) {
    fdeg = 0;
    i1 = i;
    j1 = j;

    do {
      fdeg++;
      tmp = v[i1].adj[j1];
      j1 = (v[i1].rev[j1]+1)%v[tmp].orig_valency;
      i1 = tmp;
    } while (i1 != i || j1 != j);

    cycle_length = fdeg;

    outer_cycle = (int *) malloc( sizeof(int) * cycle_length );
    if (outer_cycle == NULL) {
      fprintf(stderr, "Not enough memory");
      return;
    }

    fdeg = 0;
    do {
      outer_cycle[fdeg] = i1;
      fdeg++;
      tmp = v[i1].adj[j1];
      j1 = (v[i1].rev[j1]+1)%v[tmp].orig_valency;
      i1 = tmp;
    } while (i1 != i || j1 != j);
  }

  if (verbose) {
    fprintf(stderr, "Outer cycle: ");
    for (k = 0; k < cycle_length; k++) {
      n = outer_cycle[k];
      fprintf(stderr, " %d", n);
    }
    fprintf(stderr, "\n");
  }

  for (k = 0; k < cycle_length; k++) {
    n = outer_cycle[k];
    v[n].fixed = 1;
    v[n].v.x = radius*cos(2*k*M_PI/cycle_length);
    v[n].v.y = -radius*sin(2*k*M_PI/cycle_length);
  }

}


/* ------------------------------------------------------------------------ */


void
preprocess(void)
{
  int **seen;
  int i, j, k, t;
  int mindepth, best_i, best_j, best_fdeg;

  make_reverses();
  calculate_depths();

  seen = (int **) malloc( sizeof(int *) * (n_orig_vertices+1) );
  if (seen == NULL) {
    fprintf(stderr, "Not enough memory");
    return;
  }

  ntriangles = 0;

  for (i = 1; i <= n_orig_vertices; i++) {
    seen[i] = (int *) malloc( sizeof(int) * v[i].orig_valency );
    if (seen[i] == NULL) {
      fprintf(stderr, "Not enough memory");
      return;
    }
    for (j = 0; j < v[i].orig_valency; j++)
      seen[i][j] = 0;
    ntriangles += v[i].orig_valency;
  }
  
  tri = (struct triangle *) malloc( sizeof(struct triangle) * ntriangles );
  if (tri == NULL) {
    fprintf(stderr, "Not enough memory");
    return;
  }
  t = 0;

  mindepth = nvertices;
  best_i = 0;
  best_j = 0;
  best_fdeg = 0;

  for (i = 1; i <= n_orig_vertices; i++) {
    for (j = 0; j < v[i].orig_valency; j++) {
      if (!seen[i][j]) {
	int fdeg = 0;
	int i1 = i, j1 = j;
	int tmp;
	int depth = v[i].depth;

	do {
	  if (v[i1].depth > depth)
	    depth = v[i1].depth;

	  fdeg++;
	  seen[i1][j1] = 1;
	  tmp = v[i1].adj[j1];
	  j1 = (v[i1].rev[j1]+1)%v[tmp].orig_valency;
	  i1 = tmp;
	} while (i1 != i || j1 != j);

	if (depth < mindepth || (depth == mindepth && fdeg > best_fdeg)) {
	  mindepth = depth;
	  best_i = i;
	  best_j = j;
	  best_fdeg = fdeg;
	}

	if (++nvertices >= n_alloc_vertices) {
	  n_alloc_vertices = n_alloc_vertices * 2;
	  v = (struct vertex *)
	    realloc( v, n_alloc_vertices * sizeof(struct vertex) );
	  if (v == NULL) {
	    fprintf(stderr,
		    "Not enough memory at temporary vertex %d", nvertices);
	    return;
	  }
	}

	v[nvertices].fixed = 0;
	v[nvertices].valency = 0;
	v[nvertices].orig_valency = 0;
	v[nvertices].alloc_valency = fdeg;
	v[nvertices].v.x = v[nvertices].v.y = 0.0;
	v[nvertices].adj =
	  (int *) malloc( v[nvertices].alloc_valency * sizeof(int) );
	if (v[nvertices].adj == NULL) {
	  fprintf(stderr, "Not enough memory at vertex %d", nvertices);
	  return;
	}

	do {
	  v[i1].adj[v[i1].valency++] = nvertices;
	  v[nvertices].adj[v[nvertices].valency++] = i1;
	  tmp = v[i1].adj[j1];
	  j1 = (v[i1].rev[j1]+1)%v[tmp].orig_valency;
	  i1 = tmp;
	} while (i1 != i || j1 != j);

	for (k = 0; k < fdeg; k++) {
	  tri[t].a = nvertices;
	  tri[t].b = v[nvertices].adj[k];
	  tri[t].c = v[nvertices].adj[(k+1)%fdeg];
	  t++;
	  if (t > ntriangles) {
	    fprintf(stderr, "Too many triangles\n" );
	    exit(1);
	  }
	}
      }
    }
  }

  if (t < ntriangles) {
    fprintf( stderr, "Only %d triangles instead of expected %d\n",
	     t, ntriangles );
    ntriangles = t;
  }

  make_outer_cycle(best_i, best_j);
}


/* ------------------------------------------------------------------------ */

void
main_loop(int maxstep, int draw, int tutte_mode)
{
  int step=0;
  int i, j, k, z;
  double koef, temp, d, fact, maxdisp;
  struct vertex *a, *b, *c;
  struct coor center, delta;

  for (step = 1; step <= maxstep; step++) {
    for(i = 1; i <= nvertices; i++)
      v[i].disp.x = v[i].disp.y = 0.0;

    if (tutte_mode) {
      for (i = 1; i <= n_orig_vertices; i++) {
	for (j = 0; j < v[i].orig_valency; j++) {
	  k = v[i].adj[j];
	  if (k > i) {
	    delta.x = v[i].v.x - v[k].v.x;
	    delta.y = v[i].v.y - v[k].v.y;
	    if (tutte_mode == 2)
	      koef = norm(delta);
	    else
	      koef = 1.0;
	    v[i].disp.x -= koef * delta.x;
	    v[i].disp.y -= koef * delta.y;
	    v[k].disp.x += koef * delta.x;
	    v[k].disp.y += koef * delta.y;
	  }
	}
      }
    }
    else {
      for (i = 0; i < ntriangles; i++) {
	a = &(v[tri[i].a]);
	b = &(v[tri[i].b]);
	c = &(v[tri[i].c]);

	koef = 0.5 * (   ( b->v.y - a->v.y ) * ( c->v.x - a->v.x )
			 - ( b->v.x - a->v.x ) * ( c->v.y - a->v.y ) );
	koef *= koef;

	center.x = ( a->v.x + b->v.x + c->v.x ) / 3.0;
	center.y = ( a->v.y + b->v.y + c->v.y ) / 3.0;

	a->disp.x += koef * (center.x - a->v.x);
	a->disp.y += koef * (center.y - a->v.y);
	b->disp.x += koef * (center.x - b->v.x);
	b->disp.y += koef * (center.y - b->v.y);
	c->disp.x += koef * (center.x - c->v.x);
	c->disp.y += koef * (center.y - c->v.y);
      }
    }

    temp = 1.0 - (double)step / (double)maxstep;
    temp *= 40.0 * temp * temp;

    maxdisp = 0.0;
    for(i = 1; i <= nvertices; i++) {
      if (v[i].fixed == 0) {
	d = norm(v[i].disp);
	if (d > maxdisp)
	  maxdisp = d;
	fact = d > temp ? temp / d : 1.0;
	v[i].v.x += v[i].disp.x * fact;
	v[i].v.y += v[i].disp.y * fact;
      }
    }

#if USE_TCL
    if (draw > 0 && step % draw == 0)
      if (draw_graph(step) != TCL_OK)
	break;
#endif /*USE_TCL*/

    if (maxdisp < 0.0001 * radius)
      return;
  }

#if USE_TCL
  if (draw > 0 && maxstep % draw != 0)
    draw_graph(maxstep);
#endif /*USE_TCL*/
}


/*
** Local Variables:
** compile-command: "make -k tkschlegel"
** End:
*/

/* -- EOF tkschlegel.c -- */
