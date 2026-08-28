/*
 * math_render.c - LaTeX & MathML to Unicode Math formula rendering engine
 * for w3m-vimium
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "math_render.h"
#include "Str.h"

typedef struct {
    const char *tex;
    const char *utf8;
} MathSymbolMap;

/* Greek Letters */
static const MathSymbolMap greek_symbols[] = {
    {"\\alpha", "α"}, {"\\beta", "β"}, {"\\gamma", "γ"}, {"\\delta", "δ"},
    {"\\epsilon", "ε"}, {"\\varepsilon", "ε"}, {"\\zeta", "ζ"}, {"\\eta", "η"},
    {"\\theta", "θ"}, {"\\vartheta", "θ"}, {"\\iota", "ι"}, {"\\kappa", "κ"},
    {"\\lambda", "λ"}, {"\\mu", "μ"}, {"\\nu", "ν"}, {"\\xi", "ξ"},
    {"\\pi", "π"}, {"\\varpi", "ϖ"}, {"\\rho", "ρ"}, {"\\varrho", "ϱ"},
    {"\\sigma", "σ"}, {"\\varsigma", "ς"}, {"\\tau", "τ"}, {"\\upsilon", "υ"},
    {"\\phi", "φ"}, {"\\varphi", "ϕ"}, {"\\chi", "χ"}, {"\\psi", "ψ"},
    {"\\omega", "ω"},
    {"\\Gamma", "Γ"}, {"\\Delta", "Δ"}, {"\\Theta", "Θ"}, {"\\Lambda", "Λ"},
    {"\\Xi", "Ξ"}, {"\\Pi", "Π"}, {"\\Sigma", "Σ"}, {"\\Upsilon", "Υ"},
    {"\\Phi", "Φ"}, {"\\Psi", "Ψ"}, {"\\Omega", "Ω"},
    {NULL, NULL}
};

/* Mathematical Operators & Relations */
static const MathSymbolMap op_symbols[] = {
    {"\\geq", "≥"}, {"\\ge", "≥"}, {"\\leq", "≤"}, {"\\le", "≤"},
    {"\\neq", "≠"}, {"\\ne", "≠"}, {"\\approx", "≈"}, {"\\equiv", "≡"},
    {"\\sim", "∼"}, {"\\simeq", "≃"}, {"\\cong", "≅"}, {"\\propto", "∝"},
    {"\\in", "∈"}, {"\\notin", "∉"}, {"\\subset", "⊂"}, {"\\subseteq", "⊆"},
    {"\\supset", "⊃"}, {"\\supseteq", "⊇"}, {"\\cap", "∩"}, {"\\cup", "∪"},
    {"\\setminus", "∖"}, {"\\emptyset", "∅"}, {"\\varnothing", "∅"},
    {"\\forall", "∀"}, {"\\exists", "∃"}, {"\\nexists", "∄"}, {"\\infty", "∞"},
    {"\\to", "→"}, {"\\rightarrow", "→"}, {"\\longrightarrow", "→"},
    {"\\leftarrow", "←"}, {"\\longleftarrow", "←"}, {"\\leftrightarrow", "↔"},
    {"\\Rightarrow", "⇒"}, {"\\Longrightarrow", "⇒"}, {"\\Leftarrow", "⇐"},
    {"\\Leftrightarrow", "⇔"}, {"\\iff", "⇔"}, {"\\mapsto", "↦"},
    {"\\pm", "±"}, {"\\mp", "∓"}, {"\\times", "×"}, {"\\cdot", "·"},
    {"\\div", "÷"}, {"\\circ", "∘"}, {"\\bullet", "•"}, {"\\ast", "∗"},
    {"\\star", "★"}, {"\\ldots", "…"}, {"\\cdots", "…"}, {"\\dots", "…"},
    {"\\vdots", "⋮"}, {"\\ddots", "⋱"},
    {"\\sum", "∑"}, {"\\prod", "∏"}, {"\\coprod", "∐"},
    {"\\int", "∫"}, {"\\iint", "∬"}, {"\\iiint", "∭"}, {"\\oint", "∮"},
    {"\\partial", "∂"}, {"\\nabla", "∇"}, {"\\angle", "∠"},
    {"\\perp", "⊥"}, {"\\parallel", "∥"}, {"\\mid", "|"}, {"\\nmid", "∤"},
    {"\\land", "∧"}, {"\\lor", "∨"}, {"\\neg", "¬"}, {"\\lnot", "¬"},
    {"\\oplus", "⊕"}, {"\\otimes", "⊗"}, {"\\odot", "⊙"},
    {NULL, NULL}
};

