#ifndef COLOR_H
#define COLOR_H

// Regular text
#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"
#define DIM "\e[2m"
#define GRAY "\e[90m"

// Regular background
#define BLKB "\e[40m"
#define REDB "\e[41m"
#define GRNB "\e[42m"
#define YELB "\e[43m"
#define BLUB "\e[44m"
#define MAGB "\e[45m"
#define CYNB "\e[46m"
#define WHTB "\e[47m"

// Special
#define BOLD "\033[1m"
#define CRESET "\e[0m"

// Semantic color definitions for UI elements
#define UI GRAY
#define HEADER BLU BOLD
#define GOALCLR WHT
#define CTXCLR GRN
#define PROMPT MAG BOLD
#define SUCCESS GRN
#define ERROR RED
#define WARNING YEL
#define INFO CYN
#define DIMTEXT DIM

#endif  // COLOR_H