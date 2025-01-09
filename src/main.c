#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/axiom.h"
#include "engine/tactics.h"
#include "examples/main_utils.h"
#include "examples/rewrite_multi_argument.h"
#include "examples/rewrite_single_argument.h"
#include "examples/rewrite_under_lambda.h"
#include "examples/rewrite_open_holes.h"
#include "examples/rewrite_let_in.h"
#include "kernel/context.h"
#include "kernel/expression.h"
#include "kernel/utils.h"

void print_usage() {
  fprintf(stderr, "Usage: ./main [--proof=0|1] <example> [args]\n");
  fprintf(stderr, "Available examples:\n");
  fprintf(stderr, "  gfa <f_length> <g_wrap>\n");
  fprintf(stderr, "  haa <h_depth>\n");
  fprintf(stderr, "  let <n_depth>\n");
  fprintf(stderr, "  lambda\n");
  fprintf(stderr, "  open\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  int proof_flag = 1;
  if (strncmp(argv[1], "--proof=", 8) == 0) {
    proof_flag = atoi(argv[1] + 8);
    if (proof_flag != 0 && proof_flag != 1) {
      fprintf(stderr,
              "Invalid value for --proof. Use --proof=0 or --proof=1.\n");
      return 1;
    }
    argc--;
    argv++;
  }

  if (argc < 2) {
    print_usage();
    return 1;
  }

  if (strcmp(argv[1], "gfa") == 0) {
    if (argc != 4) {
      fprintf(stderr, "Usage: %s [--proof=0|1] gfa <f_length> <g_wrap>\n",
              argv[0]);
      return 1;
    }
    int f_length = atoi(argv[2]);
    int g_wrap = atoi(argv[3]);
    RewriteProof *rw_pf = rewrite_gfa(f_length, g_wrap);
    if (proof_flag == 0) {
      print_rwpf__no_proof(rw_pf);
    } else {
      print_rwpf__coq_ready(rw_pf);
    }
  } else if (strcmp(argv[1], "haa") == 0) {
    if (argc != 3) {
      fprintf(stderr, "Usage: %s [--proof=0|1] haa <h_depth>\n", argv[0]);
      return 1;
    }
    int h_depth = atoi(argv[2]);
    RewriteProof *rw_pf = rewrite_haa(h_depth);
    if (proof_flag == 0) {
      print_rwpf__no_proof(rw_pf);
    } else {
      print_rwpf__coq_ready(rw_pf);
    }
  } else if (strcmp(argv[1], "let") == 0) {
    if (argc != 3) {
      fprintf(stderr, "Usage: %s [--proof=0|1] let <n_depth>\n", argv[0]);
      return 1;
    }
    int n_depth = atoi(argv[2]);
    RewriteProof *rw_pf = rewrite_let_in__sharing(n_depth);
    if (proof_flag == 0) {
      print_rwpf__no_proof(rw_pf);
    } else {
      print_rwpf__coq_ready(rw_pf);
    }
  } else if (strcmp(argv[1], "lambda") == 0) {
    RewriteProof *rw_pf = rewrite_lambda_f_x();
    if (proof_flag == 0) {
      print_rwpf__no_proof(rw_pf);
    } else {
      print_rwpf__coq_ready(rw_pf);
    }
  } else if (strcmp(argv[1], "open") == 0) {
    RewriteProof *rw_pf = rewrite_open_holes();
    if (proof_flag == 0) {
      print_rwpf__no_proof(rw_pf);
    } else {
      print_rwpf__coq_ready(rw_pf);
    }
  } else {
    fprintf(stderr, "Unknown example: %s\n", argv[1]);
    print_usage();
    return 1;
  }

  return 0;
}