/* Blackboard Bold */
static const MathSymbolMap bb_symbols[] = {
    {"\\mathbb{R}", "ℝ"}, {"\\mathbb{N}", "ℕ"}, {"\\mathbb{Z}", "ℤ"},
    {"\\mathbb{Q}", "ℚ"}, {"\\mathbb{C}", "ℂ"}, {"\\mathbb{P}", "ℙ"},
    {"\\mathbb{E}", "𝔼"}, {"\\mathbb{H}", "ℍ"}, {"\\mathbb{K}", "𝕂"},
    {NULL, NULL}
};

static const char *get_subscript_char(char c)
{
    switch (c) {
    case '0': return "₀";
    case '1': return "₁";
    case '2': return "₂";
    case '3': return "₃";
    case '4': return "₄";
    case '5': return "₅";
    case '6': return "₆";
    case '7': return "₇";
    case '8': return "₈";
    case '9': return "₉";
    case 'a': return "ₐ";
    case 'e': return "ₑ";
    case 'h': return "ₕ";
    case 'i': return "ᵢ";
    case 'j': return "ⱼ";
    case 'k': return "ₖ";
    case 'l': return "ₗ";
    case 'm': return "ₘ";
    case 'n': return "ₙ";
    case 'o': return "ₒ";
    case 'p': return "ₚ";
    case 'r': return "ᵣ";
    case 's': return "ₛ";
    case 't': return "ₜ";
    case 'u': return "ᵤ";
    case 'v': return "ᵥ";
    case 'x': return "ₓ";
    case '+': return "₊";
    case '-': return "₋";
    case '=': return "₌";
    case '(': return "₍";
    case ')': return "₎";
    default: return NULL;
    }
}

static const char *get_superscript_char(char c)
{
    switch (c) {
    case '0': return "⁰";
    case '1': return "¹";
    case '2': return "²";
    case '3': return "³";
    case '4': return "⁴";
    case '5': return "⁵";
    case '6': return "⁶";
    case '7': return "⁷";
    case '8': return "⁸";
    case '9': return "⁹";
    case '+': return "⁺";
    case '-': return "⁻";
    case '=': return "⁼";
    case '(': return "⁽";
    case ')': return "⁾";
    case 'a': return "ᵃ";
    case 'b': return "ᵇ";
    case 'c': return "ᶜ";
    case 'd': return "ᵈ";
    case 'e': return "ᵉ";
    case 'f': return "ᶠ";
    case 'g': return "ᵍ";
    case 'h': return "ʰ";
    case 'i': return "ⁱ";
    case 'j': return "ʲ";
    case 'k': return "ᵏ";
    case 'l': return "ˡ";
    case 'm': return "ᵐ";
    case 'n': return "ⁿ";
    case 'o': return "ᵒ";
    case 'p': return "ᵖ";
    case 'r': return "ʳ";
    case 's': return "ˢ";
    case 't': return "ᵗ";
    case 'u': return "ᵘ";
    case 'v': return "ᵛ";
    case 'w': return "ʷ";
    case 'x': return "ˣ";
    case 'y': return "ʸ";
    case 'z': return "ᶻ";
    case 'T': return "ᵀ";
    default: return NULL;
    }
}

static int is_latex_math(const char *s)
{
    if (s == NULL)
	return 0;
    if (strstr(s, "{\\displaystyle") || strstr(s, "{\\textstyle") ||
	strstr(s, "\\displaystyle") || strstr(s, "\\frac") ||
	strstr(s, "\\sum") || strstr(s, "\\int") || strstr(s, "\\begin{") ||
	strstr(s, "\\mathbf") || strstr(s, "\\mathbb") || strstr(s, "\\operatorname") ||
	strstr(s, "\\alpha") || strstr(s, "\\beta") || strstr(s, "\\geq") ||
	strstr(s, "\\leq") || strstr(s, "\\in") || strstr(s, "\\forall") ||
	strstr(s, "\\exists") || strstr(s, "\\times") || strstr(s, "\\sqrt") ||
	strstr(s, "\\partial") || strstr(s, "\\infty"))
	return 1;
    return 0;
}

/* Extract content inside balanced braces {...} */
static char *extract_braced(const char **p_ptr)
{
    const char *p = *p_ptr;
    while (*p && isspace((unsigned char)*p))
	p++;
    if (*p != '{')
	return NULL;
    p++;
    const char *start = p;
    int depth = 1;
    while (*p && depth > 0) {
	if (*p == '{')
	    depth++;
	else if (*p == '}')
	    depth--;
	p++;
    }
    int len = (p - 1) - start;
    if (len < 0)
	len = 0;
    char *buf = malloc(len + 1);
    if (buf) {
	memcpy(buf, start, len);
	buf[len] = '\0';
    }
    *p_ptr = p;
    return buf;
}

