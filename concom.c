/*
 * Copyright (c) 2009 Justin Poliey <jdp34@njit.edu>
 * Licensed under the ISC License, see COPYING for details
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_SIZET ((size_t)(~(size_t)0)-2)

enum { CC_ERR, CC_OK };
enum { T_INIT = -2, T_EOF, T_EOL, T_WORD, T_SYMBOL, T_BQUOTE, T_EQUOTE, T_BDEF, T_EDEF };
enum { O_SYMBOL, O_NUMBER, O_QUOTATION };

struct object;
struct interpreter;

typedef int (*wordfn)(struct interpreter *i);

#define OBJ_HEADER int type; int line;
	
struct object {
	OBJ_HEADER
};
	
struct symbol {
	OBJ_HEADER
	char *string;
	int frozen;
};

struct quotation {
	OBJ_HEADER
	size_t size, cap;
	struct object **items;
};

#define OBJECT(o)    ((struct object *)(o))
#define SYMBOL(o)    ((struct symbol *)(o))
#define QUOTATION(o) ((struct quotation *)(o))

struct parser {
	char *source;
	int line;
	char c, *buffer;
	size_t bufused, bufsize;
};

struct word {
	char *name;
	wordfn fn;
	struct quotation *quote;
	struct word *next;
};

struct call_frame {
	int line;
	char *word;
	struct call_frame *next;
};

struct interpreter {
	int line, done;
	size_t top, cap;
	struct object **stack;
	struct word *words;
	struct call_frame *frame;
};

#define next(p)    (p->c = *p->source++)
#define reset(p)   (p->bufused = 0)
#define current(p) (p->c)
#define token(p)   (p->buffer)

static int save(struct parser *p, int c) {
	if (p->bufused + 1 >= p->bufsize) {
		size_t newsize;
		if (p->bufsize >= MAX_SIZET/2) {
			return CC_ERR;
		}
		newsize = p->bufsize * 2;
		p->buffer = (char *)realloc(p->buffer, sizeof(char) * newsize);
		if (p->buffer == NULL) {
			return CC_ERR;
		}
		p->bufsize = newsize;
	}
	p->buffer[p->bufused++] = (char)c;
	return CC_OK;
}

static void scan_comment(struct parser *p) {
	while (current(p) != '\r' && current(p) != '\n' && current(p) != EOF) {
		next(p);
	}
}

static void scan_symbol(struct parser *p) {
	while (isalpha(current(p)) && isupper(current(p)) && (current(p) != EOF)) {
		save(p, current(p));
		next(p);
	}
	save(p, '\0');
}

static void scan_word(struct parser *p) {
	save(p, current(p));
	next(p);
	while (((isalpha(current(p)) && islower(current(p))) || isdigit(current(p)) || (current(p) == '\'')) && (current(p) != EOF)) {
		save(p, current(p));
		next(p);
	}
	save(p, '\0');
}

void error(int fatal, const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
  	va_end(args);
  	if (fatal) {
  		exit(fatal);
  	}
}

#define fatal(FMT,ARGS...)         (error(1, "fatal: " #FMT "\n", ##ARGS));
#define parse_error(L,FMT,ARGS...) ({i->done = 1; error(0, "parse error: %d: " #FMT "\n", L, ##ARGS);})
#define runtime_error(FMT,ARGS...) ({ \
	struct call_frame *cf = i->frame;; \
	i->done = 1; \
	error(0, "runtime error: " #FMT "\n", ##ARGS); \
	while (cf != NULL) { \
		printf("called from %d: inside `%s'\n", cf->line, cf->word); \
		cf = cf->next; \
	} \
})

int scan(struct parser *p) {
	if (current(p) == T_INIT) {
		next(p);
	}
	for (;;) {
		switch (current(p)) {
			case '\n':
			case '\r':
				next(p);
				return T_EOL;
			case ' ':
			case '\t':
				next(p);
				break;
			case '#':
				next(p);
				scan_comment(p);
				break;
			case ':':
				next(p);
				return T_BDEF;
			case ';':
				next(p);
				return T_EDEF;
			case '[':
				next(p);
				return T_BQUOTE;
			case ']':
				next(p);
				return T_EQUOTE;
			case '\0':
			case EOF:
				return T_EOF;
			default:
				reset(p);
				if (isalpha(current(p))) {
					if (isupper(current(p))) {
						scan_symbol(p);
						return T_SYMBOL;
					}
					else {
						scan_word(p);
						return T_WORD;
					}
				}
				error(1, "syntax error: %d: unrecognized input `%c' 0x%X\n", p->line, current(p), current(p));
				return T_EOF;
		}
	}
}

struct word *lookup(struct interpreter *i, char *name) {
	struct word *w = i->words;
	while (w != NULL) {
		if (strcmp(w->name, name) == 0) {
			return w;
		}
		w = w->next;
	}
	return NULL;
}

#define NIL ((struct object *)0)

#define push(i, o) ({ \
	if (((i)->top + 1) >= (i)->cap) { \
		(i)->cap *= 2; \
		(i)->stack = (struct object **)realloc((i)->stack, sizeof(struct object *) * (i)->cap); \
		if ((i)->stack == NULL) { \
			fatal("not enough memory to grow stack"); \
			return CC_ERR; \
		} \
	} \
	(i)->stack[((i)->top)++] = (struct object *)(o); \
})

#define pop(i) ({ \
	if ((i)->top <= 0) { \
		runtime_error("stack underflow"); \
		return CC_ERR; \
	} \
	(i)->stack[--((i)->top)]; \
})

#define peek(i) ({ \
	(i)->stack[(i)->top-1]; \
})

struct object *quotation_eval(struct interpreter *);
struct object *call(struct interpreter *i, struct word *w, int line) {
	struct object *result;
	struct call_frame *frame;
	if ((frame = (struct call_frame *)malloc(sizeof(struct call_frame))) == NULL) {
		fatal("out of memory to add call frame");
	}
	frame->line = line;
	frame->word = w->name;
	frame->next = i->frame;
	i->frame = frame;
	if (w->fn != NULL) {
		result = (w->fn(i) == CC_ERR) ? NIL : peek(i);
	}
	else {
		push(i, w->quote);
		result = (quotation_eval(i) == NIL) ? NIL : peek(i);
	}
	frame = i->frame;
	i->frame = i->frame->next;
	free(frame);
	return result;
}

struct object *symbol_new(struct interpreter *i, char *string, int frozen) {
	struct symbol *s;
	s = (struct symbol *)malloc(sizeof(struct symbol));
	if (s == NULL) {
		fatal("not enough memory to create new symbol");
	}
	s->line = i->line;
	s->type = O_SYMBOL;
	s->string = strdup(string);
	s->frozen = frozen;
	push(i, s);
	return (struct object *)s;
}

struct object *quotation_new(struct interpreter *i) {
	struct quotation *q;
	q = (struct quotation *)malloc(sizeof(struct quotation));
	if (q == NULL) {
		fatal("not enough memory to create new quotation");
	}
	q->line = i->line;
	q->type = O_QUOTATION;
	q->size = 0;
	q->cap = 8;
	q->items = (struct object **)malloc(sizeof(struct object *) * q->cap);
	push(i, q);
	return (struct object *)q;
}

struct object *quotation_append(struct interpreter *i) {
	struct object *o;
	struct quotation *q;
	o = pop(i);
	q = (struct quotation *)pop(i);
	if (q->type != O_QUOTATION) {
		runtime_error(i->frame->line, "can't append to a non-quotation object");
		return NIL;
	}
	if ((q->size + 1) >= q->cap) {
		q->cap *= 2;
		q->items = (struct object **)realloc(q->items, sizeof(struct object *) * q->cap);
		if (q->items == NULL) {
			fatal("out of memory to grow quotation");
		}
	}
	q->items[(q->size)++] = o;
	push(i, (struct object *)q);
	return (struct object *)q;
}

struct object *quotation_eval(struct interpreter *i) {
	int j;
	struct quotation *q;
	struct word *w;
	q = (struct quotation *)pop(i);
	if (q->type != O_QUOTATION) {
		runtime_error("can't evaluate a non-quotation object");
		return NIL;
	}
	for (j = 0; j < q->size; j++) {
		switch (q->items[j]->type) {
			case O_SYMBOL:
				if (((struct symbol *)q->items[j])->frozen) {
					push(i, q->items[j]);
					break;
				}
				w = lookup(i, SYMBOL(q->items[j])->string);
				if (w == NULL) {
					runtime_error("unknown word `%s'", SYMBOL(q->items[j])->string);
					return NIL;
				}
				if (call(i, w, SYMBOL(q->items[j])->line) == NIL) {
					return NIL;
				}
				break;
			case O_QUOTATION:
				push(i, q->items[j]);
				break;
		}
	}
	return peek(i);
}

#define builtin(n,f) (define(i, n, f, NULL))

struct word *define(struct interpreter *i, char *name, wordfn fn, struct quotation *quote) {
	struct word *w;
	if ((w = lookup(i, name)) == NULL) {
		w = (struct word *)malloc(sizeof(struct word));
		w->name = strdup(name);
		w->next = i->words;
		i->words = w;
	}
	w->fn = fn;
	w->quote = quote;
	return w;
}

struct object *parse(struct parser *p, struct interpreter *i) {
	int t, indef = 0, qdepth = 0;
	char *defname;
	i->line = 1;
	quotation_new(i);
	while (((t = scan(p)) != T_EOF) && (!i->done)) {
		switch (t) {
			case T_BDEF:
				if (indef) {
					parse_error(i->line, "can't define inside a definition");
				}
				if (qdepth > 0) {
					parse_error(i->line, "can't define inside a quotation");
				}
				if ((t = scan(p)) != T_WORD) {
					parse_error(i->line, "expecting word after definition");
				}
				defname = strdup(token(p));
				indef = 1;
				quotation_new(i);
				break;
			case T_EDEF:
				if (!indef) {
					parse_error(i->line, "unexpected `;': not inside a definition");
				}
				if (qdepth != 0) {
					parse_error(i->line, "mismatched quotes inside of `%s'", defname);
				}
				indef = 0;
				define(i, defname, NULL, (struct quotation *)pop(i));
				break;
			case T_BQUOTE:
				qdepth++;
				quotation_new(i);
				break;
			case T_EQUOTE:
				qdepth--;
				if (qdepth < 0) {
					parse_error(i->line, "unexpected `]': no matching `['\n");
				}
				quotation_append(i);
				break;
			case T_SYMBOL:
				symbol_new(i, token(p), 1);
				quotation_append(i);
				break;
			case T_WORD:
				symbol_new(i, token(p), 0);
				quotation_append(i);
				break;
			case T_EOL:
				i->line++;
				break;
			case T_EOF:
				return NIL;
			default:
				return NIL;
		}
	}
	return peek(i);
}

void tree(struct object *o, int depth) {
	int j;
	switch (o->type) {
		case O_QUOTATION:
			printf("[ ");
			for (j = 0; j < ((struct quotation *)o)->size; j++) {
				tree(((struct quotation *)o)->items[j], depth + 1);
			}
			printf("] ");
			break;
		case O_SYMBOL:
			printf("%s ", ((struct symbol *)o)->string);
			break;
	}
}

#define expect(i, v, t) ( \
	v = pop(i); \
	if ((v)->type != O_##t) { \
		runtime_error("quotation expected"); \
		return CC_ERR; \
	})

int show_word(struct interpreter *i) {
	int j;
	printf("%d: ", i->top);
	for (j = 0; j < i->top; j++) {
		tree(i->stack[j], 0);
	}
	printf("\n");
	return CC_OK;
}

int zap_word(struct interpreter *i) {
	if (i->top <= 0) {
		runtime_error("stack empty");
		return CC_ERR;
	}
	pop(i);
	return CC_OK;
}

int empty_word(struct interpreter *i) {
	i->top = 0;
	return CC_OK;
}

int dup_word(struct interpreter *i) {
	struct object *o;
	struct quotation *q;
	if (i->top <= 0) {
		runtime_error("stack empty");
		return CC_ERR;
	}
	o = peek(i);
	switch (o->type) {
		case O_QUOTATION:
			if ((q = (struct quotation *)malloc(sizeof(struct quotation))) == NULL) {
				fatal("out of memory to duplicate quotation");
			}
			q->type = O_QUOTATION;
			q->size = ((struct quotation *)o)->size;
			q->cap = ((struct quotation *)o)->cap;
			if ((q->items = (struct object **)malloc(sizeof(struct object *) * q->cap)) == NULL) {
				fatal("out of memory to duplicate quotation");
			}
			memcpy(q->items, ((struct quotation *)o)->items, sizeof(struct object *) * q->cap);
			push(i, q);
			break;
		default:
			push(i, o);
			break;
	}
	return CC_OK;
}

int swap_word(struct interpreter *i) {
	struct object *a, *b;
	a = pop(i);
	b = pop(i);
	push(i, a);
	push(i, b);
	return CC_OK;
}

int cat_word(struct interpreter *i) {
	int j;
	struct object *a, *b;
	a = pop(i);
	if (a->type != O_QUOTATION) {
		push(i, a);
		runtime_error("quotation expected");
		return CC_ERR;
	}
	b = pop(i);
	if (b->type != O_QUOTATION) {
		push(i, b);
		runtime_error("quotation expected");
		return CC_ERR;
	}
	push(i, b);
	for (j = 0; j < QUOTATION(a)->size; j++) {
		push(i, QUOTATION(a)->items[j]);
		quotation_append(i);
	}
	return CC_OK;
}

int cons_word(struct interpreter *i) {
	int j;
	struct object *a, *b;
	a = pop(i);
	if (a->type != O_QUOTATION) {
		push(i, a);
		runtime_error("quotation expected");
		return CC_ERR;
	}
	b = pop(i);
	quotation_new(i);
	push(i, b);
	quotation_append(i);
	for (j = 0; j < QUOTATION(a)->size; j++) {
		push(i, QUOTATION(a)->items[j]);
		quotation_append(i);
	}
	return CC_OK;
}

int i_word(struct interpreter *i) {
	struct object *a;
	a = pop(i);
	if (a->type != O_QUOTATION) {
		push(i, a);
		runtime_error("quotation expected");
		return CC_ERR;
	}
	push(i, a);
	quotation_eval(i);
	return CC_OK;
}

int dip_word(struct interpreter *i) {
	struct object *a, *b;
	a = pop(i);
	if (a->type != O_QUOTATION) {
		push(i, a);
		runtime_error("quotation expected");
		return CC_ERR;
	}
	b = pop(i);
	push(i, a);
	quotation_eval(i);
	push(i, b);
	return CC_OK;
}

int unit_word(struct interpreter *i) {
	struct object *a;
	a = pop(i);
	quotation_new(i);
	push(i, a);
	quotation_append(i);
	return CC_OK;
}

int quit_word(struct interpreter *i) {
	i->done = 1;
	exit(0);
	return CC_OK;
}

void init(struct parser *p, struct interpreter *i) {
	p->c = T_INIT;
	p->buffer = NULL;
	p->bufused = 0;
	p->bufsize = 1;
	i->done = 0;
	i->top = 0;
	i->cap = 10;
	if ((i->stack = (struct object **)malloc(sizeof(struct object *) * i->cap)) == NULL) {
		fatal("out of memory to allocate stack");
	}
	i->words = NULL;
	i->frame = NULL;
	builtin("zap",   zap_word);
	builtin("empty", empty_word);
	builtin("i",     i_word);
	builtin("unit",  unit_word);
	builtin("dup",   dup_word);
	builtin("cat",   cat_word);
	builtin("swap",  swap_word);
	builtin("cons",  cons_word);
	builtin("dip",   dip_word);
	builtin("show",  show_word);
	builtin("exit",  quit_word);
}

int main(int argc, char **argv) {
	struct parser p;
	struct interpreter i;
	struct object *o;
	init(&p, &i);
	if (argc == 1) {
		char *prompt = ">>> ";
		char *line = NULL;
		while (1) {
			line = readline(prompt);
			if (line == NULL) {
				break;
			}
			add_history(line);
			p.source = line;
			o = parse(&p, &i);
			if (o == NIL) continue;
			if (o->type != O_QUOTATION) {
				error(0, "invalid parse\n");
			}
			else {
				quotation_eval(&i);
				show_word(&i);
			}
			i.done = 0;
			p.c = T_INIT;
		}
	}
	else if (argc == 2) {
		char buf[1024*16];
		FILE *fp = fopen(argv[1], "r");
		if (!fp) {
			perror("open");
			exit(1);
		}
		buf[fread(buf, 1, 1024*16, fp)] = '\0';
		fclose(fp);
		p.source = buf;
		o = parse(&p, &i);
		if (o == NIL) return 1;
		if (o->type != O_QUOTATION) {
			error(0, "invalid parse\n");
		}
		else {
			quotation_eval(&i);
			show_word(&i);
		}
    }
	return 0;
}

