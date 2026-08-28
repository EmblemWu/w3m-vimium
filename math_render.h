/*
 * math_render.h - LaTeX & MathML to Unicode Math formula rendering engine
 * for w3m-vimium
 */
#ifndef MATH_RENDER_H
#define MATH_RENDER_H

#include "Str.h"

/*
 * Checks if a string contains LaTeX mathematical formula notation
 * and converts it to a clean, readable Unicode math expression.
 * If the string is not a formula, returns a copy of the original string.
 */
char *render_math_latex(const char *src);

#endif /* MATH_RENDER_H */