static void render_math_internal(const char *src, Str out)
{
    const char *p = src;

    while (*p) {
	/* Skip whitespace cleanly */
	if (*p == '{') {
	    /* Check if it's a wrapper like {\displaystyle ...} */
	    if (strncmp(p, "{\\displaystyle", 14) == 0) {
		p += 14;
		continue;
	    }
	    if (strncmp(p, "{\\textstyle", 11) == 0) {
		p += 11;
		continue;
	    }
	    p++;
	    continue;
	}
	if (*p == '}') {
	    p++;
	    continue;
	}

	/* Handle LaTeX macro commands */
	if (*p == '\\') {
	    /* Spacing macros */
	    if (p[1] == ',' || p[1] == ';' || p[1] == '!' || p[1] == ' ') {
		Strcat_char(out, ' ');
		p += 2;
		continue;
	    }
	    if (strncmp(p, "\\quad", 5) == 0) {
		Strcat_charp(out, "  ");
		p += 5;
		continue;
	    }
	    if (strncmp(p, "\\qquad", 6) == 0) {
		Strcat_charp(out, "   ");
		p += 6;
		continue;
	    }

	    /* Escaped characters */
	    if (p[1] == '{' || p[1] == '}' || p[1] == '_' || p[1] == '%' || p[1] == '&' || p[1] == '$') {
		Strcat_char(out, p[1]);
		p += 2;
		continue;
	    }

	    /* Sizing / Style modifiers to strip */
	    if (strncmp(p, "\\displaystyle", 13) == 0) {
		p += 13;
		continue;
	    }
	    if (strncmp(p, "\\textstyle", 10) == 0) {
		p += 10;
		continue;
	    }
	    if (strncmp(p, "\\left", 5) == 0) {
		p += 5;
		while (*p && isspace((unsigned char)*p)) p++;
		if (*p == '.') { p++; }
		continue;
	    }
	    if (strncmp(p, "\\right", 6) == 0) {
		p += 6;
		while (*p && isspace((unsigned char)*p)) p++;
		if (*p == '.') { p++; }
		continue;
	    }
	    if (strncmp(p, "\\bigl", 5) == 0 || strncmp(p, "\\bigr", 5) == 0 ||
		strncmp(p, "\\Bigl", 5) == 0 || strncmp(p, "\\Bigr", 5) == 0 ||
		strncmp(p, "\\biggl", 6) == 0 || strncmp(p, "\\biggr", 6) == 0) {
		while (*p && !isspace((unsigned char)*p) && *p != '(' && *p != ')' && *p != '[' && *p != ']')
		    p++;
		continue;
	    }

	    /* Blackboard Bold */
	    int matched = 0;
	    for (int i = 0; bb_symbols[i].tex != NULL; i++) {
		int tlen = strlen(bb_symbols[i].tex);
		if (strncmp(p, bb_symbols[i].tex, tlen) == 0) {
		    Strcat_charp(out, bb_symbols[i].utf8);
		    p += tlen;
		    matched = 1;
		    break;
		}
	    }
	    if (matched) continue;

	    /* Fractions: \frac{A}{B} */
	    if (strncmp(p, "\\frac", 5) == 0) {
		p += 5;
		char *num = extract_braced(&p);
		char *den = extract_braced(&p);
		if (num && den) {
		    Str s_num = Strnew();
		    Str s_den = Strnew();
		    render_math_internal(num, s_num);
		    render_math_internal(den, s_den);
		    Strcat_charp(out, "(");
		    Strcat(out, s_num);
		    Strcat_charp(out, " / ");
		    Strcat(out, s_den);
		    Strcat_charp(out, ")");
		    free(num);
		    free(den);
		    continue;
		}
		if (num) free(num);
		if (den) free(den);
	    }

	    /* Square root: \sqrt{A} */
	    if (strncmp(p, "\\sqrt", 5) == 0) {
		p += 5;
		char *arg = extract_braced(&p);
		if (arg) {
		    Str s_arg = Strnew();
		    render_math_internal(arg, s_arg);
		    Strcat_charp(out, "√(");
		    Strcat(out, s_arg);
		    Strcat_charp(out, ")");
		    free(arg);
		    continue;
		}
	    }

	    /* Text wrappers: \mathbf{...}, \mathrm{...}, \operatorname{...}, \text{...} */
	    if (strncmp(p, "\\mathbf", 7) == 0 || strncmp(p, "\\mathrm", 7) == 0 ||
		strncmp(p, "\\operatorname", 13) == 0 || strncmp(p, "\\text", 5) == 0 ||
		strncmp(p, "\\mbox", 5) == 0 || strncmp(p, "\\mathcal", 8) == 0) {
		while (*p && *p != '{') p++;
		char *inner = extract_braced(&p);
		if (inner) {
		    render_math_internal(inner, out);
		    free(inner);
		    continue;
		}
	    }

	    /* Greek symbols */
	    matched = 0;
	    for (int i = 0; greek_symbols[i].tex != NULL; i++) {
		int tlen = strlen(greek_symbols[i].tex);
		if (strncmp(p, greek_symbols[i].tex, tlen) == 0 &&
		    (!isalpha((unsigned char)p[tlen]))) {
		    Strcat_charp(out, greek_symbols[i].utf8);
		    p += tlen;
		    matched = 1;
		    break;
		}
	    }
	    if (matched) continue;

	    /* Operators & Relations */
	    matched = 0;
	    for (int i = 0; op_symbols[i].tex != NULL; i++) {
		int tlen = strlen(op_symbols[i].tex);
		if (strncmp(p, op_symbols[i].tex, tlen) == 0 &&
		    (!isalpha((unsigned char)p[tlen]))) {
		    /* Add space around binary relations */
		    if (strcmp(op_symbols[i].utf8, "≥") == 0 ||
			strcmp(op_symbols[i].utf8, "≤") == 0 ||
			strcmp(op_symbols[i].utf8, "≠") == 0 ||
			strcmp(op_symbols[i].utf8, "≈") == 0 ||
			strcmp(op_symbols[i].utf8, "≡") == 0 ||
			strcmp(op_symbols[i].utf8, "∈") == 0 ||
			strcmp(op_symbols[i].utf8, "∉") == 0 ||
			strcmp(op_symbols[i].utf8, "→") == 0 ||
			strcmp(op_symbols[i].utf8, "⇒") == 0) {
			Strcat_char(out, ' ');
			Strcat_charp(out, op_symbols[i].utf8);
			Strcat_char(out, ' ');
		    } else {
			Strcat_charp(out, op_symbols[i].utf8);
		    }
		    p += tlen;
		    matched = 1;
		    break;
		}
	    }
	    if (matched) continue;

	    /* Unknown macro: skip backslash and word */
	    p++;
	    while (*p && isalpha((unsigned char)*p))
		p++;
	    continue;
	}

	/* Subscripts: _a or _{abc} */
	if (*p == '_') {
	    p++;
	    if (*p == '{') {
		p++;
		while (*p && *p != '}') {
		    const char *sub = get_subscript_char(*p);
		    if (sub)
			Strcat_charp(out, sub);
		    else
			Strcat_char(out, *p);
		    p++;
		}
		if (*p == '}') p++;
	    } else if (*p) {
		const char *sub = get_subscript_char(*p);
		if (sub)
		    Strcat_charp(out, sub);
		else {
		    Strcat_char(out, '_');
		    Strcat_char(out, *p);
		}
		p++;
	    }
	    continue;
	}

	/* Superscripts: ^2 or ^{abc} */
	if (*p == '^') {
	    p++;
	    if (*p == '{') {
		p++;
		while (*p && *p != '}') {
		    const char *sup = get_superscript_char(*p);
		    if (sup)
			Strcat_charp(out, sup);
		    else
			Strcat_char(out, *p);
		    p++;
		}
		if (*p == '}') p++;
	    } else if (*p) {
		const char *sup = get_superscript_char(*p);
		if (sup)
		    Strcat_charp(out, sup);
		else {
		    Strcat_char(out, '^');
		    Strcat_char(out, *p);
		}
		p++;
	    }
	    continue;
	}

	/* Normal characters */
	Strcat_char(out, *p);
	p++;
    }
}

char *render_math_latex(const char *src)
{
    if (src == NULL || *src == '\0')
	return (char *)src;

    if (!is_latex_math(src))
	return (char *)src;

    Str out = Strnew();
    render_math_internal(src, out);

    /* Post-clean multiple spaces */
    Str clean = Strnew();
    int last_was_space = 0;
    for (int i = 0; i < out->length; i++) {
	char c = out->ptr[i];
	if (isspace((unsigned char)c)) {
	    if (!last_was_space && clean->length > 0) {
		Strcat_char(clean, ' ');
		last_was_space = 1;
	    }
	} else {
	    Strcat_char(clean, c);
	    last_was_space = 0;
	}
    }

    return clean->ptr;
}
